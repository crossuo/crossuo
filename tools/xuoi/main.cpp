// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <unordered_map>
#include <map>
#include <stdint.h>
#include <inttypes.h>
#include <external/popts.h>

#define XUOI_THREADED 1
#if XUOI_THREADED
#include <atomic>
#include <thread>
#define XUOI_ATOMIC(x) std::atomic<x>
#define XUOI_ATOMIC_ADD(x, i) (x.fetch_add(i))
#define XUOI_THREAD_COUNT (std::thread::hardware_concurrency() - 1)
#else
#define XUOI_ATOMIC(x) x
#define XUOI_ATOMIC_ADD(x, i) (x + i)
#define XUOI_THREAD_COUNT (1)
#endif

#include "common.h"
#include "http.h"
#include "xuo_updater.h"

#include <xuocore/uop.h>
#include <xuocore/uolib.h>
#define STR_IMPLEMENTATION
#include <common/str.h>

#define XUOI_MAX_DOWNLOAD_SIZE (1024 * 1024 * 1024)
#define XUOI_AGENT_NAME "EAMythic Patch Client"

static size_t s_total = 0;
static XUOI_ATOMIC(size_t) s_received{ 0 };
static XUOI_ATOMIC(int32_t) s_total_files{ 0 };

// actual xuoi implementation
// we'll need to support crossuo manifest files with a similar api so we can
// have a unified installer for both and maybe some special case for some freeshard

#define DATA_OFFICIAL_SERVER "http://patch.uo.eamythic.com/"

#define DATA_PRODUCT_SERVER_ADDRESS "uopatch-sa/legacyrelease/uo/manifest/"
#define DATA_PRODUCT_FILE "uo-legacyrelease.prod"
#define DATA_PATCHER_SERVER_ADDRESS "uopatch-sa/legacyrelease/patcher/manifest/"
#define DATA_PATCHER_FILE "patcher.prod"

enum mft_result : uint32_t
{
    mft_ok = 0,
    mft_invalid_format,
    mft_decompress_error,
    mft_write_error,
    mft_could_not_open_path,
};

enum mft_entry_type : uint32_t
{
    mft_entry_part,
    mft_entry_file,
    mft_entry_manifest,
};

enum mft_entry_state : uint8_t
{
    state_none,
    state_need_update,
    state_download_failed,
};

struct mft_entry
{
    const char *name = nullptr;
    size_t uncompressed_len = 0;
    size_t compressed_len = 0;
    uint64_t timestamp = 0;
    uint32_t ph = 0;
    uint32_t sh = 0;
    uint32_t compression_type = 0;
    uint32_t hash = 0;
    uint32_t ft = 0;
    uint32_t meta_len = 0;
    uint32_t meta_crc = 0;
    mft_entry_type type = mft_entry_file;
    uint32_t sig_type = 0;
    const char *sig = nullptr; // sha256 base64 string
    const char *remote_path = nullptr;
    const char *pack_name = nullptr; // for packs in packages
    mft_entry_state state = state_need_update;
};

struct mft_package
{
    const char *name = nullptr;
    const char *remote_path = nullptr;
    const char *repo = nullptr;
    const char *pack_name = nullptr; // for packs in packages
    uint32_t priority = 0;
};

struct mft_stage
{
    const char *name;
    uint32_t priority = 0;
    uint32_t verify_unpacked = 0;
};

struct mft_manifest
{
    const char *remote_path = nullptr;
    std::vector<mft_entry> files;
    std::vector<uint8_t> data;
    tinyxml2::XMLDocument doc;
};

struct mft_config
{
    uint32_t thread_count = 1;
    uint32_t download_buffer_size = XUOI_MAX_DOWNLOAD_SIZE;
    bool mirror_mode = false;
    bool listing_only = false;
    bool dump_extracted = true;
    bool latest_checked = false;
    bool generate_diff = false;
    fs_path output_path;
    fs_path product_path;
};

struct mft_product
{
    uint64_t timestamp = 0;
    const char *launchfile;
    const char *manifest_repo;
    const char *file_repo;

    std::vector<mft_entry> mft_files;
    std::vector<mft_entry> files;
    std::vector<mft_entry> parts;
    std::vector<mft_package> packs;
    std::vector<mft_stage> stages;
    std::vector<mft_package> packages;
    std::vector<mft_manifest *> manifests;
    std::unordered_map<std::string, mft_entry> base_version;

    uint64_t last_version = 0;
    mft_config config;
    uint8_t **download_buffers = nullptr;
    uint8_t **download_cbuffers = nullptr;
};

mft_result mft_download(
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
                http_get_binary(upath, buffer, buffer_size);
                fs_file_write(lpath, buffer, *buffer_size); // save to local cache
            }
            bytes = *buffer_size;
        }
        break;
        case 1:
        {
            auto cl = entry.compressed_len;
            assert(cl < *buffer_size);
            if (!fs_file_read(lpath, cbuffer, buffer_size))
            {
                http_get_binary(upath, cbuffer, buffer_size);
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
            int z_err = mz_uncompress((unsigned char *)buffer, &ol, (unsigned char *)cbuffer, il);
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

    entry.uncompressed_len = node->Hex64Attribute("ul", 0);
    assert(entry.uncompressed_len < XUOI_MAX_DOWNLOAD_SIZE);
    entry.compression_type = node->UnsignedAttribute("ct", 0);
    entry.compressed_len = node->Hex64Attribute("cl", 0);
    assert(entry.compressed_len < XUOI_MAX_DOWNLOAD_SIZE);
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

mft_result mft_load(
    mft_product &prod, const uint8_t *data, size_t data_size, const char *remote_path = nullptr)
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
            mft_download(prod, prod.manifest_repo, entry.name, entry, data, cdata, &size);
            if (auto error = mft_load(prod, data, size, entry.remote_path))
                return error;
        }
    }
    return mft_ok;
}

void mft_listing_save(mft_product &prod)
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
void mft_listing_load_version(mft_product &prod, const std::string &version, T &data)
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

        std::string n = tmp;
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
int mft_diff(mft_product &prod, const T &data1, const T &data2)
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

int mft_diff(mft_product &prod)
{
    auto path = fs_path_join(
        prod.config.product_path,
        // std::to_string(prod.last_version),
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
        std::string part = e.pack_name;
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

static void mft_download_entry(mft_product &prod, mft_entry &entry, uint32_t thread_id)
{
    assert(thread_id < prod.config.thread_count);
    if (entry.state != state_need_update)
        return;

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
        mft_download(prod, prod.file_repo, meta_name, meta, data, cdata, &size);
    }

    const int MB = 1024 * 1024;
    size_t received = XUOI_ATOMIC_ADD(s_received, bytes) / MB;
    int32_t files = XUOI_ATOMIC_ADD(s_total_files, 1);
    if ((received % 128) == 0)
    {
        LOG_INFO(
            "progress: %d files, %zu/%zu MB (%3.2f%%)",
            files,
            received,
            s_total / MB,
            100 * received / float(s_total / MB));
    }
}

void mft_download_batch(mft_product &prod, std::vector<mft_entry> &entries)
{
#if XUOI_THREADED
    const uint32_t threads = XUOI_THREAD_COUNT;
    const auto total = entries.size();
    const auto batch_size = total / threads;
    const auto remainder = total % threads;
    LOG_TRACE(
        "total threads: %d, total entries: %d, batch size: %d",
        threads,
        total,
        batch_size + remainder);

    std::vector<std::thread> jobs;
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
        auto job = [start, end, tid, &prod, &entries]() {
            for (auto i = start; i < end; ++i)
            {
                mft_download_entry(prod, entries[i], tid);
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
#else
    for (auto &e : entries)
    {
        mft_download_entry(prod, e, 0);
    }
#endif // #if XUOI_THREADED
}

void mft_init(mft_product &prod, mft_config &cfg)
{
    if (!cfg.thread_count)
    {
        cfg.thread_count = 1;
    }
    if (!cfg.download_buffer_size || cfg.download_buffer_size > XUOI_MAX_DOWNLOAD_SIZE)
    {
        cfg.download_buffer_size = XUOI_MAX_DOWNLOAD_SIZE;
    }
    prod.config = cfg;

    prod.download_buffers = (uint8_t **)malloc(sizeof(uint8_t *) * cfg.thread_count);
    prod.download_cbuffers = (uint8_t **)malloc(sizeof(uint8_t *) * cfg.thread_count);
    for (uint32_t i = 0; i < cfg.thread_count; ++i)
    {
        prod.download_buffers[i] = (uint8_t *)malloc(sizeof(uint8_t) * cfg.download_buffer_size);
        prod.download_cbuffers[i] = (uint8_t *)malloc(sizeof(uint8_t) * cfg.download_buffer_size);
    }

    http_set_user_agent(XUOI_AGENT_NAME);
}

mft_result mft_product_install(mft_config &cfg, const char *product_url, const char *product_file)
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
            mft_listing_load_version(prod, std::to_string(prod.last_version), prod.base_version);
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
            int diff = mft_diff(prod);
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

        mft_download_batch(prod, prod.files);
        mft_download_batch(prod, prod.parts);

        mft_listing_save(prod);
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

        std::string diff = s_cli["diff"].get().string;
        const auto pos = strchr(diff.data(), ',') - diff.data();
        auto ver1 = std::string(diff.data(), pos);
        auto ver2 = std::string(diff.data() + pos + 1, diff.length() - pos - 1);
        LOG_INFO("Diffing %s <> %s", ver1.c_str(), ver2.c_str());

        mft_product mft;
        mft_init(mft, cfg);
        mft.launchfile = "client.exe"; // arg...
        std::map<std::string, mft_entry> data1, data2;
        mft_listing_load_version(mft, ver1, data1);
        mft_listing_load_version(mft, ver2, data2);
        mft_diff(mft, data1, data2);
        return 0;
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
        std::string product_name = s_cli["product"].get().string;
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
        if (auto res = mft_product_install(cfg, server_urls[i], product_list[i]))
        {
            LOG_ERROR("installation failed with error: %d", res);
            exit_code = -int(res);
        }
    }
    http_shutdown();
    return exit_code;
}
