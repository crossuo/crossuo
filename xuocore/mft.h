// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <map>
#include <vector>
#include <external/tinyxml2.h>
#include <common/str.h>
#include <common/fs.h>

#define MFT_MAX_DOWNLOAD_SIZE (1024 * 1024 * 1024)

// Byte-acquisition seam: resolves one resource (product manifest, sub-manifest,
// loose file or pack part) to its raw, as-shipped bytes (still compressed when
// the entry is ct=1; the caller decompresses). The built-in implementation
// reads the local cache first and falls back to HTTP, back-filling the cache;
// alternative transports (the offline blob store, the network fetch engine) plug
// in through mft_config::fetch without touching the manifest recursion.
// - url: absolute resource URL (repo + rel_path), for implementations that
//   still go to the network
// - rel_path: resource path relative to the repo root
//   ("<rpath>/<pack_name|unpacked>/<name>"); local stores mirror it 1:1
// - local_path: default local cache path of the resource
// - buffer/size: destination; in: capacity, out: number of bytes placed
using mft_fetch_fn = bool (*)(
    void *context,
    const char *url,
    const char *rel_path,
    const char *local_path,
    uint8_t *buffer,
    size_t *size);

enum mft_result : uint32_t
{
    mft_ok = 0,
    mft_invalid_format,
    mft_decompress_error,
    mft_write_error,
    mft_could_not_open_path,
    mft_could_not_download_file,
};

struct mft_config
{
    const char *agent_name = "";
    uint32_t thread_count = 1;
    uint32_t download_buffer_size = MFT_MAX_DOWNLOAD_SIZE;
    uint32_t download_retries = 5;
    bool mirror_mode = false;
    bool listing_only = false;
    bool dump_extracted = true;
    bool latest_checked = false;
    bool generate_diff = false;
    fs_path output_path;
    fs_path product_path;
    mft_fetch_fn fetch = nullptr;  // null: built-in local cache + HTTP
    void *fetch_context = nullptr; // opaque handle passed back to fetch
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
    const char *pack_name = nullptr; // for packs in packages
    const char *repo = nullptr;
    const char *remote_path = nullptr;
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

using mft_entries_map = std::map<astr_t, mft_entry>;

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
    mft_entries_map base_version;

    uint64_t last_version = 0;
    mft_config config;
    uint8_t **download_buffers = nullptr;
    uint8_t **download_cbuffers = nullptr;
};

void mft_init(mft_product &prod, mft_config &cfg);
void mft_entry_remote_path(mft_entry &entry, char out[512]);
mft_result mft_load(
    mft_product &prod, const uint8_t *data, size_t data_size, const char *remote_path = nullptr);
mft_result mft_consume_manifests(mft_product &prod);
size_t mft_download_batch(mft_product &prod, const std::vector<mft_entry> &entries);
void mft_cleanup(mft_product &prod);
