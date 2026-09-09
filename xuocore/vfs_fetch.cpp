// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_fetch: the download engine (VUOI plan §6.4). See vfs_fetch.h.

#define LOGGER_MODULE Vfs

#include "vfs_fetch.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <curl/curl.h>

#include "common.h"
#include <common/logging/logging.h>

// portable case-insensitive comparison (no CRT differences in callers)
static int vfs_fetch_strnicmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        const int ca = tolower(uint8_t(a[i]));
        const int cb = tolower(uint8_t(b[i]));
        if (ca != cb)
            return ca - cb;
        if (!ca)
            return 0;
    }
    return 0;
}

#include <common/utils.h>

// default cap of bytes buffered across in-flight downloads (backpressure)
#define VFS_FETCH_BUFFERED_MB_DEFAULT 64
// polite backoff: 250ms doubling, capped
#define VFS_FETCH_BACKOFF_MS 250
#define VFS_FETCH_BACKOFF_MAX_MS 8000
// guess used for backpressure accounting when no size is known up front
#define VFS_FETCH_SIZE_GUESS (64 * 1024)
// slack over the expected size before the transfer is aborted: guards the
// buffer against hostile or broken servers that never stop writing
#define VFS_FETCH_BODY_SLACK (64 * 1024)

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
    CURL *curl = nullptr;
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
// curl transport (shared by the workers and the manifest seam)
// ---------------------------------------------------------------------------

struct vfs_fetch_reply
{
    std::vector<uint8_t> body;
    long status = 0;
    long retry_after = -1; // seconds, when the server sent a valid header
    uint64_t cap = 0;      // abort above expected + slack (0: size unknown)
};

static size_t vfs_fetch_write(const char *data, size_t size, size_t nmemb, void *out)
{
    auto *reply = static_cast<vfs_fetch_reply *>(out);
    const size_t amount = size * nmemb;
    if (reply->cap && uint64_t(reply->body.size()) + uint64_t(amount) > reply->cap)
    {
        // returning a short count fails the transfer (CURLE_WRITE_ERROR): the
        // caller marks the item failed and the buffer cannot balloon
        LOG_ERROR("fetch: body exceeds cap (%" PRIu64 " bytes), aborting transfer", reply->cap);
        return 0;
    }
    reply->body.insert(reply->body.end(), data, data + amount);
    return amount;
}

static size_t
vfs_fetch_header(const char *data, size_t size, size_t nmemb, vfs_fetch_reply *reply)
{
    const size_t amount = size * nmemb;
    // delta-seconds form only; date forms fall back to the default backoff.
    // the header data is NOT NUL-terminated: parse it within an explicit
    // length bound instead of letting strtol run past the buffer
    if (amount > 13 && vfs_fetch_strnicmp(data, "Retry-After:", 12) == 0)
    {
        char value[32] = {};
        const size_t take = std::min(amount - 12, sizeof(value) - 1);
        memcpy(value, data + 12, take);
        const long seconds = strtol(value, nullptr, 10);
        if (seconds > 0)
            reply->retry_after = seconds;
    }
    return amount;
}

// one GET with this worker's (or the caller's) handle; fills reply on success
static bool vfs_fetch_http_get(CURL *curl, const char *url, vfs_fetch_reply *reply)
{
    reply->body.clear();
    reply->status = 0;
    reply->retry_after = -1;
    // reset per request: state from a previous transfer cannot leak, while
    // the connection cache (the reason for the persistent handle) survives
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "vuoi");
    // TLS verification is deliberately off for now, mirroring the four sites
    // in xuocore/http.cpp (the official product/file repos are plain http
    // today). Deferred: see docs/NOTES.md (MITM risk documented there; it
    // closes with M5 .meta sha256 verification or a CA-bundle strategy).
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    // a stalled connection must never hang a worker (and with it
    // vfs_fetch_stop and game exit): 10s to connect, then 30s at under 1 B/s
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, vfs_fetch_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply->body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, vfs_fetch_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, reply);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    // blobs are addressed immutably; a cached copy is as good as a fresh one
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    const CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        LOG_WARN("GET %s: %s", url, curl_easy_strerror(res));
        return false;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &reply->status);
    return true;
}

// GET + retries (plan §6.4): transient failures back off, 404 is definitive,
// 429 honors Retry-After. Returns true with body filled on success. `curl` is
// the caller's handle (a worker's persistent one: reset per attempt below
// keeps the connection cache across the retries and the queue's requests).
// `cap` aborts transfers beyond expected + slack (0: size unknown up front).
static bool
vfs_fetch_http(CURL *curl, const char *url, uint64_t cap, vfs_fetch_reply *reply, const char *label)
{
    if (!curl)
        return false;
    reply->cap = cap;
    bool ok = false;
    for (uint32_t attempt = 0; attempt < s_fetch_params.retries; ++attempt)
    {
        if (attempt != 0)
        {
            long ms = long(VFS_FETCH_BACKOFF_MS) << std::min(attempt - 1, 5u);
            if (reply->retry_after > 0)
                ms = reply->retry_after * 1000; // server asked politely
            ms = std::min<long>(ms, VFS_FETCH_BACKOFF_MAX_MS);
            LOG_DEBUG("retrying %s in %ldms (%u/%u)", label, ms, attempt + 1, s_fetch_params.retries);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        if (vfs_fetch_http_get(curl, url, reply))
        {
            if (reply->status == 200)
            {
                ok = true;
                break;
            }
            if (reply->status == 404)
            {
                LOG_DEBUG("not found: %s", url);
                break; // never heals; the caller decides what a miss means
            }
            LOG_WARN("GET %s: http status %ld", url, reply->status);
        }
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
    // assumes s_mutex held; picks the highest-precedence queued job
    auto best = s_queue.end();
    for (auto it = s_queue.begin(); it != s_queue.end(); ++it)
    {
        if (it->state != vfs_fetch_queued)
            continue;
        if (best == s_queue.end() ||
            std::make_pair(it->priority, it->seq) < std::make_pair(best->priority, best->seq))
        {
            best = it;
        }
    }
    if (best == s_queue.end())
        return false;
    *out = s_active[best->tail];
    best->state = vfs_fetch_running;
    return true;
}

static void vfs_fetch_worker_run(size_t index)
{
    vfs_fetch_worker &worker = s_workers[index];
    worker.curl = curl_easy_init();
    for (;;)
    {
        std::shared_ptr<vfs_fetch_job> job;
        {
            std::unique_lock<std::mutex> lock(s_mutex);
            s_cv_jobs.wait(lock, [] {
                return s_stop ||
                       (s_buffered < uint64_t(s_fetch_params.max_buffered) * 1024 * 1024 &&
                        std::any_of(
                            s_queue.begin(),
                            s_queue.end(),
                            [](const vfs_fetch_job &q) { return q.state == vfs_fetch_queued; }));
            });
            if (s_stop)
                break;
            if (!vfs_fetch_pop(&job))
                continue; // lost the race for the queued job
            s_buffered += job->expected ? job->expected : VFS_FETCH_SIZE_GUESS;
        }
        if (!worker.curl)
            worker.curl = curl_easy_init();

        const char *repo = vfs_fetch_repo_of(*job);
        astr_t url;
        bool ok = false;
        vfs_fetch_reply reply;
        if (repo)
        {
            url = astr_t(repo) + job->tail;
            LOG_DEBUG("fetching: %s", url.c_str());
            const uint64_t cap = job->expected ? job->expected + uint64_t(VFS_FETCH_BODY_SLACK) : 0;
            ok = vfs_fetch_http(worker.curl, url.c_str(), cap, &reply, job->tail.c_str());
            if (ok && job->expected && reply.body.size() != job->expected)
            {
                LOG_WARN(
                    "size mismatch: %s (got %zu, expected %" PRIu64 ")",
                    job->tail.c_str(),
                    reply.body.size(),
                    job->expected);
                vfs_fetch_store_purge(job->tail.c_str());
                ok = false;
            }
            if (ok)
                ok = vfs_fetch_store_write(job->tail.c_str(), reply.body);
        }

        {
            std::unique_lock<std::mutex> lock(s_mutex);
            s_stats.inflight--;
            s_stats.requests++;
            s_stats.bytes += reply.body.size();
            s_buffered -= job->expected ? job->expected : VFS_FETCH_SIZE_GUESS;
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
    if (worker.curl)
    {
        curl_easy_cleanup(worker.curl);
        worker.curl = nullptr;
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
    s_workers.resize(s_fetch_params.workers);
    for (size_t i = 0; i < s_workers.size(); ++i)
    {
        s_workers[i].thread = std::thread(vfs_fetch_worker_run, i);
    }
    LOG_INFO(
        "fetch: %u workers, %u retries, %" PRIu64 "MB buffer cap",
        s_fetch_params.workers,
        s_fetch_params.retries,
        s_fetch_params.max_buffered);
    return true;
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
    // assumes s_mutex held; dedup: an equal in-flight request is reused
    const auto found = s_active.find(tail);
    if (found != s_active.end())
        return found->second;
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
    const char *tail, uint64_t expected_size, bool manifest, uint32_t timeout_ms)
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
    const auto job = vfs_fetch_enqueue(tail, expected_size, manifest, 0);
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

    // cache-aware (§5.2): the store mirrors the remote tree, so a present
    // manifest is used as-is and HTTP only fills the gaps
    vfs_fetch_reply reply;
    const auto stored = fs_path_join(s_store_dir, rel_path);
    if (fs_path_exists(stored))
    {
        if (!fs_file_read(fs_path_ascii(stored), reply.body))
            return false;
        if (reply.body.size() > capacity)
        {
            LOG_ERROR("manifest too large: %s (%zu > %zu)", rel_path, reply.body.size(), capacity);
            return false;
        }
        memcpy(buffer, reply.body.data(), reply.body.size());
        *size = reply.body.size();
        LOG_TRACE("manifest from store: %s (%zu bytes)", rel_path, reply.body.size());
        return true;
    }

    LOG_DEBUG("manifest: %s", url);
    // one handle for this call: every retry reuses it (reset per request keeps
    // the connection cache); the workers keep their own persistent handles
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        LOG_ERROR("manifest fetch failed: %s (no curl handle)", rel_path);
        return false;
    }
    const bool fetched = vfs_fetch_http(curl, url, capacity, &reply, rel_path);
    curl_easy_cleanup(curl);
    if (!fetched)
    {
        LOG_ERROR("manifest fetch failed: %s", rel_path);
        return false;
    }
    if (reply.body.size() > capacity)
    {
        LOG_ERROR("manifest too large: %s (%zu > %zu)", rel_path, reply.body.size(), capacity);
        return false;
    }
    // keep the manifest in the store: later boots consume it offline
    if (!vfs_fetch_store_write(rel_path, reply.body))
        return false;
    memcpy(buffer, reply.body.data(), reply.body.size());
    *size = reply.body.size();
    return true;
}
