// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <vector>
#include <map>
#include <external/popts.h>
#include <external/lookup3.h>
#include <external/miniz.h>
#include <common/checksum.h>
#include "mft.h"
#include "http.h"
#include "xuo_updater.h"
#include "xuocore/uop.h"
#include "common.h"

#define XUOI_MAX_DOWNLOAD_SIZE (1024 * 1024 * 1024)

#define XUOI_THREADED 1
#if XUOI_THREADED
#include <atomic>
#include <thread>
#define XUOI_THREAD_COUNT (std::thread::hardware_concurrency() - 1)
#else
#define XUOI_THREAD_COUNT (1)
#endif

#include <atomic>
#include <thread>
static size_t s_total = 0;
static std::atomic<size_t> s_received{ 0 };
static std::atomic<int32_t> s_total_files{ 0 };

#define DATA_OFFICIAL_SERVER "http://patch.uo.eamythic.com/"
#define DATA_PRODUCT_SERVER_ADDRESS "uopatch-sa/legacyrelease/uo/manifest/"
#define DATA_PRODUCT_FILE "uo-legacyrelease.prod"
#define DATA_PATCHER_SERVER_ADDRESS "uopatch-sa/legacyrelease/patcher/manifest/"
#define DATA_PATCHER_FILE "patcher.prod"
#define XUOI_AGENT_NAME "EAMythic Patch Client"

static void xuoi_listing_save(mft_product &prod)
{
    auto path = fs_path_join(prod.config.product_path, "cache", "xuoi.csv");
    fs_path_create(fs_directory(path));
    FILE *fp = fopen(fs_path_ascii(path), "wb");
    if (!fp)
        return;

    do
    {
        fprintf(
            fp,
            "%s,0,0,%" PRIu64 ",0,0,0,0,0,%s,%s\n",
            prod.launchfile,
            prod.timestamp,
            prod.file_repo,
            prod.manifest_repo);
        for (const auto &e : prod.files)
        {
            if (e.state == state_download_failed)
                continue;

            char tmp[512] = {};
            const auto len = strlen(e.name);
            assert(len < sizeof(tmp));
            memcpy(tmp, e.name, len);
            str_lower(tmp);
            uint32_t ph = 0, sh = 0;
            hashlittle2(tmp, len, &ph, &sh);
            fprintf(
                fp,
                "%s,%08x,%08x,%" PRIu64 ",%08x,%zu,%zu,0,0\n",
                e.name,
                ph,
                sh,
                e.timestamp,
                e.hash,
                e.uncompressed_len,
                e.compressed_len);
        }
        for (const auto &e : prod.parts)
        {
            if (e.state == state_download_failed)
                continue;

            fprintf(
                fp,
                "%s/%08x%08x,%08x,%08x,%" PRIu64 ",%08x,%zu,%zu,%08x,%u\n",
                e.pack_name,
                e.ph,
                e.sh,
                e.ph,
                e.sh,
                e.timestamp,
                e.hash,
                e.uncompressed_len,
                e.compressed_len,
                e.meta_crc,
                e.meta_len);
        }
    } while (0);
    fclose(fp);
}

template <typename T>
static void xuoi_listing_load_version(mft_product &prod, const astr_t &version, T &data)
{
    if (version.empty())
        return;

    auto path =
        fs_path_join(prod.config.output_path, version, prod.launchfile, "cache", "xuoi.csv");
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
        return;

    char tmp[512], endline;
    fscanf(fp, "%s\n", tmp); // skip first line
    int count = 0;
    while (!feof(fp))
    {
        int t;
        mft_entry e;
        fscanf(
            fp,
            "%512[^,],%08x,%08x,%" PRIu64 ",%08x,%zu,%zu,%08x,%u%c%n",
            tmp,
            &e.ph,
            &e.sh,
            &e.timestamp,
            &e.hash,
            &e.uncompressed_len,
            &e.compressed_len,
            &e.meta_crc,
            &e.meta_len,
            &endline,
            &t);

        astr_t n = tmp;
        if (!e.timestamp && !e.hash && !e.uncompressed_len && !e.ph)
            break;

        count++;
        data[n] = e;
        //fprintf(stdout, "%s,%08x,%08x,%" PRIu64 ",%08x,%zu,%zu\n", n.c_str(), e.ph, e.sh, e.timestamp, e.hash, e.uncompressed_len, e.compressed_len);
    };

    fclose(fp);
    assert(data.size() == count);
    LOG_INFO("Loaded version %s(%s), with %d entries.", prod.launchfile, version.c_str(), count);
}

template <typename T>
static int xuoi_diff(mft_product &prod, const T &data1, const T &data2)
{
    auto path = fs_path_join(prod.config.output_path, "diff.csv");
    FILE *fp = fopen(fs_path_ascii(path), "wb");
    if (!fp)
        return -1;

    fprintf(fp, "name,ph,sh,timestamp,hash,ulen,clen,mc,ml,asset_id,asset_name\n");

    int differences = 0;
    int removed = 0;
    int added = 0;
    for (const auto &it1 : data1)
    {
        const auto &it2 = data2.find(it1.first);
        if (it2 == data2.end())
            continue;

        auto e = it1.second;
        auto o = it2->second;
        // timestamp can differ even if the asset did not change
        auto diff = o.hash != e.hash || o.uncompressed_len != e.uncompressed_len ||
                    o.compressed_len != e.compressed_len;
        e.state = diff ? state_need_update : state_none;
        if (!diff)
            continue;

        differences++;
        uint32_t id = 0;
        const auto asset = uop_asset_name(it1.first.c_str(), UOP_HASH(e.ph, e.sh), &id);
        fprintf(
            fp,
            "%s,%08x,%08x,%" PRIu64 ",%08x,%zu,%zu,%08x,%u,0x%04x,%s\n",
            it1.first.c_str(),
            e.ph,
            e.sh,
            e.timestamp,
            e.hash,
            e.uncompressed_len,
            e.compressed_len,
            e.meta_crc,
            e.meta_len,
            id,
            asset.c_str());
    }

    // only left side
    for (const auto &it1 : data1)
    {
        const auto &it2 = data2.find(it1.first);
        if (it2 != data2.end())
            continue;
        auto e = it1.second;
        removed++;
        uint32_t id = 0;
        const auto asset = uop_asset_name(it1.first.c_str(), UOP_HASH(e.ph, e.sh), &id);
        fprintf(
            fp,
            "<%s,%08x,%08x,%" PRIu64 ",%08x,%zu,%zu,%08x,%u,0x%04x,%s\n",
            it1.first.c_str(),
            e.ph,
            e.sh,
            e.timestamp,
            e.hash,
            e.uncompressed_len,
            e.compressed_len,
            e.meta_crc,
            e.meta_len,
            id,
            asset.c_str());
    }
    // only right side
    for (const auto &it2 : data2)
    {
        const auto &it1 = data1.find(it2.first);
        if (it1 != data1.end())
            continue;
        auto e = it2.second;
        added++;
        uint32_t id = 0;
        const auto asset = uop_asset_name(it2.first.c_str(), UOP_HASH(e.ph, e.sh), &id);
        fprintf(
            fp,
            ">%s,%08x,%08x,%" PRIu64 ",%08x,%zu,%zu,%08x,%u,0x%04x,%s\n",
            it2.first.c_str(),
            e.ph,
            e.sh,
            e.timestamp,
            e.hash,
            e.uncompressed_len,
            e.compressed_len,
            e.meta_crc,
            e.meta_len,
            id,
            asset.c_str());
    }

    fprintf(fp, "different: %d\n", differences);
    fprintf(fp, "removed: %d\n", removed);
    fprintf(fp, "added: %d\n", added);
    fclose(fp);

    return differences + removed + added;
}

static int xuoi_diff(mft_product &prod)
{
    auto path = fs_path_join(
        prod.config.product_path,
        // str_from(prod.last_version),
        // prod.launchfile,
        "cache",
        "diff.csv");
    FILE *fp = fopen(fs_path_ascii(path), "wb");

    int differences = 0;
    for (auto &e : prod.files)
    {
        auto it = prod.base_version.find(e.name);
        if (it == prod.base_version.end())
            continue;

        auto o = it->second;
        // timestamp can differ even if the asset did not change
        auto diff = o.hash != e.hash || o.uncompressed_len != e.uncompressed_len ||
                    o.compressed_len != e.compressed_len;
        e.state = diff ? state_need_update : state_none;
        if (!diff)
            continue;

        differences++;
        if (fp)
        {
            char tmp[512] = {};
            const auto len = strlen(e.name);
            assert(len < sizeof(tmp));
            memcpy(tmp, e.name, len);
            str_lower(tmp);
            uint32_t ph = 0, sh = 0;
            hashlittle2(tmp, len, &ph, &sh);
            fprintf(
                fp,
                "%s,%08x,%08x,%" PRIu64 ",%08x,%zu,%zu,0,0\n",
                e.name,
                ph,
                sh,
                e.timestamp,
                e.hash,
                e.uncompressed_len,
                e.compressed_len);
        }
    }

    char name[32] = {};
    for (auto &e : prod.parts)
    {
        astr_t part = e.pack_name;
        snprintf(name, 32, "%08x%08x", e.ph, e.sh);
        part += "/";
        part += name;

        auto it = prod.base_version.find(part);
        if (it == prod.base_version.end())
            continue;

        auto o = it->second;
        auto diff = o.hash != e.hash || o.uncompressed_len != e.uncompressed_len ||
                    o.compressed_len != e.compressed_len || o.meta_crc != e.meta_crc ||
                    o.meta_len != e.meta_len;
        e.state = diff ? state_need_update : state_none;
        if (!diff)
            continue;

        differences++;
        if (fp)
        {
            fprintf(
                fp,
                "%s/%08x%08x,%08x,%08x,%" PRIu64 ",%08x,%zu,%zu,%08x,%u\n",
                e.pack_name,
                e.ph,
                e.sh,
                e.ph,
                e.sh,
                e.timestamp,
                e.hash,
                e.uncompressed_len,
                e.compressed_len,
                e.meta_crc,
                e.meta_len);
        }
    }

    if (fp)
        fclose(fp);

    return differences;
}

static int xuoi_export_diff(
    mft_config &cfg, const char *launchfile, const astr_t &strVerA, const astr_t &strVerB)
{
    assert(launchfile);
    mft_product mft;
    mft_init(mft, cfg);
    mft.launchfile = launchfile;
    std::map<astr_t, mft_entry> data1, data2;
    xuoi_listing_load_version(mft, strVerA, data1);
    xuoi_listing_load_version(mft, strVerB, data2);
    return xuoi_diff(mft, data1, data2);
}

static mft_result
xuoi_product_install(mft_config &cfg, const char *product_url, const char *product_file)
{
    LOG_INFO("Product address: %s%s", product_url, product_file);

    std::vector<uint8_t> data;
    char tmp[1024] = {};
    snprintf(tmp, sizeof(tmp), "%s%s", product_url, product_file);
    http_get_binary(tmp, data);

    mft_product prod;
    mft_init(prod, cfg);

    auto latest_file = fs_path_join(prod.config.output_path, "latest.txt");
    if (prod.config.mirror_mode)
    {
        prod.config.dump_extracted = false;
        if (FILE *fp = fopen(fs_path_ascii(latest_file), "rb"))
        {
            fscanf(fp, "%" PRIu64 "", &prod.last_version);
            fclose(fp);
        }
        else
        {
            LOG_WARN("No previous version found, a full mirror download will be done.");
        }
    }

    mft_result res = mft_load(prod, data.data(), data.size());
    do
    {
        if (res != mft_ok)
            break;

        if (prod.config.mirror_mode)
        {
            if (!prod.config.latest_checked && prod.last_version == prod.timestamp)
            {
                LOG_INFO("mirror is up-to-date, version %lu.", prod.timestamp);
                break;
            }

            // set config as latest version already checked, so next loop for next product we skip it
            cfg.latest_checked = true;
            xuoi_listing_load_version(prod, std::to_string(prod.last_version), prod.base_version);
            prod.config.product_path = fs_path_join(
                prod.config.output_path, std::to_string(prod.timestamp), prod.launchfile);
        }

        fs_path prod_file = fs_path_join(prod.config.product_path, "cache");
        if (!fs_path_create(prod_file))
            LOG_ERROR("couldn't create output directory: %s", fs_path_ascii(prod_file));

        prod_file = fs_path_join(prod_file, product_file);
        fs_file_write(fs_path_ascii(prod_file), data);
        data.clear();
        snprintf(tmp, sizeof(tmp), "%s%s.sig", product_url, product_file);
        http_get_binary(tmp, data);
        snprintf(tmp, sizeof(tmp), "%s.sig", fs_path_ascii(prod_file));
        fs_file_write(tmp, data);

        res = mft_consume_manifests(prod);
        if (res != mft_ok)
            break;

        if (prod.config.listing_only)
            break;

        if (prod.config.mirror_mode && prod.last_version)
        {
            int diff = xuoi_diff(prod);
            if (!diff)
            {
                LOG_INFO("no differences found. up-to-date.");
                break;
            }
            LOG_INFO("found %d differences, updating.", diff);
        }

        size_t ulen = 0, clen = 0, count = 0;
        for (const auto &e : prod.files)
        {
            if (e.state != state_need_update)
                continue;
            ++count;
            ulen += e.uncompressed_len;
            clen += e.compressed_len;
        }
        s_total += clen;
        LOG_INFO(
            "total files to process: %zu (compressed: %zu, uncompressed: %zu)", count, clen, ulen);

        for (const auto &e : prod.parts)
        {
            if (e.state != state_need_update)
                continue;
            ++count;
            ulen += e.uncompressed_len;
            clen += e.compressed_len;
        }
        s_total += clen;
        LOG_INFO(
            "total packs parts to process: %zu (%zu parts, compressed: %zu, uncompressed: %zu)",
            prod.packs.size(),
            count,
            clen,
            ulen);

        size_t bytes = 0;
        bytes += mft_download_batch(prod, prod.files);
        bytes += mft_download_batch(prod, prod.parts);

        const int MB = 1024 * 1024;
        const size_t received = s_received.fetch_add(bytes) / MB;
        const int32_t files = s_total_files.fetch_add(1);
        if ((received % 128) == 0)
        {
            LOG_INFO(
                "progress: %d files, %zu/%zu MB (%3.2f%%)",
                files,
                received,
                s_total / MB,
                100 * received / float(s_total / MB));
        }

        xuoi_listing_save(prod);
        if (prod.config.mirror_mode)
        {
            if (FILE *fp = fopen(fs_path_ascii(latest_file), "wb"))
            {
                fprintf(fp, "%" PRIu64 "", prod.timestamp);
                fclose(fp);
            }
        }
    } while (0);

    mft_cleanup(prod);

    return res;
}

static void print_banner()
{
    fprintf(stdout, "xuoi - crossuo installer 0.0.1\n");
    fprintf(stdout, "Copyright (c) 2019 Danny Angelo Carminati Grein\n");
    fprintf(
        stdout, "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n");
    fprintf(stdout, "\n");
}

static po::parser s_cli;
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
    s_cli(argc, argv);

    return s_cli["help"].size() == 0;
}

int main(int argc, char **argv)
{
    if (!init_cli(argc, argv) || !s_cli["path"].was_set())
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

    crc32_init();
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
