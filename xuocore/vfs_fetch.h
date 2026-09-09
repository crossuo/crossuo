// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_fetch: the download engine (VUOI plan §6.4).
//
// A pool of worker threads (default hardware_concurrency-1, cap 8), each with
// its own curl easy handle (keep-alive), drains one priority queue of blob
// requests. Requests are deduplicated by resource tail, retried with backoff
// (mft_config::download_retries semantics, honoring 429 Retry-After), and
// bounded by a global buffered-bytes cap for backpressure.
//
// Blobs land in the M1 blob store (which mirrors the remote URL tree 1:1), so
// fetching is exactly "get missing blobs into the store, then materialize" —
// the M1 write path stays the only writer of the install files.

#pragma once

#include <stdint.h>

#include <common/fs.h>

// Same type as mft.h's mft_fetch_fn (identical alias redeclaration, legal
// C++). mft.h is not included here on purpose: it pulls tinyxml2.h, which
// errors out if common/logging.h was included first - and this header is
// consumed after it in most TUs (repo-wide include-order workaround).
using mft_fetch_fn = bool (*) (
    void *context,
    const char *url,
    const char *rel_path,
    const char *local_path,
    uint8_t *buffer,
    size_t *size);

struct vfs_fetch_params
{
    uint32_t workers = 0;        // 0: default (hardware_concurrency-1, cap 8)
    uint32_t retries = 5;        // attempts per file (transient failures only)
    uint64_t max_buffered = 64;  // global in-flight cap, megabytes
};

struct vfs_fetch_stats
{
    uint64_t requests = 0; // GETs performed
    uint64_t bytes = 0;    // body bytes received
    uint64_t retries = 0;  // retry attempts scheduled
    uint64_t failures = 0; // requests given up after the last attempt
    int32_t inflight = 0;  // requests currently queued or running
};

// start/stop the worker pool; http_init() must have run (curl globals).
// Returns false when the pool is already running.
bool vfs_fetch_start(const vfs_fetch_params &params);
bool vfs_fetch_started();
// signals the workers and joins them; queued work is abandoned
void vfs_fetch_stop();

// repo base URLs (trailing slash included), from the product manifest
void vfs_fetch_set_repos(const char *manifest_repo, const char *file_repo);
// the blob store root that mirrors the remote URL tree
void vfs_fetch_set_store(const fs_path &blobs_dir);

void vfs_fetch_stats_get(vfs_fetch_stats *out);

// blocking fetch of one resource tail ("<rpath>/<pack|unpacked>/<name>") into
// the blob store; deduplicated with any equal in-flight request. Pack parts
// and loose files use the file repo, manifests the manifest repo (see
// vfs_fetch_is_manifest). Returns false after the retries are exhausted
// (404s are definitive and fail on the first attempt).
bool vfs_fetch_blob_to_store(const char *tail, uint64_t expected_size, bool manifest);

// batch driver used by boot: fetches every tail (at `priority`, ascending =
// higher precedence) reporting progress(0..1, tail) on the calling thread as
// requests settle; returns false when at least one fetch failed
bool vfs_fetch_batch(
    const std::vector<astr_t> &tails,
    const std::vector<uint64_t> &expected_sizes,
    int priority,
    bool manifest,
    void (*progress)(float done01, const char *tail));

// mft fetch seam over HTTP for the manifest recursion: downloads the resource
// into the blob store and copies the bytes to the caller's buffer
bool vfs_fetch_manifest_fn(
    void *context,
    const char *url,
    const char *rel_path,
    const char *local_path,
    uint8_t *buffer,
    size_t *size);
