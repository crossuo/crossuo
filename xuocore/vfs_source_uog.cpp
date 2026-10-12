// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_source_uog: the "uog" json manifest format. See vfs_source_uog.h.

#define LOGGER_MODULE Vfs

#include "vfs_source_aops.h" // vfs_aops_filter_accept: the shared classifier
#include "vfs_source_uog.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

#include "common.h"
#include <common/logging/logging.h>
#include <common/str.h>

#include <external/lookup3.h> // hashlittle2: the product serial fingerprint

// ---------------------------------------------------------------------------
// format sniffing
// ---------------------------------------------------------------------------

bool vfs_uog_sniff_manifest(const uint8_t *data, size_t size)
{
    // a json object whose FileList key sits in the header: cheap to check
    // and unambiguous against the patchlist and the mythic xml shapes
    if (data == nullptr || size < 12)
        return false;
    size_t i = 0;
    while (i < size && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r'))
        ++i;
    if (i >= size || data[i] != '{')
        return false;
    static const char mark[] = "\"FileList\"";
    const size_t mark_len = sizeof(mark) - 1;
    const size_t scanned = size < 64 ? size : 64;
    for (size_t j = 0; j + mark_len <= scanned; ++j)
    {
        if (memcmp(data + j, mark, mark_len) == 0)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// text helpers (no exceptions: this code builds with -fno-exceptions)
// ---------------------------------------------------------------------------

// percent-encodes everything outside the unreserved url path set so the
// tail doubles as the store's relative path (spaces, UTF-8 bytes); the same
// encoding the aops source uses
static astr_t vfs_uog_url_encode(const astr_t &path)
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

// the wire stamp's "YYYY-MM-DDTHH:MM[:SS]" prefix -> one 14-digit number
// (YYYYMMDDhhmmss) so plan diffs see bumps; the fraction and the trailing Z
// carry nothing a diff needs. A missing seconds field pads it (00) so every
// stamp keeps the same digit count; 0 on anything else
static uint64_t vfs_uog_parse_stamp(const astr_t &when)
{
    if (when.size() < 16)
        return 0;
    static const char shape[] = "####-##-##T##:##";
    for (size_t i = 0; i < 16; ++i)
    {
        const char c = when[i];
        if (shape[i] == '#' ? (c < '0' || c > '9') : c != shape[i])
            return 0;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < 16; ++i)
    {
        if (when[i] < '0' || when[i] > '9')
            continue;
        value = value * 10 + uint64_t(when[i] - '0');
    }
    if (
        when.size() >= 19 && when[16] == ':' && when[17] >= '0' && when[17] <= '9' &&
        when[18] >= '0' && when[18] <= '9')
    {
        value = value * 100 + uint64_t(when[17] - '0') * 10 + uint64_t(when[18] - '0');
    }
    else
    {
        value = value * 100; // seconds-less wire form: pad, keep 14 digits
    }
    return value;
}

// base64 -> the 16 md5 bytes; false on any invalid length or character (the
// caller zeroes the digest, which zeroes the asset's rh)
static bool vfs_uog_base64_md5(const astr_t &in, uint8_t out[16])
{
    static const char *const alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    memset(out, 0, 16);
    size_t o = 0;
    uint32_t acc = 0;
    int bits = 0;
    bool padded = false;
    for (const char c : in)
    {
        if (c == '=')
        {
            padded = true;
            continue;
        }
        if (padded) // payload after the padding: not base64
            return false;
        const char *f = c == '\0' ? nullptr : strchr(alphabet, c);
        if (f == nullptr || o >= 16)
            return false;
        acc = (acc << 6) | uint32_t(f - alphabet);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out[o++] = uint8_t(acc >> bits);
        }
    }
    return o == 16;
}

// ---------------------------------------------------------------------------
// the json scanner: a hand-rolled reader for the manifest's fixed schema
// (no json library in xuocore, and the schema is four keys). Malformed
// input only ever sets `failed` - the build turns that into a clean false
// ---------------------------------------------------------------------------

namespace
{

struct vfs_uog_scan
{
    const uint8_t *p = nullptr;
    const uint8_t *end = nullptr;
    bool failed = false;

    vfs_uog_scan(const uint8_t *data, size_t size) : p(data), end(data + size) {}

    bool ok() const { return !failed; }

    void skip_ws()
    {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }

    // consumes the character or fails
    bool eat(char c)
    {
        skip_ws();
        if (p < end && *p == uint8_t(c))
        {
            ++p;
            return true;
        }
        failed = true;
        return false;
    }

    // consumes the character when it is next (no failure either way)
    bool accept(char c)
    {
        skip_ws();
        if (p < end && *p == uint8_t(c))
        {
            ++p;
            return true;
        }
        return false;
    }

    bool peek(char c)
    {
        skip_ws();
        return p < end && *p == uint8_t(c);
    }

    // reads a quoted string into `out` (json-unescaped); the manifest's
    // escapes in the wild are \\ and \" - any other escape passes the
    // escaped character through
    bool string(astr_t &out)
    {
        out.clear();
        skip_ws();
        if (p >= end || *p != '"')
        {
            failed = true;
            return false;
        }
        ++p;
        while (p < end)
        {
            const char c = char(*p++);
            if (c == '"')
                return true;
            if (c != '\\')
            {
                out.push_back(c);
                continue;
            }
            if (p >= end)
                break;
            out.push_back(char(*p++)); // \\ -> \, \" -> ", anything else: itself
        }
        failed = true;
        return false;
    }

    // digits-only unsigned read (the manifest carries no negative numbers
    // and this code builds without exceptions)
    bool number(uint64_t &out)
    {
        out = 0;
        skip_ws();
        const uint8_t *start = p;
        while (p < end && *p >= '0' && *p <= '9')
        {
            out = out * 10 + uint64_t(*p - '0');
            ++p;
        }
        if (p == start)
        {
            failed = true;
            return false;
        }
        return true;
    }

    // skips over any json value (string, number, object, array, literal);
    // unknown keys flow through here
    bool skip_value()
    {
        skip_ws();
        if (p >= end)
        {
            failed = true;
            return false;
        }
        if (*p == '"')
        {
            astr_t drop;
            return string(drop);
        }
        if (*p == '{' || *p == '[')
        {
            int depth = 0;
            while (p < end)
            {
                const char c = char(*p++);
                if (c == '"') // strings may carry the brackets
                {
                    --p;
                    astr_t drop;
                    if (!string(drop))
                        return false;
                    continue;
                }
                if (c == '{' || c == '[')
                    ++depth;
                else if (c == '}' || c == ']')
                {
                    if (--depth == 0)
                        return true;
                }
            }
            failed = true;
            return false;
        }
        // number, true, false, null: up to the next delimiter
        while (
            p < end && *p != ',' && *p != '}' && *p != ']' && *p != ' ' && *p != '\t' &&
            *p != '\n' && *p != '\r')
            ++p;
        return true;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// plan build
// ---------------------------------------------------------------------------

uint64_t vfs_uog_serial(const uint8_t *data, size_t size)
{
    // the codebase's content hash (lookup3): the serial is only ever
    // compared against itself, any stable 64-bit fingerprint works
    uint32_t ph = 0, sh = 0;
    hashlittle2(data, size, &ph, &sh);
    return (uint64_t(sh) << 32) | uint64_t(ph);
}

bool vfs_uog_build_plan(
    const vfs_manifest_params &params,
    const std::vector<uint8_t> &data,
    const astr_t &product_url,
    VfsPlan &out_plan)
{
    out_plan = VfsPlan();
    (void)params; // no sidecar config: the manifest url carries everything

    vfs_uog_scan scan(data.data(), data.size());
    astr_t key;

    VfsPlan plan;
    plan.manifest_repo = product_url;
    // the blobs hang off the manifest's directory: empty when the url has
    // no slash (nothing can be fetched)
    const size_t slash = product_url.rfind('/');
    plan.file_repo = slash == astr_t::npos ? astr_t() : product_url.substr(0, slash + 1);
    plan.subset = uint8_t(vfs_subset_none);

    std::map<astr_t, bool> seen; // lowercase basename: first entry installs
    size_t filtered = 0;
    size_t skipped_dupes = 0;
    size_t skipped_empty = 0;

    if (!scan.eat('{'))
    {
        LOG_ERROR("uog: manifest is not a json object");
        return false;
    }
    // FileList may sit anywhere in the top-level object (the sniff only
    // gates on it living in the header window): anything before it is
    // skipped whole
    bool have_list = false;
    while (scan.ok() && !scan.peek('}'))
    {
        if (!scan.string(key) || !scan.eat(':'))
            break;
        if (key == "FileList")
        {
            have_list = scan.eat('[');
            break;
        }
        scan.skip_value();
        scan.accept(',');
    }
    if (!have_list)
    {
        LOG_ERROR("uog: manifest carries no FileList array");
        return false;
    }

    scan.skip_ws();
    for (bool first_entry = true; scan.ok() && !scan.peek(']'); first_entry = false)
    {
        if (!first_entry && !scan.accept(','))
            break;
        if (!scan.eat('{'))
            break;

        astr_t file_name;
        uint64_t file_size = 0;
        astr_t last_write;
        astr_t md5_hash;
        bool first_pair = true;
        while (scan.ok() && !scan.peek('}'))
        {
            if (!first_pair && !scan.accept(','))
                break;
            first_pair = false;
            if (!scan.string(key) || !scan.eat(':'))
                break;
            // the four known keys in any order; anything else is tolerated
            // noise (skipped whole)
            if (key == "FileName")
                scan.string(file_name);
            else if (key == "FileSize")
                scan.number(file_size);
            else if (key == "LastWrite")
                scan.string(last_write);
            else if (key == "MD5Hash")
                scan.string(md5_hash);
            else
                scan.skip_value();
        }
        if (!scan.eat('}'))
            break;

        // relpath -> components (backslash separated on the wire, rooted:
        // the leading \\ leaves an empty first component)
        std::vector<astr_t> parts;
        size_t rpos = 0;
        while (rpos <= file_name.size())
        {
            const size_t bs = file_name.find('\\', rpos);
            parts.push_back(
                bs == astr_t::npos ? file_name.substr(rpos) : file_name.substr(rpos, bs - rpos));
            if (bs == astr_t::npos)
                break;
            rpos = bs + 1;
        }
        while (!parts.empty() && parts.front().empty())
            parts.erase(parts.begin()); // rooted paths: drop the empty head
        if (parts.empty())
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
            LOG_WARN("uog: skipping unsafe path \"%s\"", file_name.c_str());
            continue;
        }

        // installing enforces lowercase: the game's file requests resolve
        // through the case-normalized managed registry (aops lowercases its
        // remote paths only because that server convention differs). The
        // blob tail keeps the manifest's exact case: the cdn serves what
        // its manifest names.
        const astr_t name = str_lower(parts.back());
        if (!vfs_aops_filter_accept(name))
        {
            ++filtered; // not UO data: launcher/web/shard files stay out
            continue;
        }
        if (file_size == 0)
        {
            ++skipped_empty;
            continue; // no usable size: cannot verify or fetch; the name
                      // stays unreserved so a later real entry installs
        }
        if (!seen.emplace(name, true).second)
        {
            ++skipped_dupes;
            continue; // case-duplicate: the first entry installed
        }

        const uint64_t stamp = vfs_uog_parse_stamp(last_write);
        // the md5 rides along as rh: the full 128-bit digest is not wired
        // into blob verification (sizes + the product serial cover
        // integrity today), rh carries the diff-sensitivity so a
        // hash-only content bump still triggers redownload. An
        // undecodable digest zeroes rh instead of dropping the entry.
        uint8_t md5[16];
        vfs_uog_base64_md5(md5_hash, md5);
        const uint32_t rh = uint32_t(md5[0]) | (uint32_t(md5[1]) << 8) |
                            (uint32_t(md5[2]) << 16) | (uint32_t(md5[3]) << 24);

        astr_t remote;
        for (size_t i = 0; i < parts.size(); ++i)
        {
            if (i != 0)
                remote += "/";
            remote += vfs_uog_url_encode(parts[i]);
        }

        VfsPlan::VfsFile file;
        file.name = name;
        file.is_uop = false;
        file.size = file_size;
        VfsAsset asset;
        asset.name = name;
        asset.ul = size_t(file_size);
        asset.cl = size_t(file_size); // stored: the blob is the payload
        asset.ct = 0;
        asset.timestamp = stamp;
        asset.rh = rh;
        asset.blob = remote;
        file.assets.emplace_back(asset);
        plan.files.emplace_back(file);
    }
    if (!scan.eat(']'))
    {
        LOG_ERROR("uog: malformed FileList array");
        return false;
    }

    plan.product_serial = vfs_uog_serial(data.data(), data.size());
    out_plan = std::move(plan);
    LOG_INFO(
        "uog: %zu data files planned (%zu filtered, %zu duplicates, %zu empty), data url %s",
        out_plan.files.size(),
        filtered,
        skipped_dupes,
        skipped_empty,
        out_plan.file_repo.c_str());
    return !out_plan.files.empty();
}
