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
};

struct vfs_fetch_worker
{
    std::thread thread;
};

static std::mutex s_mutex;
static std::condition_variable s_cv_jobs; // wakes workers (queue/backpressure/stop)
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
// worker pool
// ---------------------------------------------------------------------------

static bool vfs_fetch_pop(std::shared_ptr<vfs_fetch_job> *out)
{
    // assumes s_mutex held; picks the highest-precedence runnable job. A
    // sweep job only runs inside the sweep lane (s_sweep_cap): when the
    // lane is full the sweep is skipped for demand and background work,
    // which is exactly the render-critical precedence the lane exists for
    const bool sweep_slot = s_sweep_running < s_sweep_cap;
    auto best = s_queue.end();
    for (auto it = s_queue.begin(); it != s_queue.end(); ++it)
    {
        if (it->state != vfs_fetch_queued)
            continue;
        if (it->priority >= VFS_FETCH_PRIO_SWEEP && !sweep_slot)
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
    if (best->priority >= VFS_FETCH_PRIO_SWEEP)
        ++s_sweep_running;
    *out = s_active[best->tail];
    best->state = vfs_fetch_running;
    return true;
}

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
            // sweep lane - with a lane slot free
            s_cv_jobs.wait(lock, [&] {
                return s_stop ||
                       (index < s_active_target &&
                        s_buffered < uint64_t(s_fetch_params.max_buffered) * 1024 * 1024 &&
                        std::any_of(
                            s_queue.begin(),
                            s_queue.end(),
                            [](const vfs_fetch_job &q)
                            {
                                return q.state == vfs_fetch_queued &&
                                       (q.priority < VFS_FETCH_PRIO_SWEEP ||
                                        s_sweep_running < s_sweep_cap);
                            }));
            });
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

        const char *repo = vfs_fetch_repo_of(*job);
        astr_t url;
        bool ok = false;
        std::vector<uint8_t> body;
        if (repo)
        {
            url = astr_t(repo) + job->tail;
            LOG_DEBUG("fetching: %s", url.c_str());
            // the expected size is also the hard body cap (transport seam:
            // exact-size validation stays here, the transfer aborts beyond)
            const uint64_t cap = job->expected;
            ok = vfs_fetch_http(
                url.c_str(), job->tail.c_str(), cap, job->manifest, &body, job->tail.c_str());
            if (ok && job->expected && body.size() != job->expected)
            {
                LOG_WARN(
                    "size mismatch: %s (got %zu, expected %" PRIu64 ")",
                    job->tail.c_str(),
                    body.size(),
                    job->expected);
                vfs_fetch_store_purge(job->tail.c_str());
                ok = false;
            }
            if (ok)
                ok = vfs_fetch_store_write(job->tail.c_str(), body);
        }

        {
            std::unique_lock<std::mutex> lock(s_mutex);
            s_stats.inflight--;
            s_stats.requests++;
            s_stats.bytes += body.size();
            s_buffered -= job->expected ? job->expected : VFS_FETCH_SIZE_GUESS;
            if (job->priority >= VFS_FETCH_PRIO_SWEEP)
                --s_sweep_running; // the lane slot frees for the next sweep
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
    const astr_t &tail, uint64_t expected, bool manifest, int priority)
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
        s_cv_done.wait(lock, [&jobs, &done] {
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
        for (size_t i = 0; i < jobs.size(); ++i)
        {
            const auto &job = jobs[i];
            if (job->state >= vfs_fetch_done && !reported[i])
            {
                reported[i] = true;
                ++done;
                if (job->state == vfs_fetch_failed)
                    ++failed;
                if (progress)
                    progress(float(done) / float(pending), job->tail.c_str());
            }
        }
        lock.unlock();
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
