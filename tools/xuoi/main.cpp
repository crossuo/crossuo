// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <stdint.h>
#include <inttypes.h>

#define CHECKSUM_IMPLEMENTATION
#include <common/checksum.h>
#include <common/utils.h>

#define MINIZ_IMPLEMENTATION
#include <external/miniz.h>
#define SCHED_USE_ASSERT
#define SCHED_IMPLEMENTATION
#include <external/mmx_sched.h>
#include <external/tinyxml2.h>

#define CURL_STATICLIB
#include <curl/curl.h>

#define DO_CURL(x)                                                                                 \
    do                                                                                             \
    {                                                                                              \
        CURLcode r = curl_easy_##x;                                                                \
        if (r != CURLE_OK)                                                                         \
            fprintf(stdout, __FILE__ ":%d:ERROR:%d: %s\n", __LINE__, r, curl_easy_strerror(r));    \
    } while (0)

// FIXME: make a default one when we do not want to use lugaru
namespace loguru
{
const char *log_system_name(int)
{
    return "none";
}
}; // namespace loguru

// FIXME: move http funcs into a common lib
size_t recv_data_string(const char *data, size_t size, size_t nmemb, std::string *str)
{
    assert(data && str && size && nmemb);
    const auto amount = size * nmemb;
    str->append(data, amount);
    return amount;
}

size_t recv_data_vector(const char *data, size_t size, size_t nmemb, std::vector<uint8_t> *vec)
{
    assert(data && vec && size && nmemb);
    const auto amount = size * nmemb;
    vec->insert(vec->end(), data, data + amount);
    return amount;
}

size_t debug_data(const char *data, size_t size, size_t nmemb, void *)
{
    assert(data && data && size && nmemb);
    fprintf(stdout, "%zu %zu\n", size, nmemb);
    fprintf(stdout, "%s\n\n", data);
    return size * nmemb;
}

static CURL *g_curl_handle = nullptr;
void http_init()
{
    if (g_curl_handle)
        return;
    curl_global_init(CURL_GLOBAL_ALL);
    curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    fprintf(
        stdout,
        "libcurl %s (%s, %s, %s, libz/%s, tinyxml2/%d.%d.%d)\n",
        info->version,
        info->host,
        info->ssl_version,
        info->libssh_version,
        info->libz_version,
        TINYXML2_MAJOR_VERSION,
        TINYXML2_MINOR_VERSION,
        TINYXML2_PATCH_VERSION);
    g_curl_handle = curl_easy_init();
}

void http_shutdown()
{
    curl_easy_cleanup(g_curl_handle);
    g_curl_handle = nullptr;
    curl_global_cleanup();
}

void http_get_binary(const char *url, std::vector<uint8_t> &data)
{
    fprintf(stdout, "url %s\n", url);
    CURL *curl = curl_easy_duphandle(g_curl_handle);
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &data));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_vector));
    DO_CURL(perform(curl));
    curl_easy_cleanup(curl);
}

void http_get_string(const char *url, std::string &data)
{
    fprintf(stdout, "url %s\n", url);
    CURL *curl = curl_easy_duphandle(g_curl_handle);
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &data));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_string));
    DO_CURL(perform(curl));
    curl_easy_cleanup(curl);
}

template <typename T>
bool file_read(const char *file, T &result)
{
    FILE *fp = fopen(file, "rb");
    if (!fp)
        return false;
    fseek(fp, 0, SEEK_END);
    const size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    result.reserve(len + 1);
    result.resize(len + 1);
    const size_t read = fread((void *)result.data(), 1, len, fp);
    fclose(fp);
    if (read != len)
        return false;
    return true;
}

template <typename T>
bool file_write(const char *file, T &input)
{
    FILE *fp = fopen(file, "wb");
    if (!fp)
        return false;
    const size_t wrote = fwrite((void *)input.data(), 1, input.size(), fp);
    fclose(fp);
    if (wrote != input.size())
        return false;
    return true;
}

// actual xuoi implementation
// we'll need to support crossuo manifest files with a similar api so we can
// have a unified installer for both and maybe some special case for some freeshard

#define DATA_SERVER_ADDRESS "http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/manifest/"
#define DATA_MANIFEST_FILE DATA_SERVER_ADDRESS "uo-legacyrelease.prod"

enum mft_result : uint32_t
{
    mft_ok = 0,
    mft_panic,
    mft_invalid_format,
    mft_decompress_error,
    mft_write_error,
};

enum mft_entry_type : uint32_t
{
    mft_entry_file,
    mft_entry_manifest,
};

// FIXME: convert all std::string to char *, but then we must ensure tinyxml document stay alive
// this will require a few changes as we can copy-construct/move it
struct mft_entry
{
    std::string name;
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
    std::string sig; // sha256 base64 string
    std::string remote_path;
};

struct mft_package
{
    std::string name;
    std::string remote_path;
    uint32_t priority = 0;
    std::vector<mft_entry> entries;
};

struct mft_stage
{
    std::string name;
    uint32_t priority = 0;
    uint32_t verify_unpacked = 0;
    std::vector<mft_package> packages;
};

struct mft_instance
{
    uint64_t timestamp = 0;
    std::string launchfile;
    std::string manifest_repo;
    std::string file_repo;
    std::vector<mft_stage> stages;
    std::vector<mft_entry> manifests;
    std::vector<mft_entry> files;
    std::unordered_map<std::string, mft_package> packs;
    tinyxml2::XMLDocument doc;
};

static const char *s_outdir = nullptr;

static mft_result mft_entry_load(tinyxml2::XMLElement *node, mft_entry &entry)
{
    auto n = node->Attribute("n");
    if (n)
        entry.name = n;

    entry.uncompressed_len = node->Hex64Attribute("ul", 0);
    entry.compression_type = node->UnsignedAttribute("ct", 0);
    entry.compressed_len = node->Hex64Attribute("cl", 0);
    entry.timestamp = node->Hex64Attribute("t", 0);
    entry.hash = (uint32_t)node->Hex64Attribute("rh", 0);

    entry.ph = (uint32_t)node->Hex64Attribute("ph", 0);
    entry.sh = (uint32_t)node->Hex64Attribute("sh", 0);
    entry.ft = node->UnsignedAttribute("ft", 0);
    entry.meta_len = (uint32_t)node->Hex64Attribute("ml", 0);
    entry.meta_crc = (uint32_t)node->Hex64Attribute("mc", 0);

    if (n)
    {
        fprintf(
            stdout,
            "file> %64s (%10" PRIu64 ", %10" PRIu64 ") @ %10" PRIu64 " [0x%08x]",
            entry.name.c_str(),
            entry.uncompressed_len,
            entry.compressed_len,
            entry.timestamp,
            entry.hash);
    }
    else
    {
        fprintf(
            stdout,
            "part> (%10" PRIu64 ", %10" PRIu64 ") @ %10" PRIu64 " [0x%08x] ",
            entry.uncompressed_len,
            entry.compressed_len,
            entry.timestamp,
            entry.hash);
        fprintf(
            stdout,
            "%12u %12u (meta 0x%08x, size: %3d) [%d]",
            entry.ph,
            entry.sh,
            entry.meta_crc,
            entry.meta_len,
            entry.ft);
    }
    fprintf(stdout, "\n");
    return mft_ok;
}

static mft_result mft_entries_load(
    tinyxml2::XMLElement *node,
    std::vector<mft_entry> &entries,
    mft_entry_type type,
    const char *remote_path)
{
    const char *tag = type == mft_entry_manifest ? "manifest" : "f";
    auto entry = node->FirstChildElement(tag);
    while (entry)
    {
        mft_entry e;
        e.type = type;
        e.remote_path = remote_path;
        mft_entry_load(entry, e);
        entries.emplace_back(e);
        entry = entry->NextSiblingElement(tag);
    }
    return mft_ok;
}

static mft_result mft_manifests_load(
    tinyxml2::XMLElement *node, std::vector<mft_entry> &manifests, const char *remote_path)
{
    return mft_entries_load(node, manifests, mft_entry_manifest, remote_path);
}

static mft_result
mft_files_load(tinyxml2::XMLElement *node, std::vector<mft_entry> &files, const char *remote_path)
{
    return mft_entries_load(node, files, mft_entry_file, remote_path);
}

static mft_result
mft_package_load(tinyxml2::XMLElement *node, mft_package &pkg, const char *remote_path = nullptr)
{
    pkg.name = node->Attribute("name");
    pkg.remote_path = node->Attribute("rpath");
    if (pkg.remote_path.empty())
        pkg.remote_path = remote_path;
    pkg.priority = node->UnsignedAttribute("priority", 0);
    fprintf(stdout, "> %s/%s (%d)\n", pkg.remote_path.c_str(), pkg.name.c_str(), pkg.priority);
    return mft_manifests_load(node, pkg.entries, pkg.remote_path.c_str());
}

static mft_result mft_packs_load(
    tinyxml2::XMLElement *node,
    std::unordered_map<std::string, mft_package> &packs,
    const char *remote_path)
{
    auto pack = node->FirstChildElement("p");
    while (pack)
    {
        const char *name = pack->Attribute("name");
        if (packs.find(name) == packs.end())
            packs[name] = {};
        mft_package &pkg = packs[name];
        pkg.name = name;
        pkg.remote_path = pack->Attribute("rpath");
        if (pkg.remote_path.empty())
            pkg.remote_path = remote_path;
        pkg.priority = pack->UnsignedAttribute("priority", 0);
        fprintf(stdout, "pack> %s/%s\n", pkg.remote_path.c_str(), pkg.name.c_str());
        if (auto files = pack->FirstChildElement("files"))
            if (auto error = mft_files_load(files, pkg.entries, pkg.remote_path.c_str()))
                return error;
        pack = node->NextSiblingElement("p");
    }
    return mft_ok;
}

static mft_result
mft_stage_load(tinyxml2::XMLElement *node, mft_stage &stg, const char *remote_path = nullptr)
{
    stg.name = node->Attribute("name");
    stg.verify_unpacked = node->UnsignedAttribute("verifyunpacked", 0);
    stg.priority = node->UnsignedAttribute("priority", 0);
    fprintf(stdout, "> %s,%d(%d)\n", stg.name.c_str(), stg.verify_unpacked, stg.priority);
    auto packages = node->FirstChildElement("packages");
    if (packages)
    {
        auto package = packages->FirstChildElement("package");
        while (package)
        {
            mft_package p;
            mft_package_load(package, p, remote_path);
            stg.packages.emplace_back(p);
            package = package->NextSiblingElement("package");
        }
    }
    return mft_ok;
}

static mft_result mft_product_load(
    tinyxml2::XMLElement *node, mft_instance &product, const char *remote_path = nullptr)
{
    product.timestamp = node->Unsigned64Attribute("serial", 0);
    product.launchfile = node->Attribute("launchfile");
    fprintf(stdout, "product timestamp: %" PRIu64 "\n", product.timestamp);

    product.manifest_repo = node->FirstChildElement("manifestrepos")
                                ->FirstChildElement("repo")
                                ->Attribute("url"); // FIXME: handle errors
    product.file_repo = node->FirstChildElement("filerepos")
                            ->FirstChildElement("repo")
                            ->Attribute("url"); // FIXME: handle errors
    fprintf(
        stdout,
        "manifest: %s\nfiles: %s\n",
        product.manifest_repo.c_str(),
        product.file_repo.c_str());
    auto stages = node->FirstChildElement("stages");
    if (stages)
    {
        auto stage = stages->FirstChildElement("stage");
        while (stage)
        {
            mft_stage s;
            mft_stage_load(stage, s, remote_path);
            product.stages.emplace_back(s);
            stage = stage->NextSiblingElement("stage");
        }
    }
    return mft_ok;
}

mft_result mft_load(const std::string &data, mft_instance &mft, const char *remote_path = nullptr)
{
    mft.doc.Parse(data.c_str(), data.size());

    auto root = mft.doc.FirstChildElement("MythicMFT");
    if (!root)
        return mft_invalid_format;

    if (auto product = root->FirstChildElement("product"))
        return mft_product_load(product, mft, remote_path);

    if (auto manifest = root->FirstChildElement("manifest"))
    {
        if (auto manifests = manifest->FirstChildElement("manifests"))
            if (auto error = mft_manifests_load(manifests, mft.manifests, remote_path))
                return error;

        if (auto files = manifest->FirstChildElement("files"))
            if (auto error = mft_files_load(files, mft.files, remote_path))
                return error;

        if (auto packs = manifest->FirstChildElement("packs"))
            if (auto error = mft_packs_load(packs, mft.packs, remote_path))
                return error;
    }

    return mft_ok;
}

// FIXME: ugly
static void mft_prepare(mft_instance &mft)
{
    for (auto &stage : mft.stages)
    {
        for (auto &package : stage.packages)
        {
            for (auto &entry : package.entries)
            {
                switch (entry.type)
                {
                    case mft_entry_manifest:
                    {
                        mft.manifests.emplace_back(entry);
                    }
                    break;
                    case mft_entry_file:
                    {
                        mft.files.emplace_back(entry);
                    }
                    break;
                    default:
                        break;
                }
            }
        }
    }
}

mft_result mft_download(const std::string &repo, const mft_entry &entry, std::vector<uint8_t> &data)
{
    char ipath[256] = {};
    snprintf(ipath, sizeof(ipath), "%s/%s", entry.remote_path.c_str(), entry.name.c_str());
    fprintf(stdout, "reading %s ", ipath);

    size_t ul = entry.uncompressed_len;
    data.reserve(ul);

    switch (entry.compression_type)
    {
        case 0:
        {
            if (!file_read(ipath, data))
            {
                char upath[512] = {};
                snprintf(upath, sizeof(upath), "%s%s", repo.c_str(), ipath);
                http_get_binary(upath, data);
            }
        }
        break;
        case 1:
        {
            const auto cl = entry.compressed_len;
            std::vector<uint8_t> cbuf;
            cbuf.reserve(cl);
            if (!file_read(ipath, cbuf))
            {
                char upath[512] = {};
                snprintf(upath, sizeof(upath), "%s%s", repo.c_str(), ipath);
                http_get_binary(upath, cbuf);
            }

            fprintf(stdout, "(decompressing) ");
            data.resize(ul);
            auto ol = checked_cast<uLongf>(ul);
            auto il = checked_cast<uLongf>(cl);
            int z_err = mz_uncompress(data.data(), &ol, cbuf.data(), il);
            if (z_err != Z_OK || ul != entry.uncompressed_len)
            {
                fprintf(stdout, "decompression error %d\n", z_err);
                return mft_decompress_error;
            }
        }
        break;
        default:
            break;
    }

    char opath[512] = {};
    snprintf(
        opath, sizeof(opath), "%s/%s/%s", s_outdir, entry.remote_path.c_str(), entry.name.c_str());
    if (!file_write(opath, data))
    {
        fprintf(stdout, "error writing\n");
        return mft_write_error;
    }

    fprintf(stdout, "writing\n");
    return mft_ok;
}

static void mft_merge_instances(mft_instance &dst, mft_instance &src)
{
    dst.manifests.insert(dst.manifests.end(), src.manifests.begin(), src.manifests.end());
    dst.files.insert(dst.files.end(), src.files.begin(), src.files.end());
    for (auto &e : src.packs)
    {
        if (dst.packs.find(e.first) == dst.packs.end())
            dst.packs[e.first] = {};
        auto &d = dst.packs[e.first];
        auto &s = e.second;
        d.entries.insert(d.entries.end(), s.entries.begin(), s.entries.end());
    }
}

bool mft_consume_manifests(mft_instance &mft)
{
    std::vector<mft_entry> manifests;
    mft.manifests.swap(manifests);
    for (const auto &entry : manifests)
    {
        assert(entry.type == mft_entry_manifest);
        std::vector<uint8_t> data;
        mft_download(mft.manifest_repo, entry, data);
        std::string s((const char *)data.data(), data.size());
        mft_instance m;
        if (!mft_load(s, m, entry.remote_path.c_str()))
        {
            if (m.manifest_repo.empty())
                m.manifest_repo = mft.manifest_repo;
            if (m.file_repo.empty())
                m.file_repo = mft.file_repo;

            mft_prepare(m);
            mft_merge_instances(mft, m);
        }
    }
    return !mft.manifests.empty();
}

void mft_consume_files(mft_instance &mft)
{
    std::vector<mft_entry> files;
    mft.files.swap(files);
    for (const auto &entry : files)
    {
        assert(entry.type == mft_entry_file);
        std::vector<uint8_t> data;
        mft_download(mft.file_repo, entry, data);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stdout, "xuoi - crossuo installer 0,0.1\n");
        fprintf(stdout, "Copyright (c) 2019 Danny Angelo Carminati Grein\n");
        fprintf(
            stdout,
            "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n");
        fprintf(stdout, "\n");
        fprintf(stdout, "Usage: %s <output dir> <product manifest> \n", argv[0]);
        fprintf(stdout, "   ex: %s out uo-legacyrelease.prod\n", argv[0]);
        return 0;
    }

    s_outdir = argv[1];
    assert(s_outdir);

    const char *product = "uo-legacyrelease.prod";
    if (argc == 2)
        product = argv[2];
    assert(product);

    crc32_init();
    http_init();

    std::string str;
    //std::vector<uint8_t> data;
    //http_get_binary("http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest//notes/unpacked.mft", data);
    file_read(product, str);
    //auto str = http_get_string(curl, DATA_MANIFEST_FILE);
    //fprintf(stdout, "%s\n", str.c_str());

    mft_instance mft;
    if (!mft_load(str, mft))
    {
        mft_prepare(mft);
        while (mft_consume_manifests(mft))
        {
        }
        size_t ulen = 0, clen = 0;
        for (const auto e : mft.files)
        {
            ulen += e.uncompressed_len;
            clen += e.compressed_len;
        }
        fprintf(
            stdout,
            "total files to process: %zu (compressed: %zu, uncompressed: %zu)\n",
            mft.files.size(),
            clen,
            ulen);
        size_t parts = 0;
        for (const auto p : mft.packs)
        {
            parts += p.second.entries.size();
            for (const auto e : p.second.entries)
            {
                ulen += e.uncompressed_len;
                clen += e.compressed_len;
            }
        }
        fprintf(
            stdout,
            "total packs to process: %zu (%zu parts, compressed: %zu, uncompressed: %zu)\n",
            mft.packs.size(),
            parts,
            clen,
            ulen);
        //mft_consume_files(mft);
    }

    http_shutdown();
    return 0;
}
