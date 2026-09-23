// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Network

#if defined(XUO_EMSC)
// web transport: emscripten_fetch on a dedicated worker thread. Blocking
// semantics match the curl implementation (callers are the vfs fetch pool
// threads and the boot path), and the browser main thread is never involved,
// so the game loop keeps running while transfers are in flight.
//
// NOTE: the browser enforces CORS - the streaming product must be served
// from the same origin as the page (or from a CORS-enabled mirror).
#include "http.h"
#include <string.h>
#include <vector>
#include <string>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <emscripten/fetch.h>

#include <common/logging/logging.h>
#include <common/fs.h>

#define LOG_ERROR(...) Error(LOGGER_MODULE, __VA_ARGS__)

static const char *s_agentName = nullptr;

namespace {

enum class http_op
{
    op_get,
    op_get_range,
};

struct http_request
{
    http_op op = http_op::op_get;
    std::string url;
    std::string range;   // "bytes=a-b" for ranged gets
    size_t body_cap = 0; // 0: unbounded
    // results
    bool ok = false;
    long status = 0;
    std::vector<uint8_t> body;
    bool done = false;
};

std::mutex s_mutex;
std::condition_variable s_cv;
std::deque<http_request *> s_queue;
bool s_worker_stop = false;
std::thread s_worker;

void http_perform(http_request &req)
{
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

    const char *headers[3] = {};
    if (req.op == http_op::op_get_range && !req.range.empty())
    {
        // user agent is a forbidden header name in browsers - skipped
        headers[0] = "Range";
        headers[1] = req.range.c_str();
        attr.requestHeaders = headers;
    }

    emscripten_fetch_t *fetch = emscripten_fetch(&attr, req.url.c_str());
    if (fetch == nullptr)
    {
        req.status = 0;
        req.ok = false;
        return;
    }

    req.status = (long)fetch->status;
    if (fetch->status == 200 && fetch->data && fetch->numBytes > 0)
    {
        req.body.assign(fetch->data, fetch->data + fetch->numBytes);
        req.ok = true;
        if (req.body_cap && req.body.size() > req.body_cap)
        {
            LOG_ERROR(
                "transfer exceeded the body cap (%zu > %zu): %s",
                req.body.size(),
                req.body_cap,
                req.url.c_str());
            req.ok = false;
        }
    }
    else if (fetch->status != 200)
    {
        LOG_ERROR(
            "transport failed (%ld): %s",
            req.status,
            fetch->status ? req.url.c_str() : "network error");
    }
    emscripten_fetch_close(fetch);
}

void http_worker()
{
    for (;;)
    {
        http_request *req = nullptr;
        {
            std::unique_lock<std::mutex> lock(s_mutex);
            s_cv.wait(lock, [] { return s_worker_stop || !s_queue.empty(); });
            if (!s_queue.empty())
            {
                req = s_queue.front();
                s_queue.pop_front();
            }
            else if (s_worker_stop)
            {
                return;
            }
        }
        if (req)
        {
            http_perform(*req);
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                req->done = true;
            }
            s_cv.notify_all();
        }
    }
}

// serializes the call over the dedicated transport worker: the caller blocks
// exactly like the curl implementation does
bool http_submit(http_request &req)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_worker_stop)
            return false;
        s_queue.push_back(&req);
    }
    s_cv.notify_all();
    std::unique_lock<std::mutex> lock(s_mutex);
    s_cv.wait(lock, [&req] { return req.done; });
    return req.ok;
}

} // namespace

void http_init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_worker.joinable())
        s_worker = std::thread(http_worker);
}

void http_shutdown()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_worker_stop = true;
    }
    s_cv.notify_all();
    if (s_worker.joinable())
        s_worker.join();
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
    http_request req;
    req.op = http_op::op_get;
    req.url = url;
    req.body_cap = body_cap;
    if (!http_submit(req))
        return false;
    out->status = req.status;
    out->body = std::move(req.body);
    return req.ok && out->status == 200;
}

bool http_get_range(
    const char *url, const char *agent, uint64_t offset, uint64_t length, http_response *out)
{
    (void)agent;
    if (!url || !out)
        return false;
    http_request req;
    req.op = http_op::op_get_range;
    req.url = url;
    req.body_cap = length ? length : 0;
    char range[64];
    snprintf(range, sizeof(range), "bytes=%llu-%llu",
             (unsigned long long)offset, (unsigned long long)(offset + length - 1));
    req.range = range;
    if (!http_submit(req))
        return false;
    out->status = req.status;
    // like the curl implementation: only honor a real range answer (206 with
    // the exact byte count) - a 200 would deliver the whole file
    out->body = std::move(req.body);
    return req.ok && out->status == 206 && out->body.size() == length;
}

bool http_get_binary(const char *url, const uint8_t *buf, size_t *size)
{
    if (!url || !buf || !size)
        return false;
    http_request req;
    req.op = http_op::op_get;
    req.url = url;
    if (!http_submit(req))
        return false;
    if (req.status != 200 || req.body.size() > *size)
        return false;
    memcpy((void *)buf, req.body.data(), req.body.size());
    *size = req.body.size();
    return true;
}

bool http_get_binary(const char *url, std::vector<uint8_t> &data)
{
    http_request req;
    req.op = http_op::op_get;
    req.url = url;
    if (!http_submit(req))
    {
        data.clear();
        return false;
    }
    if (req.status != 200)
    {
        data.clear();
        return false;
    }
    data = std::move(req.body);
    return true;
}

bool http_get_string(const char *url, std::string &data)
{
    http_request req;
    req.op = http_op::op_get;
    req.url = url;
    if (!http_submit(req))
    {
        data.clear();
        return false;
    }
    if (req.status != 200)
    {
        data.clear();
        return false;
    }
    data.assign(req.body.begin(), req.body.end());
    return true;
}

bool http_get_file(const char *url, const char *filename)
{
    http_request req;
    req.op = http_op::op_get;
    req.url = url;
    if (!http_submit(req))
        return false;
    if (req.status != 200 || req.body.empty())
        return false;
    return fs_file_write(filename, req.body.data(), req.body.size());
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

#include "http.h"#else // #if defined(XUO_EMSC)

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
    const char *url, const char *agent, uint64_t offset, uint64_t length, http_response *out)
{
    if (!url || !*url || out == nullptr || length == 0)
        return false;
    const std::string range =
        std::to_string(offset) + "-" + std::to_string(offset + length - 1);
    *out = http_perform(url, agent, range.c_str(), length);
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
#endif // #if defined(XUO_EMSC)
