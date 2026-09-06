// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Installer

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <external/popts.h>
#include <common/checksum.h>
#include <xuocore/mft.h>
#include <xuocore/http.h>
#include <xuocore/xuoi.h>
#include <xuocore/common.h>
#include <xuocore/uop.h>
#include "keypatch.h"
#include "xuo_updater.h"

#define XUOI_AGENT_NAME "EAMythic Patch Client"
#define XUOI_MAX_DOWNLOAD_SIZE (1024 * 1024 * 1024)

#define XUOI_THREADED 1
#if XUOI_THREADED
#include <atomic>
#include <thread>
#define XUOI_THREAD_COUNT (std::thread::hardware_concurrency() - 1)
#else
#define XUOI_THREAD_COUNT (1)
#endif

#define DATA_OFFICIAL_SERVER "http://patch.uo.eamythic.com/"
#define DATA_PRODUCT_SERVER_ADDRESS "uopatch-sa/legacyrelease/uo/manifest/"
#define DATA_PRODUCT_FILE "uo-legacyrelease.prod"
#define DATA_PATCHER_SERVER_ADDRESS "uopatch-sa/legacyrelease/patcher/manifest/"
#define DATA_PATCHER_FILE "patcher.prod"

static po::parser s_cli;

static bool xuoi_keypatch_hex_to_bin(const char *hex, std::vector<uint8_t> &out)
{
    const size_t len = strlen(hex);
    if (len == 0 || (len & 1) != 0)
        return false;
    out.reserve(len / 2);
    for (size_t i = 0; i < len; i += 2)
    {
        char buf[3] = { hex[i], hex[i + 1], 0 };
        char *end = nullptr;
        const long v = strtol(buf, &end, 16);
        if (!end || *end != 0 || v < 0 || v > 0xff)
            return false;
        out.push_back(uint8_t(v));
    }
    return true;
}

static bool xuoi_keypatch_load_data(const char *src, std::vector<uint8_t> &out, size_t max_size)
{
    if (FILE *fp = fopen(src, "rb"))
    {
        fseek(fp, 0, SEEK_END);
        const long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (size <= 0 || size_t(size) > max_size)
        {
            fclose(fp);
            return false;
        }
        out.resize(size_t(size));
        const bool ok = fread(out.data(), 1, size_t(size), fp) == size_t(size);
        fclose(fp);
        return ok;
    }
    return xuoi_keypatch_hex_to_bin(src, out);
}

static void xuoi_keypatch_print_blob(const uint8_t blob[keypatch::KEY_BLOB_LEN])
{
    const int e_len = blob[138];
    const int e_start = 142 + (3 - e_len);
    LOG_INFO("key blob: len=%u type=%u version=%u", blob[0], blob[2], blob[4]);
    LOG_INFO("modulus (%u bytes):", blob[5]);
    for (int i = 0; i < 129; i += 32)
    {
        char line[97] = {};
        for (int j = i; j < i + 32 && j < 129; ++j)
            snprintf(&line[(j - i) * 2], 3, "%02x", blob[9 + j]);
        LOG_INFO("  %s", line);
    }
    LOG_INFO("exponent: %u bytes", e_len);
    for (int i = 0; i < e_len; ++i)
        fprintf(stdout, "%02x", blob[e_start + i]);
    fprintf(stdout, "\n");
}

static int xuoi_keypatch_run()
{
    char err[256] = {};
    const bool show = s_cli["showkey"].was_set();
    const bool patch = s_cli["keypatch"].was_set();
    const char *exepath =
        (patch ? s_cli["keypatch"] : s_cli["showkey"]).get().string.c_str();

    if (show)
    {
        std::vector<uint8_t> image;
        if (!xuoi_keypatch_load_data(exepath, image, 64 * 1024 * 1024))
        {
            LOG_ERROR("couldn't read launcher image: %s", exepath);
            return -1;
        }
        size_t offset = 0;
        uint32_t va = 0;
        keypatch::find_key_ctor(image.data(), image.size(), &offset, &va, err, sizeof(err));
        uint8_t blob[keypatch::KEY_BLOB_LEN] = {};
        if (!keypatch::read_patched_key(image.data(), image.size(), blob, err, sizeof(err)))
        {
            LOG_ERROR("%s", err);
            LOG_ERROR("launcher not patched yet? use --keypatch to replace the key");
            return -1;
        }
        LOG_INFO("patched key found at %s", exepath);
        xuoi_keypatch_print_blob(blob);
        return 0;
    }

    std::vector<uint8_t> modulus;
    std::vector<uint8_t> exponent;
    if (!s_cli["modulus"].was_set())
    {
        LOG_ERROR("--keypatch requires --modulus (raw big-endian file path or hex string)");
        return -1;
    }
    const char *modsrc = s_cli["modulus"].get().string.c_str();
    const char *expsrc = s_cli["exponent"].was_set() ? s_cli["exponent"].get().string.c_str() : "010001";
    if (!xuoi_keypatch_load_data(modsrc, modulus, keypatch::KEY_MODULUS_LEN))
    {
        LOG_ERROR("couldn't load modulus (raw file or hex string): %s", modsrc);
        return -1;
    }
    if (!xuoi_keypatch_hex_to_bin(expsrc, exponent) || exponent.empty())
    {
        LOG_ERROR("invalid exponent hex string: %s", expsrc);
        return -1;
    }
    if (modulus.empty() || modulus.size() > keypatch::KEY_MODULUS_LEN)
    {
        LOG_ERROR("invalid modulus size: %zu (max %zu)", modulus.size(), keypatch::KEY_MODULUS_LEN);
        return -1;
    }

    uint8_t blob[keypatch::KEY_BLOB_LEN] = {};
    if (!keypatch::build_key_blob(modulus.data(), modulus.size(), exponent.data(), exponent.size(), blob))
    {
        LOG_ERROR("failed to build key blob");
        return -1;
    }
    if (!keypatch::patch_file(exepath, blob, err, sizeof(err)))
    {
        LOG_ERROR("patch failed: %s", err);
        return -1;
    }

    std::vector<uint8_t> image_buf;
    uint8_t verify[keypatch::KEY_BLOB_LEN] = {};
    if (!xuoi_keypatch_load_data(exepath, image_buf, 64 * 1024 * 1024) ||
        !keypatch::read_patched_key(image_buf.data(), image_buf.size(), verify, err, sizeof(err)) ||
        memcmp(verify, blob, keypatch::KEY_BLOB_LEN) != 0)
    {
        LOG_ERROR("post-patch verification failed");
        return -1;
    }
    LOG_INFO("launcher key replaced successfully: %s", exepath);
    xuoi_keypatch_print_blob(blob);
    return 0;
}

static void print_banner()
{
    fprintf(stdout, "xuoi - crossuo installer 0.0.1\n");
    fprintf(stdout, "Copyright (c) 2020 Danny Angelo Carminati Grein\n");
    fprintf(
        stdout,
        "License AGPLv3+: GNU Affero GPL version 3 or later <http://gnu.org/licenses/agpl.html>.\n");
    fprintf(stdout, "\n");
}

static bool init_cli(int argc, char *argv[])
{
    s_cli["help"].abbreviation('h').description("print this help screen");
    s_cli["listing"]
        .abbreviation('l')
        .type(po::string)
        .description("only dump install file listing");
    s_cli["path"].abbreviation('o').type(po::string).description("install path");
    s_cli["product"]
        .abbreviation('p')
        .type(po::string)
        .description("product manifest (patcher.prod, uo-legacyrelease.prod)");
    s_cli["mirror"]
        .abbreviation('m')
        .type(po::string)
        .description(
            "mirror path - only mirror server files, checking for updates against previous version");
    s_cli["xuo"].abbreviation('x').description("install/update CrossUO client");
    s_cli["debug"].abbreviation('g').description("use localhost debug servers");
    s_cli["diff"]
        .abbreviation('d')
        .type(po::string)
        .description("output diff information between two releases: versionA,versionB");
    s_cli["keypatch"]
        .abbreviation('k')
        .type(po::string)
        .description(
            "replace the public key embedded in the UO.exe launcher (path); requires --modulus");
    s_cli["modulus"]
        .abbreviation('n')
        .type(po::string)
        .description("RSA modulus for --keypatch: raw big-endian file path or hex string");
    s_cli["exponent"]
        .abbreviation('e')
        .type(po::string)
        .description("RSA exponent hex string for --keypatch (default 010001)");
    s_cli["showkey"]
        .abbreviation('s')
        .type(po::string)
        .description("show the embedded public key of a patched UO.exe launcher (path)");
    s_cli(argc, argv);

    return s_cli["help"].size() == 0;
}

static void
xuoi_listing_load_version(mft_product &prod, const astr_t &version, mft_entries_map &data)
{
    if (version.empty())
        return;

    const auto path =
        fs_path_join(prod.config.output_path, version, prod.launchfile, "cache", "xuoi.csv");
    const int count = xuoi_listing_load(path, data);
    LOG_INFO("Loaded version %s(%s), with %d entries.", prod.launchfile, version.c_str(), count);
}

static int xuoi_export_diff(
    mft_config &cfg, const char *launchfile, const astr_t &strVerA, const astr_t &strVerB)
{
    assert(launchfile);
    mft_product mft;
    mft_init(mft, cfg);
    mft.launchfile = launchfile;
    mft_entries_map data1, data2;
    xuoi_listing_load_version(mft, strVerA, data1);
    xuoi_listing_load_version(mft, strVerB, data2);
    return xuoi_diff(mft, data1, data2);
}

int main(int argc, char **argv)
{
    if (!init_cli(argc, argv))
    {
        print_banner();
        s_cli.print_help(std::cout);
        return 0;
    }

    if (s_cli["keypatch"].was_set() || s_cli["showkey"].was_set())
        return xuoi_keypatch_run();

    if (!s_cli["path"].was_set())
    {
        print_banner();
        s_cli.print_help(std::cout);
        return 0;
    }

    const char *outdir = s_cli["path"].get().string.c_str();
    auto outpath = fs_path_from(outdir);
    if (!fs_path_create(outpath))
    {
        LOG_ERROR("couldn't create output directory: %s", outdir);
        return -int(mft_could_not_open_path);
    }

    mft_config cfg;
    cfg.agent_name = XUOI_AGENT_NAME;
    cfg.download_buffer_size = XUOI_MAX_DOWNLOAD_SIZE;
    cfg.thread_count = XUOI_THREAD_COUNT;
    cfg.mirror_mode = s_cli["mirror"].was_set();
    cfg.listing_only = s_cli["listing"].was_set();
    cfg.output_path = outpath;

    http_init();
    int exit_code = 0;
    const char *name = xuo_platform_name();
    LOG_INFO("Platform: %s", name);

    if (s_cli["diff"].was_set())
    {
        uop_populate_asset_names();
        astr_t diff = s_cli["diff"].get().string;
        const auto pos = strchr(diff.data(), ',') - diff.data();
        auto ver1 = astr_t(diff.data(), pos);
        auto ver2 = astr_t(diff.data() + pos + 1, diff.length() - pos - 1);
        LOG_INFO("Diffing %s <> %s", ver1.c_str(), ver2.c_str());
        return xuoi_export_diff(cfg, "client.exe", ver1, ver2);
    }

    if (s_cli["xuo"].was_set())
    {
        auto ctx = xuo_init(outdir);
        auto r = xuo_update_apply(ctx);
        xuo_shutdown(ctx);
        return r;
    }

    const char *debug_product_urls[] = { "http://localhost/" DATA_PATCHER_SERVER_ADDRESS,
                                         "http://localhost/" DATA_PRODUCT_SERVER_ADDRESS };
    const char *product_urls[] = { DATA_OFFICIAL_SERVER DATA_PATCHER_SERVER_ADDRESS,
                                   DATA_OFFICIAL_SERVER DATA_PRODUCT_SERVER_ADDRESS };
    const char *product_list[] = { DATA_PATCHER_FILE, DATA_PRODUCT_FILE };
    const int product_max = 2;
    const bool specific_product = s_cli["product"].was_set();
    int product_idx = 0;
    int product_count = product_max;
    if (specific_product)
    {
        astr_t product_name = s_cli["product"].get().string;
        for (int i = 0; i < product_max; ++i)
        {
            if (product_name.compare(product_list[i]) == 0)
            {
                product_idx = i;
                product_count = 1;
                break;
            }
        }
    }

    const bool debug_server = s_cli["debug"].was_set();
    const char **server_urls = debug_server ? debug_product_urls : product_urls;

    int count = 0;
    for (int i = product_idx; i < product_max && count < product_count; ++i, ++count)
    {
        if (auto res = xuoi_product_install(cfg, server_urls[i], product_list[i]))
        {
            LOG_ERROR("installation failed with error: %d", res);
            exit_code = -int(res);
        }
    }
    http_shutdown();
    return exit_code;
}
