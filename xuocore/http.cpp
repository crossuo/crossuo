// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Network

#if defined(XUO_EMSC)
// web transport: synchronous XHR executed on the calling pthread (sync XHR
// is allowed in workers, and the emscripten main loop / browser main thread
// are never involved). Same semantics as the curl implementation: real HTTP
// status codes, Range support, body caps. The streaming product must be
// same-origin (or CORS-enabled) - see scripts/serve-wasm.py.
#include "http.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <algorithm>
#include <atomic>
#include <vector>
#include <string>

#include <emscripten.h>

#include <common/logging/logging.h>
#include <common/fs.h>

#define LOG_ERROR(...) Error(LOGGER_MODULE, __VA_ARGS__)
#define LOG_INFO(...) Info(LOGGER_MODULE, __VA_ARGS__)

// responses at or above this size travel as ranged chunks: a single
// multi-megabyte XHR response overflows the browser's in-memory handling
// (Firefox spills big responses to temp files and fails them outright with
// NS_ERROR_FILE_TOO_BIG), while every range stays small. The file repos
// serve ranges (the streaming install depends on it); a server that ignores
// Range keeps the single-response path below.
static const size_t VFS_HTTP_CHUNK_MIN = 8 * 1024 * 1024;
static const size_t VFS_HTTP_CHUNK_SIZE = 8 * 1024 * 1024;

// web download accounting (http.h): written from pool workers and the main
// thread alike - both counters are lock-free atomics
static std::atomic<uint64_t> s_web_bytes_done{ 0 };
static std::atomic<uint64_t> s_web_bytes_total{ 0 };

uint64_t vfs_web_bytes_done(void)
{
    return s_web_bytes_done.load(std::memory_order_relaxed);
}

uint64_t vfs_web_bytes_total(void)
{
    return s_web_bytes_total.load(std::memory_order_relaxed);
}

void vfs_web_bytes_expect(uint64_t bytes)
{
    if (bytes != 0)
        s_web_bytes_total.fetch_add(bytes, std::memory_order_relaxed);
}

// assembles [0, size) from VFS_HTTP_CHUNK_SIZE ranged GETs (http_get_range
// validated each part as a real 206 of the exact length)
static bool http_get_chunked(const char *url, uint64_t size, size_t body_cap, http_response *out)
{
    http_response assembled;
    assembled.body.reserve(size_t(size));
    for (uint64_t offset = 0; offset < size;)
    {
        const uint64_t length = std::min<uint64_t>(VFS_HTTP_CHUNK_SIZE, size - offset);
        http_response part;
        if (!http_get_range(url, nullptr, offset, length, &part))
        {
            LOG_ERROR(
                "chunked transfer failed at +%llu/%llu: %s",
                (unsigned long long)offset,
                (unsigned long long)size,
                url);
            return false;
        }
        assembled.body.insert(assembled.body.end(), part.body.begin(), part.body.end());
        offset += length;
    }
    if (body_cap != 0 && assembled.body.size() > body_cap)
    {
        LOG_ERROR(
            "transfer exceeded the body cap (%zu > %zu): %s",
            assembled.body.size(),
            body_cap,
            url);
        return false;
    }
    LOG_INFO(
        "chunked transfer: %llu bytes in %llu ranges: %s",
        (unsigned long long)assembled.body.size(),
        (unsigned long long)((size + VFS_HTTP_CHUNK_SIZE - 1) / VFS_HTTP_CHUNK_SIZE),
        url);
    assembled.status = 200;
    *out = std::move(assembled);
    return true;
}

// single blocking GET; returns the malloc'd body (free() it). status
// receives the HTTP code; total receives the resource size from
// Content-Range (-1 when the header is absent).
static uint8_t *http_xhr_get(const char *url, const char *range, long *status,
                             size_t *len, long *total)
{
    *status = 0;
    *len = 0;
    if (total)
        *total = -1;
    uint8_t *data = (uint8_t *)EM_ASM_PTR(
    {
        const url = UTF8ToString($0);
        const rangeHeader = UTF8ToString($1);
        try
        {
            const x = new XMLHttpRequest();
            x.open('GET', url, false); // sync: allowed on workers
            // fixed transfer guard, the web counterpart of the curl
            // transport's 10s connect + 30s low-speed aborts: a sync XHR has
            // no split stages, so one total bound stands in for both. Without
            // it a stalled same-origin request parks the calling pthread
            // forever (punctual in-game fetches included), freezing the
            // client with no way to time out
            x.timeout = 60000;
            if (rangeHeader)
                x.setRequestHeader('Range', rangeHeader);
            x.responseType = 'arraybuffer';
            x.send(null);
            const arr = x.response ? new Uint8Array(x.response) : new Uint8Array(0);
            const ptr = _malloc(arr.length + 1);
            HEAPU8.set(arr, ptr);
            ptr[arr.length] = 0;
            setValue($2, x.status, 'i32');
            setValue($3, arr.length, 'i32');
            if ($4)
            {
                const cr = x.getResponseHeader('Content-Range');
                const slash = cr ? cr.indexOf('/') : -1;
                setValue($4, slash >= 0 ? parseInt(cr.substring(slash + 1)) : -1, 'i32');
            }
            return ptr;
        }
        catch (e)
        {
            setValue($2, 0, 'i32');
            setValue($3, 0, 'i32');
            return 0;
        }
    }, url, range ? range : "", status, len, total);
    // accounting (http.h): every successful response body, counted once at
    // this chokepoint. 404/error bodies stay uncounted (a few hundred bytes
    // of noise), the chunked probe contributes its single byte and the
    // assembled ranges their payloads - together exactly the body once
    if (data != nullptr && len != 0 && (*status == 200 || *status == 206))
        s_web_bytes_done.fetch_add(uint64_t(len), std::memory_order_relaxed);
    return data;
}

void http_init()
{
}
void http_shutdown()
{
}
void http_set_user_agent(const char *name)
{
    // browsers forbid overriding the User-Agent header
    (void)name;
}

bool http_get(const char *url, const char *agent, size_t body_cap, http_response *out)
{
    (void)agent;
    if (!url || !out)
        return false;

    // Plain whole GET first: the chunked assembler below builds the exact
    // same body in RAM anyway, but a single 200 is what browser HTTP caches
    // can actually reuse - the streaming proxy marks whole responses with an
    // ETag + immutable Cache-Control, and that treatment never attaches to a
    // storm of 206 chunks (each re-boot would re-download every part). The
    // probe + ranged assembly is kept as the escape hatch for servers that
    // fail whole-file transfers: transport error / timeout / non-200 status,
    // or browsers giving up on very large single responses (Firefox
    // NS_ERROR_FILE_TOO_BIG). Inherently-ranged granule/stream traffic keeps
    // its 206 reads by design through http_get_range.
    //
    // Cap semantics are unchanged (vfs_fetch passes the flat 512 MB web cap):
    // a body that downloads fine but exceeds the cap is a failure, not a
    // fallback trigger - the chunked path enforces the very same cap on the
    // assembled size, so re-downloading everything through it can only fail
    // again, one full extra transfer later.
    long status = 0;
    {
        size_t len = 0;
        uint8_t *data = http_xhr_get(url, nullptr, &status, &len, nullptr);
        if (data && status == 200)
        {
            if (body_cap && len > body_cap)
            {
                LOG_ERROR("transfer exceeded the body cap (%zu > %zu): %s", len, body_cap, url);
                free(data);
                return false;
            }
            out->status = status;
            out->body.assign(data, data + len);
            free(data);
            return true;
        }
        if (data)
            free(data);
        LOG_INFO(
            "whole-body GET failed (%ld), falling back to ranged chunks: %s", status, url);
    }

    // ranged fallback: the probe's Content-Range decides the size (a cap-only
    // path would misbehave when the cap is an upper bound, e.g. manifest
    // buffers). No range support (or no size, or a cap below the chunking
    // floor): nothing left to try.
    if (body_cap >= VFS_HTTP_CHUNK_MIN)
    {
        size_t len = 0;
        long total = -1;
        uint8_t *probe = http_xhr_get(url, "bytes=0-0", &status, &len, &total);
        if (probe)
            free(probe);
        if (status == 206 && len == 1 && total > 0)
            return http_get_chunked(url, uint64_t(total), body_cap, out);
    }
    LOG_ERROR("transport failed (%ld): %s", status, url);
    return false;
}

bool http_get_range(
    const char *url,
    const char *agent,
    uint64_t offset,
    uint64_t length,
    http_response *out,
    int64_t *out_total)
{
    (void)agent;
    if (!url || !out)
        return false;
    char range[64];
    snprintf(range, sizeof(range), "bytes=%llu-%llu", (unsigned long long)offset, (unsigned long long)(offset + length - 1));
    long status = 0;
    size_t len = 0;
    long total = -1;
    uint8_t *data = http_xhr_get(url, range, &status, &len, &total);
    if (!data)
    {
        LOG_ERROR("transport failed (%ld): %s", status, url);
        return false;
    }
    if (out_total != nullptr)
    {
        *out_total = total;
    }
    // only honor a real range answer (206) - a 200 would deliver the whole
    // file. A clamped tail (the request ran past EOF: 206 shorter than the
    // asked length, exactly the remaining bytes) is a valid answer and the
    // only one small resources can ever give: the stream-file lane chunks
    // fixed 2MB reads and files smaller than that (multi.mul, the music
    // config, facet charts on small shards) always clamp on the first read.
    const bool clamped_tail = total > 0 && len == size_t(total - offset) && len < length;
    if (status != 206 || (len != length && !clamped_tail))
    {
        LOG_ERROR("range fetch mismatch (%ld, %zu vs %llu): %s", status, len, (unsigned long long)length, url);
        free(data);
        return false;
    }
    out->status = 206;
    out->body.assign(data, data + len);
    free(data);
    return true;
}

bool http_get_binary(const char *url, const uint8_t *buf, size_t *size)
{
    if (!url || !buf || !size)
        return false;
    long status = 0;
    size_t len = 0;
    long total = -1;
    uint8_t *data = http_xhr_get(url, nullptr, &status, &len, &total);
    if (!data || status != 200)
    {
        LOG_ERROR("transport failed (%ld): %s", status, url);
        return false;
    }
    if (len > *size)
    {
        LOG_ERROR("transfer exceeded the buffer size (%zu > %zu): %s", len, *size, url);
        free(data);
        return false;
    }
    memcpy((void *)buf, data, len);
    *size = len;
    free(data);
    return true;
}

bool http_get_binary(const char *url, std::vector<uint8_t> &data)
{
    long status = 0;
    size_t len = 0;
    long total = -1;
    uint8_t *raw = http_xhr_get(url, nullptr, &status, &len, &total);
    if (!raw || status != 200)
    {
        LOG_ERROR("transport failed (%ld): %s", status, url);
        return false;
    }
    data.assign(raw, raw + len);
    free(raw);
    return true;
}

bool http_get_string(const char *url, std::string &data)
{
    long status = 0;
    size_t len = 0;
    long total = -1;
    uint8_t *raw = http_xhr_get(url, nullptr, &status, &len, &total);
    if (!raw || status != 200)
    {
        LOG_ERROR("transport failed (%ld): %s", status, url);
        return false;
    }
    data.assign(raw, raw + len);
    free(raw);
    return true;
}

bool http_get_file(const char *url, const char *filename)
{
    long status = 0;
    size_t len = 0;
    long total = -1;
    uint8_t *raw = http_xhr_get(url, nullptr, &status, &len, &total);
    if (!raw || status != 200)
    {
        LOG_ERROR("http_get_file failed (%ld): %s", status, url);
        return false;
    }
    bool ok = fs_file_write(filename, raw, len);
    free(raw);
    return ok;
}

std::string http_urlencode(std::string str)
{
    static const char *hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(str.size());
    for (char c : str)
    {
        const unsigned char ch = (unsigned char)c;
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == '/')
        {
            out += c;
        }
        else
        {
            out += '%';
            out += hex[ch >> 4];
            out += hex[ch & 0x0f];
        }
    }
    return out;
}

#else // #if defined(XUO_EMSC)

#include "http.h"

#include <vector>
#include <string.h>
#include <assert.h>

#include <curl/curl.h>
#include <external/tinyxml2.h> // not really needed here, just for the version info

#include <common/logging/logging.h>
#include <common/fs.h>

#include "common.h"

static const char *s_agentName = nullptr;

#define DO_CURL(x)                                                                                 \
    do                                                                                             \
    {                                                                                              \
        CURLcode r = curl_easy_##x;                                                                \
        if (r != CURLE_OK)                                                                         \
            LOG_ERROR(__FILE__ ":%d:ERROR:%d: %s", __LINE__, r, curl_easy_strerror(r));            \
    } while (0)

#define DO_CURL_RET(ret, x)                                                                        \
    ret = curl_easy_##x;                                                                           \
    if (ret != CURLE_OK)                                                                           \
        LOG_ERROR(__FILE__ ":%d:ERROR:%d: %s", __LINE__, ret, curl_easy_strerror(ret));

static size_t recv_data_string(const char *data, size_t size, size_t nmemb, std::string *str)
{
    assert(data && str && size && nmemb);
    const auto amount = size * nmemb;
    str->append(data, amount);
    return amount;
}

static size_t recv_data_vector(const char *data, size_t size, size_t nmemb, std::vector<char> *vec)
{
    assert(data && vec && size && nmemb);
    const auto amount = size * nmemb;
    vec->insert(vec->end(), data, data + amount);
    return amount;
}

struct http_recv_buf
{
    size_t max_len;
    size_t offset;
    const uint8_t *data;
};
static size_t recv_data(const char *data, size_t size, size_t nmemb, http_recv_buf *buf)
{
    assert(data && buf && size && nmemb);
    const auto amount = size * nmemb;
    assert(buf->offset + amount < buf->max_len);
    memcpy((void *)&buf->data[buf->offset], data, amount);
    buf->offset += amount;
    return amount;
}

static CURL *s_curl_handle = nullptr;
void http_init()
{
    if (s_curl_handle)
        return;
    curl_global_init(CURL_GLOBAL_ALL);
    curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    LOG_INFO(
        "libcurl %s (%s, %s, libz/%s, tinyxml2/%d.%d.%d)",
        info->version,
        info->host,
        info->ssl_version,
        info->libz_version,
        TINYXML2_MAJOR_VERSION,
        TINYXML2_MINOR_VERSION,
        TINYXML2_PATCH_VERSION);
    s_curl_handle = curl_easy_init();
}

void http_set_user_agent(const char *name)
{
    s_agentName = name;
}

void http_shutdown()
{
    assert(s_curl_handle && "http_init wasn't called");
    curl_easy_cleanup(s_curl_handle);
    s_curl_handle = nullptr;
    curl_global_cleanup();
}

// ---------------------------------------------------------------------------
// extended GET (status / retry-after / body cap / range) - the transfer seam
// the streaming layer (vfs_transport) runs on. One thread-local easy handle,
// reset per request: the connection cache survives across a thread's
// requests (keep-alive for thousands of small range GETs) while every other
// option state resets.
// ---------------------------------------------------------------------------

struct http_header_state
{
    long retry_after = -1;
};

struct http_body_state
{
    std::vector<uint8_t> *body;
    size_t cap; // 0: unbounded
};

static size_t http_write_body(const char *data, size_t size, size_t nmemb, void *out)
{
    auto *st = static_cast<http_body_state *>(out);
    const size_t amount = size * nmemb;
    // hard body cap: transfers exceeding it abort (CURLE_WRITE_ERROR), which
    // is how a range-ignoring server (200 + full body) is cut off before it
    // can balloon
    if (st->cap != 0 && st->body->size() + amount > st->cap)
    {
        LOG_ERROR("GET: body exceeds cap (%zu bytes), aborting transfer", st->cap);
        return 0;
    }
    st->body->insert(st->body->end(), data, data + amount);
    return amount;
}

static size_t http_header(const char *data, size_t size, size_t nmemb, void *out)
{
    auto *st = static_cast<http_header_state *>(out);
    const size_t amount = size * nmemb;
    // delta-seconds form only; date forms fall back to the caller's backoff
    if (amount > 13 && strncasecmp(data, "Retry-After:", 12) == 0)
    {
        char value[32] = {};
        const size_t take = amount - 12 < sizeof(value) - 1 ? amount - 12 : sizeof(value) - 1;
        memcpy(value, data + 12, take);
        const long seconds = strtol(value, nullptr, 10);
        if (seconds > 0)
            st->retry_after = seconds;
    }
    return amount;
}

static http_response http_perform(
    const char *url, const char *agent, const char *range, size_t body_cap)
{
    http_response out;

    // thread-local easy handle: reset per request so option state cannot
    // leak, while the connection cache (the reason for a persistent handle)
    // survives across this thread's requests
    static thread_local CURL *tls_handle = nullptr;
    if (!tls_handle)
        tls_handle = curl_easy_init();
    CURL *curl = tls_handle;
    if (!curl)
    {
        LOG_ERROR("GET %s: no curl handle", url);
        return out;
    }

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, agent && *agent ? agent : "CrossUO");
    // TLS verification is deliberately off for now: the official product and
    // file repos are plain http today (see docs/NOTES.md - closes with .meta
    // sha256 verification or a CA-bundle strategy)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    // a stalled connection must never hang a worker: 10s to connect, then
    // 30s at under 1 B/s
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_body);
    http_body_state body_state{ &out.body, body_cap };
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_state);
    http_header_state header_state;
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, http_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_state);
    // blobs are addressed immutably; a cached copy is as good as a fresh one
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    if (range)
        curl_easy_setopt(curl, CURLOPT_RANGE, range);

    const CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        LOG_ERROR("GET %s: %s", url, curl_easy_strerror(res));
        out.body.clear();
        out.status = 0;
        return out;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out.status);
    out.retry_after = header_state.retry_after;
    return out;
}

bool http_get(const char *url, const char *agent, size_t body_cap, http_response *out)
{
    if (!url || !*url || out == nullptr)
        return false;
    *out = http_perform(url, agent, nullptr, body_cap);
    return out->status != 0;
}

bool http_get_range(
    const char *url,
    const char *agent,
    uint64_t offset,
    uint64_t length,
    http_response *out,
    int64_t *out_total)
{
    if (!url || !*url || out == nullptr || length == 0)
        return false;
    const std::string range =
        std::to_string(offset) + "-" + std::to_string(offset + length - 1);
    *out = http_perform(url, agent, range.c_str(), length);
    if (out_total != nullptr)
    {
        // curl has no Content-Range total parsed here; callers only need it
        // on the web transport
        *out_total = -1;
    }
    return out->status == 206 && out->body.size() == length;
}

bool http_get_binary(const char *url, const uint8_t *buf, size_t *size)
{
    assert(s_curl_handle && "http_init wasn't called");
    assert(url && "invalid url");
    assert(buf && size);
    http_recv_buf tmp = { *size, 0, buf };
    LOG_INFO("Downloading %s", url);
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    if (s_agentName)
        DO_CURL(setopt(curl, CURLOPT_USERAGENT, s_agentName));
    DO_CURL(setopt(curl, CURLOPT_SSL_VERIFYPEER, 0));
    DO_CURL(setopt(curl, CURLOPT_FOLLOWLOCATION, 1));
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &tmp));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data));
    CURLcode result;
    DO_CURL_RET(result, perform(curl));
    curl_easy_cleanup(curl);
    *size = tmp.offset;
    return result == CURLE_OK;
}

bool http_get_binary(const char *url, std::vector<uint8_t> &data)
{
    assert(s_curl_handle && "http_init wasn't called");
    assert(url && "invalid url");
    LOG_INFO("Downloading %s", url);
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    if (s_agentName)
        DO_CURL(setopt(curl, CURLOPT_USERAGENT, s_agentName));
    DO_CURL(setopt(curl, CURLOPT_SSL_VERIFYPEER, 0));
    DO_CURL(setopt(curl, CURLOPT_FOLLOWLOCATION, 1));
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &data));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_vector));
    CURLcode result;
    DO_CURL_RET(result, perform(curl));
    curl_easy_cleanup(curl);
    return result == CURLE_OK;
}

bool http_get_string(const char *url, std::string &data)
{
    assert(s_curl_handle && "http_init wasn't called");
    assert(url && "invalid url");
    LOG_INFO("Downloading %s", url);
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    if (s_agentName)
        DO_CURL(setopt(curl, CURLOPT_USERAGENT, s_agentName));
    DO_CURL(setopt(curl, CURLOPT_SSL_VERIFYPEER, 0));
    DO_CURL(setopt(curl, CURLOPT_FOLLOWLOCATION, 1));
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &data));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_string));
    CURLcode result;
    DO_CURL_RET(result, perform(curl));
    curl_easy_cleanup(curl);
    return result == CURLE_OK;
}

bool http_get_file(const char *url, const char *filename)
{
    assert(s_curl_handle && "http_init wasn't called");
    assert(url && "invalid url");
    assert(filename && "invalid filename");
    LOG_INFO("Downloading %s", url);
    FILE *fp = fopen(filename, "wb");
    if (!fp)
        return false;
    LOG_TRACE("url %s", url);
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    if (s_agentName)
        DO_CURL(setopt(curl, CURLOPT_USERAGENT, s_agentName));
    DO_CURL(setopt(curl, CURLOPT_SSL_VERIFYPEER, 0));
    DO_CURL(setopt(curl, CURLOPT_FOLLOWLOCATION, 1));
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, fp));
    CURLcode result;
    DO_CURL_RET(result, perform(curl));
    curl_easy_cleanup(curl);
    fclose(fp);
    return result == CURLE_OK;
}

std::string http_urlencode(std::string str)
{
    std::string encoded;
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    char *out = curl_easy_escape(curl, str.c_str(), (int)str.length());
    if (out)
    {
        encoded = out;
        curl_free(out);
    }
    curl_easy_cleanup(curl);
    return encoded;
}

// web download accounting (http.h): the curl transport carries no byte
// accounting - the splash/progress surfaces it feeds are web-only. The no-op
// stubs keep every caller linking; the readers answer "nothing known".
uint64_t vfs_web_bytes_done(void)
{
    return 0;
}

uint64_t vfs_web_bytes_total(void)
{
    return 0;
}

void vfs_web_bytes_expect(uint64_t bytes)
{
    (void)bytes;
}
#endif // #if defined(XUO_EMSC)
