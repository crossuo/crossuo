// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_fetch: the download engine. See vfs_fetch.h.

#define LOGGER_MODULE Vfs

#include "vfs_fetch.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "vfs_transport.h"

#include "common.h"
#include <common/logging/logging.h>

#include <common/utils.h>

// default cap of bytes buffered across in-flight downloads (backpressure)
#define VFS_FETCH_BUFFERED_MB_DEFAULT 64
// polite backoff: 250ms doubling, capped
#define VFS_FETCH_BACKOFF_MS 250
#define VFS_FETCH_BACKOFF_MAX_MS 8000
// guess used for backpressure accounting when no size is known up front
#define VFS_FETCH_SIZE_GUESS (64 * 1024)
// the stream-file chunk lane (web): ONE paced chunk in flight across the
// whole registry (the old render-thread gate's semantics, moved into the
// pool); chunks always leave demand headroom (see the admission checks)
#define VFS_STREAM_LANE_CAP 1

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

enum vfs_fetch_job_state : uint8_t
{
    vfs_fetch_queued = 0,
    vfs_fetch_running,
    vfs_fetch_done,
    vfs_fetch_failed,
};

struct vfs_fetch_job
{
    astr_t tail;
    bool manifest = false;
    int priority = 0;
    uint64_t seq = 0;          // FIFO within a priority
    uint64_t expected = 0;     // exact size when known (0: unknown)
    uint8_t state = vfs_fetch_queued;
    bool stream = false;       // stream-file chunk step (web lane)
    int64_t runnable_at = 0;   // steady ms: pacing gate (stream chunks)
    // bundle membership: the slot this job rides (-1: a plain job) and
    // whether it waits for the riding worker to settle it. bundle_spent
    // marks the one-time bundle retry: a member whose bundle failed is
    // re-queued as a plain job exactly once, never gathered again
    bool bundle_wait = false;
    int bundle_slot = -1;
    bool bundle_spent = false;
    // the riding job of a bundle: owns the sweep-lane slot the gather took
    // (cleared by its own completion, which is also the only place the
    // decrement happens - a requeued ride settles its accounting once)
    bool bundled_ride = false;
    // set by settle on a failed bundle's RIDER: its worker is still
    // executing, so the completion must keep to the accounting and leave
    // the freshly requeued job alone. A dedicated flag because the shared
    // job's `state` legitimately reads queued while running (only the
    // s_queue copy carries the running mark in this engine)
    bool bundle_requeued = false;
};

struct vfs_fetch_worker
{
    std::thread thread;
};

static std::mutex s_mutex;
static std::condition_variable s_cv_jobs; // wakes workers (queue/backpressure/stop)

// request bundling (see vfs_fetch_bundle_params): empty url = disabled
static vfs_fetch_bundle_params s_bundle;
// one bundle in flight carries its members; keyed by slot index
struct vfs_fetch_bundle_slot
{
    astr_t key;                 // content address of the member list
    std::vector<astr_t> tails;  // members, in answer order
    uint8_t state = 0;          // 0 queued/running, 1 done, 2 failed
};
static std::vector<vfs_fetch_bundle_slot> s_bundles;
// copy of s_bundle.max_bytes for the worker (set under the same lock)
static uint64_t s_bundle_max_bytes = 4ull << 20;
// consecutive bundle refusals (transport cannot bundle / 404): after
// BUNDLE_REFUSAL_MAX the engine stops trying for the rest of the session -
// every gather wave would otherwise pay one dead request forever
static uint32_t s_bundle_refusals = 0;
static const uint32_t BUNDLE_REFUSAL_MAX = 3;
static std::condition_variable s_cv_done; // wakes waiters on job completion
static std::vector<vfs_fetch_job> s_queue;
static std::map<astr_t, std::shared_ptr<vfs_fetch_job>> s_active; // dedup by tail
static std::vector<vfs_fetch_worker> s_workers;
static vfs_fetch_params s_fetch_params;
static vfs_fetch_stats s_stats;
static fs_path s_store_dir;
static astr_t s_manifest_repo;
static astr_t s_file_repo;
static uint64_t s_seq = 0;
static uint64_t s_buffered = 0; // bytes admitted to workers (backpressure)
static bool s_running = false;
static bool s_stop = false;
// in-game demotion: workers with index >= s_active_target park between jobs
// (the pool shrinks without joining); the sweep lane caps concurrent
// sweep-priority jobs so one worker always stays free for demand
static uint32_t s_active_target = 0;
static int s_sweep_cap = 0;
static int s_sweep_running = 0;
static int s_stream_running = 0; // stream-file chunk jobs in flight (0..1)

static int64_t vfs_fetch_now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

#if defined(XUO_EMSC)
static astr_t vfs_fetch_stream_key(const char *name)
{
    // namespaced so a stream key can never collide with a blob tail
    return astr_t("stream:") + name;
}
#endif

// admission checks, shared by pop and the workers' wait predicate. A
// stream chunk runs only while the demand reservation holds: a running
// chunk shrinks the sweep lane (instead of the other way round), so
// sweep + stream jobs can never occupy the worker that DEMAND block
// fetches are entitled to
static bool vfs_fetch_runnable(const vfs_fetch_job &q)
{
    if (q.state != vfs_fetch_queued)
        return false;
    if (q.stream)
    {
        if (s_stream_running >= VFS_STREAM_LANE_CAP)
            return false;
        if (s_sweep_running + s_stream_running >= s_sweep_cap)
            return false;
        return q.runnable_at <= vfs_fetch_now_ms();
    }
    if (q.priority >= VFS_FETCH_PRIO_SWEEP)
        return s_sweep_running + s_stream_running < s_sweep_cap;
    return true;
}

static const char *vfs_fetch_repo_of(const vfs_fetch_job &job)
{
    const astr_t &repo = job.manifest ? s_manifest_repo : s_file_repo;
    if (repo.empty())
    {
        LOG_ERROR("no %s repo configured (product manifest provides them)", job.manifest ? "manifest" : "file");
        return nullptr;
    }
    return repo.c_str();
}

// ---------------------------------------------------------------------------
// transport seam: the engine owns the retry policy on top of ONE
// transport attempt (the transfer mechanics live in vfs_transport)
// ---------------------------------------------------------------------------

// fetch body sink: appends the transfer body (the engine keeps the
// whole-body buffering semantics; a transport may deliver chunk-wise)
static bool vfs_fetch_body_sink(void *context, const uint8_t *data, size_t size)
{
    auto *body = static_cast<std::vector<uint8_t> *>(context);
    body->insert(body->end(), data, data + size);
    return true;
}

// bundle sink: one call per member body (the transport knows the framing);
// the engine writes them in order once the whole answer arrived
static bool vfs_fetch_bundle_sink(void *context, const uint8_t *data, size_t size)
{
    auto *bodies = static_cast<std::vector<std::vector<uint8_t>> *>(context);
    bodies->emplace_back(data, data + size);
    (void)size;
    return true;
}

// GET + retries: transient failures back off, 404 is definitive,
// 429 honors Retry-After. Returns true with `body` filled on success.
// `cap` aborts transfers beyond it (0: size unknown up front); the exact-size
// validation stays with the caller.
static bool vfs_fetch_http(
    const char *url,
    const char *tail,
    uint64_t cap,
    bool manifest,
    std::vector<uint8_t> *body,
    const char *label)
{
    bool ok = false;
    long retry_after = -1;
    for (uint32_t attempt = 0; attempt < s_fetch_params.retries; ++attempt)
    {
        if (attempt != 0)
        {
            long ms = long(VFS_FETCH_BACKOFF_MS) << std::min(attempt - 1, 5u);
            if (retry_after > 0)
                ms = retry_after * 1000; // server asked politely
            ms = std::min<long>(ms, VFS_FETCH_BACKOFF_MAX_MS);
            LOG_DEBUG("retrying %s in %ldms (%u/%u)", label, ms, attempt + 1, s_fetch_params.retries);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        body->clear();
        const vfs_transport_result res = vfs_transport_active()->fetch(
            url, tail, cap, vfs_fetch_body_sink, body, 0, manifest);
        retry_after = res.retry_after;
        if (res.ok)
        {
            ok = true;
            break;
        }
        if (res.status == 404)
        {
            LOG_DEBUG("not found: %s", url);
            break; // never heals; the caller decides what a miss means
        }
        if (res.status != 0)
            LOG_WARN("GET %s: http status %ld", url, res.status);
        {
            std::unique_lock<std::mutex> lock(s_mutex);
            s_stats.retries++;
        }
    }
    return ok;
}

// writes a downloaded blob into the store (the store mirrors the URL tree)
static bool vfs_fetch_store_write(const char *tail, const std::vector<uint8_t> &body)
{
    const auto path = fs_path_join(s_store_dir, tail);
    fs_path_create(fs_directory(path)); // the tail's directories may be new
    if (!fs_file_write(fs_path_ascii(path), body))
    {
        LOG_ERROR("could not store blob: %s", tail);
        return false;
    }
    LOG_TRACE("stored blob: %s (%zu bytes)", tail, body.size());
    return true;
}

// a size-mismatched store file can only be a truncated download: drop it so
// the next attempt starts clean (the store is content-addressed by size)
static void vfs_fetch_store_purge(const char *tail)
{
    const auto path = fs_path_join(s_store_dir, tail);
    if (fs_path_exists(path))
    {
        LOG_WARN("purging partial blob: %s", tail);
        fs_del(path);
    }
}

// ---------------------------------------------------------------------------
// request bundling: batch the queued small blobs into one GET
// ---------------------------------------------------------------------------

// FNV-1a: the member list -> the bundle's content address (stable across
// sessions; only ever compared to the same computation)
static astr_t vfs_bundle_key(const std::vector<astr_t> &tails)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (const auto &t : tails)
    {
        for (unsigned char c : t)
        {
            h ^= c;
            h *= 0x100000001b3ull;
        }
        h ^= 0x1f; // separator
        h *= 0x100000001b3ull;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
    return astr_t(buf);
}

// the url the transport fetches for a member list: <url>/<count>-<key>?m=...
// Each member value is member_ref + tail - a reference the SERVER can
// resolve (e.g. "/patch/files/<tail>"), percent-encoded as one query value
static astr_t vfs_bundle_url_locked(const std::vector<astr_t> &tails, const astr_t &key)
{
    astr_t url = s_bundle.url + "/" + std::to_string(tails.size()) + "-" + key + "?m=";
    for (size_t i = 0; i < tails.size(); ++i)
    {
        if (i != 0)
            url += "&m=";
        const astr_t ref =
            (!s_bundle.member_ref.empty() && s_bundle.member_ref.back() == '/')
                ? s_bundle.member_ref + tails[i]
                : s_bundle.member_ref + "/" + tails[i];
        // percent-encode the path separators: each member is its own query
        // value, decoded once by the server (blob_query_url -> percent_decode)
        for (unsigned char c : ref)
        {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                url += char(c);
            else
            {
                char enc[4];
                snprintf(enc, sizeof(enc), "%%%02X", c);
                url += enc;
            }
        }
    }
    return url;
}

// called with s_mutex held: fold the queued whole-blob jobs into one bundle
// when the configured limits allow it. The first member's job is the one the
// worker rides (its state flips to running); the rest wait on the slot and
// are marked done by the completing worker AFTER the members are on disk.
static bool vfs_bundle_gather_locked(std::shared_ptr<vfs_fetch_job> *ride)
{
    if (s_bundle.url.empty() || s_bundle_refusals >= BUNDLE_REFUSAL_MAX)
        return false;
    std::vector<astr_t> tails;
    std::vector<std::shared_ptr<vfs_fetch_job>> jobs;
    uint64_t bytes = 0;
    for (auto &job : s_queue)
    {
        if (job.state != vfs_fetch_queued || job.stream || job.manifest)
            continue;
        // a member whose bundle already failed rides the plain path only
        if (job.bundle_spent)
            continue;
        // unknown sizes cannot be budgeted against max_bytes
        if (job.expected == 0)
            continue;
        if (!s_bundle.member_prefix.empty() &&
            job.tail.compare(0, s_bundle.member_prefix.size(), s_bundle.member_prefix) != 0)
            continue;
        if (tails.size() >= s_bundle.max_members)
            break;
        if (!tails.empty() && bytes + job.expected > s_bundle.max_bytes)
            break;
        bytes += job.expected;
        tails.push_back(job.tail);
        jobs.push_back(s_active[job.tail]);
    }
    if (tails.empty())
        return false;
    // one member is a plain GET (and one job per worker already covers it)
    if (tails.size() < 2)
        return false;

    vfs_fetch_bundle_slot slot;
    slot.key = vfs_bundle_key(tails);
    slot.tails = tails;
    s_bundles.push_back(slot);
    const int index = int(s_bundles.size()) - 1;
    for (size_t i = 0; i < jobs.size(); ++i)
    {
        auto &job = *jobs[i];
        job.state = vfs_fetch_running;
        job.bundle_slot = index;
        job.bundle_wait = i != 0;
        job.bundled_ride = i == 0;
    }
    for (auto &q : s_queue)
    {
        // the copies carry the running mark exactly like pop does: the
        // worker wait predicate must not see them runnable again, and the
        // settle erase/requeue predicates match on running copies
        if (q.state != vfs_fetch_queued)
            continue;
        for (const auto &tail : tails)
        {
            if (q.tail == tail)
            {
                q.state = vfs_fetch_running;
                break;
            }
        }
    }
    *ride = jobs[0];
    LOG_DEBUG(
        "fetch: bundling %zu members (%llu bytes) as %s",
        tails.size(),
        (unsigned long long)bytes,
        vfs_bundle_url_locked(tails, slot.key).c_str());
    return true;
}

// called with s_mutex held: settle every member of a finished bundle. On
// success the members are on disk (the riding worker wrote them) and each
// waiter is done. On failure each member is re-queued ONCE as a plain job
// (bundle_spent keeps it out of any later bundle) - the demand retry ladder
// lives in vfs_fetch_http, which the bundle path bypasses, so this requeue
// IS the retry. A member that fails its plain requeue settles failed like
// any plain job. s_queue entries are erased here as well: only the riding
// job's own completion would otherwise remove them.
static void vfs_bundle_settle_locked(int index, bool ok)
{
    if (index < 0 || index >= int(s_bundles.size()))
        return;
    auto &slot = s_bundles[index];
    slot.state = ok ? 1 : 2;
    if (ok)
        s_stats.bundled++;
    for (const auto &tail : slot.tails)
    {
        auto it = s_active.find(tail);
        if (it == s_active.end())
            continue;
        auto &job = *it->second;
        if (!ok)
        {
            // EVERY member rides again as a plain job, the rider included:
            // its worker is still executing, so the rider is flagged for its
            // completion (which must keep to the accounting and leave the
            // freshly requeued job alone)
            const bool was_rider = !job.bundle_wait;
            job.bundle_slot = -1;
            job.bundle_wait = false;
            job.bundle_spent = true;
            job.bundle_requeued = was_rider;
            job.state = vfs_fetch_queued;
            for (auto &q : s_queue)
            {
                if (q.tail == tail && q.state == vfs_fetch_running)
                {
                    q.state = vfs_fetch_queued;
                    q.bundle_slot = -1;
                    q.bundle_wait = false;
                    q.bundle_spent = true;
                    break;
                }
            }
            continue;
        }
        if (!job.bundle_wait)
            continue; // the riding job's own completion settles it
        {
            // counted as in flight at enqueue: settle exactly once here.
            // s_buffered is NOT decremented: the engine only adds a popped
            // job's expected (the rider's) to s_buffered - the waiters were
            // never added, so subtracting them here underflows the backpressure
            // counter and parks the whole pool once it wraps
            s_stats.inflight--;
            job.state = vfs_fetch_done;
            job.bundle_slot = -1;
            job.bundle_wait = false;
            s_queue.erase(std::remove_if(
                              s_queue.begin(),
                              s_queue.end(),
                              [&tail](const vfs_fetch_job &q) {
                                  return q.tail == tail && q.state == vfs_fetch_running;
                              }),
                          s_queue.end());
            s_active.erase(it);
        }
    }
    if (!ok)
        s_cv_jobs.notify_all(); // the requeued members are runnable now
    // the slot index is never reused: a late worker may still hold it
}

// one bundle transfer on a worker: build the url from the slot, let the
// transport deliver each member, write them to the store, settle the jobs.
// `bytes_out` receives the delivered member bytes (stats). A refusal
// (transport cannot bundle / 404) counts toward the adaptive disable: after
// BUNDLE_REFUSAL_MAX consecutive ones the engine stops gathering for the
// session - a bundle-less server must not cost one dead request per wave.
static bool vfs_fetch_bundle_run(const vfs_fetch_job &job, uint64_t *bytes_out)
{
    astr_t url;
    std::vector<std::vector<uint8_t>> bodies;
    std::vector<astr_t> tails;
    int slot_index = -1;
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        if (job.bundle_slot < 0 || job.bundle_slot >= int(s_bundles.size()))
            return false;
        auto &slot = s_bundles[job.bundle_slot];
        tails = slot.tails;
        slot_index = job.bundle_slot;
        url = vfs_bundle_url_locked(slot.tails, slot.key);
    }
    const vfs_transport_result res = vfs_transport_active()->fetch_bundled(
        url.c_str(),
        job.tail.c_str(),
        s_bundle_max_bytes,
        vfs_fetch_bundle_sink,
        &bodies,
        0,
        false);
    bool ok = res.ok && bodies.size() == tails.size();
    if (ok)
    {
        for (size_t i = 0; i < tails.size() && ok; ++i)
            ok = vfs_fetch_store_write(tails[i].c_str(), bodies[i]);
    }
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        if (res.status != 0)
            s_stats.bundles++; // one served bundle request (either outcome)
        if (ok)
        {
            s_bundle_refusals = 0;
        }
        else if (res.status == 0 || res.status == 404)
        {
            if (++s_bundle_refusals >= BUNDLE_REFUSAL_MAX && !s_bundle.url.empty())
            {
                s_bundle.url.clear();
                LOG_INFO("fetch: bundling refused %u times, disabled for the session", s_bundle_refusals);
            }
        }
        vfs_bundle_settle_locked(slot_index, ok);
    }
    if (ok && bytes_out)
        *bytes_out = res.bytes;
    if (!ok)
        LOG_WARN("bundle failed (%zu members, status %ld): %s", tails.size(), res.status, url.c_str());
    return ok;
}

// ---------------------------------------------------------------------------
// worker pool
// ---------------------------------------------------------------------------

static bool vfs_fetch_pop(std::shared_ptr<vfs_fetch_job> *out)
{
    // assumes s_mutex held; picks the highest-precedence runnable job. A
    // sweep job only runs inside the sweep lane (s_sweep_cap): when the
    // lane is full the sweep is skipped for demand and background work,
    // which is exactly the render-critical precedence the lane exists for
    // a bundle of queued small blobs is one job for the lane accounting
    if (vfs_bundle_gather_locked(out))
    {
        ++s_sweep_running; // rides a lane slot (never demand-preempting)
        return true;
    }
    auto best = s_queue.end();
    for (auto it = s_queue.begin(); it != s_queue.end(); ++it)
    {
        if (!vfs_fetch_runnable(*it))
            continue;
        if (
            best == s_queue.end() ||
            std::make_pair(it->priority, it->seq) < std::make_pair(best->priority, best->seq))
        {
            best = it;
        }
    }
    if (best == s_queue.end())
        return false;
    if (best->stream)
        ++s_stream_running;
    else if (best->priority >= VFS_FETCH_PRIO_SWEEP)
        ++s_sweep_running;
    *out = s_active[best->tail];
    best->state = vfs_fetch_running;
    return true;
}

// assumes s_mutex held; see the definition in the requests section (the
// worker's completion re-queues stream chunk loops through it)
static std::shared_ptr<vfs_fetch_job> vfs_fetch_enqueue(
    const astr_t &tail, uint64_t expected, bool manifest, int priority,
    bool stream = false, int64_t runnable_at = 0);

static void vfs_fetch_worker_run(size_t index)
{
    vfs_fetch_worker &worker = s_workers[index];
    (void)worker;
    for (;;)
    {
        std::shared_ptr<vfs_fetch_job> job;
        {
            std::unique_lock<std::mutex> lock(s_mutex);
            // parked workers (index >= s_active_target, the in-game
            // demotion) sleep here until stop; the rest wait for a runnable
            // job: queued, inside the backpressure budget, and - for the
            // sweep and stream lanes - with a lane slot free. A queued
            // paced stream chunk that is not yet runnable wakes the wait
            // at its gate instead of polling
            const auto pred = [&] {
                if (s_stop)
                    return true;
                if (index >= s_active_target || s_buffered >= uint64_t(s_fetch_params.max_buffered) * 1024 * 1024)
                    return false;
                return std::any_of(s_queue.begin(), s_queue.end(), vfs_fetch_runnable);
            };
            const int64_t now = vfs_fetch_now_ms();
            std::chrono::steady_clock::time_point gate = std::chrono::steady_clock::time_point::max();
            for (const auto &q : s_queue)
            {
                if (q.state == vfs_fetch_queued && q.stream && q.runnable_at > now)
                {
                    const auto at = std::chrono::steady_clock::time_point(std::chrono::milliseconds(q.runnable_at));
                    gate = std::min(gate, at);
                }
            }
            if (gate == std::chrono::steady_clock::time_point::max())
                s_cv_jobs.wait(lock, pred);
            else
                s_cv_jobs.wait_until(lock, gate, pred);
            if (s_stop)
            {
#if defined(XUO_EMSC)
                // park instead of exiting: the browser main thread cannot
                // block on pthread_join, so the pool is reused by the next
                // vfs_fetch_start instead of being torn down
                continue;
#else
                break;
#endif
            }
            if (!vfs_fetch_pop(&job))
                continue; // lost the race for the queued job
            s_buffered += job->expected ? job->expected : VFS_FETCH_SIZE_GUESS;
        }

        int stream_again_ms = -1;
        std::vector<uint8_t> body;
        uint64_t bundle_bytes = 0;
        bool ok = false;
        if (job->stream)
        {
#if defined(XUO_EMSC)
            // one paced chunk on the worker: the render thread never issues
            // stream transfers. A negative answer ends the file's loop; a
            // non-negative one re-queues the chunk at that pace below
            stream_again_ms = vfs_stream_pool_step(job->tail.c_str() + 7); // strlen("stream:")
            ok = true;
#endif
        }
        else
        {
            const char *repo = vfs_fetch_repo_of(*job);
            astr_t url;
            if (repo)
            {
                url = astr_t(repo) + job->tail;
                LOG_DEBUG("fetching: %s", url.c_str());
                // the expected size is also the hard body cap on native (the
                // transfer aborts beyond it). Web: the normalizing proxy may
                // serve plain rebuilt containers that legitimately exceed the
                // manifest's compressed size - use a flat policy cap instead.
#if defined(XUO_EMSC)
                const uint64_t cap = 512ull << 20;
#else
                const uint64_t cap = job->expected;
#endif
                if (job->bundle_slot >= 0)
                {
                    // a bundle: the run writes every member (including this
                    // tail) to the store itself - the shared size check and
                    // store write below MUST NOT run for it (the rider's
                    // `body` is empty by design, the check would purge the
                    // member the run just wrote)
                    ok = vfs_fetch_bundle_run(*job, &bundle_bytes);
                }
                else
                {
                    ok = vfs_fetch_http(
                        url.c_str(),
                        job->tail.c_str(),
                        cap,
                        job->manifest,
                        &body,
                        job->tail.c_str());
                    if (ok && job->expected && body.size() != job->expected)
                    {
#if defined(XUO_EMSC)
                        // normalized (plain) transfers legitimately differ from
                        // the manifest's compressed size; only a truncated
                        // transfer is a failure
                        if (body.size() < job->expected)
                        {
                            LOG_WARN("size mismatch (truncated): %s", job->tail.c_str());
                            vfs_fetch_store_purge(job->tail.c_str());
                            ok = false;
                        }
                        else
                        {
                            LOG_INFO(
                                "size accepted (normalized): %s (%zu bytes)",
                                job->tail.c_str(),
                                body.size());
                        }
#else
                        LOG_WARN(
                            "size mismatch: %s (got %zu, expected %" PRIu64 ")",
                            job->tail.c_str(),
                            body.size(),
                            job->expected);
                        vfs_fetch_store_purge(job->tail.c_str());
                        ok = false;
#endif
                    }
                    if (ok)
                        ok = vfs_fetch_store_write(job->tail.c_str(), body);
                }
            }
        }

        {
            std::unique_lock<std::mutex> lock(s_mutex);
            // a settle-requeued bundle ride is back in the queue as a plain
            // job: its accounting completes HERE, but there is nothing to
            // fail or erase (settle owns the requeue). The flag - never the
            // job state, which legitimately reads queued while running
            const bool requeued = job->bundle_requeued;
            job->bundle_requeued = false;
            // the requeued-ride completion (this one) must NOT spend the job's
            // own accounting: the plain re-execution that follows is the real
            // completion (its pop re-adds s_buffered). Counting both drains
            // s_stats.inflight past zero and vfs_fetch_idle never fires again
            if (!requeued)
            {
                s_stats.inflight--;
                s_stats.requests++;
                s_stats.bytes += body.size() + bundle_bytes;
            }
            s_buffered -= job->expected ? job->expected : VFS_FETCH_SIZE_GUESS;
            if (job->stream)
                --s_stream_running; // the chunk lane frees for the next step
            else if (job->bundled_ride)
            {
                // mirrors the gather's lane take, exactly once: the flag
                // clears here so a requeued ride never decrements twice
                --s_sweep_running;
                job->bundled_ride = false;
            }
            else if (job->priority >= VFS_FETCH_PRIO_SWEEP)
                --s_sweep_running; // the lane slot frees for the next sweep
            if (!requeued)
            {
                job->state = ok ? vfs_fetch_done : vfs_fetch_failed;
                if (!ok)
                    s_stats.failures++;
                s_queue.erase(std::remove_if(
                                  s_queue.begin(),
                                  s_queue.end(),
                                  [&job](const vfs_fetch_job &q) {
                                      return q.tail == job->tail && q.state == vfs_fetch_running;
                                  }),
                              s_queue.end());
                s_active.erase(job->tail);
            }
            if (job->stream && stream_again_ms >= 0)
            {
                // keep the file's chunk loop alive: re-queue at the stream
                // tier with its pace gate (a demand read miss meanwhile can
                // promote the next copy through vfs_fetch_enqueue)
                vfs_fetch_enqueue(
                    job->tail, 0, false, VFS_FETCH_PRIO_STREAM, true,
                    vfs_fetch_now_ms() + stream_again_ms);
            }
        }
        s_cv_done.notify_all();
        s_cv_jobs.notify_all();
    }
}

bool vfs_fetch_start(const vfs_fetch_params &params)
{
    std::unique_lock<std::mutex> lock(s_mutex);
    if (s_running)
        return false;
    s_fetch_params = params;
    if (!s_fetch_params.workers)
    {
        uint32_t hw = std::thread::hardware_concurrency();
        s_fetch_params.workers = hw > 1 ? hw - 1 : 1;
        s_fetch_params.workers = std::min(s_fetch_params.workers, 8u);
    }
    if (!s_fetch_params.retries)
        s_fetch_params.retries = 1;
    if (!s_fetch_params.max_buffered)
        s_fetch_params.max_buffered = VFS_FETCH_BUFFERED_MB_DEFAULT;
    s_stop = false;
    s_running = true;
    s_active_target = s_fetch_params.workers;
    s_sweep_cap = s_active_target > 1 ? int(s_active_target - 1) : 1;
    s_sweep_running = 0;
#if defined(XUO_EMSC)
    // the worker threads park across stop/start cycles: spawn once, reuse
    if (s_workers.empty())
    {
        s_workers.resize(s_fetch_params.workers);
        for (size_t i = 0; i < s_workers.size(); ++i)
        {
            s_workers[i].thread = std::thread(vfs_fetch_worker_run, i);
        }
    }
    if (s_active_target > s_workers.size())
        s_active_target = s_workers.size();
#else
    s_workers.resize(s_fetch_params.workers);
    for (size_t i = 0; i < s_workers.size(); ++i)
    {
        s_workers[i].thread = std::thread(vfs_fetch_worker_run, i);
    }
#endif
    LOG_INFO(
        "fetch: %u workers, %u retries, %" PRIu64 "MB buffer cap",
        s_fetch_params.workers,
        s_fetch_params.retries,
        s_fetch_params.max_buffered);
    return true;
}

void vfs_fetch_set_ingame()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_running || s_active_target <= 1)
        return;
    // the pool drops by one worker (parked between jobs); the sweep lane
    // keeps one slot of headroom below the target so a worker always
    // remains free for render-critical demand
    s_active_target = s_fetch_params.workers > 1 ? s_fetch_params.workers - 1 : 1;
    s_sweep_cap = int(s_active_target) - 1 > 0 ? int(s_active_target) - 1 : 1;
    LOG_INFO(
        "fetch: in-game demotion: %u active workers, sweep lane %d",
        s_active_target,
        s_sweep_cap);
    s_cv_jobs.notify_all();
}

// true when the engine has nothing queued or in flight (all the blobs the
// current plan requested are either stored or terminally failed); used by
// the web bring-up to hold game data init until the streamed files are
// complete on disk
bool vfs_fetch_idle()
{
    std::unique_lock<std::mutex> lock(s_mutex);
    if (!s_running)
        return true;
    return s_queue.empty() && s_stats.inflight == 0;
}

uint32_t vfs_fetch_active_workers()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_active_target;
}

int vfs_fetch_sweep_running()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_sweep_running;
}

bool vfs_fetch_started()
{
    std::unique_lock<std::mutex> lock(s_mutex);
    return s_running;
}

void vfs_fetch_stop()
{
    {
        std::unique_lock<std::mutex> lock(s_mutex);
        if (!s_running)
            return;
        s_stop = true;
    }
    s_cv_jobs.notify_all();
    s_cv_done.notify_all();
#if defined(XUO_EMSC)
    // the browser main thread cannot block on pthread_join (the tab would
    // freeze): workers park on the stop flag and the pool is reused by the
    // next vfs_fetch_start
    std::unique_lock<std::mutex> lock(s_mutex);
    s_queue.clear();
    s_active.clear();
    s_running = false;
#else
    for (auto &worker : s_workers)
    {
        if (worker.thread.joinable())
            worker.thread.join();
    }
    std::unique_lock<std::mutex> lock(s_mutex);
    s_workers.clear();
    s_queue.clear();
    s_active.clear();
    s_running = false;
#endif
}

void vfs_fetch_set_repos(const char *manifest_repo, const char *file_repo)
{
    std::unique_lock<std::mutex> lock(s_mutex);
    s_manifest_repo = manifest_repo ? manifest_repo : "";
    s_file_repo = file_repo ? file_repo : "";
}

void vfs_fetch_set_bundle(const vfs_fetch_bundle_params &params)
{
    std::unique_lock<std::mutex> lock(s_mutex);
    s_bundle = params;
    if (!s_bundle.url.empty() && s_bundle.url.back() == '/')
        s_bundle.url.pop_back();
    s_bundle_max_bytes = s_bundle.max_bytes ? s_bundle.max_bytes : (4ull << 20);
    LOG_INFO(
        "fetch: bundling %s (prefix '%s', max %u members, %llu bytes)",
        s_bundle.url.empty() ? "disabled" : "enabled",
        s_bundle.member_prefix.c_str(),
        s_bundle.max_members,
        (unsigned long long)s_bundle.max_bytes);
}

void vfs_fetch_clear_bundle()
{
    std::unique_lock<std::mutex> lock(s_mutex);
    s_bundle = vfs_fetch_bundle_params();
}

void vfs_fetch_set_store(const fs_path &blobs_dir)
{
    std::unique_lock<std::mutex> lock(s_mutex);
    s_store_dir = blobs_dir;
    fs_path_create(s_store_dir);
}

void vfs_fetch_stats_get(vfs_fetch_stats *out)
{
    std::unique_lock<std::mutex> lock(s_mutex);
    if (out)
        *out = s_stats;
}

// ---------------------------------------------------------------------------
// requests
// ---------------------------------------------------------------------------

static std::shared_ptr<vfs_fetch_job> vfs_fetch_enqueue(
    const astr_t &tail, uint64_t expected, bool manifest, int priority,
    bool stream, int64_t runnable_at)
{
    // assumes s_mutex held; dedup: an equal in-flight request is reused.
    // A STILL-QUEUED duplicate is PROMOTED when the new request is higher
    // precedence - demand landing on a sweep-queued tail must jump the
    // queue, not wait behind the lane. A running transfer cannot be
    // preempted (the wait is bounded by the blob itself plus the caller's
    // timeout). Both copies move together: the queue node is what pop
    // reads, the shared job what the completing worker reads; the shared
    // state only flips at completion, so the queue node's state is the
    // authoritative still-queued signal.
    const auto found = s_active.find(tail);
    if (found != s_active.end())
    {
        for (auto &q : s_queue)
        {
            if (q.tail != tail || q.state != vfs_fetch_queued)
                continue;
            if (priority < q.priority)
                q.priority = priority;
            if (priority < found->second->priority)
                found->second->priority = priority;
            break;
        }
        return found->second;
    }
    auto job = std::make_shared<vfs_fetch_job>();
    job->tail = tail;
    job->manifest = manifest;
    job->priority = priority;
    job->expected = expected;
    job->seq = s_seq++;
    job->stream = stream;
    job->runnable_at = runnable_at;
    s_queue.push_back(*job);
    s_active[tail] = job;
    s_stats.inflight++;
    return job;
}

bool vfs_fetch_blob_to_store(
    const char *tail,
    uint64_t expected_size,
    bool manifest,
    uint32_t timeout_ms,
    int priority)
{
    if (!tail || !*tail)
        return false;
    {
        // fast path: the store already has it (idempotent, offline-friendly)
        const auto path = fs_path_join(s_store_dir, tail);
        if (fs_path_exists(path))
            return true;
    }
    std::unique_lock<std::mutex> lock(s_mutex);
    if (!s_running)
    {
        LOG_ERROR("fetch: pool not running (vfs_fetch_start)");
        return false;
    }
    const auto job = vfs_fetch_enqueue(tail, expected_size, manifest, priority);
    lock.unlock();
    s_cv_jobs.notify_all();

    lock.lock();
    if (timeout_ms == 0)
    {
        s_cv_done.wait(lock, [&job] { return job->state >= vfs_fetch_done || s_stop; });
    }
    else if (!s_cv_done.wait_for(
                 lock, std::chrono::milliseconds(timeout_ms),
                 [&job] { return job->state >= vfs_fetch_done || s_stop; }))
    {
        // the job stays in flight (the store keeps whatever lands); the
        // caller treats the timeout as a failed fetch (trap: zero-fill path)
        LOG_ERROR("fetch: timed out after %u ms waiting for %s", timeout_ms, tail);
        return false;
    }
    return job->state == vfs_fetch_done;
}

bool vfs_fetch_batch(
    const std::vector<astr_t> &tails,
    const std::vector<uint64_t> &expected_sizes,
    int priority,
    bool manifest,
    void (*progress)(float done01, const char *tail))
{
    if (tails.empty())
        return true;
    std::unique_lock<std::mutex> lock(s_mutex);
    if (!s_running)
    {
        LOG_ERROR("fetch: pool not running (vfs_fetch_start)");
        return false;
    }

    size_t pending = 0;
    std::vector<std::shared_ptr<vfs_fetch_job>> jobs;
    jobs.reserve(tails.size());
    for (size_t i = 0; i < tails.size(); ++i)
    {
        const auto path = fs_path_join(s_store_dir, tails[i]);
        if (fs_path_exists(path))
            continue; // already in the store: nothing to request
        const uint64_t expected = i < expected_sizes.size() ? expected_sizes[i] : 0;
        jobs.push_back(vfs_fetch_enqueue(tails[i], expected, manifest, priority));
    }
    pending = jobs.size();
    lock.unlock();
    s_cv_jobs.notify_all();

    // progress is pumped on the calling thread (boot thread for T0) as
    // requests settle; no game state is touched here
    size_t done = 0;
    size_t failed = 0;
    std::vector<bool> reported(pending, false);
    while (done < pending && !s_stop)
    {
        lock.lock();
        // timed wait: the caller's progress callback doubles as the splash
        // repaint tick (native boots render on the splash's progress bar) -
        // per-asset settles alone leave multi-second silent gaps and the
        // splash looks frozen/flickering
        s_cv_done.wait_for(lock, std::chrono::milliseconds(100), [&jobs, &done] {
            if (s_stop)
                return true;
            size_t settled = 0;
            for (const auto &job : jobs)
                if (job->state >= vfs_fetch_done)
                    ++settled;
            return settled > done;
        });
        if (s_stop)
            break; // shutdown: abandon the rest, the store keeps what landed
        bool advanced = false;
        for (size_t i = 0; i < jobs.size(); ++i)
        {
            const auto &job = jobs[i];
            if (job->state >= vfs_fetch_done && !reported[i])
            {
                reported[i] = true;
                ++done;
                advanced = true;
                if (job->state == vfs_fetch_failed)
                    ++failed;
                if (progress)
                    progress(float(done) / float(pending), job->tail.c_str());
            }
        }
        lock.unlock();
        // heartbeat between settles: same ratio, empty label (callers render
        // from it; the boot logger skips the empty-label ticks)
        if (progress && !advanced)
            progress(float(done) / float(pending), "");
    }
    return failed == 0;
}

void vfs_fetch_queue_blob(const char *tail, uint64_t expected_size, bool manifest, int priority)
{
    if (!tail || !*tail)
        return;
    {
        // fast path: the store already has it (idempotent, like every fetch)
        const auto path = fs_path_join(s_store_dir, tail);
        if (fs_path_exists(path))
            return;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_running)
        return;
    vfs_fetch_enqueue(tail, expected_size, manifest, priority);
    s_cv_jobs.notify_all(); // the demand granule heals only if a worker wakes
}

#if defined(XUO_EMSC)
void vfs_fetch_queue_stream(const char *name, int priority)
{
    if (!name || !*name)
        return;
    std::unique_lock<std::mutex> lock(s_mutex);
    if (!s_running)
        return;
    // dedup: a queued copy is promoted, a running chunk keeps the file's
    // loop alive (its own re-queue lands with the next completion)
    vfs_fetch_enqueue(vfs_fetch_stream_key(name), 0, false, priority, true, 0);
    lock.unlock();
    s_cv_jobs.notify_all();
}

int vfs_fetch_stream_running()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_stream_running;
}
#endif

bool vfs_fetch_manifest_fn(
    void *context,
    const char *url,
    const char *rel_path,
    const char *local_path,
    uint8_t *buffer,
    size_t *size)
{
    (void)context;
    (void)local_path;
    if (!url || !rel_path || !buffer || !size)
        return false;
    const size_t capacity = *size;

    // cache-aware: the store mirrors the remote tree, so a present
    // manifest is used as-is and HTTP only fills the gaps
    std::vector<uint8_t> cached;
    const auto stored = fs_path_join(s_store_dir, rel_path);
    if (fs_path_exists(stored))
    {
        if (!fs_file_read(fs_path_ascii(stored), cached))
            return false;
        if (cached.size() > capacity)
        {
            LOG_ERROR("manifest too large: %s (%zu > %zu)", rel_path, cached.size(), capacity);
            return false;
        }
        memcpy(buffer, cached.data(), cached.size());
        *size = cached.size();
        LOG_TRACE("manifest from store: %s (%zu bytes)", rel_path, cached.size());
        return true;
    }

    LOG_DEBUG("manifest: %s", url);
    // the manifest seam shares the engine's retry policy and the active
    // transport: an injected transport serves manifests too, so the
    // plan recursion runs without curl in tests
    std::vector<uint8_t> body;
    const bool fetched = vfs_fetch_http(url, rel_path, capacity, true, &body, rel_path);
    if (!fetched)
    {
        LOG_ERROR("manifest fetch failed: %s", rel_path);
        return false;
    }
    if (body.size() > capacity)
    {
        LOG_ERROR("manifest too large: %s (%zu > %zu)", rel_path, body.size(), capacity);
        return false;
    }
    // keep the manifest in the store: later boots consume it offline
    if (!vfs_fetch_store_write(rel_path, body))
        return false;
    memcpy(buffer, body.data(), body.size());
    *size = body.size();
    return true;
}
