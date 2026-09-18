// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vuoi - virtual installation CLI. Offline mode: plan/materialize/verify over
// a local blob store. --streaming-fetch runs
// the boot-equivalent flow over HTTP (plan -> fetch missing -> materialize).
// With none of the vfs modes given, the legacy listing path (uoi_load_contents)
// runs unchanged.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <map>
#include <vector>
#include <external/popts.h>
#include <xuocore/http.h>
#include <xuocore/mft.h>
#include <xuocore/uo_installer.h>
#include <xuocore/vfs.h>

static po::parser s_cli;

static const char *VUOI_VERIFY_STATUS_NAME[] = { "OK", "MISSING-BLOB", "MISSING-BLOCK", "CORRUPT" };

// per-asset label: a state-DB stub reports missing-block (the bucket), but
// the flag upgrades the printed label so placeholders are visible as such
static const char *vuoi_verify_label(const vfs_verify_asset &asset)
{
    return asset.stubbed ? "STUB" : VUOI_VERIFY_STATUS_NAME[asset.status];
}

static bool init_cli(int argc, char *argv[])
{
    s_cli["help"].abbreviation('h').description("print this help screen");
    s_cli["plan"].description("build the asset plan from the blob store and persist it");
    s_cli["materialize"].description(
        "materialize install files from the blob store (offline, idempotent)");
    s_cli["verify"].description(
        "verify materialized files against plan and blobs "
        "(exit 0: clean, nonzero: corrupt assets or internal failure)");
    s_cli["streaming-fetch"].description(
        "streamed install: plan, fetch what is missing over http and "
        "materialize (needs --path; --product accepts a url or a file)");
    s_cli["streaming-offline"].description(
        "streaming: never hit the network, blob store only");
    s_cli["check"].description(
        "check for game data updates: the remote product serial against "
        "the installed plan (read-only; exit 0 up-to-date, 1 update "
        "available, nonzero failure)");
    s_cli["update"].description(
        "update the streamed install: apply the plan diff, fetch the "
        "missing assets and materialize (needs --path)");
    s_cli["streaming-workers"].type(po::u32).description(
        "streaming: download workers (default cpu-1, max 8)");
    s_cli["streaming-record"].type(po::string).description(
        "streaming: record asset accesses to <file.csv>");
    s_cli["streaming-record-scenario"].type(po::string).description(
        "streaming recorder scenario name");
    s_cli["embed-record"].type(po::string).description(
        "deployment: convert a recorded CSV into a"
        " vfs_manifest_embedded.cpp table on stdout");
    s_cli["blobs"]
        .abbreviation('b')
        .type(po::string)
        .description("blob store directory (mirrors the patch server URL tree)");
    s_cli["path"].abbreviation('o').type(po::string).description("install path");
    s_cli["product"]
        .abbreviation('p')
        .type(po::string)
        .description(
            "product manifest: resolved against the CWD first, then inside the blob store "
            "(default: uo-legacyrelease.prod)");
    s_cli(argc, argv);

    return s_cli["help"].size() == 0;
}

static int vuoi_plan()
{
    VfsPlan plan;
    if (!vfs_plan_rebuild(plan))
    {
        fprintf(stderr, "plan: failed\n");
        return -int(mft_could_not_download_file);
    }
    printf("plan: %s (serial %" PRIu64 ")\n", "ok", plan.product_serial);
    size_t assets = 0;
    size_t holes = 0;
    uint64_t bytes = 0;
    for (const auto &file : plan.files)
    {
        const char *kind = file.is_uop ? "uop  " : "loose";
        printf(
            "  %s %-28s %12" PRIu64 " bytes  %zu assets\n",
            kind,
            file.name.c_str(),
            file.size,
            file.assets.size());
        for (const auto &asset : file.assets)
        {
            ++assets;
            bytes +=
                asset.file_offset ? uint64_t(asset.ml) + (asset.ct == 0 ? asset.ul : asset.cl) : 0;
            if (vfs_asset_is_hole(asset))
                ++holes;
        }
    }
    printf(
        "summary: %zu files, %zu assets (%zu holes), %" PRIu64 " block bytes\n",
        plan.files.size(),
        assets,
        holes,
        bytes);
    return 0;
}

static int vuoi_materialize()
{
    vfs_materialize_stats stats;
    if (!vfs_materialize(&stats))
    {
        fprintf(stderr, "materialize: failed (io errors or unusable blobs)\n");
        return -int(mft_write_error);
    }
    printf(
        "materialize: %zu/%zu assets (%zu written, %zu present, %zu holes, %zu missing blobs, %zu failed), %" PRIu64
        " bytes written\n",
        stats.assets_fetched + stats.assets_present + stats.assets_hole,
        stats.assets_total,
        stats.assets_fetched,
        stats.assets_present,
        stats.assets_hole,
        stats.assets_missing,
        stats.assets_failed,
        stats.bytes_written);
    return 0;
}

static int vuoi_verify()
{
    vfs_verify_result result;
    const int corrupt = vfs_verify(&result);
    if (corrupt < 0)
    {
        fprintf(stderr, "verify: failed\n");
        return -int(mft_invalid_format);
    }
    for (const auto &file : result.files)
    {
        printf(
            "  %-32s %zu ok, %zu missing-blob, %zu missing-block, %zu corrupt%s\n",
            file.name.c_str(),
            file.counts[vfs_verify_ok],
            file.counts[vfs_verify_missing_blob],
            file.counts[vfs_verify_missing_block],
            file.counts[vfs_verify_corrupt],
            file.sealed ? " [sealed]" : "");
        for (const auto &asset : file.assets)
        {
            if (asset.status != vfs_verify_ok)
            {
                if (!asset.name.empty())
                    printf("    %s: %s\n", asset.name.c_str(), vuoi_verify_label(asset));
                else
                    printf("    %016" PRIx64 ": %s\n", asset.uop_hash, vuoi_verify_label(asset));
            }
        }
    }
    printf(
        "verify: %s (%zu files, %d corrupt assets)\n",
        result.corrupted ? "CORRUPT" : "OK",
        result.files.size(),
        corrupt);
    return corrupt > 0 ? -int(mft_decompress_error) : 0;
}

// ---------------------------------------------------------------------------
// --embed-record (deployment step): converts a recorder CSV
// (hash,pack,ul,tier_hint,first_access_ms,scenario) into the generated
// vfs_manifest_embedded.cpp - the table the client compiles in as its T1
// order. Recorded order is preserved; malformed/duplicate hashes are skipped
// with a warning (stale or unknown entries can only cost extra T2 faults).
// ---------------------------------------------------------------------------

struct vuoi_embedded_entry
{
    uint64_t hash = 0;
    astr_t pack; // pack name (hash != 0) or the loose-file name (hash == 0)
};

// one RFC4180 field from the current line position; unquotes as needed
static astr_t vuoi_csv_field(const char **cursor)
{
    const char *p = *cursor;
    astr_t field;
    if (*p == '"')
    {
        ++p;
        while (*p != '\0')
        {
            if (*p == '"')
            {
                if (p[1] == '"')
                {
                    field += '"';
                    p += 2;
                    continue;
                }
                ++p;
                break;
            }
            field += *p++;
        }
    }
    else
    {
        while (*p != '\0' && *p != ',' && *p != '\n' && *p != '\r')
            field += *p++;
    }
    if (*p == ',')
        ++p;
    *cursor = p;
    return field;
}

static int vuoi_embed_record()
{
    const auto path = fs_path_from(s_cli["embed-record"].get().string.c_str());
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
    {
        fprintf(stderr, "embed-record: could not open %s\n", fs_path_ascii(path));
        return -int(mft_could_not_open_path);
    }
    std::vector<uint8_t> data;
    char buf[4096];
    size_t got = 0;
    while ((got = fread(buf, 1, sizeof(buf), fp)) > 0)
        data.insert(data.end(), buf, buf + got);
    fclose(fp);
    astr_t csv(data.begin(), data.end());

    std::vector<vuoi_embedded_entry> entries;
    std::map<astr_t, bool> seen;
    size_t skipped = 0;
    size_t line_no = 0;
    size_t pos = 0;
    while (pos <= csv.size())
    {
        const size_t eol = csv.find('\n', pos);
        const astr_t line =
            csv.substr(pos, eol == astr_t::npos ? astr_t::npos : eol - pos);
        pos = eol == astr_t::npos ? csv.size() + 1 : eol + 1;
        ++line_no;
        if (line.empty())
            continue; // blank line: skip, do not stop the parse
        const char *cursor = line.c_str();
        const astr_t hash_text = vuoi_csv_field(&cursor);
        const astr_t pack = vuoi_csv_field(&cursor);
        if (line_no == 1 && hash_text == "hash")
            continue; // header
        if (hash_text.empty() && pack.empty())
            continue;
        uint64_t hash = 0;
        char *end = nullptr;
        hash = strtoull(hash_text.c_str(), &end, 16);
        if (end == nullptr || *end != '\0')
        {
            fprintf(stderr, "embed-record: line %zu: bad hash \"%s\", skipped\n",
                    line_no, hash_text.c_str());
            ++skipped;
            continue;
        }
        // identity: packs by hash, loose files by name
        const astr_t key = hash != 0 ? astr_t("p/") + pack + "/" + hash_text
                                     : astr_t("l/") + pack;
        if (seen.find(key) != seen.end())
        {
            fprintf(stderr, "embed-record: line %zu: duplicate %s, skipped\n",
                    line_no, key.c_str() + 2);
            ++skipped;
            continue;
        }
        seen[key] = true;
        entries.push_back({ hash, pack });
    }

    printf("// SPDX-License-Identifier: AGPL-3.0-or-later\n");
    printf("// SPDX-FileCopyrightText: 2026 CrossUO Team\n");
    printf("\n");
    printf("// vfs_manifest_embedded: GENERATED by vuoi --embed-record\n");
    printf("// from a recorded run; %zu entries in first-access order.\n", entries.size());
    printf("// Stale lists only reorder T1 - they can never break correctness.\n");
    printf("\n");
    printf("#include \"vfs_manifest.h\"\n");
    printf("\n");
    printf("static const std::vector<vfs_embedded_entry> s_entries = {\n");
    for (const auto &entry : entries)
    {
        if (entry.hash != 0)
            printf("    { 0x%016llx, \"%s\", \"\" },\n",
                   (unsigned long long)entry.hash, entry.pack.c_str());
        else
            printf("    { 0, \"\", \"%s\" },\n", entry.pack.c_str());
    }
    printf("};\n");
    printf("\n");
    printf("const std::vector<vfs_embedded_entry> &vfs_manifest_embedded()\n");
    printf("{\n");
    printf("    return s_entries;\n");
    printf("}\n");
    fprintf(stderr, "embed-record: %zu entries, %zu skipped\n", entries.size(), skipped);
    return 0;
}

// shared vfs params for the streaming verbs (--streaming-fetch/--update/--check)
static vfs_params vuoi_params_build()
{
    vfs_params params;
    if (s_cli["blobs"].was_set())
        params.blobs_dir = fs_path_from(s_cli["blobs"].get().string.c_str());
    if (s_cli["path"].was_set())
        params.install_dir = fs_path_from(s_cli["path"].get().string.c_str());
    if (s_cli["product"].was_set())
    {
        const auto product = s_cli["product"].get().string;
        if (product.rfind("http://", 0) == 0 || product.rfind("https://", 0) == 0)
            params.product_url = product; // downloaded into the blob store
        else
            params.product_file = fs_path_from(product.c_str());
    }
    params.enabled = true;
    params.offline = s_cli["streaming-offline"].was_set();
    // the client's layout: the blob store lives under the install dir when
    // not given explicitly (boot requires both)
    if (fs_path_empty(params.blobs_dir) && !fs_path_empty(params.install_dir))
        params.blobs_dir = fs_path_join(params.install_dir, "cache", "blobs");
    if (s_cli["streaming-workers"].was_set())
        params.workers = s_cli["streaming-workers"].get().u32;
    if (s_cli["streaming-record"].was_set())
    {
        params.record_path = fs_path_from(s_cli["streaming-record"].get().string.c_str());
        if (s_cli["streaming-record-scenario"].was_set())
            params.record_scenario = s_cli["streaming-record-scenario"].get().string;
    }
    return params;
}

static int vuoi_streaming_fetch()
{
    const vfs_params params = vuoi_params_build();
    vfs_set_params(params);

    http_init();
    if (!vfs_boot(nullptr))
    {
        vfs_shutdown();
        fprintf(stderr, "streaming-fetch: boot failed (see log)\n");
        return -int(mft_could_not_download_file);
    }
    vfs_boot_wait(); // headless: finish the background tier too

    VfsStats stats;
    vfs_stats(&stats);
    printf(
        "streaming-fetch: %zu/%zu files, %" PRIu64 "/%" PRIu64 " bytes%s\n",
        stats.files_done,
        stats.files_total,
        stats.bytes_done,
        stats.bytes_total,
        params.offline ? " (offline)" : "");
    const int corrupt = vuoi_verify();
    vfs_shutdown();
    return corrupt;
}

// --update: converge the streamed install to the latest product. The boot
// applies the plan diff, fetch_all pulls the remaining delta (all tiers,
// the update wave included), materialize writes it out
static int vuoi_update()
{
    vfs_set_params(vuoi_params_build());

    http_init();
    if (!vfs_boot(nullptr))
    {
        vfs_shutdown();
        fprintf(stderr, "update: boot failed (see log)\n");
        return -int(mft_could_not_download_file);
    }
    if (!vfs_fetch_all(nullptr))
        fprintf(stderr, "update: some downloads failed (see log)\n");
    vfs_boot_wait(); // finish the background tier + update wave

    vfs_materialize_stats stats;
    const bool materialized = vfs_materialize(&stats);
    printf(
        "update: %zu written, %zu present, %zu missing, %zu failed\n",
        stats.assets_fetched,
        stats.assets_present,
        stats.assets_missing,
        stats.assets_failed);
    vfs_shutdown();
    return materialized ? 0 : -int(mft_write_error);
}

// --check: read-only product-serial comparison against the installed plan
static int vuoi_check()
{
    vfs_set_params(vuoi_params_build());

    http_init();
    vfs_update_info info;
    const bool ok = vfs_update_check(&info);
    http_shutdown();
    if (!ok)
    {
        fprintf(stderr, "check: failed (see log)\n");
        return -int(mft_could_not_download_file);
    }
    if (!info.plan_found)
    {
        printf("check: no streamed install found\n");
        return 1;
    }
    if (info.update_available)
        printf(
            "check: update available (plan %" PRIu64 ", product %" PRIu64 ")\n",
            info.local_serial,
            info.remote_serial);
    else
        printf("check: up to date (serial %" PRIu64 ")\n", info.remote_serial);
    return info.update_available ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (!init_cli(argc, argv))
    {
        s_cli.print_help(std::cout);
        return 0;
    }

    const bool vfs_mode =
        s_cli["plan"].was_set() || s_cli["materialize"].was_set() || s_cli["verify"].was_set();
    if (s_cli["embed-record"].was_set())
        return vuoi_embed_record();
    if (s_cli["check"].was_set())
        return vuoi_check();
    if (s_cli["update"].was_set() || s_cli["streaming-fetch"].was_set())
        return s_cli["update"].was_set() ? vuoi_update() : vuoi_streaming_fetch();
    if (!vfs_mode)
    {
        http_init();
        uoi_load_contents();
        return 0;
    }

    vfs_params params;
    if (s_cli["blobs"].was_set())
        params.blobs_dir = fs_path_from(s_cli["blobs"].get().string.c_str());
    if (s_cli["path"].was_set())
        params.install_dir = fs_path_from(s_cli["path"].get().string.c_str());
    if (s_cli["product"].was_set())
        params.product_file = fs_path_from(s_cli["product"].get().string.c_str());
    vfs_set_params(params);

    if (s_cli["plan"].was_set())
        return vuoi_plan();
    if (s_cli["materialize"].was_set())
        return vuoi_materialize();
    return vuoi_verify();
}
