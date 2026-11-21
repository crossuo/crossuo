// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

#include "ArtReplacer.h"

#include <stdio.h>
#include <string.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <common/logging/logging.h>
#include <external/gfx/stb_image.h>
#include <xuocore/common.h>       // LOG_* compat macros
#include <xuocore/uodata_color.h> // uo_color32_to_16

#define LOGGER_MODULE Client

namespace
{
fs_path s_root;
bool s_initialized = false;
std::mutex s_mutex; // double-buffered draw threads share the lookup cache

// (kind << 32) | id -> replacement file. A missing id is NOT cached: the
// patch install writes these files while the game runs, and the login screen
// resolves ids early - a cached "absent" would pin the native graphic for the
// whole session. A resolved hit is cached (replace-on-disk is rare; the next
// launch picks it up).
std::unordered_map<uint64_t, fs_path> s_resolved;

static const char *const s_kind_dirs[] = { "gumps", "art", "land" };
// png first: the format's own; bmp/jpg hand-placed extras
static const char *const s_exts[] = { ".png", ".bmp", ".jpg", ".jpeg" };

fs_path find_images_root(const fs_path &install_dir)
{
    static const char *const names[] = { "ExternalImages", "externalimages" };
    for (const char *name : names)
    {
        const fs_path path = fs_path_join(install_dir, name);
        if (fs_path_exists(path))
        {
            return path;
        }
    }
    // installed names are lowercase; a hand-made tree may not be. The case
    // walk (fs_insensitive) is a desktop convenience only - the web lane has
    // its own file table and resolves exact names.
    if (fs_path_empty(install_dir))
    {
        return fs_path();
    }
    const fs_path insensitive = fs_insensitive(fs_path_join(install_dir, "ExternalImages"));
    return fs_path_exists(insensitive) ? insensitive : fs_path();
}

// decimal ("83") or hex ("0x53") id stem candidates, in resolution order
size_t id_stems(uint16_t id, char out[2][16])
{
    snprintf(out[0], 16, "%u", unsigned(id));
    snprintf(out[1], 16, "0x%x", unsigned(id));
    return 2;
}

// first readable <root>/<kind>/<id>.<ext>: the flat layout the patch planner
// installs, plus an optional intermediate folder for hand-organized trees
bool resolve(int kind, uint16_t id, fs_path &out)
{
    static const char *const subdirs[] = { "", "gumps/", "art/", "land/" };
    char stems[2][16];
    const size_t stem_count = id_stems(id, stems);
    for (const char *subdir : subdirs)
    {
        for (size_t si = 0; si < stem_count; ++si)
        {
            for (const char *ext : s_exts)
            {
                const fs_path path = fs_path_join(
                    s_root, astr_t(s_kind_dirs[kind]) + "/" + subdir + stems[si] + ext);
                if (fs_path_exists(path))
                {
                    out = path;
                    return true;
                }
            }
        }
    }
    return false;
}

// png/bmp/jpg -> the engine's 16-bit color-key format (the only layout the
// game's art pipeline speaks: 0 transparent, 0x8000 | RGB555 opaque). Alpha
// is 1-bit, same as every mul/uop graphic; stb delivers top-down RGBA8 and
// UO rows are top-down too, so no flip.
bool decode(const fs_path &path, std::vector<uint16_t> &pixels, short &width, short &height)
{
    int w = 0;
    int h = 0;
    int comp = 0;
    uint8_t *rgba = stbi_load(fs_path_ascii(path), &w, &h, &comp, 4);
    if (rgba == nullptr)
    {
        LOG_ERROR(
            "art replacer: could not decode \"%s\": %s",
            fs_path_ascii(path),
            stbi_failure_reason());
        return false;
    }
    if (w <= 0 || h <= 0 || w > 0x4000 || h > 0x4000)
    {
        LOG_ERROR("art replacer: \"%s\" has an unusable size (%dx%d)", fs_path_ascii(path), w, h);
        stbi_image_free(rgba);
        return false;
    }
    pixels.resize(size_t(w) * size_t(h));
    for (int i = 0; i < w * h; ++i)
    {
        const uint8_t *px = rgba + size_t(i) * 4;
        pixels[size_t(i)] =
            px[3] < 128 ? uint16_t(0) :
                          uint16_t(0x8000 | uo_color32_to_16((px[0] << 16) | (px[1] << 8) | px[2]));
    }
    stbi_image_free(rgba);
    width = short(w);
    height = short(h);
    return true;
}
} // namespace

bool art_replacer_decode(const fs_path &path, std::vector<uint16_t> &pixels, short &width, short &height)
{
    return decode(path, pixels, width, height);
}

void art_replacer_init(const fs_path &install_dir)
{
    if (s_initialized || fs_path_empty(install_dir))
    {
        return;
    }
    s_root = find_images_root(install_dir);
    s_initialized = true; // even without a root: a later patch install is
                          // tested per id, so nothing needs re-initializing
    if (!fs_path_empty(s_root))
    {
        LOG_INFO("art replacer: images root %s", fs_path_ascii(s_root));
    }
}

bool art_replacer_has(int kind, uint16_t id)
{
    if (!s_initialized || fs_path_empty(s_root) || kind < 0 || kind > 2)
    {
        return false;
    }
    const uint64_t key = (uint64_t(kind) << 32) | id;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_resolved.find(key) != s_resolved.end())
    {
        return true;
    }
    fs_path path;
    if (!resolve(kind, id, path))
    {
        return false;
    }
    s_resolved.emplace(key, path);
    return true;
}

bool art_replacer_pixels(
    int kind, uint16_t id, std::vector<uint16_t> &pixels, short &width, short &height)
{
    if (!art_replacer_has(kind, id))
    {
        return false;
    }
    fs_path path;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        path = s_resolved[(uint64_t(kind) << 32) | id];
    }
    return decode(path, pixels, width, height);
}
