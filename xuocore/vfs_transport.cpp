// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_transport: the default HTTP implementation.

#define LOGGER_MODULE Vfs

#include "vfs_transport.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <string>

#include <xuocore/http.h>

#include "common.h"
#include <common/logging/logging.h>

#define VUOI_AGENT_NAME "EAMythic Patch Client"

static std::atomic<vfs_transport *> s_override{ nullptr }; // injected transport, if any

namespace
{

vfs_transport_result vfs_transport_deliver(
    const vfs_transport_sink sink, void *sink_context, const http_response &reply)
{
    vfs_transport_result result;
    result.status = reply.status;
    result.retry_after = reply.retry_after;
    result.ok = reply.status != 0;
    if (result.ok && sink)
    {
        // one call with the whole body: the engine buffers exactly as before;
        // a streaming consumer can drain chunk-wise instead
        if (sink(sink_context, reply.body.data(), reply.body.size()))
            result.bytes = reply.body.size();
        else
            result.ok = false; // sink rejected the body
    }
    return result;
}

class transport_http final : public vfs_transport
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
        (void)timeout_ms; // the http layer's fixed guards apply
        (void)manifest;   // cache-policy hint for web transports
        http_response reply;
        if (!http_get(url, VUOI_AGENT_NAME, size_t(expected_size), &reply))
            return {};
        // only a real 200 is a delivered blob: 404/410/5xx must fail the
        // transfer WITH their status - the engine treats 404 as definitive
        // (no retry) and other codes as transient
        if (reply.status != 200)
        {
            LOG_WARN("GET %s: status %ld", url, reply.status);
            vfs_transport_result result;
            result.status = reply.status;
            result.retry_after = reply.retry_after;
            return result;
        }
        return vfs_transport_deliver(sink, sink_context, reply);
    }

    vfs_transport_result range(
        const char *url,
        const char *tail,
        uint64_t offset,
        uint64_t length,
        vfs_transport_sink sink,
        void *sink_context,
        uint32_t timeout_ms,
        bool manifest) override
    {
        (void)tail;       // the URL is the transfer target (see the header)
        (void)timeout_ms; // the http layer's fixed guards apply
        (void)manifest;
        vfs_transport_result result;
        if (!url || !*url || length == 0)
            return result;
        http_response reply;
        int64_t total = -1;
        if (!http_get_range(url, VUOI_AGENT_NAME, offset, length, &reply, &total))
        {
            LOG_WARN(
                "range GET %s: status %ld, %zu bytes (wanted %" PRIu64 ")",
                url,
                reply.status,
                reply.body.size(),
                length);
            result.total = total;
            return result;
        }
        result.ok = true;
        result.status = reply.status;
        result.total = total;
        if (sink)
        {
            if (sink(sink_context, reply.body.data(), reply.body.size()))
                result.bytes = reply.body.size();
            else
                result.ok = false;
        }
        return result;
    }
};

transport_http s_default;
} // namespace

// base default: transports without range support (test fakes) keep the
// whole-fetch fallback - callers see ok == false with status 0
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
    return vfs_transport_result(); // unsupported: whole-fetch fallback
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
