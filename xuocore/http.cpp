// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Network

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
        LOG_WARN("GET: body exceeds cap (%zu bytes), aborting transfer", st->cap);
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
        LOG_WARN("GET %s: %s", url, curl_easy_strerror(res));
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