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

// request priorities (ascending = higher precedence). Demand (traps, T0,
// the blocking single-blob path) preempts everything; the background tier
// (T1, the update wave, fetch_all) preempts the sweep; the sweep - the
// continuous T2/T3 download - runs strictly below anything user-driven.
// The arena demand granule path rides one above the background tier; the
// web stream-file chunk lane shares that tier (re-queued chunks; a fresh
// read miss promotes its file's chunk to the demand tier)
#define VFS_FETCH_PRIO_DEMAND 0
#define VFS_FETCH_PRIO_BG 1
#define VFS_FETCH_PRIO_STREAM 2
#define VFS_FETCH_PRIO_SWEEP 3

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

// in-game demotion (the game calls it from its first-frame notification):
// parks one worker (the pool drops from N to N-1 active; N is already
// cores-1 by default, so the game keeps at least two cores clear of the
// download pool with the defaults) and caps the sweep lane at active-1 so
// a worker always stays free for demand. One-way until the next start.
void vfs_fetch_set_ingame();
// introspection (tests): the current active-worker target and how many
// sweep-lane jobs are running right now
// true when nothing is queued or in flight (or the engine is not running)
bool vfs_fetch_idle();

uint32_t vfs_fetch_active_workers();
int vfs_fetch_sweep_running();

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
    uint32_t timeout_ms = 0,
    int priority = VFS_FETCH_PRIO_DEMAND);

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

#if defined(XUO_EMSC)
// the stream-file chunk lane (web): vfs_stream_file_read/ensure demand a
// file's chunk loop through here instead of transferring on the render
// thread. The registry keeps the loop alive: every executed chunk step
// re-queues itself at VFS_FETCH_PRIO_STREAM until the file leaves its
// streaming phase. Deduplicated/promoted like any other request; a no-op
// when the pool is not running.
void vfs_fetch_queue_stream(const char *name, int priority);

// introspection: stream chunk jobs currently running (0 or 1)
int vfs_fetch_stream_running();

// pool seam for one chunk step, implemented by the vfs stream registry
// (vfs.cpp): runs ONE paced range-GET + inflate + write for the named
// stream file ON A POOL WORKER and returns the milliseconds until the file
// wants its next chunk, or a negative value when the file left its
// streaming phase (Ready/Failed/cooling: no job should follow).
int vfs_stream_pool_step(const char *name);
#endif

// mft fetch seam over HTTP for the manifest recursion: downloads the resource
// into the blob store and copies the bytes to the caller's buffer
bool vfs_fetch_manifest_fn(
    void *context,
    const char *url,
    const char *rel_path,
    const char *local_path,
    uint8_t *buffer,
    size_t *size);
