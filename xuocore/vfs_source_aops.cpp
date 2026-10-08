// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_source_aops: the "aops" patch-manifest format. See vfs_source_aops.h.

#define LOGGER_MODULE Vfs

#include "vfs_source_aops.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <map>

#include "common.h"
#include <common/logging/logging.h>
#include <common/str.h>

#include <external/lookup3.h> // hashlittle2: the product serial fingerprint

// ---------------------------------------------------------------------------
// format sniffing
// ---------------------------------------------------------------------------

bool vfs_aops_sniff_patchlist(const uint8_t *data, size_t size)
{
    // UTF-16LE BOM plus at least one pipe in the first line, or plain text
    // whose first line carries pipes; the ini/patchcfg and the official XML
    // have their own shapes
    if (data == nullptr || size < 8)
        return false;
    if (data[0] == 0xFF && data[1] == 0xFE)
    {
        for (size_t i = 2; i + 1 < size; i += 2)
        {
            if (data[i] == '\n' && data[i + 1] == 0)
                break;
            if (data[i] == '|' && data[i + 1] == 0)
                return true;
        }
        return false;
    }
    size_t line = 0;
    bool pipe = false;
    for (; line < size && line < 512; ++line)
    {
        if (data[line] == '\n' || data[line] == '\r')
            break;
        if (data[line] == '|')
            pipe = true;
    }
    return pipe && line > 8;
}

bool vfs_aops_sniff_patchcfg(const uint8_t *data, size_t size)
{
    // the ini carries the patch system section; everything else about it is
    // shard-specific
    if (data == nullptr || size < 16)
        return false;
    static const char mark[] = "[patch_system_configuration]";
    const size_t mark_len = sizeof(mark) - 1;
    const size_t scanned = size < 4096 ? size : 4096;
    for (size_t i = 0; i + mark_len <= scanned; ++i)
    {
        if (memcmp(data + i, mark, mark_len) == 0)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// text helpers (no exceptions: this code builds with -fno-exceptions)
// ---------------------------------------------------------------------------

// decodes UTF-16LE (with or without BOM) to UTF-8
static bool vfs_aops_utf16_to_utf8(const uint8_t *data, size_t size, astr_t &out)
{
    out.clear();
    if (size < 2)
        return false;
    if (data[0] == 0xFF && data[1] == 0xFE)
    {
        data += 2;
        size -= 2;
    }
    if (size % 2 != 0)
        return false;
    out.reserve(size / 2);
    for (size_t i = 0; i < size; i += 2)
    {
        const uint32_t unit = uint32_t(data[i]) | (uint32_t(data[i + 1]) << 8);
        uint32_t cp = unit;
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < size)
        {
            const uint32_t low = uint32_t(data[i + 2]) | (uint32_t(data[i + 3]) << 8);
            if (low >= 0xDC00 && low <= 0xDFFF)
            {
                cp = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            }
        }
        if (cp < 0x80)
        {
            out.push_back(char(cp));
        }
        else if (cp < 0x800)
        {
            out.push_back(char(0xC0 | (cp >> 6)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back(char(0xF0 | (cp >> 18)));
            out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
    }
    return true;
}

// percent-encodes everything outside the unreserved url path set so the
// tail doubles as the store's relative path (spaces, UTF-8 bytes)
static astr_t vfs_aops_url_encode(const astr_t &path)
{
    static const char hex[] = "0123456789ABCDEF";
    astr_t out;
    out.reserve(path.size() + 8);
    for (const char c : path)
    {
        const auto uc = uint8_t(c);
        if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') ||
            (uc >= '0' && uc <= '9') || uc == '-' || uc == '_' || uc == '.' ||
            uc == '~' || uc == '/')
        {
            out.push_back(c);
        }
        else
        {
            out.push_back('%');
            out.push_back(hex[uc >> 4]);
            out.push_back(hex[uc & 0x0F]);
        }
    }
    return out;
}

// digits-only parse of a decimal field (0 on anything else); the manifest
// carries no negative numbers and this code builds without exceptions
static uint64_t vfs_aops_parse_number(const astr_t &field)
{
    uint64_t value = 0;
    for (const char c : field)
    {
        if (c < '0' || c > '9')
            return 0;
        value = value * 10 + uint64_t(c - '0');
    }
    return value;
}

// ---------------------------------------------------------------------------
// the UO data filter: what a client install is made of
// ---------------------------------------------------------------------------

namespace
{

// extensions that are UO data by themselves (any stem)
const char *const VFS_AOPS_EXT_DATA[] = {
    ".mul",
    ".idx",
    ".uop",
    ".def",
    ".bin",
    ".iff",
};

// language-suffixed client text (<stem><lang>): the stock client's known
// families only - a shard's "whatever.esp" is not game data
const char *const VFS_AOPS_LANG_STEMS[] = {
    "cliloc.",   "skilname.", "skill16.",   "skill30.",   "chat.",
    "gesture.",  "intloc",    "intro.",     "options.",   "optnuotd.",
    "optuotd.",  "professn.", "tilehelp.",  "tooltips.",  "gt_",
};
const char *const VFS_AOPS_LANGS[] = {
    ".cht", ".chs", ".deu", ".enu", ".esp", ".fra", ".jpn", ".kor",
};

// exact data-file names outside the classes above (client config like
// uo.cfg / login.cfg and launcher files stay out)
const char *const VFS_AOPS_NAMES[] = {
    "arttable.cfg", "blueprints.tbp", "conf_ignore.lst", "credits.dat",
    "default.mac",  "doors.txt",      "floors.txt",      "gamegen.txt",
    "helpindx.dat", "ignore.lst",     "keynames.txt",    "langcode.iff",
    "macros.txt",   "macros2d.txt",   "misc.txt",        "mobtypes.txt",
    "multimap.rle", "obscene.lst",    "prof.txt",        "roof.txt",
    "stairs.txt",   "teleprts.txt",   "tile1024.txt",    "tile256.txt",
    "uobscene.lst", "version.dat",    "version.txt",     "walls.txt",
};

template <size_t N>
bool vfs_aops_has_ext(const astr_t &lower, const char *const (&exts)[N])
{
    for (size_t i = 0; i < N; ++i)
    {
        const size_t len = strlen(exts[i]);
        if (lower.size() > len && lower.compare(lower.size() - len, len, exts[i]) == 0)
            return true;
    }
    return false;
}

template <size_t N>
bool vfs_aops_is_name(const astr_t &lower, const char *const (&names)[N])
{
    for (size_t i = 0; i < N; ++i)
    {
        if (lower == names[i])
            return true;
    }
    return false;
}

} // namespace

bool vfs_aops_filter_accept(const astr_t &basename_lower)
{
    if (basename_lower.empty())
        return false;
    if (vfs_aops_has_ext(basename_lower, VFS_AOPS_EXT_DATA))
        return true;
    if (vfs_aops_has_ext(basename_lower, VFS_AOPS_LANGS))
    {
        for (const char *stem : VFS_AOPS_LANG_STEMS)
        {
            if (basename_lower.rfind(stem, 0) == 0)
                return true;
        }
        return false;
    }
    return vfs_aops_is_name(basename_lower, VFS_AOPS_NAMES);
}

// ---------------------------------------------------------------------------
// patchcfg ini
// ---------------------------------------------------------------------------

bool vfs_aops_parse_patchcfg(
    const uint8_t *data, size_t size, const astr_t &root, vfs_aops_config *out)
{
    if (data == nullptr || size == 0 || out == nullptr)
        return false;
    astr_t text;
    text.reserve(size);
    for (size_t i = 0; i < size; ++i)
    {
        if (data[i] != '\r')
            text.push_back(char(data[i]));
    }

    astr_t section;
    bool have_patch = false;
    size_t pos = 0;
    while (pos < text.size())
    {
        const size_t eol = text.find('\n', pos);
        const astr_t line =
            text.substr(pos, eol == astr_t::npos ? astr_t::npos : eol - pos);
        if (eol == astr_t::npos)
            break;
        pos = eol + 1;
        if (line.empty() || line.front() == ';' || line.front() == '#')
            continue;
        if (line.front() == '[')
        {
            const size_t end = line.find(']');
            section = end == astr_t::npos ? astr_t() : line.substr(1, end - 1);
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == astr_t::npos)
            continue;
        const astr_t key = line.substr(0, eq);
        astr_t value = line.substr(eq + 1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
            value.pop_back();
        if (section == "project")
        {
            if (key == "custom_extension")
                out->custom_extension = value;
            else if (key == "patch_salt_url")
                out->patch_salt_url = value;
        }
        else if (section == "patch_system_configuration")
        {
            have_patch = true;
            if (key == "online_patch_directory")
                out->patch_directory = value;
            else if (key == "online_patchlist_file")
                out->patchlist_url = value;
            else if (key == "update_local_file")
                out->version_file = value;
        }
    }
    if (!have_patch)
        return false;
    // relative urls resolve against the root the caller pointed at
    const auto resolve = [&root](astr_t &url) {
        if (url.empty() || url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0)
            return;
#if defined(XUO_EMSC)
        // web: the streaming proxy's rewritten patchcfg hands out
        // same-origin proxy paths (/stream/<encoded upstream>) - they are
        // already absolute for the page origin, a root-relative join would
        // double-prefix them. Native keeps the root-relative semantics
        // (patch-server relative directories).
        if (url.front() == '/')
            return;
#endif
        if (url.front() == '/')
            url.insert(0, root);
        else
            url.insert(0, root.back() == '/' ? root : root + "/");
    };
    resolve(out->patch_directory);
    resolve(out->patchlist_url);
    resolve(out->patch_salt_url);
    if (!out->patch_directory.empty() && out->patch_directory.back() != '/')
        out->patch_directory += '/';
    return true;
}

// ---------------------------------------------------------------------------
// plan build
// ---------------------------------------------------------------------------

astr_t vfs_aops_product_url(const astr_t &root_or_url)
{
    astr_t url = root_or_url;
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    static const char *const leaves[] = { "/patchlist", "/patchcfg" };
    for (const char *leaf : leaves)
    {
        const size_t len = strlen(leaf);
        if (url.size() > len && url.compare(url.size() - len, len, leaf) == 0)
        {
            url.resize(url.size() - len);
            break;
        }
    }
    url += "/patchlist";
    return url;
}

astr_t vfs_aops_product_url_canonical(const astr_t &root_or_url)
{
    // a leaf with an extension names a file (a full manifest url: the mythic
    // "uo-legacyrelease.prod", a "prod.xml", ...): pass it through. Anything
    // else is a bare root (trailing /, .../patchlist, .../patchcfg): normalize
    const astr_t &url = root_or_url;
    const size_t slash = url.find_last_of('/');
    if (slash != astr_t::npos && url.find('.', slash) != astr_t::npos)
        return url;
    return vfs_aops_product_url(url);
}

uint64_t vfs_aops_serial(const uint8_t *data, size_t size)
{
    // the codebase's content hash (lookup3): the serial is only ever
    // compared against itself, any stable 64-bit fingerprint works
    uint32_t ph = 0, sh = 0;
    hashlittle2(data, size, &ph, &sh);
    return (uint64_t(sh) << 32) | uint64_t(ph);
}

// the patchcfg sits beside the patchlist; cached in the blob store so
// offline rebuilds keep working
static bool vfs_aops_load_patchcfg(
    const vfs_manifest_params &params, const astr_t &product_url, vfs_aops_config *out)
{
    astr_t root = product_url;
    const size_t slash = root.rfind('/');
    root = slash == astr_t::npos ? astr_t() : root.substr(0, slash + 1);
    const fs_path cache = fs_path_join(params.blobs_dir, "aops", "patchcfg");

    std::vector<uint8_t> data;
    if (fs_file_read(fs_path_ascii(cache), data) &&
        vfs_aops_sniff_patchcfg(data.data(), data.size()))
    {
        return vfs_aops_parse_patchcfg(data.data(), data.size(), root, out);
    }
    if (params.fetch == nullptr || root.empty())
    {
        LOG_WARN("aops: no cached patchcfg and nothing to fetch it with: defaults");
        out->custom_extension = "aops";
        return true; // a cached plan still rebuilds with the default layout
    }
    const astr_t url = root + "patchcfg";
    data.clear();
    uint8_t buffer[16384];
    size_t size = sizeof(buffer);
    // the seam downloads into the store AND the buffer; the store copy is
    // the offline cache for the next rebuild
    if (!params.fetch(
            params.fetch_context, url.c_str(), "aops/patchcfg", fs_path_ascii(cache),
            buffer, &size))
    {
        LOG_WARN("aops: could not download the patchcfg: %s", url.c_str());
        out->custom_extension = "aops";
        return true; // a cached plan still rebuilds with the default layout
    }
    if (!vfs_aops_parse_patchcfg(buffer, size, root, out))
    {
        LOG_WARN("aops: unrecognized patchcfg at %s: using defaults", url.c_str());
        out->custom_extension = "aops";
    }
    return true;
}

bool vfs_aops_build_plan(
    const vfs_manifest_params &params,
    const std::vector<uint8_t> &patchlist,
    const astr_t &product_url,
    VfsPlan &out_plan)
{
    out_plan = VfsPlan();

    vfs_aops_config cfg;
    vfs_aops_load_patchcfg(params, product_url, &cfg);

    astr_t text;
    if (!vfs_aops_utf16_to_utf8(patchlist.data(), patchlist.size(), text))
    {
        LOG_ERROR("aops: odd-sized patchlist is not UTF-16");
        return false;
    }

    VfsPlan plan;
    plan.manifest_repo = cfg.patchlist_url;
    plan.file_repo = cfg.patch_directory;
    plan.subset = uint8_t(vfs_subset_none);

    std::map<astr_t, bool> seen; // lowercase basename: first entry installs
    size_t skipped_dupes = 0;
    size_t line_start = 0;
    while (line_start < text.size())
    {
        const size_t eol = text.find('\n', line_start);
        const size_t end = eol == astr_t::npos ? text.size() : eol;
        astr_t line = text.substr(line_start, end - line_start);
        line_start = eol == astr_t::npos ? text.size() : eol + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty())
            continue;

        // ten pipe-separated fields; anything else is tolerated noise
        astr_t fields[10];
        size_t count = 0;
        size_t pos = 0;
        while (count < 10)
        {
            const size_t pipe = line.find('|', pos);
            fields[count++] =
                pipe == astr_t::npos ? line.substr(pos) : line.substr(pos, pipe - pos);
            if (pipe == astr_t::npos)
                break;
            pos = pipe + 1;
        }
        if (count != 10)
            continue;

        // relpath -> components (backslash separated on the wire)
        const auto &rel = fields[0];
        std::vector<astr_t> parts;
        size_t rpos = 0;
        while (rpos <= rel.size())
        {
            const size_t bs = rel.find('\\', rpos);
            parts.push_back(
                bs == astr_t::npos ? rel.substr(rpos) : rel.substr(rpos, bs - rpos));
            if (bs == astr_t::npos)
                break;
            rpos = bs + 1;
        }
        if (parts.empty() || parts.back().empty())
            continue;
        bool safe = true;
        for (const auto &part : parts)
        {
            if (!vfs_path_component_ok(part.c_str()))
            {
                safe = false;
                break;
            }
        }
        if (!safe)
        {
            LOG_WARN("aops: skipping unsafe path \"%s\"", rel.c_str());
            continue;
        }

        // installing enforces lowercase: the game's file requests resolve
        // through the case-normalized managed registry
        const astr_t name = str_lower(parts.back());
        if (!vfs_aops_filter_accept(name))
            continue; // not UO data: launcher/web/shard files stay out
        if (!seen.emplace(name, true).second)
        {
            ++skipped_dupes;
            continue; // case-duplicate: the first entry installed
        }

        const uint64_t stamp_date = vfs_aops_parse_number(fields[1]); // DDMMYYYY
        const uint64_t stamp_hms =
            vfs_aops_parse_number(fields[2]) * 10000ull +
            vfs_aops_parse_number(fields[3]) * 100ull + vfs_aops_parse_number(fields[4]);
        const uint64_t size = vfs_aops_parse_number(fields[6]);
        if (size == 0)
            continue; // no usable size: cannot verify or fetch
        // wire stamp -> one number (YYYYMMDDhhmmss) so plan diffs see bumps
        const uint64_t stamp =
            (stamp_date % 10000ull) * 10000000000ull + // YYYY
            (stamp_date / 1000000ull) * 100000000ull + // MM
            ((stamp_date / 10000ull) % 100ull) * 1000000ull + // DD
            stamp_hms;

        parts.back() = name;
        astr_t remote;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (i != 0)
                remote += "/";
            remote += vfs_aops_url_encode(parts[i]);
        }
        remote += vfs_aops_url_encode(cfg.custom_extension);

        VfsPlan::VfsFile file;
        file.name = name;
        file.is_uop = false;
        file.size = size;
        VfsAsset asset;
        asset.name = name;
        asset.ul = size_t(size);
        asset.cl = size_t(size); // stored: the blob is the payload
        asset.ct = 0;
        asset.timestamp = stamp;
        asset.blob = remote;
        file.assets.emplace_back(asset);
        plan.files.emplace_back(file);
    }

    plan.product_serial = vfs_aops_serial(patchlist.data(), patchlist.size());
    out_plan = std::move(plan);
    LOG_INFO(
        "aops: %zu data files planned (%zu case-duplicates skipped), data url %s",
        out_plan.files.size(),
        skipped_dupes,
        cfg.patch_directory.c_str());
    return !out_plan.files.empty();
}
