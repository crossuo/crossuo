// AGPLv3 License
// Copyright (c) 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Installer

#include "uop.h"
#include "mft.h"
#include "http.h"
#include "common.h"
#include <external/lookup3.h>

#include <inttypes.h>
#include <vector>
#include <map>
#include <atomic>
#include <thread>
static std::atomic<size_t> s_received{ 0 };
static std::atomic<int32_t> s_total_files{ 0 };
constexpr int MB = 1024 * 1024;

void xuoi_listing_save(const fs_path &path, const mft_product &prod)
{
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

int xuoi_listing_load(const fs_path &path, mft_entries_map &data)
{
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
        return 0;

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
    return count;
}

int xuoi_diff(mft_product &prod, const mft_entries_map &data1, const mft_entries_map &data2)
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

static int xuoi_diff_mirror(mft_product &prod)
{
    auto path = fs_path_join(prod.config.product_path, "cache", "diff.csv");
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

mft_result xuoi_product_install(mft_config &cfg, const char *product_url, const char *product_file)
{
    size_t expect_size = 0;
    size_t received_size = 0;
    int expect_files = 0;
    int received_files = 0;
    std::vector<astr_t> failures;
    LOG_INFO("Product address: %s%s", product_url, product_file);

    std::vector<uint8_t> data;
    char tmp[1024] = {};
    snprintf(tmp, sizeof(tmp), "%s%s", product_url, product_file);
    if (!http_get_binary(tmp, data))
        return mft_could_not_download_file;

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
            const auto version = std::to_string(prod.last_version);
            // FIXME: mirror mode may need use xuoi_diff instead xuoi_diff_mirror
            const auto base_path = fs_path_join(
                prod.config.output_path, version, prod.launchfile, "cache", "xuoi.csv");
            xuoi_listing_load(base_path, prod.base_version);
            prod.config.product_path = fs_path_join(
                prod.config.output_path, std::to_string(prod.timestamp), prod.launchfile);
        }
        else if (fs_path_empty(prod.config.product_path))
        {
            cfg.product_path =
                fs_path_join(cfg.output_path, std::to_string(prod.timestamp), prod.launchfile);
            prod.config.product_path = cfg.product_path;
        }

        fs_path prod_file = fs_path_join(prod.config.product_path, "cache");
        if (!fs_path_create(prod_file))
            LOG_ERROR("couldn't create output directory: %s", fs_path_ascii(prod_file));

        prod_file = fs_path_join(prod_file, product_file);
        fs_file_write(fs_path_ascii(prod_file), data);
        data.clear();
        snprintf(tmp, sizeof(tmp), "%s%s.sig", product_url, product_file);
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

        if (prod.config.listing_only)
        {
            const auto listing_path = fs_path_join(prod.config.product_path, "cache", "xuoi.csv");
            xuoi_listing_save(listing_path, prod);
            break;
        }

        if (prod.config.mirror_mode && prod.last_version)
        {
            int diff = xuoi_diff_mirror(prod);
            if (!diff)
            {
                LOG_INFO("no differences found. up-to-date.");
                break;
            }
            LOG_INFO("found %d differences, updating.", diff);
        }

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
        LOG_INFO(
            "total files to process: %u (compressed: %zu, uncompressed: %zu)",
            expect_files,
            clen,
            ulen);

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
            "total packs parts to process: %u (%zu parts, compressed: %zu, uncompressed: %zu)",
            parts,
            prod.packs.size(),
            clen,
            ulen);

        received_size += mft_download_batch(prod, prod.files);
        received_size += mft_download_batch(prod, prod.parts);

        const size_t received = s_received.fetch_add(received_size) / MB;
        const int32_t files = s_total_files.fetch_add(1);
        if ((received % 128) == 0)
        {
            LOG_INFO(
                "progress: %d files, %zu/%zu MB (%3.2f%%)",
                files,
                received,
                expect_size / MB,
                100 * received / float(expect_size / MB));
        }

        const auto listing_path = fs_path_join(prod.config.product_path, "cache", "xuoi.csv");
        xuoi_listing_save(listing_path, prod);
        if (prod.config.mirror_mode)
        {
            if (FILE *fp = fopen(fs_path_ascii(latest_file), "wb"))
            {
                fprintf(fp, "%" PRIu64 "", prod.timestamp);
                fclose(fp);
            }
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

    const int rx = static_cast<int>(received_size / MB);
    const int total = static_cast<int>(expect_size / MB);
    LOG_INFO("Files %u/%u (%u MiB /%u MiB)", received_files, expect_files, rx, total);

    return res;
}
