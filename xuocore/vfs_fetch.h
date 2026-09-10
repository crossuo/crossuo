// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_fetch: the download engine.
//
// A pool of worker threads (default hardware_concurrency-1, cap 8) drains one
// priority queue of blob requests. Requests are deduplicated by resource
// tail, retried with backoff (mft_config::download_retries semantics,
// honoring 429 Retry-After), and bounded by a global buffered-bytes cap for
// backpressure. The transfer mechanics themselves live behind the
// vfs_transport seam: the engine owns queue, dedup and
// retry policy, the active transport owns one GET. The built-in curl
// transport keeps the native wire behavior byte-identical; a web build swaps
// it without touching this file.
//
// Blobs land in the blob store (which mirrors the remote URL tree 1:1), so
// fetching is exactly "get missing blobs into the store, then materialize" —
// the store's write path stays the only writer of the install files.

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
// (404s are definitive and fail on the first attempt), or when `timeout_ms`
// (0 = wait forever) elapses first - on timeout the request stays in flight
// and the caller treats the fetch as failed (the trap zero-fills the range)
bool vfs_fetch_blob_to_store(
    const char *tail,
    uint64_t expected_size,
    bool manifest,
    uint32_t timeout_ms = 0);

// batch driver used by boot: fetches every tail (at `priority`, ascending =
// higher precedence) reporting progress(0..1, tail) on the calling thread as
// requests settle; returns false when at least one fetch failed
bool vfs_fetch_batch(
    const std::vector<astr_t> &tails,
    const std::vector<uint64_t> &expected_sizes,
    int priority,
    bool manifest,
    void (*progress)(float done01, const char *tail));

// fire-and-forget enqueue (arena demand path): queues the
// blob download at `priority` and returns immediately - the consumer heals
// when the blob lands in the store. Deduplicated with equal in-flight
// requests; a no-op when the pool is not running or the blob is stored.
void vfs_fetch_queue_blob(const char *tail, uint64_t expected_size, bool manifest, int priority);

// mft fetch seam over HTTP for the manifest recursion: downloads the resource
// into the blob store and copies the bytes to the caller's buffer
bool vfs_fetch_manifest_fn(
    void *context,
    const char *url,
    const char *rel_path,
    const char *local_path,
    uint8_t *buffer,
    size_t *size);
