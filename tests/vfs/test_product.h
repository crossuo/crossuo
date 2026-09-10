// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// Shared generator for the streaming-install pipeline tests. Builds two products
// into a temp root:
//
//   test_build_product         - a fully synthetic mini product: one pack
//                                (zlib/stored/hole parts), one empty pack,
//                                and two loose files (one in a nested dir)
//   test_build_fixture_product - the captured live fixtures (prod.xml + pkg.mft +
//                                the captured artLegacyMUL part blob) with
//                                the remaining manifests synthesized empty
//
// Layout produced under <root>: prod.xml, blobs/ (store, mirroring the URL
// tree), install/ (materialization target). No network, no game state.

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <external/tinyxml2.h>
#include <external/zlib_amalg.h>

#include "test_main.h"

#include <common/fs.h>

#ifndef VFS_FIXTURE_DIR
#define VFS_FIXTURE_DIR "tests/vfs/fixtures"
#endif

// ---------------------------------------------------------------------------
// byte helpers
// ---------------------------------------------------------------------------

static inline std::vector<uint8_t> test_pattern(size_t size)
{
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; i++)
    {
        data[i] = uint8_t((i * 31 + (i >> 8)) & 0xff);
    }
    return data;
}

static inline std::vector<uint8_t> test_zlib(const std::vector<uint8_t> &payload)
{
    uLongf dst_len = compressBound(uLong(payload.size()));
    std::vector<uint8_t> out(dst_len);
    auto *src = reinterpret_cast<Bytef const *>(payload.data());
    if (compress2(out.data(), &dst_len, src, uLong(payload.size()), Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        vfs_test_fail(__FILE__, __LINE__, "zlib compression failed");
        return {};
    }
    out.resize(size_t(dst_len));
    return out;
}

// the metadata bytes vfs_store synthesizes: [type=3][size=ml-4][zeros]
static inline std::vector<uint8_t> test_synth_metadata(uint32_t ml)
{
    std::vector<uint8_t> meta(ml, 0);
    if (ml >= 4)
    {
        const uint16_t type = 3;
        const uint16_t size = uint16_t(ml - 4);
        memcpy(meta.data(), &type, 2);
        memcpy(meta.data() + 2, &size, 2);
    }
    return meta;
}

static inline astr_t test_hex64(uint64_t value)
{
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%llx", (unsigned long long)value);
    return astr_t(buf);
}

static inline astr_t test_remote_name(uint32_t ph, uint32_t sh)
{
    char buf[32] = {};
    snprintf(buf, sizeof(buf), "%08x%08x", ph, sh);
    return astr_t(buf);
}

static inline bool test_write_file(const fs_path &path, const std::vector<uint8_t> &bytes)
{
    fs_path_create(fs_directory(path));
    if (!fs_file_write(fs_path_ascii(path), bytes))
    {
        vfs_test_fail(__FILE__, __LINE__, "could not write %s", fs_path_ascii(path));
        return false;
    }
    return true;
}

// stable temp root that never lands in the repo checkout (see test_uop_parse);
// unique per process so reruns never see a previous run's leftovers
static inline fs_path test_temp_root()
{
#ifdef _MSC_VER
    const char *env = getenv("TMP");
    if (env == nullptr)
    {
        env = getenv("TEMP");
    }
    return fs_path_from(env != nullptr ? env : ".");
#else
    return fs_path_from("/tmp");
#endif
}

static inline fs_path test_dir(const char *name)
{
    char unique[64];
    snprintf(unique, sizeof(unique), "%s_%u", name, (unsigned)getpid());
    const auto dir = fs_path_join(test_temp_root(), "vfs_tests_tmp", unique);
    fs_path_create(dir);
    return dir;
}

// ---------------------------------------------------------------------------
// product description
// ---------------------------------------------------------------------------

struct test_part_def
{
    uint32_t ph = 0, sh = 0;
    uint32_t ct = 0; // 0 stored, 1 zlib
    uint32_t ml = 0;
    uint32_t mc = 0;
    std::vector<uint8_t> payload; // expected decoded content (ul bytes)
    std::vector<uint8_t> blob;    // bytes placed in the store
    astr_t blob_rel;              // store path relative to the blobs root
    // manifest timestamp of the part (update tests bump one part's t)
    uint64_t timestamp = 0x6a8dd30c;

    uint64_t uop_hash() const { return (static_cast<uint64_t>(sh) << 32) | ph; }
};

struct test_loose_def
{
    astr_t name;
    uint32_t ct = 0;
    uint32_t ph = 0, sh = 0;
    std::vector<uint8_t> payload;
    std::vector<uint8_t> blob;
    astr_t blob_rel;
};

struct test_product_data
{
    fs_path root;    // holds prod.xml
    fs_path blobs;   // blob store root
    fs_path install; // materialization target
    uint64_t serial = 424242;
    astr_t pack_name = "pack0.uop";
    astr_t pack_rel = "base";
    std::vector<test_part_def> parts;
    std::vector<test_loose_def> loose;
    // raw manifest entries appended verbatim to the synthesized manifests:
    // negative tests for the plan's path-traversal rejection (entries the
    // plan must skip, so they never need blobs)
    std::vector<astr_t> extra_packs; // extra "<p ...>...</p>" in the pack manifest
    std::vector<astr_t> extra_loose; // extra "<f .../>" in the package manifest
    // valid extra loose files, registered before the manifests are written
    // (name, ct, uncompressed size); empty in the default products
    struct extra_loose_def
    {
        astr_t name;
        uint32_t ct = 0;
        size_t size = 0;
    };
    std::vector<extra_loose_def> extra_valid_loose;
    // optional override of the first part's remote-name hash (ph, sh); zero
    // keeps the generated default. Used to make a part T0 (MainMisc core).
    uint32_t first_part_ph = 0;
    uint32_t first_part_sh = 0;
    // test_write_product internal: extra_valid_loose was folded into loose
    bool extras_appended = false;
};

// registers a pack part (and its blob) in the product description
static inline void test_add_part(
    test_product_data &prod,
    uint32_t ph,
    uint32_t sh,
    uint32_t ct,
    uint32_t ml,
    size_t payload_size)
{
    test_part_def part;
    part.ph = ph;
    part.sh = sh;
    part.ct = ct;
    part.ml = ml;
    part.payload = test_pattern(payload_size);
    part.blob = ct == 1 && payload_size > 0 ? test_zlib(part.payload) : part.payload;
    if (ml)
    {
        const auto meta = test_synth_metadata(ml);
        part.mc = adler32(1, meta.data(), int(meta.size()));
    }
    part.blob_rel = astr_t(prod.pack_rel) + "/" + prod.pack_name + "/" + test_remote_name(ph, sh);
    prod.parts.emplace_back(part);
}

static inline void
test_add_loose(test_product_data &prod, const char *name, uint32_t ct, size_t payload_size)
{
    test_loose_def loose;
    loose.name = name;
    loose.ct = ct;
    loose.payload = test_pattern(payload_size);
    loose.blob = ct == 1 && payload_size > 0 ? test_zlib(loose.payload) : loose.payload;
    vfs_name_hash(name, loose.ph, loose.sh);
    loose.blob_rel = astr_t(prod.pack_rel) + "/unpacked/" + test_remote_name(loose.ph, loose.sh);
    prod.loose.emplace_back(loose);
}

// manifest entry line for a pack part, hex attributes as the patcher ships
static inline astr_t test_part_xml(const test_part_def &part, uint64_t timestamp)
{
    char buf[512] = {};
    snprintf(
        buf,
        sizeof(buf),
        "<f ph=\"%x\" sh=\"%x\" ul=\"%s\" ct=\"%u\" cl=\"%s\" ml=\"%x\" mc=\"%x\" t=\"%s\"/>",
        part.ph,
        part.sh,
        test_hex64(part.payload.size()).c_str(),
        part.ct,
        test_hex64(part.blob.size()).c_str(),
        part.ml,
        part.mc,
        test_hex64(timestamp).c_str());
    return astr_t(buf);
}

static inline astr_t test_loose_xml(const test_loose_def &loose, uint64_t timestamp)
{
    char buf[512] = {};
    snprintf(
        buf,
        sizeof(buf),
        "<f n=\"%s\" ul=\"%s\" ct=\"%u\" cl=\"%s\" t=\"%s\"/>",
        loose.name.c_str(),
        test_hex64(loose.payload.size()).c_str(),
        loose.ct,
        test_hex64(loose.blob.size()).c_str(),
        test_hex64(timestamp).c_str());
    return astr_t(buf);
}

static inline std::vector<uint8_t> test_bytes(const astr_t &text)
{
    return std::vector<uint8_t>(text.begin(), text.end());
}

static inline astr_t test_join(const std::vector<astr_t> &parts)
{
    astr_t joined;
    for (const auto &part : parts)
    {
        joined += part;
    }
    return joined;
}

// removes outputs a previous test run may have left behind: the install dir
// files and the state DB (fs_del unlinks files only, which is all we need)
static inline void test_clean_outputs(const test_product_data &prod)
{
    const char *files[] = {
        "pack0.uop",
        "empty.uop",
        "artLegacyMUL.uop",
        "tiledata.mul",
        "unifont1.mul",
        "Music/Digital/Britainpos.mp3",
    };
    for (const char *name : files)
    {
        fs_del(fs_path_join(prod.install, name));
    }
    fs_del(fs_path_join(prod.root, "journal.log"));
    fs_del(fs_path_join(prod.root, "plan.bin"));
    fs_del(fs_path_join(prod.root, "state.bin"));
    fs_del(fs_path_join(prod.install, "cache", "state", "journal.log"));
    fs_del(fs_path_join(prod.install, "cache", "state", "state.bin"));
    fs_del(fs_path_join(prod.install, "cache", "state", "plan.bin"));
}

// ---------------------------------------------------------------------------
// the synthetic mini product
// ---------------------------------------------------------------------------

//   base/pack0.uop.mft : 4 parts (zlib+meta, stored, zlib+big meta, hole)
//   base/empty.uop.mft : a pack with zero parts (must produce no file)
//   base/pkg.mft       : references both manifests + the 2 loose files
//   prod.xml           : one package (base) referencing pkg.mft
//
// regenerates every manifest + blob under prod.root from the current prod
// description (test_write_product); test_build_product sets a fresh root up
// fills an empty product description with the default mini product (plus any
// registered extra loose files); a no-op when parts/loose are already set
static inline void test_fill_product(test_product_data &prod)
{
    // callers may prefill parts/loose for custom layouts (trap tests use
    // page-straddling block sizes); the default mini product otherwise
    if (prod.parts.empty() && prod.loose.empty())
    {
        test_add_part(
            prod,
            prod.first_part_ph ? prod.first_part_ph : 0x00000001,
            prod.first_part_sh ? prod.first_part_sh : 0x11111111,
            1,
            12,
            4096);
        test_add_part(prod, 0x00000002, 0x22222222, 0, 0, 777);
        test_add_part(prod, 0x00000003, 0x33333333, 1, 88, 1000);
        test_add_part(prod, 0x00000004, 0x44444444, 1, 0, 0); // the hole
        test_add_loose(prod, "tiledata.mul", 0, 2048);
        test_add_loose(prod, "Music/Digital/Britainpos.mp3", 1, 3333);
    }
    // valid extra loose files, registered before the manifests are written;
    // appended once (a test_write_product rebuild does not duplicate them)
    if (!prod.extras_appended)
    {
        for (const auto &extra : prod.extra_valid_loose)
        {
            test_add_loose(prod, extra.name.c_str(), extra.ct, extra.size);
        }
        prod.extras_appended = true;
    }
}

static inline bool test_write_product(test_product_data &prod)
{
    // pack manifest with all parts (including the hole entry)
    astr_t files;
    for (const auto &part : prod.parts)
    {
        files += test_part_xml(part, part.timestamp);
    }
    const astr_t pack_xml = astr_t("<MythicMFT><manifest><packs><p name=\"") + prod.pack_name +
                            "\" rpath=\"" + prod.pack_name + "\"><files>" + files + "</files></p>" +
                            test_join(prod.extra_packs) + "</packs></manifest></MythicMFT>";
    const auto pack_blob = test_zlib(test_bytes(pack_xml));
    if (!test_write_file(
            fs_path_join(prod.blobs, prod.pack_rel, prod.pack_name + ".mft"), pack_blob))
        return false;

    // the pack part blobs (the hole has none)
    for (const auto &part : prod.parts)
    {
        if (!part.blob.empty() &&
            !test_write_file(fs_path_join(prod.blobs, part.blob_rel), part.blob))
            return false;
    }

    // an empty pack: consumed by the plan, but it must produce no file
    const astr_t empty_xml = astr_t(
        "<MythicMFT><manifest><packs><p name=\"empty.uop\" "
        "rpath=\"empty.uop\"><files></files></p></packs></manifest>"
        "</MythicMFT>");
    const auto empty_blob = test_zlib(test_bytes(empty_xml));
    if (!test_write_file(fs_path_join(prod.blobs, prod.pack_rel, "empty.uop.mft"), empty_blob))
        return false;

    // base package manifest: sub-manifests + loose files
    astr_t manifests;
    const auto sub_manifest = [&manifests](const char *mft_name, size_t ul, size_t cl)
    {
        char buf[512] = {};
        snprintf(
            buf,
            sizeof(buf),
            "<manifest n=\"%s\" ul=\"%s\" ct=\"1\" cl=\"%s\" t=\"6a8dd2b1\"/>",
            mft_name,
            test_hex64(ul).c_str(),
            test_hex64(cl).c_str());
        manifests += astr_t(buf);
    };
    sub_manifest((prod.pack_name + ".mft").c_str(), pack_xml.size(), pack_blob.size());
    sub_manifest("empty.uop.mft", empty_xml.size(), empty_blob.size());

    astr_t loose_files;
    for (const auto &loose : prod.loose)
    {
        loose_files += test_loose_xml(loose, 0x6a8dd2b1);
        if (!test_write_file(fs_path_join(prod.blobs, loose.blob_rel), loose.blob))
            return false;
    }
    const astr_t pkg_xml = astr_t("<MythicMFT><manifest><manifests>") + manifests +
                           "</manifests><files>" + loose_files + test_join(prod.extra_loose) +
                           "</files></manifest></MythicMFT>";
    const auto pkg_blob = test_zlib(test_bytes(pkg_xml));
    if (!test_write_file(fs_path_join(prod.blobs, prod.pack_rel, "pkg.mft"), pkg_blob))
        return false;

    // product manifest
    char product[1024] = {};
    snprintf(
        product,
        sizeof(product),
        "<MythicMFT><product serial=\"%llu\" launchfile=\"client.exe\">"
        "<manifestrepos><repo url=\"http://manifest.test/uo/manifest/\"/></manifestrepos>"
        "<filerepos><repo url=\"http://files.test/uo/files/\"/></filerepos>"
        "<stages><stage name=\"main\" priority=\"10\" verifyunpacked=\"0\"><packages>"
        "<package name=\"base\" rpath=\"base\" priority=\"0\">"
        "<manifest n=\"pkg.mft\" ul=\"%s\" ct=\"1\" cl=\"%s\" t=\"6a8dd30f\"/>"
        "</package></packages></stage></stages></product></MythicMFT>",
        (unsigned long long)prod.serial,
        test_hex64(pkg_xml.size()).c_str(),
        test_hex64(pkg_blob.size()).c_str());
    return test_write_file(fs_path_join(prod.root, "prod.xml"), test_bytes(astr_t(product)));
}

// builds the synthetic mini product into a fresh temp root
static inline bool test_build_product(test_product_data &prod, const char *name)
{
    prod.root = test_dir(name);
    prod.blobs = fs_path_join(prod.root, "blobs");
    prod.install = fs_path_join(prod.root, "install");
    test_clean_outputs(prod);
    test_fill_product(prod);
    return test_write_product(prod);
}

// points the product's repos at a running fixture server (the generated
// prod.xml ships the offline "*.test" urls, which no test resolves)
static inline bool test_product_patch_repos(
    const test_product_data &prod, const astr_t &manifest_url, const astr_t &file_url)
{
    const auto path = fs_path_join(prod.root, "prod.xml");
    std::vector<uint8_t> data;
    if (!fs_file_read(fs_path_ascii(path), data))
        return false;
    astr_t xml(data.begin(), data.end());
    const auto replace = [&xml](const astr_t &from, const astr_t &to)
    {
        const size_t at = xml.find(from);
        if (at == astr_t::npos)
            return false;
        xml.replace(at, from.size(), to);
        return true;
    };
    if (!replace("http://manifest.test/uo/manifest/", manifest_url))
        return false;
    if (!replace("http://files.test/uo/files/", file_url))
        return false;
    return test_write_file(path, test_bytes(xml));
}

// deletes a blob from the server-side store (404 exercises)
static inline void test_product_remove_blob(const test_product_data &prod, const astr_t &rel)
{
    fs_del(fs_path_join(prod.blobs, rel));
}

// ---------------------------------------------------------------------------
// the captured live fixtures as a product (all other manifests synthesized empty)
// ---------------------------------------------------------------------------

static const char *FIXTURE_ART_ENTRY =
    "<f ph=\"3cf84\" sh=\"fc599ef4\" ul=\"1dc2\" ct=\"1\" cl=\"1917\" ml=\"88\" mc=\"9fd7f3c6\"/>";
static const size_t FIXTURE_ART_PAYLOAD = 7618; // ul = 0x1dc2

static inline bool test_read_fixture(const char *name, std::vector<uint8_t> &out)
{
    const auto path = fs_path_join(fs_path_from(VFS_FIXTURE_DIR), name);
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
    {
        vfs_test_fail(__FILE__, __LINE__, "could not open fixture %s", name);
        return false;
    }
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize(size_t(size));
    const size_t read = fread(out.data(), 1, out.size(), fp);
    fclose(fp);
    if (read != out.size())
    {
        vfs_test_fail(__FILE__, __LINE__, "short read on fixture %s", name);
        return false;
    }
    return true;
}

static inline bool test_build_fixture_product(test_product_data &prod, const char *name)
{
    prod.root = test_dir(name);
    prod.blobs = fs_path_join(prod.root, "blobs");
    prod.install = fs_path_join(prod.root, "install");
    test_clean_outputs(prod);
    prod.serial = 1787679382; // the captured product serial
    prod.pack_name = "artLegacyMUL.uop";
    prod.pack_rel = "base";

    std::vector<uint8_t> prod_xml, pkg_xml, part_blob;
    if (!test_read_fixture("prod.xml", prod_xml))
        return false;
    if (!test_read_fixture("pkg.mft", pkg_xml))
        return false;
    if (!test_read_fixture("part_0003cf84fc599ef4.zlib", part_blob))
        return false;

    // the captured artLegacyMUL.uop part blob, mirrored from the file repo
    if (!test_write_file(
            fs_path_join(
                prod.blobs, "base", prod.pack_name, test_remote_name(0x0003cf84, 0xfc599ef4)),
            part_blob))
        return false;

    // the one pack manifest we are interested in, holding the captured entry
    const astr_t art_xml = astr_t(
                               "<MythicMFT><manifest><packs><p name=\"artLegacyMUL.uop\" "
                               "rpath=\"artLegacyMUL.uop\"><files>") +
                           FIXTURE_ART_ENTRY + "</files></p></packs></manifest></MythicMFT>";
    if (!test_write_file(
            fs_path_join(prod.blobs, "base", "artLegacyMUL.uop.mft"),
            test_zlib(test_bytes(art_xml))))
        return false;

    // base/pkg.mft lists every sub-manifest; all but ours are synthesized
    // empty (their content is irrelevant to this product slice)
    tinyxml2::XMLDocument doc;
    doc.Parse(reinterpret_cast<char *>(pkg_xml.data()), pkg_xml.size());
    auto root = doc.FirstChildElement("MythicMFT");
    auto manifest = root ? root->FirstChildElement("manifest") : nullptr;
    auto manifests = manifest ? manifest->FirstChildElement("manifests") : nullptr;
    auto node = manifests ? manifests->FirstChildElement("manifest") : nullptr;
    if (!node)
    {
        vfs_test_fail(__FILE__, __LINE__, "fixture pkg.mft has no manifests");
        return false;
    }
    const astr_t empty_xml = "<MythicMFT><manifest/></MythicMFT>";
    const auto empty_blob = test_zlib(test_bytes(empty_xml));
    while (node)
    {
        const char *mft_name = node->Attribute("n");
        if (mft_name && strcmp(mft_name, "artLegacyMUL.uop.mft") != 0)
        {
            if (!test_write_file(fs_path_join(prod.blobs, "base", astr_t(mft_name)), empty_blob))
                return false;
        }
        node = node->NextSiblingElement("manifest");
    }

    // the base package manifest itself (the fixture is the decompressed XML)
    if (!test_write_file(fs_path_join(prod.blobs, "base", "pkg.mft"), test_zlib(pkg_xml)))
        return false;

    // the prod.xml notes package references its own pkg.mft
    if (!test_write_file(fs_path_join(prod.blobs, "notes", "pkg.mft"), empty_blob))
        return false;

    // the fixture product manifest, verbatim
    return test_write_file(fs_path_join(prod.root, "prod.xml"), prod_xml);
}
