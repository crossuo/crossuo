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
#include <vector>
#include <string>

#include <emscripten.h>

#include <common/logging/logging.h>
#include <common/fs.h>

#define LOG_ERROR(...) Error(LOGGER_MODULE, __VA_ARGS__)

static const char *s_agentName = nullptr;

// performs a blocking GET; returns the malloc'd body (free() it) or nullptr.
// status receives the HTTP code (0 when the request failed at the transport
// level).
static uint8_t *http_xhr(const char *url, const char *range, long *status, size_t *len)
{
    *status = 0;
    *len = 0;
    uint8_t *data = (uint8_t *)EM_ASM_PTR(
    {
        const url = UTF8ToString($0);
        const rangeHeader = UTF8ToString($1);
        try
        {
            const x = new XMLHttpRequest();
            x.open('GET', url, false); // sync: allowed on workers
            if (rangeHeader)
                x.setRequestHeader('Range', rangeHeader);
            x.responseType = 'arraybuffer';
            x.send(null);
            const data = x.response ? new Uint8Array(x.response) : new Uint8Array(0);
            const ptr = _malloc(data.length + 1);
            HEAPU8.set(data, ptr);
            ptr[data.length] = 0;
            setValue($2, x.status, 'i32');
            setValue($3, data.length, 'i32');
            return ptr;
        }
        catch (e)
        {
            setValue($2, 0, 'i32');
            setValue($3, 0, 'i32');
            return 0;
        }
    },
        url, range ? range : "", status, len);
    if (data)
    {
        // the trailing terminator byte is not part of the payload
        *len = *len;
    }
    return data;
}

static bool http_xhr_body(const char *url, const char *range, size_t body_cap,
                          long *status, std::vector<uint8_t> &body)
{
    size_t len = 0;
    uint8_t *data = http_xhr(url, range, status, &len);
    if (!data)
    {
        LOG_ERROR("transport failed (0): %s", url);
        return false;
    }
    *status = *status;
    if (*status != 200 && *status != 206)
    {
        LOG_ERROR("transport failed (%ld): %s", *status, url);
        free(data);
        return false;
    }
    if (body_cap && len > body_cap)
    {
        LOG_ERROR("transfer exceeded the body cap (%zu > %zu): %s", len, body_cap, url);
        free(data);
        return false;
    }
    body.assign(data, data + len);
    free(data);
    return true;
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
    long status = 0;
    if (!http_xhr_body(url, nullptr, body_cap, &status, out->body))
    {
        out->status = status;
        out->body.clear();
        return false;
    }
    out->status = status;
    return out->status == 200;
}

bool http_get_range(
    const char *url, const char *agent, uint64_t offset, uint64_t length, http_response *out)
{
    (void)agent;
    if (!url || !out)
        return false;
    char range[64];
    snprintf(range, sizeof(range), "bytes=%llu-%llu",
             (unsigned long long)offset, (unsigned long long)(offset + length - 1));
    long status = 0;
    std::vector<uint8_t> body;
    if (!http_xhr_body(url, range, length, &status, body))
    {
        out->status = status;
        return false;
    }
    // like the curl implementation: only honor a real range answer (206 with
    // the exact byte count) - a 200 would deliver the whole file
    out->status = status;
    out->body = std::move(body);
    return out->status == 206 && out->body.size() == length;
}

bool http_get_binary(const char *url, const uint8_t *buf, size_t *size)
{
    if (!url || !buf || !size)
        return false;
    long status = 0;
    std::vector<uint8_t> body;
    if (!http_xhr_body(url, nullptr, 0, &status, body) || status != 200)
        return false;
    if (body.size() > *size)
        return false;
    memcpy((void *)buf, body.data(), body.size());
    *size = body.size();
    return true;
}

bool http_get_binary(const char *url, std::vector<uint8_t> &data)
{
    long status = 0;
    if (!http_xhr_body(url, nullptr, 0, &status, data) || status != 200)
    {
        data.clear();
        return false;
    }
    return true;
}

bool http_get_string(const char *url, std::string &data)
{
    long status = 0;
    std::vector<uint8_t> body;
    if (!http_xhr_body(url, nullptr, 0, &status, body) || status != 200)
    {
        data.clear();
        return false;
    }
    data.assign(body.begin(), body.end());
    return true;
}

bool http_get_file(const char *url, const char *filename)
{
    long status = 0;
    std::vector<uint8_t> body;
    if (!http_xhr_body(url, nullptr, 0, &status, body) || status != 200)
    {
        LOG_ERROR("http_get_file failed (%ld): %s", status, url);
        return false;
    }
    return fs_file_write(filename, body.data(), body.size());
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
