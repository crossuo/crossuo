// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vuoi - virtual installation CLI. M1: offline plan/materialize/verify over a
// local blob store (VUOI plan §6, milestone M1). M2: --streaming-fetch runs
// the boot-equivalent flow over HTTP (plan -> fetch missing -> materialize).
// With none of the vfs modes given, the legacy listing path (uoi_load_contents)
// runs unchanged.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <external/popts.h>
#include <xuocore/http.h>
#include <xuocore/mft.h>
#include <xuocore/uo_installer.h>
#include <xuocore/vfs.h>

static po::parser s_cli;

static const char *VUOI_VERIFY_STATUS_NAME[] = { "OK", "MISSING-BLOB", "MISSING-BLOCK", "CORRUPT" };

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
    s_cli["streaming-workers"].type(po::u32).description(
        "streaming: download workers (default cpu-1, max 8)");
    s_cli["streaming-record"].type(po::string).description(
        "streaming: record asset accesses to <file.csv>");
    s_cli["streaming-record-scenario"].type(po::string).description(
        "streaming recorder scenario name");
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
                    printf(
                        "    %s: %s\n", asset.name.c_str(), VUOI_VERIFY_STATUS_NAME[asset.status]);
                else
                    printf(
                        "    %016" PRIx64 ": %s\n",
                        asset.uop_hash,
                        VUOI_VERIFY_STATUS_NAME[asset.status]);
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

static int vuoi_streaming_fetch()
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
    if (s_cli["streaming-workers"].was_set())
        params.workers = s_cli["streaming-workers"].get().u32;
    if (s_cli["streaming-record"].was_set())
    {
        params.record_path = fs_path_from(s_cli["streaming-record"].get().string.c_str());
        if (s_cli["streaming-record-scenario"].was_set())
            params.record_scenario = s_cli["streaming-record-scenario"].get().string;
    }
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

int main(int argc, char **argv)
{
    if (!init_cli(argc, argv))
    {
        s_cli.print_help(std::cout);
        return 0;
    }

    const bool vfs_mode =
        s_cli["plan"].was_set() || s_cli["materialize"].was_set() || s_cli["verify"].was_set();
    if (s_cli["streaming-fetch"].was_set())
        return vuoi_streaming_fetch();
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
