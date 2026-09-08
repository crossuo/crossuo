// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// Golden-vector tests for the VUOI download URL / name-hash scheme.
//
// All vectors were captured live from the EA/Broadsword UO patch servers on
// 2026-09-08 (see tests/vfs/fixtures/README.md). The tests never touch the
// network: the schemes below are encoded against the recorded fixtures and
// against URLs that were fetched successfully during authoring.
//
// Word order note (verified live against artLegacyMUL.uop_0.mft, 1627 name
// matches): for hashlittle2(name) -> (pc, pb) the patch manifest stores
// ph = pc and sh = pb, the remote file name is "%08x%08x" (ph, sh), and the
// UOP stored hash (UopFileEntry::Hash, as returned by uo_jenkins_hash) is
// (sh << 32) | ph. I.e. the manifest "ph" word is the LOW word of the UOP
// hash and "sh" is the HIGH word.

#define LOGGER_MODULE TestVfs

#include <stdint.h>
#include <stdio.h>

#include <external/tinyxml2.h>

#include "test_main.h"

#include <common/checksum.h>
#include <common/fs.h>
#include <common/logging/logging.h>
#include <xuocore/uop.h>

#ifndef VFS_FIXTURE_DIR
#define VFS_FIXTURE_DIR "tests/vfs/fixtures"
#endif

// captured live hosts/repositories (prod.xml is the frozen copy of this state)
static const char *PRODUCT_BOOTSTRAP_URL =
    "http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/manifest/uo-legacyrelease.prod";
static const char *MANIFEST_REPO =
    "http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/";
static const char *FILE_REPO =
    "http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/";

static fs_path fixture_path(const char *name)
{
    return fs_path_join(fs_path_from(VFS_FIXTURE_DIR), name);
}

// ---------------------------------------------------------------------------
// name hashing (shared helpers from test_main.h)
// ---------------------------------------------------------------------------

static astr_t remote_name_from_words(uint32_t ph, uint32_t sh)
{
    char name[32] = {};
    snprintf(name, sizeof(name), "%08x%08x", ph, sh);
    return astr_t(name);
}

static astr_t remote_name(const char *name)
{
    uint32_t ph = 0, sh = 0;
    vfs_name_hash(name, ph, sh);
    return remote_name_from_words(ph, sh);
}

// ---------------------------------------------------------------------------
// URL construction, the contract vfs_manifest (M1) must implement
// ---------------------------------------------------------------------------

// product file sits in the manifest repo root, referenced by literal name
static astr_t url_product(const astr_t &bootstrap_base, const char *product_name)
{
    return bootstrap_base + product_name;
}

// sub-manifests are fetched by LITERAL name (hash URLs 404 on the server)
static astr_t url_manifest(const astr_t &manifest_repo, const astr_t &rpath, const char *name)
{
    return manifest_repo + rpath + "/" + name;
}

// loose (non-pack) files live under "unpacked/", addressed by their name hash
static astr_t url_loose_file(const astr_t &file_repo, const astr_t &rpath, const char *name)
{
    return file_repo + rpath + "/unpacked/" + remote_name(name);
}

// pack parts are addressed by the ph/sh words from the manifest XML, they are
// UOP hashes taken verbatim and are NOT derived from any file name
static astr_t url_pack_part(
    const astr_t &file_repo, const astr_t &rpath, const char *pack, uint32_t ph, uint32_t sh)
{
    return file_repo + rpath + "/" + pack + "/" + remote_name_from_words(ph, sh);
}

// ---------------------------------------------------------------------------
// golden name-hash vectors
// ---------------------------------------------------------------------------

VFS_TEST(hash_pkg_mft)
{
    uint32_t ph = 0, sh = 0;
    vfs_name_hash("pkg.mft", ph, sh);
    TEST_CHECK(ph == 0xc995923e);
    TEST_CHECK(sh == 0x3e976a74);
    TEST_CHECK_STR(remote_name("pkg.mft"), "c995923e3e976a74");
}

VFS_TEST(hash_product_manifest)
{
    TEST_CHECK_STR(remote_name("uo-legacyrelease.prod"), "9b8e963d6d803fbc");
}

VFS_TEST(hash_client_exe)
{
    TEST_CHECK_STR(remote_name("client.exe"), "9c055ce001caca21");
}

VFS_TEST(hash_lowercases_the_name)
{
    // hashing is done on the lowercased name (mft.cpp:431), so any case
    // spelling of the same file must produce the same remote name
    TEST_CHECK_STR(remote_name("PKG.MFT"), "c995923e3e976a74");
    TEST_CHECK_STR(remote_name("Pkg.Mft"), "c995923e3e976a74");
    TEST_CHECK_STR(remote_name("Client.EXE"), "9c055ce001caca21");
    // sanity: different names still hash differently
    TEST_CHECK(remote_name("pkg.mft") != remote_name("unpacked.mft"));
}

VFS_TEST(hash_word_order_matches_uop_hash)
{
    // the stored UOP hash is (sh << 32) | ph of the hashlittle2 words, i.e.
    // uo_jenkins_hash(name) == UOP_HASH(sh, ph) -- the same value CrossUO's
    // CUopMappedFile::GetAsset looks up (xuocore/uodata.cpp:216)
    uint32_t ph = 0, sh = 0;
    vfs_name_hash("pkg.mft", ph, sh);
    const uint64_t uop_hash = uo_jenkins_hash("pkg.mft");
    TEST_CHECK(uop_hash == UOP_HASH(sh, ph));
    TEST_CHECK(uop_hash == 0x3e976a74c995923eull);
    TEST_CHECK(uop_hash == vfs_uop_hash("pkg.mft"));
    TEST_CHECK(UOP_HASH_PH(uop_hash) == sh);
    TEST_CHECK(UOP_HASH_SH(uop_hash) == ph);
}

// ---------------------------------------------------------------------------
// golden URL vectors (one case per resource kind)
// ---------------------------------------------------------------------------

VFS_TEST(url_product)
{
    // bootstrap: the product manifest, referenced from the patcher config
    const astr_t base = "http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/manifest/";
    TEST_CHECK_STR(url_product(base, "uo-legacyrelease.prod"), PRODUCT_BOOTSTRAP_URL);
}

VFS_TEST(url_manifest_uses_literal_name)
{
    // the exact URL the base pkg.mft was captured from
    TEST_CHECK_STR(
        url_manifest(MANIFEST_REPO, "base", "pkg.mft"),
        "http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/base/pkg.mft");
    // manifests must NOT be name-hashed into the URL
    TEST_CHECK(
        url_manifest(MANIFEST_REPO, "base", "pkg.mft").find(remote_name("pkg.mft")) ==
        astr_t::npos);
    TEST_CHECK(
        url_manifest(MANIFEST_REPO, "base", "unpacked.mft").find("unpacked.mft") != astr_t::npos);
}

VFS_TEST(url_loose_file_uses_name_hash)
{
    // client.exe is a loose file (launchfile from prod.xml): "unpacked/" prefix
    // plus the hash of its lowercased name
    TEST_CHECK_STR(
        url_loose_file(FILE_REPO, "base", "client.exe"),
        "http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/base/unpacked/9c055ce001caca21");
}

VFS_TEST(url_pack_part_uses_manifest_words)
{
    // part captured live (and kept as a fixture): ph/sh come from the
    // artLegacyMUL.uop_0.mft entry <f ph="3cf84" sh="fc599ef4" ...>, NOT from
    // hashing any name
    TEST_CHECK_STR(
        url_pack_part(FILE_REPO, "base", "artLegacyMUL.uop", 0x0003cf84, 0xfc599ef4),
        "http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/base/artLegacyMUL.uop/0003cf84fc599ef4");
    // proof they are not name hashes: no pack name hashes to those words
    TEST_CHECK(remote_name("artLegacyMUL.uop") != "0003cf84fc599ef4");
    TEST_CHECK(remote_name("0003cf84fc599ef4") != "0003cf84fc599ef4");
}

// ---------------------------------------------------------------------------
// fixture-driven golden checks
// ---------------------------------------------------------------------------

VFS_TEST(fixture_prod_xml)
{
    tinyxml2::XMLDocument doc;
    TEST_CHECK_MSG(
        doc.LoadFile(fs_path_ascii(fixture_path("prod.xml"))) == tinyxml2::XML_SUCCESS,
        "prod.xml failed to parse");
    auto *product = doc.FirstChildElement("MythicMFT");
    TEST_CHECK(product != nullptr);
    product = product->FirstChildElement("product");
    TEST_CHECK(product != nullptr);

    // golden protocol facts recorded from the live product manifest
    TEST_CHECK_STR(product->Attribute("serial"), "1787679382");
    TEST_CHECK_STR(product->Attribute("launchfile"), "client.exe");

    auto *manifest_repo = product->FirstChildElement("manifestrepos")->FirstChildElement("repo");
    auto *file_repo = product->FirstChildElement("filerepos")->FirstChildElement("repo");
    TEST_CHECK(manifest_repo != nullptr && file_repo != nullptr);
    TEST_CHECK_STR(manifest_repo->Attribute("url"), MANIFEST_REPO);
    TEST_CHECK_STR(file_repo->Attribute("url"), FILE_REPO);

    // the sub-manifest/package URLs recorded in prod.xml must rebuild from the
    // fixture repos; the product file itself lives on the bootstrap host
    // (patch.uo.eamythic.com), while the product lists its own repo hosts
    TEST_CHECK_STR(
        url_manifest(manifest_repo->Attribute("url"), "base", "pkg.mft"),
        "http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest/base/pkg.mft");
    TEST_CHECK_STR(
        url_loose_file(file_repo->Attribute("url"), "base", "client.exe"),
        "http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files/base/"
        "unpacked/9c055ce001caca21");
}

VFS_TEST(fixture_pkg_mft_sub_manifests)
{
    tinyxml2::XMLDocument doc;
    TEST_CHECK_MSG(
        doc.LoadFile(fs_path_ascii(fixture_path("pkg.mft"))) == tinyxml2::XML_SUCCESS,
        "pkg.mft failed to parse");
    auto *manifests = doc.FirstChildElement("MythicMFT");
    TEST_CHECK(manifests != nullptr);
    manifests = manifests->FirstChildElement("manifest");
    TEST_CHECK(manifests != nullptr);
    manifests = manifests->FirstChildElement("manifests");
    TEST_CHECK(manifests != nullptr);

    // base pkg.mft -> the per-package sub-manifest list, all zlib compressed
    int count = 0;
    bool has_unpacked = false, has_art = false;
    for (auto *m = manifests->FirstChildElement("manifest"); m != nullptr;
         m = m->NextSiblingElement("manifest"))
    {
        ++count;
        TEST_CHECK_STR(m->Attribute("ct"), "1"); // everything ships zlib (ct=1)
        const char *n = m->Attribute("n");
        TEST_CHECK_MSG(n != nullptr, "sub-manifest %d has no name", count);
        if (astr_t(n) == "unpacked.mft")
        {
            has_unpacked = true;
            TEST_CHECK_STR(m->Attribute("ul"), "2015b"); // 0x2015b = 131611 bytes
        }
        if (astr_t(n) == "artLegacyMUL.uop.mft")
        {
            has_art = true;
        }
        // every sub-manifest URL is built from the literal name
        TEST_CHECK(url_manifest(MANIFEST_REPO, "base", n).find(n) != astr_t::npos);
    }
    TEST_CHECK_MSG(count == 11, "expected 11 sub-manifests, got %d", count);
    TEST_CHECK(has_unpacked);
    TEST_CHECK(has_art);
}
