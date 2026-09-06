// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Launcher

#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <algorithm>
#include <external/tinyxml2.h>
#include <external/xxhash.h>
#include <xuocore/http.h>
#include <xuocore/common.h>

#define MINIZ_IMPLEMENTATION
#include <external/miniz.h> // mz_zip_archive

#include "xuo_updater.h"

#define XUOL_THREADED 0
#if XUOL_THREADED
#include <atomic>
#include <thread>
#define XUOL_ATOMIC(x) std::atomic<x>
#define XUOL_ATOMIC_ADD(x, i) (x.fetch_add(i))
#define XUOL_THREAD_COUNT (std::thread::hardware_concurrency() - 1)
#else
#define XUOL_ATOMIC(x) x
#define XUOL_ATOMIC_ADD(x, i) (x + i)
#define XUOL_THREAD_COUNT (1)
#endif

enum class xuo_channel : uint8_t
{
    stable,
    beta,
};

enum xuo_result
{
    xuo_ok,
    xuo_invalid_manifest,
    xuo_checksum_failed,
    xuo_could_not_open_path,
    xuo_could_not_write_file,
    xuo_could_not_deflate_file,
    xuo_could_not_copy_file,
    xuo_could_not_download_file,
    xuo_install_failed,
};

struct xuo_file
{
    const char *name = nullptr;
    const char *zipFilename = nullptr;
    uint64_t hash = 0;
    uint64_t zipHash = 0;
    bool launcher = false;
    bool executable = false;
};

struct xuo_release
{
    const char *name = nullptr;
    const char *version = nullptr;
    bool latest = false;
    std::vector<xuo_file> files;
};

struct xuo_config
{
    fs_path cache_path;
    fs_path output_path;
    xuo_channel channel = xuo_channel::stable;
    bool initialized = false;
};

struct xuo_context
{
    const char *platform;
    xuo_config config;

    std::vector<xuo_release> releases;
    std::vector<uint8_t> changelog;
    std::vector<uint8_t> manifest;
    tinyxml2::XMLDocument doc;
};

#if defined(XUO_LINUX)
const char *xuo_platform_name()
{
    return "linux";
}
#elif defined(XUO_OSX)
const char *xuo_platform_name()
{
    return "osx";
}
#else  // MACOS
const char *xuo_platform_name()
{
    return "win64";
}
#endif // WIN64

static fs_path s_cachePath;

void xuo_set_cache_directory(const char *cachePath)
{
    s_cachePath = fs_path_from(cachePath);
    fs_path_create(s_cachePath);
}

static uint64_t xuo_get_hash(const fs_path &filename)
{
    size_t len = 0;
    auto ptr = fs_map(filename, &len);
    if (!len || !ptr)
    {
        if (ptr)
            fs_unmap(ptr, len);
        LOG_ERROR("failed to read file: %s", fs_path_ascii(filename));
        return 0;
    }
    auto hash = XXH64(static_cast<void *>(ptr), len, 0x2593);
    fs_unmap(ptr, len);

    return hash;
}

static void xuo_changelog_load(xuo_context &ctx)
{
    const char *changelog_addr = XUOL_UPDATER_HOST "release/changelog.html";
    static std::vector<uint8_t> changelog_file;
    if (changelog_file.empty())
        http_get_binary(changelog_addr, changelog_file);
    ctx.changelog = changelog_file;
}

static xuo_result xuo_release_load(xuo_context &ctx, tinyxml2::XMLElement *node)
{
    xuo_release rel;
    rel.name = node->Attribute("name");
    rel.version = node->Attribute("version");
    rel.latest = node->BoolAttribute("latest", false);
    LOG_TRACE("# release: %s version: %s (latest: %d)", rel.name, rel.version, rel.latest);
    auto fnode = node->FirstChildElement("file");
    while (fnode)
    {
        xuo_file file;
        file.name = fnode->Attribute("name");
        file.executable = fnode->BoolAttribute("exe", false);
        file.hash = fnode->Hex64Attribute("hash", 0);
        file.zipFilename = fnode->Attribute("data");
        file.zipHash = fnode->Hex64Attribute("datahash", 0);
        LOG_TRACE(
            "  file: %s hash: %" PRIx64 " at %s (%" PRIx64 ")",
            file.name,
            file.hash,
            file.zipFilename,
            file.zipHash);
        // workaround for backwards compat with older manifest version
        if (!file.executable)
        {
            if (!strcasecmp(file.name, "crossuo"))
                file.executable = true;
            if (!strcasecmp(file.name, "xuolauncher"))
                file.executable = true;
            if (!strcasecmp(file.name, "xuoassist"))
                file.executable = true;
        }
        rel.files.emplace_back(file);
        fnode = fnode->NextSiblingElement("file");
    }
    ctx.releases.emplace_back(rel);
    return xuo_ok;
}

static xuo_result xuo_manifest_load(xuo_context &ctx, const char *platform, xuo_channel channel)
{
    const char *manifest_addr = XUOL_UPDATER_HOST "release/%s%s.manifest.xml";
    char addr[512];
    snprintf(
        addr, sizeof(addr), manifest_addr, platform, channel == xuo_channel::beta ? "-beta" : "");
    LOG_INFO("downloading manifest %s", addr);
    std::vector<uint8_t> data;
    if (!http_get_binary(addr, data))
        return xuo_could_not_download_file;

    ctx.manifest.swap(data);
    ctx.platform = platform;
    ctx.doc.Parse((char *)ctx.manifest.data(), ctx.manifest.size());
    auto root = ctx.doc.FirstChildElement("manifest");
    if (!root)
        return xuo_invalid_manifest;

    auto release = root->FirstChildElement("release");
    while (release)
    {
        xuo_release_load(ctx, release);
        release = release->NextSiblingElement("release");
    }
    return xuo_ok;
}

static xuo_result xuo_update_file(xuo_context &ctx, xuo_release &rel, xuo_file &file)
{
    fs_path ipath = fs_path_join(ctx.config.cache_path, file.zipFilename);
    fs_path idir = fs_directory(ipath);
    if (!fs_path_create(idir))
    {
        LOG_ERROR("failed to create directory: %s", fs_path_ascii(idir));
        return xuo_could_not_open_path;
    }

    fs_path opath = fs_path_join(ctx.config.output_path, file.name);
    fs_path odir = fs_directory(opath);
    if (!fs_path_create(odir))
    {
        LOG_ERROR("failed to create directory: %s", fs_path_ascii(odir));
        return xuo_could_not_open_path;
    }

    char upath[512] = {};
    snprintf(upath, sizeof(upath), XUOL_UPDATER_HOST "update/%s", file.zipFilename);

    auto lpath = fs_path_ascii(ipath);
    if (!fs_path_exists(ipath))
    {
        if (!http_get_file(upath, lpath))
        {
            LOG_ERROR("could not write file: %s", lpath);
            return xuo_could_not_write_file;
        }
    }

    auto icrc = xuo_get_hash(ipath);
    if (icrc != file.zipHash)
    {
        if (fs_path_is_file(ipath))
            fs_del(ipath);
        LOG_ERROR("checksum validation failed: %s", fs_path_ascii(ipath));
        return xuo_checksum_failed;
    }

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    mz_bool status = mz_zip_reader_init_file(&zip, lpath, 0);
    if (!status)
    {
        LOG_ERROR("could not open zip file: %s", lpath);
        return xuo_could_not_deflate_file;
    }

    if (mz_zip_reader_get_num_files(&zip) != 1)
    {
        LOG_ERROR("error reading zip file: %s", lpath);
        return xuo_could_not_deflate_file;
    }

    status = mz_zip_reader_extract_to_file(&zip, 0, fs_path_ascii(opath), 0);
    mz_zip_reader_end(&zip);
    if (!status)
    {
        LOG_ERROR("cold not uncompress zip file '%s' as '%s'", lpath, fs_path_ascii(opath));
        return xuo_could_not_deflate_file;
    }

    auto ocrc = xuo_get_hash(opath);
    if (ocrc != file.hash)
    {
        fs_del(opath);
        LOG_ERROR("uncompressed file checksum error: %s", fs_path_ascii(opath));
        return xuo_checksum_failed;
    }

    if (file.executable)
        fs_chmod(opath, fs_mode(FS_READ | FS_WRITE | FS_EXEC));

    return xuo_ok;
}

static bool xuo_install_release(xuo_context &ctx, xuo_release &release)
{
    XUOL_ATOMIC(uint32_t) errors{ 0 };

#if XUOL_THREADED
    const uint32_t threads = XUOL_THREAD_COUNT;
    std::vector<std::thread> jobs;
    for (uint32_t tid = 0; tid < threads; ++tid)
    {
        auto job = [tid, threads, &errors, &ctx, &release]() {
            for (auto i = tid; i < release.files.size(); i += threads)
            {
                if (xuo_update_file(ctx, release, release.files[i]) != xuo_ok)
                {
                    errors = XUOL_ATOMIC_ADD(errors, 1);
                }
            }
        };

        jobs.push_back(std::thread(job));
    }
    for (auto &job : jobs)
    {
        job.join();
    }
#else
    for (size_t i = 0; i < release.files.size(); ++i)
    {
        if (xuo_update_file(ctx, release, release.files[i]) != xuo_ok)
        {
            errors = XUOL_ATOMIC_ADD(errors, 1);
        }
    }
#endif

    return errors == 0;
}

bool xuo_release_get(xuo_context *ctx, const char *name, const char *version)
{
    if (!ctx || !ctx->config.initialized)
        return false;

    auto it = std::find_if(
        ctx->releases.begin(), ctx->releases.end(), [name, version](xuo_release &e) -> bool {
            if (strcasecmp(e.name, name) == 0)
            {
                if (version == nullptr && e.latest)
                    return true;
                if (version && strcasecmp(e.version, version) == 0)
                    return true;
            }
            return false;
        });
    if (it == ctx->releases.end())
        return false;

    xuo_release &rel = (*it);
    return xuo_install_release(*ctx, rel);
}

static bool
xuo_release_check(const fs_path &path, const xuo_release &rel, std::vector<xuo_file> &files)
{
    for (const auto &f : rel.files)
    {
        auto fname = fs_path_join(path, f.name);
        bool want = false;
        if (!fs_path_exists(fname))
            want = true;
        else if (xuo_get_hash(fname) != f.hash)
            want = true;
        if (want)
            files.push_back(f);
    }
    return !files.empty();
}

static bool xuo_update_check(xuo_context &ctx, xuo_release &updates, bool quick)
{
    bool found = false;
    for (const auto &r : ctx.releases)
    {
        if (!r.latest)
            continue;
        found |= xuo_release_check(ctx.config.output_path, r, updates.files);
        if (found && quick)
            return true;
    }
    return found;
}

void xuo_releases_iterate(xuo_context *ctx, xuo_release_cb pFunc)
{
    if (!ctx || !ctx->config.initialized)
        return;
    assert(pFunc && "missing iterator callback");
    for (const auto &r : ctx->releases)
    {
        pFunc(r.name, r.version, r.latest);
    }
}

const char *xuo_changelog(xuo_context *ctx)
{
    if (!ctx || !ctx->config.initialized)
        return nullptr;
    return (char *)ctx->changelog.data();
}

bool xuo_update_check(xuo_context *ctx)
{
    if (!ctx || !ctx->config.initialized)
        return false;
    xuo_release updates;
    return xuo_update_check(*ctx, updates, true);
}

bool xuo_update_apply(xuo_context *ctx)
{
    if (!ctx || !ctx->config.initialized)
        return false;
    xuo_release updates;
    if (xuo_update_check(*ctx, updates, false))
        return xuo_install_release(*ctx, updates);
    return false;
}

xuo_context *xuo_init(const char *path, bool beta)
{
    static xuo_context ctx;
    if (ctx.config.initialized)
        return nullptr;

    ctx.config.channel = beta ? xuo_channel::beta : xuo_channel::stable;
    http_set_user_agent(XUOL_AGENT_NAME);
    xuo_changelog_load(ctx);
    if (xuo_manifest_load(ctx, xuo_platform_name(), ctx.config.channel) != xuo_ok)
    {
        LOG_ERROR("could not load update manifest: invalid format");
        return nullptr;
    }

    ctx.config.cache_path = fs_path_join(fs_path_appdata(), "xuolauncher", "cache");
    ctx.config.output_path = fs_path_from(path);

    fs_path dir = ctx.config.cache_path;
    if (!fs_path_create(dir))
    {
        LOG_ERROR("failed to create directory: %s", fs_path_ascii(dir));
        return nullptr;
    }

    dir = ctx.config.output_path;
    if (!fs_path_create(dir))
    {
        LOG_ERROR("failed to create directory: %s", fs_path_ascii(dir));
        return nullptr;
    }

    ctx.config.initialized = true;
    return &ctx;
}

void xuo_shutdown(xuo_context *ctx)
{
    if (!ctx)
        return;
    ctx->config.initialized = false;
    ctx->releases.clear();
    ctx->changelog.clear();
    ctx->manifest.clear();
    ctx->doc.Clear();
}
