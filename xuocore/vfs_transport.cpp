// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_transport: the default curl implementation. The transfer mechanics are
// the code that used to live in vfs_fetch, moved behind this seam so a web
// build can replace the transport without touching the
// engine: same wire behavior, same guards, same log lines.

#define LOGGER_MODULE Vfs

#include "vfs_transport.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <string>

#include <curl/curl.h>

#include "common.h"
#include <common/logging/logging.h>

namespace
{
std::atomic<vfs_transport *> s_override{ nullptr }; // injected transport, if any

// portable case-insensitive comparison (no CRT differences in callers)
int transport_strnicmp(const char *a, const char *b, size_t n)
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

struct transport_reply
{
    std::vector<uint8_t> body;
    long status = 0;
    long retry_after = -1; // seconds, when the server sent a valid header
    uint64_t cap = 0;      // abort above this size (0: size unknown)
};

size_t transport_write(const char *data, size_t size, size_t nmemb, void *out)
{
    auto *reply = static_cast<transport_reply *>(out);
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

size_t transport_header(const char *data, size_t size, size_t nmemb, transport_reply *reply)
{
    const size_t amount = size * nmemb;
    // delta-seconds form only; date forms fall back to the default backoff.
    // the header data is NOT NUL-terminated: parse it within an explicit
    // length bound instead of letting strtol run past the buffer
    if (amount > 13 && transport_strnicmp(data, "Retry-After:", 12) == 0)
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

// per-thread handle: reset per request (state cannot leak) while the
// connection cache - the reason for a persistent handle - survives across
// a worker's queued requests and retries. The manifest seam runs on the
// calling (boot) thread and gets the same treatment.
struct transport_handle
{
    CURL *curl = nullptr;
    ~transport_handle()
    {
        if (curl)
            curl_easy_cleanup(curl);
    }
};

transport_handle &transport_thread_handle()
{
    thread_local transport_handle handle;
    return handle;
}

// one GET with this thread's handle; fills reply on success
bool transport_get(const char *url, transport_reply *reply)
{
    reply->body.clear();
    reply->status = 0;
    reply->retry_after = -1;

    CURL *curl = transport_thread_handle().curl;
    if (!curl)
    {
        curl = curl_easy_init();
        transport_thread_handle().curl = curl;
    }
    if (!curl)
    {
        LOG_ERROR("GET %s: no curl handle", url);
        return false;
    }
    // reset per request: state from a previous transfer cannot leak, while
    // the connection cache (the reason for the persistent handle) survives
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "vuoi");
    // TLS verification is deliberately off for now, mirroring the four sites
    // in xuocore/http.cpp (the official product/file repos are plain http
    // today). Deferred: see docs/NOTES.md (MITM risk documented there; it
    // closes with .meta sha256 verification or a CA-bundle strategy).
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    // a stalled connection must never hang a worker (and with it
    // vfs_fetch_stop and game exit): 10s to connect, then 30s at under 1 B/s
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, transport_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &reply->body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, transport_header);
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

class transport_curl final : public vfs_transport
{
public:
    vfs_transport_result fetch(
        const char *url,
        const char *tail,
        uint64_t expected_size,
        vfs_transport_sink sink,
        void *sink_context,
        uint32_t timeout_ms,
        bool manifest) override
    {
        (void)tail;       // the URL is the transfer target (see the header)
        (void)manifest;   // cache-policy hint for web transports
        (void)timeout_ms; // 0 (or not): the fixed curl guards above apply
        vfs_transport_result result;
        if (!url || !*url)
            return result;
        transport_reply reply;
        reply.cap = expected_size; // exact body cap (see the header)
        if (!transport_get(url, &reply))
            return result;
        result.status = reply.status;
        result.retry_after = reply.retry_after;
        result.ok = reply.status == 200;
        if (result.ok && sink)
        {
            // one call with the whole body: the engine buffers exactly as
            // before; a streaming consumer can drain chunk-wise instead
            if (sink(sink_context, reply.body.data(), reply.body.size()))
                result.bytes = reply.body.size();
            else
                result.ok = false; // sink rejected the body
        }
        return result;
    }
};

transport_curl s_default;
} // namespace

vfs_transport_result vfs_transport::range(
    const char *url,
    const char *tail,
    uint64_t offset,
    uint64_t length,
    vfs_transport_sink sink,
    void *sink_context,
    uint32_t timeout_ms,
    bool manifest)
{
    (void)url;
    (void)tail;
    (void)offset;
    (void)length;
    (void)sink;
    (void)sink_context;
    (void)timeout_ms;
    (void)manifest;
    return vfs_transport_result(); // unsupported by default (whole-fetch fallback)
}

vfs_transport *vfs_transport_set(vfs_transport *transport)
{
    // atomic exchange: workers read the active transport while tests swap it
    return s_override.exchange(transport);
}

vfs_transport *vfs_transport_active()
{
    vfs_transport *override_ = s_override.load();
    return override_ != nullptr ? override_ : &s_default;
}

vfs_transport *vfs_transport_default()
{
    return &s_default;
}
