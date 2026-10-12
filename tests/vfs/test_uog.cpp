// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// uog tests: the streaming json manifest source. Sniffing (the json shape
// against the other product formats), the lookup3 content serial, the plan
// build (names, stamps, md5 prefixes, url tails, repo derivation, the
// noise tolerance), the format dispatch inside vfs_manifest_build, the
// bare-root boot ladder onto /Manifest.json and the serial update flow.

#define LOGGER_MODULE TestVfs

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "test_main.h"
#include "test_product.h"
#include "test_server.h"

#include <common/fs.h>
#include <common/logging/logging.h>
#include <xuocore/http.h>
#include <xuocore/vfs.h>
#include <xuocore/vfs_manifest.h>
#include <xuocore/vfs_source_uog.h>

// mutes the production logging of expected negative-path warnings so the
// suite output stays deterministic
struct uog_log_mute
{
    eLogSystem saved;
    uog_log_mute()
        : saved(g_LogEnabled)
    {
        g_LogEnabled = LogSystemNone;
    }
    ~uog_log_mute() { g_LogEnabled = saved; }
};

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static std::vector<uint8_t> test_uog_bytes(const char *json)
{
    return std::vector<uint8_t>(json, json + strlen(json));
}

static bool test_uog_build(const char *json, const char *product_url, VfsPlan &plan)
{
    const std::vector<uint8_t> data = test_uog_bytes(json);
    vfs_manifest_params params;
    return vfs_uog_build_plan(params, data, product_url, plan);
}

static const VfsPlan::VfsFile *find_file(const VfsPlan &plan, const char *name)
{
    for (const auto &file : plan.files)
    {
        if (file.name == name)
            return &file;
    }
    return nullptr;
}

static bool test_read_file(const fs_path &path, std::vector<uint8_t> &out)
{
    if (!fs_file_read(fs_path_ascii(path), out))
    {
        vfs_test_fail(__FILE__, __LINE__, "could not read %s", fs_path_ascii(path));
        return false;
    }
    return true;
}

// writes the patchlist as utf-16 le with bom and crlf (the wire format)
static void test_write_patchlist(const fs_path &path, const std::vector<astr_t> &lines)
{
    astr_t utf16;
    utf16.push_back(char(0xFF));
    utf16.push_back(char(0xFE));
    for (const auto &line : lines)
    {
        for (const char c : line)
        {
            utf16.push_back(c);
            utf16.push_back(char(0));
        }
        utf16.push_back('\r');
        utf16.push_back(char(0));
        utf16.push_back('\n');
        utf16.push_back(char(0));
    }
    std::vector<uint8_t> payload(utf16.begin(), utf16.end());
    fs_file_write(path, payload);
}

// the one-entry server manifest: cliloc.enu (the language-stem filter's
// exact match) plus one filtered launcher file for shape; the data file's
// blob is the file itself on the cdn (the store mirrors the url tree)
static astr_t test_uog_server_manifest(size_t cliloc_size)
{
    char buf[512];
    snprintf(
        buf,
        sizeof(buf),
        "{\"FileList\":["
        "{\"FileName\":\"\\\\cliloc.enu\",\"FileSize\":%zu,"
        "\"LastWrite\":\"2025-10-28T20:41:01.8507375Z\","
        "\"MD5Hash\":\"yv66vhI0VniavN7wESIzRA==\"},"
        "{\"FileName\":\"\\\\ClassicUO.exe\",\"FileSize\":4096,"
        "\"LastWrite\":\"2025-10-28T20:41:02Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"}]}",
        cliloc_size);
    return astr_t(buf);
}

// the wire sample (the uogamers shape): seven entries -> four planned data
// files, one filtered launcher binary, one nested shard-tool file, one
// zero-size entry. In these literals \\\\ is one manifest escape pair, so
// "\\\\anim.idx" is the rooted wire path \anim.idx. The md5 prefixes the rh
// asserts below expect (first four base64 bytes, little-endian uint32):
//   yoqPkQQ6atGuo0UNJDjxTQ== -> ca8a8f91... -> 0x918f8aca
//   3q2+7wARIjNEVWZ3iJmquw== -> deadbeef... -> 0xefbeadde
//   ASNFZ4mrze/+3LqYdlQyEA== -> 01234567... -> 0x67452301
//   yv66vhI0VniavN7wESIzRA== -> cafebabe... -> 0xbebafeca
static const char *const UOG_MANIFEST =
    "{\n"
    "  \"FileList\": [\n"
    "    {\"FileName\":\"\\\\anim.idx\",\"FileSize\":1785720,"
    "\"LastWrite\":\"2025-10-28T20:41:01.8507375Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"},\n"
    "    {\"FileName\":\"\\\\AnimationFrame1.uop\",\"FileSize\":123456,"
    "\"LastWrite\":\"2025-10-28T20:41:02.5000000Z\",\"MD5Hash\":\"3q2+7wARIjNEVWZ3iJmquw==\"},\n"
    "    {\"FileName\":\"Sounds\\\\map 0.mul\",\"FileSize\":2048,"
    "\"LastWrite\":\"2024-01-02T03:04\",\"MD5Hash\":\"ASNFZ4mrze/+3LqYdlQyEA==\"},\n"
    "    {\"FileName\":\"\\\\Cliloc.ENU\",\"FileSize\":654,"
    "\"LastWrite\":\"not-a-stamp\",\"MD5Hash\":\"yv66vhI0VniavN7wESIzRA==\"},\n"
    "    {\"FileName\":\"\\\\ClassicUO.exe\",\"FileSize\":5000000,"
    "\"LastWrite\":\"2025-10-28T20:41:06Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"},\n"
    "    {\"FileName\":\"\\\\Sallos\\\\hotkeys.xml\",\"FileSize\":4096,"
    "\"LastWrite\":\"2025-10-28T20:41:07Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"},\n"
    "    {\"FileName\":\"\\\\empty.mul\",\"FileSize\":0,"
    "\"LastWrite\":\"2025-10-28T20:41:08Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"}\n"
    "  ]\n"
    "}\n";

// tolerance specimen: entry keys in reverse order, unknown keys of every
// value shape, a case-duplicate, a zero-size entry, an unsafe path, a
// non-UO file, a nameless entry and trailing top-level noise
static const char *const UOG_NOISE_MANIFEST =
    "{\n"
    "  \"Generated\": \"2025-10-28T20:40:00.0000000Z\",\n"
    "  \"FileList\": [\n"
    "    {\"FileSize\":3072,\"Checksum\":\"0123456789abcdef\","
    "\"LastWrite\":\"2025-01-02T03:04:05Z\",\"Meta\":{\"Depth\":[1,{\"Deeper\":null}]},"
    "\"Version\":7,\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\",\"FileName\":\"\\\\TILEDATA.MUL\"},\n"
    "    {\"FileName\":\"\\\\tiledata.mul\",\"FileSize\":999,"
    "\"LastWrite\":\"2025-01-02T03:04:06Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"},\n"
    "    {\"FileName\":\"\\\\anim.idx\",\"FileSize\":0,"
    "\"LastWrite\":\"2025-01-02T03:04:07Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"},\n"
    "    {\"FileName\":\"\\\\..\\\\evil.mul\",\"FileSize\":400,"
    "\"LastWrite\":\"2025-01-02T03:04:08Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"},\n"
    "    {\"FileName\":\"\\\\readme.txt\",\"FileSize\":12,"
    "\"LastWrite\":\"2025-01-02T03:04:09Z\",\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"},\n"
    "    {\"FileSize\":10,\"MD5Hash\":\"not base64!!\"}\n"
    "  ],\n"
    "  \"Generated\": \"2025-10-28T20:41:01.8507375Z\"\n"
    "}\n";

// malformed tail specimens: the array / the entry never closes
static const char *const UOG_UNCLOSED_ARRAY =
    "{\"FileList\":[{\"FileName\":\"\\\\cliloc.enu\",\"FileSize\":10}";
static const char *const UOG_UNCLOSED_ENTRY =
    "{\"FileList\":[{\"FileName\":\"\\\\cliloc.enu\",\"FileSize\":10";

// filler for the 64-byte scan window: with 46 chars the "FileList" mark
// starts at byte 54 and just fits (54 + 10 = 64); with 47 it starts at 55
#define UOG_PAD_FITS "0123456789012345678901234567890123456789012345"
#define UOG_PAD_PAST "01234567890123456789012345678901234567890123456"

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

// the shape check: json object + the FileList key inside the header window
VFS_TEST(uog_sniff)
{
    const auto minimal = test_uog_bytes("{\"FileList\":[]}");
    TEST_CHECK(vfs_uog_sniff_manifest(minimal.data(), minimal.size()));
    // leading whitespace before the object is fine
    const auto padded = test_uog_bytes("\n\t {\"FileList\":[]}");
    TEST_CHECK(vfs_uog_sniff_manifest(padded.data(), padded.size()));
    // the realistic sample with its escaped backslashes
    TEST_CHECK(vfs_uog_sniff_manifest(test_uog_bytes(UOG_MANIFEST).data(), strlen(UOG_MANIFEST)));
    // the mark ending exactly at byte 64 still fits the window
    const auto fits = test_uog_bytes("{\"A\":\"" UOG_PAD_FITS "\",\"FileList\":[]}");
    TEST_CHECK_MSG(
        vfs_uog_sniff_manifest(fits.data(), fits.size()),
        "the mark at byte 54 must fit the 64-byte window");
    // rejects: empty, a bare brace, other json, the mark pushed past the
    // window, the mythic xml shape, the utf-16 aops patchlist bytes
    TEST_CHECK(!vfs_uog_sniff_manifest(nullptr, 0));
    TEST_CHECK(!vfs_uog_sniff_manifest(test_uog_bytes("").data(), 0));
    TEST_CHECK(!vfs_uog_sniff_manifest(test_uog_bytes("{").data(), 1));
    const auto other = test_uog_bytes("{\"Other\":123}");
    TEST_CHECK(!vfs_uog_sniff_manifest(other.data(), other.size()));
    // size 10 < 12 even with the mark present
    TEST_CHECK(!vfs_uog_sniff_manifest(test_uog_bytes("{\"FileList").data(), 10));
    const auto past = test_uog_bytes("{\"A\":\"" UOG_PAD_PAST "\",\"FileList\":[]}");
    TEST_CHECK_MSG(
        !vfs_uog_sniff_manifest(past.data(), past.size()),
        "the mark at byte 55 must fall outside the 64-byte window");
    const auto mythic = test_uog_bytes("<MythicMFT><product serial=\"1\"/></MythicMFT>");
    TEST_CHECK(!vfs_uog_sniff_manifest(mythic.data(), mythic.size()));
    const uint8_t utf16[] = { 0xFF, 0xFE, 't', 0, 'e', 0, 's', 0, 't', 0, '1', 0,
                              '\\', 0,    'a', 0, '.', 0, 'm', 0, 'u', 0, 'l', 0 };
    TEST_CHECK(!vfs_uog_sniff_manifest(utf16, sizeof(utf16)));
}

// the product serial: a stable lookup3 fingerprint of the raw bytes
VFS_TEST(uog_serial)
{
    const std::vector<uint8_t> data = test_uog_bytes(UOG_MANIFEST);
    const uint64_t serial = vfs_uog_serial(data.data(), data.size());
    TEST_CHECK_MSG(vfs_uog_serial(data.data(), data.size()) == serial, "the serial must be stable");

    // any content change moves it: one digit of one FileSize ...
    astr_t edited(data.begin(), data.end());
    const size_t at = edited.find("1785720");
    TEST_CHECK_MSG(at != astr_t::npos, "the sample size must be present");
    edited[at] += 1;
    const auto digit = test_uog_bytes(edited.c_str());
    TEST_CHECK_MSG(
        vfs_uog_serial(digit.data(), digit.size()) != serial, "a digit change must move the serial");

    // ... a byte of whitespace ... and the size alone
    const auto spaced = test_uog_bytes((edited + " ").c_str());
    TEST_CHECK_MSG(
        vfs_uog_serial(spaced.data(), spaced.size()) != serial,
        "whitespace is content: it must move the serial");
    TEST_CHECK(vfs_uog_serial(data.data(), data.size() - 1) != serial);

    // independent of the sniff: garbage serializes fine (and stays stable)
    const uint8_t garbage[] = "not json at all \x01\x02\xff";
    const uint64_t garbage_serial = vfs_uog_serial(garbage, sizeof(garbage) - 1);
    TEST_CHECK(vfs_uog_serial(garbage, sizeof(garbage) - 1) == garbage_serial);
    TEST_CHECK(!vfs_uog_sniff_manifest(garbage, sizeof(garbage) - 1));
}

// the plan build: names, sizes, stamps, md5 prefixes, url tails, repos
VFS_TEST(uog_build_plan)
{
    const char *const url = "https://cdn.uogamers.com/client/Manifest.json";
    VfsPlan plan;
    TEST_CHECK_MSG(test_uog_build(UOG_MANIFEST, url, plan), "the sample manifest must build");

    // 4 planned: anim.idx, animationframe1.uop, map 0.mul, cliloc.enu (the
    // launcher binary and the shard tool filtered, the zero-size skipped)
    TEST_CHECK_MSG(plan.files.size() == 4, "got %zu files", plan.files.size());
    TEST_CHECK_STR(plan.manifest_repo, url);
    TEST_CHECK_STR(plan.file_repo, "https://cdn.uogamers.com/client/");

    for (const auto &file : plan.files)
    {
        // loose-file shape: one asset per file, stored, no uop hash
        TEST_CHECK_MSG(file.assets.size() == 1, "%s must carry one asset", file.name.c_str());
        TEST_CHECK(!file.is_uop);
        const auto &asset = file.assets[0];
        TEST_CHECK(asset.uop_hash == 0);
        TEST_CHECK(asset.ct == 0);
        TEST_CHECK(asset.ul == file.size);
        TEST_CHECK(asset.cl == file.size);
        TEST_CHECK_MSG(
            file.name == str_lower(file.name), "names install lowercase: %s", file.name.c_str());
        TEST_CHECK_STR(asset.name, file.name);
    }

    // entry 0: the rooted sample path, fraction + Z stripped from the stamp
    const auto *anim = find_file(plan, "anim.idx");
    TEST_CHECK_MSG(anim != nullptr, "anim.idx missing from the plan");
    TEST_CHECK(anim->size == 1785720);
    TEST_CHECK(anim->assets[0].timestamp == 20251028204101ULL);
    TEST_CHECK(anim->assets[0].rh == 0x918f8aca); // md5 ca8a8f91...
    TEST_CHECK_STR(anim->assets[0].blob, "anim.idx");

    // entry 1: the tail keeps the manifest's exact case, the name does not
    const auto *animframe = find_file(plan, "animationframe1.uop");
    TEST_CHECK_MSG(animframe != nullptr, "animationframe1.uop missing from the plan");
    TEST_CHECK(animframe->size == 123456);
    TEST_CHECK(animframe->assets[0].timestamp == 20251028204102ULL);
    TEST_CHECK(animframe->assets[0].rh == 0xefbeadde); // md5 deadbeef...
    TEST_CHECK_STR(animframe->assets[0].blob, "AnimationFrame1.uop");

    // entry 2: nested dir, original-case dir kept, the space escaped; the
    // seconds-less stamp pads to the 14-digit form (seconds 00)
    const auto *map0 = find_file(plan, "map 0.mul");
    TEST_CHECK_MSG(map0 != nullptr, "map 0.mul missing from the plan");
    TEST_CHECK(map0->size == 2048);
    TEST_CHECK(map0->assets[0].timestamp == 20240102030400ULL);
    TEST_CHECK(map0->assets[0].rh == 0x67452301); // md5 01234567...
    TEST_CHECK_STR(map0->assets[0].blob, "Sounds/map%200.mul");

    // entry 3: the exact-name filter is case-insensitive, the tail keeps
    // the manifest's case; a malformed stamp degrades to 0
    const auto *cliloc = find_file(plan, "cliloc.enu");
    TEST_CHECK_MSG(cliloc != nullptr, "cliloc.enu missing from the plan");
    TEST_CHECK(cliloc->size == 654);
    TEST_CHECK(cliloc->assets[0].timestamp == 0);
    TEST_CHECK(cliloc->assets[0].rh == 0xbebafeca); // md5 cafebabe...
    TEST_CHECK_STR(cliloc->assets[0].blob, "Cliloc.ENU");

    // the serial is the raw manifest bytes' fingerprint
    TEST_CHECK(plan.product_serial == vfs_uog_serial(test_uog_bytes(UOG_MANIFEST).data(), strlen(UOG_MANIFEST)));

    // a url without any slash cannot carry blobs: the file repo stays empty
    VfsPlan bare;
    TEST_CHECK_MSG(test_uog_build(UOG_MANIFEST, "Manifest.json", bare), "the bare url must build");
    TEST_CHECK(bare.files.size() == 4);
    TEST_CHECK_STR(bare.manifest_repo, "Manifest.json");
    TEST_CHECK(bare.file_repo.empty());

    // a FileList with zero acceptable entries builds nothing (clean false)
    VfsPlan nothing;
    TEST_CHECK(!test_uog_build(
        "{\"FileList\":[{\"FileName\":\"\\\\readme.txt\",\"FileSize\":12,"
        "\"LastWrite\":\"2025-10-28T20:41:01.8507375Z\","
        "\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"}]}",
        url,
        nothing));
    TEST_CHECK(nothing.files.empty());
}

// the scanner's tolerance: reversed key order, unknown keys, duplicates,
// zero sizes, unsafe paths, non-UO names - and clean failure on truncation
VFS_TEST(uog_build_plan_noise)
{
    VfsPlan plan;
    {
        uog_log_mute mute; // the skipped entries log warnings by design
        TEST_CHECK_MSG(
            test_uog_build(UOG_NOISE_MANIFEST, "https://cdn.test/m/Manifest.json", plan),
            "the noise must be tolerated");
    }

    // only the first tiledata entry installed (3072, not the 999 dupe);
    // the zero-size, the traversal, readme.txt and the nameless entry
    // contributed nothing
    TEST_CHECK_MSG(plan.files.size() == 1, "got %zu files", plan.files.size());
    const auto *tiledata = find_file(plan, "tiledata.mul");
    TEST_CHECK_MSG(tiledata != nullptr, "tiledata.mul missing from the plan");
    TEST_CHECK(tiledata->size == 3072);
    TEST_CHECK(tiledata->assets[0].timestamp == 20250102030405ULL);
    TEST_CHECK(tiledata->assets[0].rh == 0x918f8aca);

    // truncation is a clean false: no files, no crash
    VfsPlan broken;
    {
        uog_log_mute mute;
        TEST_CHECK_MSG(
            !test_uog_build(UOG_UNCLOSED_ARRAY, "https://cdn.test/m/Manifest.json", broken),
            "an unclosed array must fail the build");
        TEST_CHECK(broken.files.empty());
        TEST_CHECK_MSG(
            !test_uog_build(UOG_UNCLOSED_ENTRY, "https://cdn.test/m/Manifest.json", broken),
            "an unclosed entry must fail the build");
        TEST_CHECK(broken.files.empty());
    }
}

// the format dispatch inside vfs_manifest_build: the uog json, the mythic
// xml product and the aops patchlist each build their own plan
VFS_TEST(uog_dispatch_build)
{
    const auto root = test_dir("uog_dispatch");

    // 1. the uog product file (json on disk): dispatched by the sniff
    const auto uog_file = fs_path_join(root, "Manifest.json");
    TEST_CHECK(test_write_file(uog_file, test_uog_bytes(UOG_MANIFEST)));
    vfs_manifest_params params;
    params.product_file = uog_file;
    params.product_url = "https://cdn.uogamers.com/client/Manifest.json";
    params.blobs_dir = fs_path_join(root, "blobs");
    TEST_CHECK(fs_path_create(params.blobs_dir));

    VfsPlan plan;
    {
        uog_log_mute mute; // the dispatch logs each format's build noise
        TEST_CHECK_MSG(vfs_manifest_build(params, plan), "the uog product file must build");
    }
    TEST_CHECK_MSG(plan.files.size() == 4, "got %zu files", plan.files.size());
    TEST_CHECK(find_file(plan, "anim.idx") != nullptr);
    TEST_CHECK(!plan.files[0].is_uop); // the uog plan has no packs
    TEST_CHECK_STR(plan.manifest_repo, "https://cdn.uogamers.com/client/Manifest.json");
    TEST_CHECK_STR(plan.file_repo, "https://cdn.uogamers.com/client/");

    // 2. the same call with the mythic xml product: still its own walker
    // (the plan carries packs, which no json/patchlist plan ever does)
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "uog_dispatch_mft"));
    params = vfs_manifest_params();
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.blobs_dir = prod.blobs;
    VfsPlan mft_plan;
    {
        uog_log_mute mute;
        TEST_CHECK_MSG(vfs_manifest_build(params, mft_plan), "the mythic product must build");
    }
    bool has_pack = false;
    for (const auto &file : mft_plan.files)
    {
        if (file.is_uop)
            has_pack = true;
    }
    TEST_CHECK_MSG(has_pack, "the mythic plan must carry its pack");

    // 3. the aops patchlist (utf-16 le on disk): still its own source
    const auto patchlist = fs_path_join(root, "patchlist");
    test_write_patchlist(
        patchlist,
        { "test1\\Cliloc.enu|01012024|1|2|3|0|654|No|No|bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" });
    params = vfs_manifest_params();
    params.product_file = patchlist;
    params.product_url = "https://shard.test/fc/patchlist";
    params.blobs_dir = fs_path_join(root, "blobs");
    VfsPlan aops_plan;
    {
        uog_log_mute mute; // no cached patchcfg: defaults, with a warning
        TEST_CHECK_MSG(vfs_manifest_build(params, aops_plan), "the patchlist must build");
    }
    TEST_CHECK_MSG(aops_plan.files.size() == 1, "got %zu files", aops_plan.files.size());
    TEST_CHECK_MSG(
        aops_plan.files[0].name == "cliloc.enu",
        "got %s",
        aops_plan.files[0].name.c_str());
    TEST_CHECK(!aops_plan.files[0].is_uop);
}

// the boot ladder: a bare product root (no leaf, no trailing /) ladders
// onto /Manifest.json; the cached second boot requests nothing
VFS_TEST(uog_boot_fetch_ladder)
{
    test_vfs_shutdown guard;
    uog_log_mute mute; // the network paths are noise-heavy
    const auto root = test_dir("uog_boot");

    // the cdn layout: the manifest at the served root, the data file beside
    // it (the blob tail resolves against the manifest's directory)
    const std::vector<uint8_t> cliloc_payload(654, 0xCD);
    TEST_CHECK(test_write_file(fs_path_join(root, "cliloc.enu"), cliloc_payload));
    TEST_CHECK(test_write_file(
        fs_path_join(root, "Manifest.json"),
        test_uog_bytes(test_uog_server_manifest(654).c_str())));

    test_server srv;
    TEST_CHECK(test_server_start(srv, root, false));
    http_init(); // engine-level use: the hit counters run before any boot

    vfs_params params;
    params.enabled = true;
    params.install_dir = fs_path_join(root, "install");
    params.blobs_dir = fs_path_join(root, "blobs");
    params.product_url = test_server_url(srv, ""); // the bare root
    vfs_set_params(params);

    // the ladder probes in order: the bare root (a 404), then
    // /Manifest.json wins; /patchlist must never be requested
    const uint64_t root_hits = test_server_hits(srv, "/");
    const uint64_t manifest_hits = test_server_hits(srv, "/Manifest.json");
    const uint64_t patchlist_hits = test_server_hits(srv, "/patchlist");
    TEST_CHECK_MSG(vfs_boot(nullptr), "the bare-root boot must ladder onto Manifest.json");
    vfs_boot_wait(); // headless: also converge the background sweep
    TEST_CHECK_MSG(
        test_server_hits(srv, "/") == root_hits + 1,
        "the ladder must try the url as given first");
    TEST_CHECK_MSG(
        test_server_hits(srv, "/Manifest.json") == manifest_hits + 1,
        "exactly one /Manifest.json request must win the ladder");
    TEST_CHECK_MSG(
        test_server_hits(srv, "/patchlist") == patchlist_hits,
        "the ladder must stop at Manifest.json");

    // the plan is the uog plan, its repo derives from the won candidate,
    // and the on-demand data file converged to the exact bytes
    VfsPlan plan;
    TEST_CHECK(vfs_plan_ensure(plan));
    TEST_CHECK_MSG(
        plan.files.size() == 1 && plan.files[0].name == "cliloc.enu",
        "got %zu files",
        plan.files.size());
    TEST_CHECK_STR(plan.file_repo, test_server_url(srv, "/"));
    std::vector<uint8_t> bytes;
    TEST_CHECK_MSG(
        test_read_file(fs_path_join(params.install_dir, "cliloc.enu"), bytes),
        "cliloc.enu not materialized");
    TEST_CHECK_MSG(bytes == cliloc_payload, "the materialized bytes must match the blob");
    vfs_shutdown();

    // the cached path (--streaming-no-update): the persisted manifest is
    // the truth, not a single request
    params.update_check = false;
    vfs_set_params(params);
    const uint64_t total = test_server_total_hits(srv);
    TEST_CHECK_MSG(vfs_boot(nullptr), "the cached boot failed");
    vfs_boot_wait();
    vfs_shutdown();
    TEST_CHECK_MSG(
        test_server_total_hits(srv) == total,
        "the cached boot made %" PRIu64 " requests",
        test_server_total_hits(srv) - total);
    TEST_CHECK_MSG(
        test_server_hits(srv, "/Manifest.json") == manifest_hits + 1,
        "the cached boot must not re-request Manifest.json");

    // a bare root with a path: canonical rewrites it onto /patchlist before
    // the ladder (the dot-bearing 127.0.0.1 root above skips that rewrite),
    // so this exercises the de-leafed rung: .../client/patchlist (404) then
    // .../client/Manifest.json wins, and the dup patchlist never re-fires
    const auto client_dir = fs_path_join(root, "client");
    TEST_CHECK(fs_path_create(client_dir));
    const std::vector<uint8_t> map_payload(2048, 0xAB);
    TEST_CHECK(test_write_file(fs_path_join(client_dir, "map0.mul"), map_payload));
    TEST_CHECK(test_write_file(
        fs_path_join(client_dir, "Manifest.json"),
        test_uog_bytes(
            "{\"FileList\":[{\"FileName\":\"\\\\map0.mul\",\"FileSize\":2048,"
            "\"LastWrite\":\"2025-10-28T20:41:01.8507375Z\","
            "\"MD5Hash\":\"yoqPkQQ6atGuo0UNJDjxTQ==\"}]}")));

    vfs_params client_params;
    client_params.enabled = true;
    client_params.install_dir = fs_path_join(root, "install_client");
    client_params.blobs_dir = fs_path_join(root, "blobs_client");
    client_params.product_url = test_server_url(srv, "/client"); // the pathed bare root
    vfs_set_params(client_params);
    const uint64_t client_patchlist_hits = test_server_hits(srv, "/client/patchlist");
    const uint64_t client_manifest_hits = test_server_hits(srv, "/client/Manifest.json");
    TEST_CHECK_MSG(
        vfs_boot(nullptr), "the pathed bare-root boot must ladder onto client/Manifest.json");
    vfs_boot_wait();
    VfsPlan client_plan;
    TEST_CHECK(vfs_plan_ensure(client_plan));
    TEST_CHECK_MSG(
        client_plan.files.size() == 1 && client_plan.files[0].name == "map0.mul",
        "got %zu files",
        client_plan.files.size());
    TEST_CHECK_STR(client_plan.file_repo, test_server_url(srv, "/client/"));
    TEST_CHECK_MSG(
        test_server_hits(srv, "/client/patchlist") == client_patchlist_hits + 1,
        "the rewritten patchlist root must be tried exactly once");
    TEST_CHECK_MSG(
        test_server_hits(srv, "/client/Manifest.json") == client_manifest_hits + 1,
        "the de-leafed Manifest.json rung must win");
    vfs_shutdown();
    test_server_stop(srv);
}

// the serial dispatch through the public update flow: the remote serial is
// the uog fingerprint of the served bytes and a content bump shows
VFS_TEST(uog_update_serial)
{
    test_vfs_shutdown guard;
    uog_log_mute mute;
    const auto root = test_dir("uog_update");

    const std::vector<uint8_t> cliloc_payload(654, 0xCD);
    TEST_CHECK(test_write_file(fs_path_join(root, "cliloc.enu"), cliloc_payload));
    const astr_t manifest = test_uog_server_manifest(654);
    const auto manifest_path = fs_path_join(root, "Manifest.json");
    TEST_CHECK(test_write_file(manifest_path, test_uog_bytes(manifest.c_str())));

    test_server srv;
    TEST_CHECK(test_server_start(srv, root, false));

    vfs_params params;
    params.enabled = true;
    params.install_dir = fs_path_join(root, "install");
    params.blobs_dir = fs_path_join(root, "blobs");
    params.product_url = test_server_url(srv, "/Manifest.json");
    vfs_set_params(params);

    // install: the boot plans, the sweep converges, the plan persists
    TEST_CHECK_MSG(vfs_boot(nullptr), "the uog boot failed");
    vfs_boot_wait();
    vfs_shutdown();
    vfs_set_params(params);

    const auto v1 = test_uog_bytes(manifest.c_str());
    vfs_update_info info;
    TEST_CHECK_MSG(vfs_update_check(&info), "the update check failed");
    TEST_CHECK_MSG(info.plan_found, "the plan persisted");
    TEST_CHECK_MSG(!info.update_available, "the same manifest is up to date");
    TEST_CHECK_MSG(
        info.remote_serial == vfs_uog_serial(v1.data(), v1.size()),
        "the remote serial must be the uog fingerprint of the raw bytes");
    TEST_CHECK(info.local_serial == info.remote_serial);

    // upstream bump: one FileSize changes (a re-signed manifest, same blob)
    astr_t bumped = manifest;
    const size_t at = bumped.find("654");
    TEST_CHECK_MSG(at != astr_t::npos, "the cliloc size must be present");
    bumped.replace(at, 3, "700");
    TEST_CHECK(test_write_file(manifest_path, test_uog_bytes(bumped.c_str())));

    const auto v2 = test_uog_bytes(bumped.c_str());
    TEST_CHECK_MSG(vfs_update_check(&info), "the check after the bump failed");
    TEST_CHECK_MSG(info.update_available, "the bumped manifest must show");
    TEST_CHECK_MSG(
        info.remote_serial == vfs_uog_serial(v2.data(), v2.size()),
        "the remote serial must track the new bytes");
    TEST_CHECK_MSG(
        info.local_serial == vfs_uog_serial(v1.data(), v1.size()),
        "the local serial stays on the old bytes");
    test_server_stop(srv);
}
