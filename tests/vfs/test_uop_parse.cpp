// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// Golden tests for MYP/UOP archive parsing, exercised through the production
// path: CFileManager::UopLoadFile (xuocore/uodata.cpp:850) over a synthetic
// archive built in memory, plus the real captured artLegacyMUL.uop part.
//
// The synthetic layout is the one vfs_store must assemble per asset:
//   [MYP header][section(s)][UopFileEntry table]
//   per entry: [metadata block][payload blob]
// The client never validates metadata content or Checksum while walking the
// table; DecompressBlock (xuocore/uodata.cpp:227) only needs Flags,
// CompressedSize and DecompressedSize.

#define LOGGER_MODULE TestVfs

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <direct.h> // _rmdir
#else
#include <unistd.h> // rmdir
#endif

#include <vector>

#include <external/zlib_amalg.h>

#include "test_main.h"

#include <common/fs.h>
#include <common/logging/logging.h>
#include <xuocore/uodata.h>
#include <xuocore/uop.h>

#ifndef VFS_FIXTURE_DIR
#define VFS_FIXTURE_DIR "tests/vfs/fixtures"
#endif

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static const char *TEST_UOP_NAME = "vfs_test.uop";
static const char *TEST_TMP_DIR = "vfs_tests_tmp";

// kept from the captured artLegacyMUL.uop_0.mft entry:
// <f ph="3cf84" sh="fc599ef4" ul="1dc2" ct="1" cl="1917" ml="88" mc="9fd7f3c6"/>
static const uint32_t FIXTURE_PART_PH = 0x0003cf84;
static const uint32_t FIXTURE_PART_SH = 0xfc599ef4;
static const size_t FIXTURE_PART_COMPRESSED = 6423; // cl = 0x1917
static const size_t FIXTURE_PART_PAYLOAD = 7618;    // ul = 0x1dc2
static const uint32_t FIXTURE_PART_METADATA = 88;   // ml

static fs_path fixture_path(const char *name)
{
    return fs_path_join(fs_path_from(VFS_FIXTURE_DIR), name);
}

// stable temp root that never lands in the repo checkout: /tmp on POSIX,
// %TEMP% on Windows, with "." as a last-resort fallback
static fs_path temp_root()
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

static fs_path temp_dir()
{
    const auto dir = fs_path_join(temp_root(), TEST_TMP_DIR);
    fs_path_create(dir);
    return dir;
}

static void remove_temp_file(const fs_path &dir)
{
    fs_del(fs_path_join(dir, TEST_UOP_NAME));
}

// fs_del unlinks files only; the directory itself must go through rmdir
static void remove_temp_dir(const fs_path &dir)
{
#ifdef _MSC_VER
    _rmdir(fs_path_ascii(dir));
#else
    rmdir(fs_path_ascii(dir));
#endif
}

// removes the temp file AND directory on all exit paths: TEST_CHECK returns
// early from the test body, so the cleanup must run from this destructor
struct temp_dir_guard
{
    fs_path dir;
    temp_dir_guard()
        : dir(temp_dir())
    {
    }
    ~temp_dir_guard()
    {
        remove_temp_file(dir);
        remove_temp_dir(dir);
    }
};

static std::vector<uint8_t> read_fixture_file(const char *name)
{
    std::vector<uint8_t> data;
    auto *fp = fopen(fs_path_ascii(fixture_path(name)), "rb");
    if (!fp)
    {
        vfs_test_fail(__FILE__, __LINE__, "could not open fixture %s", name);
        return data;
    }
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(fp);
        vfs_test_fail(__FILE__, __LINE__, "fixture %s is empty", name);
        return data;
    }
    data.resize(size_t(size));
    const size_t amount = fread(data.data(), 1, data.size(), fp);
    fclose(fp);
    if (amount != data.size())
    {
        vfs_test_fail(__FILE__, __LINE__, "short read on fixture %s", name);
    }
    return data;
}

static std::vector<uint8_t> make_pattern(size_t size)
{
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; i++)
    {
        data[i] = uint8_t((i * 31 + (i >> 8)) & 0xff);
    }
    return data;
}

struct test_block
{
    uint64_t hash = 0;
    uint64_t offset = 0;
    uint32_t flags = 0;
    uint32_t metadata_size = 0;
    std::vector<uint8_t> payload; // decompressed content (ul)
    std::vector<uint8_t> stored;  // bytes after the metadata block (cl)
};

// Flags=1: the block ships zlib compressed (manifest ct=1), like the patcher
static void make_zlib_block(
    test_block &block,
    const char *name,
    const std::vector<uint8_t> &payload,
    uint32_t metadata_size)
{
    uLongf dst_len = compressBound(uLong(payload.size()));
    std::vector<uint8_t> stored(dst_len);
    auto *src = reinterpret_cast<Bytef const *>(payload.data());
    TEST_CHECK_MSG(
        compress2(stored.data(), &dst_len, src, uLong(payload.size()), Z_DEFAULT_COMPRESSION) ==
            Z_OK,
        "zlib compression failed");
    stored.resize(dst_len);
    block.hash = vfs_uop_hash(name);
    block.flags = 1;
    block.metadata_size = metadata_size;
    block.payload = payload;
    block.stored = stored;
}

// Flags=0: the block is stored uncompressed, the client memcpy's it
static void make_stored_block(
    test_block &block,
    const char *name,
    const std::vector<uint8_t> &payload,
    uint32_t metadata_size)
{
    block.hash = vfs_uop_hash(name);
    block.flags = 0;
    block.metadata_size = metadata_size;
    block.payload = payload;
    block.stored = payload;
}

// the real captured artLegacyMUL.uop part, placed in a UOP exactly as the
// patch system lays it out: [ml metadata bytes][downloaded zlib blob]
static void make_fixture_part_block(test_block &block)
{
    block.stored = read_fixture_file("part_0003cf84fc599ef4.zlib");
    TEST_CHECK_MSG(
        block.stored.size() == FIXTURE_PART_COMPRESSED,
        "fixture part size changed: %zu",
        block.stored.size());
    block.hash = UOP_HASH(FIXTURE_PART_SH, FIXTURE_PART_PH); // stored: (sh << 32) | ph
    block.flags = 1;
    block.metadata_size = FIXTURE_PART_METADATA;
    block.payload.resize(FIXTURE_PART_PAYLOAD);
    uLongf dst_len = block.payload.size();
    auto *src = reinterpret_cast<Bytef const *>(block.stored.data());
    TEST_CHECK_MSG(
        uncompress(block.payload.data(), &dst_len, src, uLong(block.stored.size())) == Z_OK,
        "fixture part failed to decompress");
    TEST_CHECK(dst_len == FIXTURE_PART_PAYLOAD);
}

// Builds the archive bytes: [header][sections][entry tables][data blocks...].
// Each element of `sections` becomes one UopSection, chained through
// NextSection (the last ends the chain with 0). A trailing "hole" entry
// (Offset == 0, all zero bytes) is appended to every section: the UopLoadFile
// walker must skip it.
static std::vector<uint8_t>
build_uop(uint32_t version, uint32_t magic, const std::vector<std::vector<test_block>> &sections)
{
    size_t entry_count = 0;
    size_t block_count = 0;
    for (const auto &blocks : sections)
    {
        entry_count += blocks.size() + 1; // + hole entry
        block_count += blocks.size();
    }

    // the sections (and their entry tables) sit back to back after the header,
    // all payloads follow the last entry table
    size_t cursor = sizeof(UopHeader);
    std::vector<size_t> section_offset(sections.size());
    std::vector<size_t> entries_offset(sections.size());
    for (size_t s = 0; s < sections.size(); s++)
    {
        section_offset[s] = cursor;
        cursor += sizeof(UopSection);
        entries_offset[s] = cursor;
        cursor += sizeof(UopFileEntry) * (sections[s].size() + 1);
    }
    const size_t data_offset = cursor;

    uint64_t total = data_offset;
    for (const auto &blocks : sections)
    {
        for (const auto &block : blocks)
        {
            total += block.metadata_size + block.stored.size();
        }
    }

    std::vector<uint8_t> out(size_t(total), 0);

    UopHeader header;
    header.Magic = magic;
    header.Version = version;
    header.Signature = 0x12345678;
    header.SectionOffset = sections.empty() ? 0 : uint64_t(section_offset[0]);
    header.FileCapacity = uint32_t(entry_count);
    header.FileCount = uint32_t(block_count);
    header.SectionCount = uint32_t(sections.size());
    header.Unk1 = 1;
    header.Unk2 = 0;
    memcpy(out.data(), &header, sizeof(header));

    uint64_t offset = data_offset;
    for (size_t s = 0; s < sections.size(); s++)
    {
        const auto &blocks = sections[s];

        UopSection section;
        section.FileCount = uint32_t(blocks.size()) + 1; // + hole entry
        section.NextSection = s + 1 < sections.size() ? uint64_t(section_offset[s + 1]) : 0;
        memcpy(out.data() + section_offset[s], &section, sizeof(section));

        for (size_t i = 0; i < blocks.size(); i++)
        {
            const auto &block = blocks[i];
            UopFileEntry entry;
            entry.Offset = offset;
            entry.MetadataSize = block.metadata_size;
            entry.CompressedSize = uint32_t(block.stored.size());
            entry.DecompressedSize = uint32_t(block.payload.size());
            entry.Hash = block.hash;
            entry.Checksum = 0; // not validated by the client load path
            entry.Flags = uint16_t(block.flags);
            memcpy(out.data() + entries_offset[s] + sizeof(entry) * i, &entry, sizeof(entry));

            if (block.metadata_size > 0)
            {
                // type 4 metadata (RSA signature) chain header, big-endian words:
                // key id 21 (0x0015) and a 128-byte signature (0x0080), as in the
                // real artLegacyMUL parts; the load path ignores the content
                std::vector<uint8_t> meta(block.metadata_size, 0);
                if (block.metadata_size >= sizeof(UopFileMetadata4))
                {
                    UopFileMetadata4 sig;
                    sig.SigType = 0x1500; // file bytes 00 15 = BE 0x0015
                    sig.SigSize = 0x8000; // file bytes 00 80 = BE 0x0080
                    memcpy(meta.data(), &sig, sizeof(sig));
                }
                memcpy(out.data() + offset, meta.data(), meta.size());
            }
            memcpy(
                out.data() + offset + block.metadata_size,
                block.stored.data(),
                block.stored.size());
            offset += block.metadata_size + block.stored.size();
        }
        // the hole entry (all zero, Offset == 0) stays at index blocks.size()
    }
    return out;
}

// single-section convenience form: one section ending the chain, as produced
// by the synthetic golden tests above
static std::vector<uint8_t>
build_uop(uint32_t version, uint32_t magic, const std::vector<test_block> &blocks)
{
    return build_uop(version, magic, std::vector<std::vector<test_block>>{ blocks });
}

static void write_temp_file(const char *name, const std::vector<uint8_t> &bytes)
{
    const auto path = fs_path_join(temp_dir(), name);
    auto *fp = fs_open(path, FS_WRITE);
    TEST_CHECK_MSG(fp != nullptr, "could not write %s", name);
    fwrite(bytes.data(), 1, bytes.size(), fp);
    fs_close(fp);
}

// mutes the production logging of the load path (mmap info, expected errors
// of negative tests) so the suite output stays deterministic
struct log_mute
{
    eLogSystem saved;
    log_mute()
        : saved(g_LogEnabled)
    {
        g_LogEnabled = LogSystemNone;
    }
    ~log_mute() { g_LogEnabled = saved; }
};

// loads archive bytes through the exact production boot path: a temp data dir
// (uo_data_init) plus UopLoadFile resolving the name via UOFilePath
static bool uop_load(const std::vector<uint8_t> &bytes, CUopMappedFile &file)
{
    write_temp_file(TEST_UOP_NAME, bytes);
    uo_data_init(fs_path_ascii(temp_dir()), 0, false);
    log_mute mute;
    return g_FileManager.UopLoadFile(file, TEST_UOP_NAME, false);
}

// ---------------------------------------------------------------------------
// golden parse tests
// ---------------------------------------------------------------------------

static void run_synthetic_uop_test(uint32_t version)
{
    temp_dir_guard guard;

    std::vector<test_block> blocks;
    test_block zlib_block;
    make_zlib_block(zlib_block, "build/vfstest/00000001.bin", make_pattern(4096), 88);
    blocks.push_back(zlib_block);
    test_block stored_block;
    make_stored_block(stored_block, "build/vfstest/00000002.bin", make_pattern(777), 12);
    blocks.push_back(stored_block);
    test_block part_block;
    make_fixture_part_block(part_block);
    blocks.push_back(part_block);

    const auto bytes = build_uop(version, MYP_MAGIC, blocks);

    CUopMappedFile file;
    TEST_CHECK_MSG(uop_load(bytes, file), "UopLoadFile failed for version %u", version);
    TEST_CHECK(file.Header != nullptr);
    TEST_CHECK(file.Header->Magic == MYP_MAGIC);
    TEST_CHECK(file.Header->Version == version);
    // the trailing hole entry (Offset == 0) must not be mapped
    TEST_CHECK(file.FileCount() == 3);

    // lookups by name and by stored hash (production path, uodata.cpp:216)
    auto *zlib_entry = file.GetAsset("build/vfstest/00000001.bin");
    TEST_CHECK(zlib_entry != nullptr);
    TEST_CHECK(zlib_entry == file.GetAsset(vfs_uop_hash("build/vfstest/00000001.bin")));
    TEST_CHECK(file.HasAsset(vfs_uop_hash("build/vfstest/00000001.bin")));
    TEST_CHECK(file.GetAsset("build/vfstest/not-there.bin") == nullptr);

    // Flags=1: zlib round-trip
    TEST_CHECK(zlib_entry->Flags == 1);
    TEST_CHECK(file.GetData(zlib_entry) == zlib_block.payload);

    // Flags=0: memcpy path (uodata.cpp:236)
    auto *stored_entry = file.GetAsset("build/vfstest/00000002.bin");
    TEST_CHECK(stored_entry != nullptr && stored_entry->Flags == 0);
    TEST_CHECK(file.GetData(stored_entry) == stored_block.payload);

    // raw and metadata accessors
    TEST_CHECK(file.GetRaw(zlib_entry) == zlib_block.stored);
    const auto meta = file.GetMeta(zlib_entry);
    TEST_CHECK(meta.size() == 88);
    TEST_CHECK(meta[0] == 0x00 && meta[1] == 0x15); // type 4 signature, key id 21 (BE)
    TEST_CHECK(meta[2] == 0x00 && meta[3] == 0x80); // signature size 128 (BE)

    // the real captured pack part, placed at [metadata][blob] and read back
    // through the production decompression path
    auto *part_entry = file.GetAsset(UOP_HASH(FIXTURE_PART_SH, FIXTURE_PART_PH));
    TEST_CHECK(part_entry != nullptr);
    TEST_CHECK(part_entry->MetadataSize == FIXTURE_PART_METADATA);
    TEST_CHECK(part_entry->CompressedSize == FIXTURE_PART_COMPRESSED);
    TEST_CHECK(part_entry->DecompressedSize == FIXTURE_PART_PAYLOAD);
    TEST_CHECK(file.GetData(part_entry) == part_block.payload);
    TEST_CHECK(file.GetData(part_entry).size() == FIXTURE_PART_PAYLOAD);
}

VFS_TEST(uop_parse_synthetic_v4)
{
    run_synthetic_uop_test(4);
}

VFS_TEST(uop_parse_synthetic_v5)
{
    run_synthetic_uop_test(5);
}

VFS_TEST(uop_rejects_wrong_magic)
{
    temp_dir_guard guard;

    std::vector<test_block> blocks;
    test_block block;
    make_zlib_block(block, "build/vfstest/00000001.bin", make_pattern(64), 88);
    blocks.push_back(block);

    CUopMappedFile file;
    TEST_CHECK(!uop_load(build_uop(4, 0xdeadbeef, blocks), file));
    TEST_CHECK(file.FileCount() == 0); // nothing may be mapped from a bad file
}

VFS_TEST(uop_rejects_version_3)
{
    temp_dir_guard guard;

    std::vector<test_block> blocks;
    test_block block;
    make_zlib_block(block, "build/vfstest/00000001.bin", make_pattern(64), 88);
    blocks.push_back(block);

    CUopMappedFile file;
    TEST_CHECK(!uop_load(build_uop(3, MYP_MAGIC, blocks), file));
    TEST_CHECK(file.FileCount() == 0);
}

VFS_TEST(uop_rejects_version_6)
{
    temp_dir_guard guard;

    std::vector<test_block> blocks;
    test_block block;
    make_zlib_block(block, "build/vfstest/00000001.bin", make_pattern(64), 88);
    blocks.push_back(block);

    CUopMappedFile file;
    TEST_CHECK(!uop_load(build_uop(6, MYP_MAGIC, blocks), file));
    TEST_CHECK(file.FileCount() == 0);
}

// A short download: fewer compressed bytes arrive than the entry promises, so
// the zlib stream is incomplete. DecompressBlock (xuocore/uodata.cpp:227)
// must fail and GetData must return empty instead of garbage, without
// crashing; the (broken) entry itself still maps
VFS_TEST(uop_truncated_zlib_returns_empty)
{
    temp_dir_guard guard;

    std::vector<test_block> blocks;
    test_block block;
    make_zlib_block(block, "build/vfstest/00000001.bin", make_pattern(4096), 88);
    const auto expected = block.payload;
    block.stored.resize(block.stored.size() / 2); // chop the zlib stream in half
    blocks.push_back(block);

    CUopMappedFile file;
    TEST_CHECK(uop_load(build_uop(4, MYP_MAGIC, blocks), file));
    TEST_CHECK(file.FileCount() == 1);

    auto *entry = file.GetAsset("build/vfstest/00000001.bin");
    TEST_CHECK(entry != nullptr);
    log_mute mute; // DecompressBlock logs the expected failure
    TEST_CHECK(file.GetData(entry).empty());
    TEST_CHECK(file.GetData(entry) != expected);
}

// A chain of two sections: the first points at the second through
// NextSection, the second ends the chain with 0; entries from both sections
// must resolve and FileCount must total both
VFS_TEST(uop_two_section_chain)
{
    temp_dir_guard guard;

    std::vector<test_block> first_section;
    test_block zlib_block;
    make_zlib_block(zlib_block, "build/vfstest/00000001.bin", make_pattern(4096), 88);
    first_section.push_back(zlib_block);

    std::vector<test_block> second_section;
    test_block stored_block;
    make_stored_block(stored_block, "build/vfstest/00000002.bin", make_pattern(777), 12);
    second_section.push_back(stored_block);
    test_block part_block;
    make_fixture_part_block(part_block);
    second_section.push_back(part_block);

    std::vector<std::vector<test_block>> sections;
    sections.push_back(first_section);
    sections.push_back(second_section);

    CUopMappedFile file;
    TEST_CHECK_MSG(uop_load(build_uop(4, MYP_MAGIC, sections), file), "UopLoadFile failed");
    TEST_CHECK(file.Header != nullptr);
    TEST_CHECK(file.Header->SectionCount == 2);
    // 3 real entries mapped (1 + 2), the per-section holes are not
    TEST_CHECK(file.FileCount() == 3);

    auto *first_entry = file.GetAsset("build/vfstest/00000001.bin");
    TEST_CHECK(first_entry != nullptr);
    TEST_CHECK(file.GetData(first_entry) == zlib_block.payload);

    auto *second_entry = file.GetAsset("build/vfstest/00000002.bin");
    TEST_CHECK(second_entry != nullptr);
    TEST_CHECK(file.GetData(second_entry) == stored_block.payload);

    auto *part_entry = file.GetAsset(UOP_HASH(FIXTURE_PART_SH, FIXTURE_PART_PH));
    TEST_CHECK(part_entry != nullptr);
    TEST_CHECK(file.GetData(part_entry) == part_block.payload);

    TEST_CHECK(file.GetAsset("build/vfstest/not-there.bin") == nullptr);
}

// ---------------------------------------------------------------------------
// fixture round-trip (no network, bytes committed with the fixtures)
// ---------------------------------------------------------------------------

VFS_TEST(uop_fixture_part_roundtrip)
{
    test_block part;
    make_fixture_part_block(part);
    TEST_CHECK(part.stored.size() == FIXTURE_PART_COMPRESSED);
    TEST_CHECK(part.payload.size() == FIXTURE_PART_PAYLOAD);
    // the captured blob is the zlib payload that sits at [metadata][blob]
    // inside artLegacyMUL.uop: the UOP hash words (sh << 32) | ph match the
    // manifest attributes ph="3cf84" sh="fc599ef4"
    TEST_CHECK(part.hash == 0xfc599ef40003cf84ull);
}

// ---------------------------------------------------------------------------
// string dictionary payload shape (the raw-vs-coded dispatch)
//
// The current string_dictionary.uop payload is the flat dictionary: a
// UopDictionary header followed by Count - 1 length-prefixed strings. The
// old dispatch keyed on the magic's byte 3 being 0x8E and ran the bwt
// decoder over raw payloads whose per-build magic happens to carry that
// byte - the decoder failed and the empty-payload fallback crashed the
// client (vector operator[] on a cleared buffer). The walker below is the
// dispatch now; these tests pin the regression shape.
// ---------------------------------------------------------------------------

VFS_TEST(uop_dictionary_walks_raw_with_8e_magic)
{
    // 7.0.85-era magic ends 0xe6, newer builds (the UOGamers repack) end
    // 0x8e: both are raw dictionaries, neither may take the bwt path
    std::vector<uint8_t> payload(sizeof(UopDictionary) + 2 + 3, 0);
    const uint64_t magic = 0x01d65c3f8e0e3880ull;
    memcpy(&payload[0], &magic, sizeof(magic));
    const uint32_t count = 2; // 1-based: slot 0 reserved, one string
    memcpy(&payload[8], &count, sizeof(count));
    const uint16_t len = 3;
    memcpy(&payload[sizeof(UopDictionary)], &len, sizeof(len));
    payload[sizeof(UopDictionary) + 2] = 'a';
    payload[sizeof(UopDictionary) + 3] = 'b';
    payload[sizeof(UopDictionary) + 4] = 'c';
    TEST_CHECK(uop_dictionary_walks(payload));
    TEST_CHECK(payload[3] == 0x8E); // the old tell fires on this raw payload
}

VFS_TEST(uop_dictionary_walks_rejects_malformed)
{
    // header too short
    TEST_CHECK(!uop_dictionary_walks(std::vector<uint8_t>(8, 0)));
    // count of zero
    std::vector<uint8_t> zero(sizeof(UopDictionary), 0);
    TEST_CHECK(!uop_dictionary_walks(zero));
    // string running past the payload end
    std::vector<uint8_t> over(sizeof(UopDictionary) + 2, 0);
    const uint32_t count = 2;
    memcpy(&over[8], &count, sizeof(count));
    over[sizeof(UopDictionary)] = 0xFF; // declares 255 bytes, none follow
    TEST_CHECK(!uop_dictionary_walks(over));
    // trailing junk after the last string (not an exact fit)
    std::vector<uint8_t> tail(sizeof(UopDictionary) + 2 + 3 + 1, 0);
    memcpy(&tail[8], &count, sizeof(count));
    const uint16_t short_len = 3;
    memcpy(&tail[sizeof(UopDictionary)], &short_len, sizeof(short_len));
    tail.back() = 'x'; // one byte past the walked end
    TEST_CHECK(!uop_dictionary_walks(tail));
    // a bwt-coded stream shape: coded bytes cannot walk as valid strings
    std::vector<uint8_t> coded(sizeof(UopDictionary) + 2048, 0);
    memcpy(&coded[0], &count, sizeof(count)); // garbage counts from noise
    coded[8] = 0xAB;
    TEST_CHECK(!uop_dictionary_walks(coded));
}

VFS_TEST(uop_dictionary_bwt_fallback_stays_bounded)
{
    // a walk-invalid stream still routes through bwt_decompress: garbage
    // headers (sum <= 0, or sums that outrun the buffer) must come back as
    // a clean false, never an out-of-bounds read
    std::vector<uint8_t> garbage(4096, 0xAB);
    std::vector<uint8_t> out;
    TEST_CHECK(!bwt_decompress(garbage, out));
    TEST_CHECK(out.empty());
}
