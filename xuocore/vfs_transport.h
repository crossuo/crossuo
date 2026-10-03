// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_transport: the pluggable HTTP transfer seam behind the fetch engine
// (web groundwork). The engine (vfs_fetch) owns the
// queue, dedup, retry policy and worker pool; a transport owns exactly one
// transfer: turn a URL into body bytes.
//
// The built-in implementation is the curl path that used to live inside
// vfs_fetch, moved here verbatim - with no transport injected the engine
// behaves byte-identically to the unseamed code. A web build swaps in an
// emscripten_fetch/fetch-based transport through the same vtable; the
// seam is proven by tests that inject a fake (no curl, no network).
//
// Why the absolute URL (and not tail + repo): sub-manifest URLs are composed
// by the mft engine from the product manifest's own <manifestrepos> - on a
// first boot those repos are not known to the fetch engine yet. The URL is
// therefore resolved once by the caller and passed in; `tail` (the store
// identity) travels along for logging and as a stable cache/dedup key.

#pragma once

#include <stdint.h>

#include <common/fs.h>

// outcome of one transport operation
struct vfs_transport_result
{
    bool ok = false;       // HTTP 200 and the body fully delivered
    long status = 0;       // HTTP status (0: transport-level failure)
    long retry_after = -1; // Retry-After seconds, when the server sent one
    uint64_t bytes = 0;    // body bytes delivered to the sink
    int64_t total = -1;    // resource total from Content-Range (ranges only)
};

// receives body bytes as they arrive (once, with the whole buffer, for the
// curl transport). Returning false aborts the transfer.
using vfs_transport_sink = bool (*)(void *context, const uint8_t *data, size_t size);

struct vfs_transport
{
    // ONE transfer attempt. No retries: the engine owns the backoff policy
    // and reads `result.status` / `result.retry_after` to steer it. The sink
    // is invoked only on a successful (HTTP 200) transfer.
    //
    // `expected_size` doubles as the hard body cap: the exact on-wire size
    // for blobs (0: unknown - the engine validates afterwards), or the
    // caller's buffer capacity for manifests; the transfer aborts beyond it.
    //
    // `timeout_ms` bounds the whole transfer; 0 keeps the transport defaults
    // (curl: 10s connect + 30s low-speed guards).
    //
    // `manifest` marks manifest-repo traffic - the default transport ignores
    // it; a web transport may key its cache on it.
    virtual vfs_transport_result fetch(
        const char *url,
        const char *tail,
        uint64_t expected_size,
        vfs_transport_sink sink,
        void *sink_context,
        uint32_t timeout_ms,
        bool manifest) = 0;

    // byte-range GET of [offset, offset + length) of the resource (MUL
    // range streaming natively, web granule requests). Unsupported by default:
    // `ok` stays false with status 0 and callers keep their whole-fetch
    // fallback. `#if defined(__EMSCRIPTEN__)` hook: the web transport
    // implements granule range reads here.
    virtual vfs_transport_result range(
        const char *url,
        const char *tail,
        uint64_t offset,
        uint64_t length,
        vfs_transport_sink sink,
        void *sink_context,
        uint32_t timeout_ms,
        bool manifest);

    // best-effort abort of in-flight transfers (engine shutdown). The curl
    // transport is synchronous and has nothing to cancel; a web transport
    // aborts its async fetches here.
    virtual void cancel() { (void)0; }

    virtual ~vfs_transport() = default;
};

// the transport the fetch engine uses. `vfs_transport_set(nullptr)` restores
// the built-in curl transport. The pointer is NOT owned by the module: the
// setter keeps it alive for the engine's lifetime (tests own their fakes and
// restore null/unset afterwards). Returns the previous override (nullptr for
// the default), so callers can stack and unwind injections.
vfs_transport *vfs_transport_set(vfs_transport *transport);
// the active transport (the injected one, or the curl default; never null)
vfs_transport *vfs_transport_active();

// the built-in curl transport (process-wide singleton; per-thread handles)
vfs_transport *vfs_transport_default();
