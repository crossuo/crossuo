// AGPLv3 License
// Copyright (c) 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Data

#include <stdint.h>
#include <inttypes.h>
#include <atomic>
#include <thread>
#include <external/lookup3.h>

#include "mft.h"
#include "http.h"
#include "common.h"

#include <common/utils.h>
#include <common/logging/logging.h>

#define STR_IMPLEMENTATION
#include <common/str.h>

#include <external/zlib_amalg.h>

static mft_result mft_download(
    mft_product &prod,
    const char *repo,
    const char *remote_name,
    const mft_entry &entry,
    const uint8_t *buffer,
    const uint8_t *cbuffer,
    size_t *buffer_size,
    size_t *downloaded_len = nullptr)
{
    assert(repo && remote_name && buffer && buffer_size);
    const bool in_pack = entry.type != mft_entry_manifest;

    // create paths
    fs_path path = fs_path_join(prod.config.product_path, "cache");
    path = fs_path_join(path, entry.remote_path);
    if (entry.pack_name)
        path = fs_path_join(path, entry.pack_name);
    else if (in_pack)
        path = fs_path_join(path, "unpacked");
    path = fs_path_join(path, remote_name);
    fs_path dir = fs_directory(path);
    if (!fs_path_create(dir))
    {
        LOG_ERROR("failed to create directory: %s", fs_path_ascii(dir));
        return mft_could_not_open_path;
    }
    auto lpath = fs_path_ascii(path);

    // build resource url
    char upath[512] = {};
    if (entry.pack_name)
        snprintf(
            upath,
            sizeof(upath),
            "%s%s/%s/%s",
            repo,
            entry.remote_path,
            entry.pack_name,
            remote_name);
    else
        snprintf(
            upath,
            sizeof(upath),
            "%s%s/%s%s",
            repo,
            entry.remote_path,
            in_pack ? "unpacked/" : "",
            remote_name);

    size_t ul = entry.uncompressed_len;
    assert(ul < *buffer_size);

    size_t bytes = 0;
    switch (entry.compression_type)
    {
        case 0:
        {
            if (!fs_file_read(lpath, buffer, buffer_size))
            {
                if (!http_get_binary(upath, buffer, buffer_size))
                    return mft_could_not_download_file;

                fs_file_write(lpath, buffer, *buffer_size); // save to local cache
            }
            bytes = *buffer_size;
        }
        break;
        case 1: // zlib level 6 (default)
        {
            auto cl = entry.compressed_len;
            assert(cl < *buffer_size);
            if (!fs_file_read(lpath, cbuffer, buffer_size))
            {
                if (!http_get_binary(upath, cbuffer, buffer_size))
                    return mft_could_not_download_file;

                fs_file_write(lpath, cbuffer, *buffer_size); // save to local cache
            }
            cl = *buffer_size;
            bytes = cl;
            if (cl != entry.compressed_len)
            {
                LOG_WARN(
                    "(%s)%s: compressed len mismatch: cl = %d, blob = %d",
                    entry.name,
                    upath,
                    int(entry.compressed_len),
                    int(cl));
            }
            auto ol = checked_cast<uLongf>(ul);
            auto il = checked_cast<uLongf>(cl);
            int z_err = uncompress((unsigned char *)buffer, &ol, (unsigned char *)cbuffer, il);
            if (z_err != Z_OK)
            {
                LOG_ERROR("(%s)%s: decompression error %d", entry.name, upath, z_err);
                return mft_decompress_error;
            }

            *buffer_size = ol;
        }
        break;
        default:
            break;
    }

    if (prod.config.dump_extracted)
    {
        auto local_name = entry.name ? entry.name : remote_name;
        path = fs_path_join(prod.config.product_path, "dump", local_name);
        dir = fs_directory(path);
        fs_path_create(dir);
        auto opath = fs_path_ascii(path);
        if (!fs_file_write(opath, buffer, *buffer_size))
        {
            LOG_ERROR("error dumping decompressed data");
            return mft_write_error;
        }
    }

    if (downloaded_len)
        *downloaded_len = bytes;

    return mft_ok;
}

static mft_result mft_entry_load(tinyxml2::XMLElement *node, mft_entry &entry)
{
    auto n = node->Attribute("n");
    if (n)
        entry.name = n;

    entry.uncompressed_len = checked_cast<size_t>(node->Hex64Attribute("ul", 0));
    assert(entry.uncompressed_len < MFT_MAX_DOWNLOAD_SIZE);
    entry.compression_type = node->UnsignedAttribute("ct", 0);
    entry.compressed_len = checked_cast<size_t>(node->Hex64Attribute("cl", 0));
    assert(entry.compressed_len < MFT_MAX_DOWNLOAD_SIZE);
    entry.timestamp = node->Hex64Attribute("t", 0);
    entry.hash = (uint32_t)node->Hex64Attribute("rh", 0);

    entry.ph = (uint32_t)node->Hex64Attribute("ph", 0);
    entry.sh = (uint32_t)node->Hex64Attribute("sh", 0);
    entry.ft = node->UnsignedAttribute("ft", 0);
    entry.meta_len = (uint32_t)node->Hex64Attribute("ml", 0);
    entry.meta_crc = (uint32_t)node->Hex64Attribute("mc", 0);

    if (n)
    {
        // hack as first level pkg.mft may not have correctly reported sizes
        if (strcmp(n, "pkg.mft") == 0)
        {
            entry.compressed_len = 0;
        }
        LOG_TRACE(
            "file> %64s (%10" PRIu64 ", %10" PRIu64 ") @ %10" PRIu64 " [0x%08x]",
            entry.name,
            entry.uncompressed_len,
            entry.compressed_len,
            entry.timestamp,
            entry.hash);
    }
    else
    {
        LOG_TRACE(
            "part> (%10" PRIu64 ", %10" PRIu64 ") @ %10" PRIu64
            " [0x%08x] rpath: %08x%08x (meta 0x%08x, size: %3d) [%d]",
            entry.uncompressed_len,
            entry.compressed_len,
            entry.timestamp,
            entry.hash,
            entry.ph,
            entry.sh,
            entry.meta_crc,
            entry.meta_len,
            entry.ft);
    }

    return mft_ok;
}

static mft_result mft_entries_load(
    tinyxml2::XMLElement *node,
    std::vector<mft_entry> &entries,
    mft_entry_type type,
    const char *remote_path,
    const char *pack_name = nullptr)
{
    const char *tag = type == mft_entry_manifest ? "manifest" : "f";
    auto entry = node->FirstChildElement(tag);
    while (entry)
    {
        mft_entry e;
        e.type = type;
        e.remote_path = remote_path;
        e.pack_name = pack_name;
        mft_entry_load(entry, e);
        entries.emplace_back(e);
        entry = entry->NextSiblingElement(tag);
    }
    return mft_ok;
}

static mft_result
mft_manifests_load(mft_product &prod, tinyxml2::XMLElement *node, const char *remote_path)
{
    return mft_entries_load(node, prod.mft_files, mft_entry_manifest, remote_path);
}

static mft_result
mft_files_load(mft_product &prod, tinyxml2::XMLElement *node, const char *remote_path)
{
    return mft_entries_load(node, prod.files, mft_entry_file, remote_path);
}

static mft_result mft_part_load(
    mft_product &prod, tinyxml2::XMLElement *node, const char *remote_path, const char *pack_name)
{
    return mft_entries_load(node, prod.parts, mft_entry_part, remote_path, pack_name);
}

static mft_result
mft_packs_load(mft_product &prod, tinyxml2::XMLElement *node, const char *remote_path)
{
    auto pack = node->FirstChildElement("p");
    while (pack)
    {
        const char *name = pack->Attribute("name");
        LOG_TRACE("reading pack: %s", name);
        mft_package pkg;
        pkg.name = name;
        pkg.pack_name = pack->Attribute("rpath");
        pkg.remote_path = remote_path;
        pkg.priority = pack->UnsignedAttribute("priority", 0);
        LOG_TRACE("* pack: %s (%s)", pkg.remote_path, pkg.name);
        if (auto files = pack->FirstChildElement("files"))
            if (auto error = mft_part_load(prod, files, pkg.remote_path, pkg.pack_name))
                return error;
        prod.packs.emplace_back(pkg);
        pack = node->NextSiblingElement("p");
    }
    return mft_ok;
}

static mft_result
mft_package_load(mft_product &prod, tinyxml2::XMLElement *node, const char *remote_path)
{
    mft_package pkg;
    pkg.name = node->Attribute("name");
    pkg.remote_path = node->Attribute("rpath");
    if (!pkg.remote_path)
        pkg.remote_path = remote_path;
    pkg.priority = node->UnsignedAttribute("priority", 0);
    LOG_TRACE("* package: %s (%s, prio: %d)", pkg.remote_path, pkg.name, pkg.priority);
    auto r = mft_manifests_load(prod, node, pkg.remote_path);
    prod.packages.emplace_back(pkg);
    return r;
}

static mft_result
mft_stage_load(mft_product &prod, tinyxml2::XMLElement *node, const char *remote_path)
{
    mft_stage stg;
    stg.name = node->Attribute("name");
    stg.verify_unpacked = node->UnsignedAttribute("verifyunpacked", 0);
    stg.priority = node->UnsignedAttribute("priority", 0);
    LOG_TRACE("# stage: %s verify: %d (prio: %d)", stg.name, stg.verify_unpacked, stg.priority);
    auto packages = node->FirstChildElement("packages");
    if (packages)
    {
        auto package = packages->FirstChildElement("package");
        while (package)
        {
            mft_package_load(prod, package, remote_path);
            package = package->NextSiblingElement("package");
        }
    }
    prod.stages.emplace_back(stg);
    return mft_ok;
}

static mft_result
mft_product_load(mft_product &prod, tinyxml2::XMLElement *node, const char *remote_path)
{
    prod.timestamp = node->Unsigned64Attribute("serial", 0);
    prod.launchfile = node->Attribute("launchfile");
    LOG_TRACE("product timestamp: %" PRIu64 "", prod.timestamp);

    if (auto mr = node->FirstChildElement("manifestrepos"))
        if (auto repo = mr->FirstChildElement("repo"))
            if (auto url = repo->Attribute("url"))
                prod.manifest_repo = url;

    if (auto mr = node->FirstChildElement("filerepos"))
        if (auto repo = mr->FirstChildElement("repo"))
            if (auto url = repo->Attribute("url"))
                prod.file_repo = url;

    LOG_TRACE("manifest: %s", prod.manifest_repo);
    LOG_TRACE("files: %s", prod.file_repo);

    if (auto stages = node->FirstChildElement("stages"))
    {
        auto stage = stages->FirstChildElement("stage");
        while (stage)
        {
            mft_stage_load(prod, stage, remote_path);
            stage = stage->NextSiblingElement("stage");
        }
    }
    return mft_ok;
}

mft_result
mft_load(mft_product &prod, const uint8_t *data, size_t data_size, const char *remote_path)
{
    auto mft = new mft_manifest();
    assert(mft);
    prod.manifests.push_back(mft);

    mft->data.resize(data_size);
    memcpy(mft->data.data(), data, data_size);
    mft->doc.Parse((char *)mft->data.data(), mft->data.size());
    if (!mft->remote_path)
        mft->remote_path = remote_path;

    auto root = mft->doc.FirstChildElement("MythicMFT");
    if (!root)
        return mft_invalid_format;

    if (auto product = root->FirstChildElement("product"))
        if (auto error = mft_product_load(prod, product, mft->remote_path))
            return error;

    if (auto manifest = root->FirstChildElement("manifest"))
    {
        if (auto manifests = manifest->FirstChildElement("manifests"))
            if (auto error = mft_manifests_load(prod, manifests, mft->remote_path))
                return error;

        if (auto files = manifest->FirstChildElement("files"))
            if (auto error = mft_files_load(prod, files, mft->remote_path))
                return error;

        if (auto packs = manifest->FirstChildElement("packs"))
            if (auto error = mft_packs_load(prod, packs, mft->remote_path))
                return error;
    }

    return mft_ok;
}

void mft_cleanup(mft_product &prod)
{
    for (auto mft : prod.manifests)
    {
        delete mft;
    }
    prod.manifests.clear();

    for (uint32_t i = 0; i < prod.config.thread_count; ++i)
    {
        free(prod.download_buffers[i]);
        free(prod.download_cbuffers[i]);
    }
    free(prod.download_buffers);
    free(prod.download_cbuffers);
}

mft_result mft_consume_manifests(mft_product &prod)
{
    while (prod.mft_files.size() > 0)
    {
        std::vector<mft_entry> manifests;
        prod.mft_files.swap(manifests);
        for (const auto &entry : manifests)
        {
            LOG_TRACE("loading: %s", entry.name);
            assert(entry.type == mft_entry_manifest);
            mft_product m;
            auto data = prod.download_buffers[0];
            auto cdata = prod.download_cbuffers[0];
            size_t size = prod.config.download_buffer_size;
            if (auto error =
                    mft_download(prod, prod.manifest_repo, entry.name, entry, data, cdata, &size))
                return error;
            if (auto error = mft_load(prod, data, size, entry.remote_path))
                return error;
        }
    }
    return mft_ok;
}

static void mft_entry_remote_name(mft_entry &entry, char name[32])
{
    if (entry.type == mft_entry_part)
    {
        assert(!entry.name);
        assert(entry.ph);
        assert(entry.sh);
    }
    else if (entry.type == mft_entry_file)
    {
        assert(entry.name);
        char tmp[512] = {};
        const auto len = strlen(entry.name);
        assert(len < sizeof(tmp));
        memcpy(tmp, entry.name, len);
        str_lower(tmp);
        hashlittle2(tmp, len, &entry.ph, &entry.sh);
    }
    snprintf(name, 32, "%08x%08x", entry.ph, entry.sh);
}

static size_t mft_download_entry(mft_product &prod, mft_entry &entry, uint32_t thread_id)
{
    assert(thread_id < prod.config.thread_count);

    char name[32] = {};
    mft_entry_remote_name(entry, name);

    LOG_TRACE(
        "%s%s/%s/%s %s%s",
        prod.file_repo,
        entry.remote_path, // base / notes
        entry.pack_name ? entry.pack_name : "unpacked",
        name,
        entry.name ? "-> " : "",
        entry.name ? entry.name : "");

    auto data = prod.download_buffers[thread_id];
    auto cdata = prod.download_cbuffers[thread_id];
    size_t size = prod.config.download_buffer_size;
    size_t bytes = 0;
    size_t meta_bytes = 0;
    auto res = mft_download(prod, prod.file_repo, name, entry, data, cdata, &size, &bytes);
    entry.state = res == mft_ok ? state_none : state_download_failed;

    if (entry.type == mft_entry_part)
    {
        size = prod.config.download_buffer_size;
        char meta_name[128] = {};
        const auto l = strlen(name);
        memcpy(meta_name, name, l);
        memcpy(&meta_name[l], ".meta", 5);

        mft_entry meta = entry;
        meta.name = meta_name;
        meta.compressed_len = 0;
        meta.compression_type = 0;
        meta.uncompressed_len = entry.meta_len;
        meta.hash = entry.meta_crc;
        meta.sig = {};
        meta.sig_type = 0;
        mft_download(prod, prod.file_repo, meta_name, meta, data, cdata, &size, &meta_bytes);
    }

    return bytes + meta_bytes;
}

namespace
{
template <class InputIt, class OutputIt, class UnaryPredicate>
OutputIt remove_copy_if(InputIt first, InputIt last, OutputIt d_first, UnaryPredicate p)
{
    for (; first != last; ++first)
    {
        if (!p(*first))
        {
            *d_first++ = *first;
        }
    }
    return d_first;
}
} // namespace

size_t mft_download_batch(mft_product &prod, const std::vector<mft_entry> &allentries)
{
    std::vector<mft_entry> entries;
    ::remove_copy_if(
        allentries.begin(),
        allentries.end(),
        std::back_inserter(entries),
        [](const mft_entry &entry) {
            return entry.state != state_need_update && entry.state != state_download_failed;
        });

    const uint32_t threads = prod.config.thread_count;
    size_t bytes = 0;
    if (threads > 1)
    {
        const auto total = uint32_t(entries.size());
        const auto batch_size = total / threads;
        const auto remainder = total % threads;
        LOG_INFO(
            "total threads: %d, total entries: %d, batch size: %d",
            threads,
            total,
            batch_size + remainder);

        std::vector<std::thread> jobs;
        std::atomic<size_t> bytesTotal{ 0 };
        for (uint32_t tid = 0; tid < threads; ++tid)
        {
            const auto start = batch_size * tid;
            auto end = batch_size * (tid + 1);
            if (tid + 1 == threads)
                end += remainder;
            if (end == start)
                continue;

            LOG_TRACE(
                "scheduling job for thread %d with batch range from %d to %d", tid + 1, start, end);
            auto job = [start, end, tid, &prod, &entries, &bytesTotal]() {
                for (auto i = start; i < end; ++i)
                {
                    size_t l = mft_download_entry(prod, entries[i], tid);
                    bytesTotal.fetch_add(l);
                }
            };
            jobs.push_back(std::thread(job));
            if (end > total)
                break;
        }
        for (auto &job : jobs)
        {
            job.join();
        }
        bytes = bytesTotal;
    }
    else
    {
        for (auto &e : entries)
        {
            bytes += mft_download_entry(prod, e, 0);
        }
    }

    std::vector<mft_entry> currentEntries = entries;
    std::vector<mft_entry> failedEntries;
    for (int i = 0; 0 < prod.config.download_retries; i++)
    {
        ::remove_copy_if(
            currentEntries.begin(),
            currentEntries.end(),
            std::back_inserter(failedEntries),
            [](const mft_entry &entry) { return entry.state != state_download_failed; });

        if (failedEntries.size() == 0)
            break;

        LOG_INFO("retrying downloading %zu failed files (%d)...", failedEntries.size(), i);
        for (auto &e : failedEntries)
        {
            bytes += mft_download_entry(prod, e, 0);
        }

        currentEntries = failedEntries;
    }

    return bytes;
}

void mft_init(mft_product &prod, mft_config &cfg)
{
    if (!cfg.thread_count)
    {
        cfg.thread_count = 1;
    }
    if (!cfg.download_buffer_size || cfg.download_buffer_size > MFT_MAX_DOWNLOAD_SIZE)
    {
        cfg.download_buffer_size = MFT_MAX_DOWNLOAD_SIZE;
    }
    prod.config = cfg;

    prod.download_buffers = (uint8_t **)malloc(sizeof(uint8_t *) * cfg.thread_count);
    prod.download_cbuffers = (uint8_t **)malloc(sizeof(uint8_t *) * cfg.thread_count);
    for (uint32_t i = 0; i < cfg.thread_count; ++i)
    {
        prod.download_buffers[i] = (uint8_t *)malloc(sizeof(uint8_t) * cfg.download_buffer_size);
        prod.download_cbuffers[i] = (uint8_t *)malloc(sizeof(uint8_t) * cfg.download_buffer_size);
    }

    http_set_user_agent(cfg.agent_name);
}
