// AGPLv3 License
// Copyright (c) 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Installer

#include "mft.h"
#include "http.h"
#include "common.h"
#include <inttypes.h>

#define DATA_PRODUCT_SERVER "http://patch.uo.eamythic.com/"
#define DATA_PRODUCT_PATH "uopatch-sa/legacyrelease/uo/manifest/"
#define DATA_PRODUCT_URL DATA_PRODUCT_SERVER DATA_PRODUCT_PATH
#define DATA_PRODUCT_FILE "uo-legacyrelease.prod"
#define UOI_AGENT_NAME "EAMythic Patch Client"
#define UOI_MAX_DOWNLOAD_SIZE (1024 * 1024 * 1024)

bool uoi_load_contents()
{
    size_t expect_size = 0;
    size_t received_size = 0;
    int expect_files = 0;
    int received_files = 0;
    fs_path cache_path = fs_path_join(fs_path_appdata(), "xuolauncher", "uo");
    if (!fs_path_create(cache_path))
    {
        LOG_ERROR("failed to create cache directory: %s", fs_path_ascii(cache_path));
        return false;
    }

    mft_config cfg;
    cfg.agent_name = UOI_AGENT_NAME;
    cfg.download_buffer_size = UOI_MAX_DOWNLOAD_SIZE;
    cfg.thread_count = 4;
    cfg.listing_only = true;
    cfg.output_path = cache_path;

    std::vector<astr_t> failures;
    std::vector<uint8_t> data;
    char tmp[1024] = {};
    snprintf(tmp, sizeof(tmp), "%s%s", DATA_PRODUCT_URL, DATA_PRODUCT_FILE);
    LOG_INFO("Product address: %s", tmp);
    if (!http_get_binary(tmp, data))
        return false;

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
    cfg.product_path = fs_path_join(cache_path, std::to_string(prod.timestamp), prod.launchfile);
    prod.config.product_path = cfg.product_path;
    do
    {
        if (res != mft_ok)
            break;

        fs_path prod_file = fs_path_join(cfg.product_path, "cache");
        if (!fs_path_create(prod_file))
        {
            LOG_ERROR("couldn't create output directory: %s", fs_path_ascii(prod_file));
            return false;
        }

        prod_file = fs_path_join(prod_file, DATA_PRODUCT_FILE);
        fs_file_write(fs_path_ascii(prod_file), data);
        data.clear();
        snprintf(tmp, sizeof(tmp), "%s%s.sig", DATA_PRODUCT_URL, DATA_PRODUCT_FILE);
        if (!http_get_binary(tmp, data))
        {
            failures.emplace_back(tmp);
            continue;
        }
        snprintf(tmp, sizeof(tmp), "%s.sig", fs_path_ascii(prod_file));
        fs_file_write(tmp, data);

        res = mft_consume_manifests(prod);
        if (res != mft_ok)
            break;

        /*if (prod.config.mirror_mode && prod.last_version)
        {
            int diff = xuoi_diff(prod);
            if (!diff)
            {
                LOG_INFO("no differences found. up-to-date.");
                break;
            }
            LOG_INFO("found %d differences, updating.", diff);
        }*/

        size_t ulen = 0, clen = 0;
        for (const auto &e : prod.files)
        {
            if (e.state != state_need_update)
                continue;
            ++expect_files;
            ulen += e.uncompressed_len;
            clen += e.compressed_len;
        }
        expect_size += clen;
        LOG_INFO("total files: %u (compressed: %zu, uncompressed: %zu)", expect_files, clen, ulen);

        int parts = 0;
        for (const auto &e : prod.parts)
        {
            if (e.state != state_need_update)
                continue;
            ++expect_files;
            ++parts;
            ulen += e.uncompressed_len;
            clen += e.compressed_len;
        }
        expect_size += clen;
        LOG_INFO(
            "total parts to process: %u in %zu packs (compressed: %zu, uncompressed: %zu)",
            parts,
            prod.packs.size(),
            clen,
            ulen);

        if (prod.config.listing_only)
            break;

        received_size += mft_download_batch(prod, prod.files);
        received_size += mft_download_batch(prod, prod.parts);
        //xuoi_listing_save(prod);
        if (FILE *fp = fopen(fs_path_ascii(latest_file), "wb"))
        {
            fprintf(fp, "%" PRIu64 "", prod.timestamp);
            fclose(fp);
        }

        received_files = static_cast<int>(prod.files.size() + prod.parts.size() - failures.size());
    } while (0);

    mft_cleanup(prod);

    if (!failures.empty())
    {
        LOG_ERROR("Failed to download %zu files", failures.size());
        for (const auto &entry : failures)
        {
            LOG_ERROR("\t%s\n", entry.c_str());
        }
    }

    const int MB = 1024 * 1024;
    const int rx = static_cast<int>(received_size / MB);
    const int total = static_cast<int>(expect_size / MB);
    LOG_INFO("Files %u/%u (%u MiB /%u MiB)", received_files, expect_files, rx, total);

    return res == mft_ok;
}
