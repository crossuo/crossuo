// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

// response of the extended GET helpers below: status is the HTTP code (0
// when the transfer failed at the transport level), retry_after carries the
// parsed Retry-After header in seconds (-1 when the server sent none), body
// holds the payload.
struct http_response
{
    long status = 0;
    long retry_after = -1;
    std::vector<uint8_t> body;
};

// GET with the standard guards (10s connect, 30s low-speed, redirects
// followed) and a hard body cap: transfers exceeding it are aborted and
// reported as failed (0: unbounded). `agent` overrides the global user
// agent when non-null. Body is delivered on any completed exchange; check
// `status` for 200 before trusting it.
bool http_get(const char *url, const char *agent, size_t body_cap, http_response *out);

// ranged GET (bytes=offset-(offset+length-1)); the body is only delivered
// when the server honors the range (206 + exactly length bytes) - a 200
// answer is aborted and reported failed, so callers never receive megabytes
// they did not ask for. `out_total` (optional) receives the resource total
// from the answer's Content-Range (-1 when unavailable) - the tail chunk of
// a length-clamped stream learns the real size this way.
bool http_get_range(
    const char *url,
    const char *agent,
    uint64_t offset,
    uint64_t length,
    http_response *out,
    int64_t *out_total = nullptr);

// ---------------------------------------------------------------------------
// web download accounting (wasm): the emscripten XHR transport counts every
// successful (HTTP 200/206) response body it delivers into one process-wide
// atomic pair. http_xhr_get is the single chokepoint ALL web traffic shares -
// transport fetch/range (loose blobs, granule ranges, stream-file chunks,
// heals, the fetch engine), the manifest getters, the chunked assembler - so
// everything is counted exactly once, with no per-callsite bookkeeping. Each
// distinct (url, range) target counts once per session: the browser http
// cache re-serves identical bodies with no new wire traffic, and retry or
// re-walk cycles re-request the same target, so re-counting repeats would
// unmoor the counter from reality (multi-GB "streamed" values while the
// network carried megabytes). A ranged request answered 200 (server ignored
// the range) is never counted: http_get_range rejects that body outright.
// The chunked transfer's bytes=0-0 probe contributes its 1 byte; the
// assembled 8MB ranges are the body itself. `vfs_web_bytes_expect` grows the
// denominator as the plan and the streaming lanes learn sizes (vfs.cpp
// registers them) - also once per target, so the done/total ratio stays
// monotonic and bounded. Readers feed vfs_boot_web_progress() and the page
// shell (shell.html). The page cannot call the exported getters reliably: in
// PROXY_TO_PTHREAD builds the exports bind on the worker where the module
// instantiates. Instead `vfs_web_progress_addrs` hands the page the static
// pair's address and it reads the two i64 counters straight out of the
// shared heap (HEAPU64) - pthread builds share one heap across main +
// workers. Native builds carry no-op stubs (curl stays uncounted: the splash
// surfaces are web-only), so callers link everywhere and read zeros; the
// address getter answers 0 and the page treats that as "not available".
// ---------------------------------------------------------------------------
extern "C" {
uint64_t vfs_web_bytes_done(void);
uint64_t vfs_web_bytes_total(void);
void vfs_web_bytes_expect(uint64_t bytes);
uint32_t vfs_web_progress_addrs(void);
}

void http_init();
void http_shutdown();
void http_set_user_agent(const char *name);
bool http_get_binary(const char *url, const uint8_t *buf, size_t *size);
bool http_get_binary(const char *url, std::vector<uint8_t> &data);
bool http_get_string(const char *url, std::string &data);
bool http_get_file(const char *url, const char *filename);
std::string http_urlencode(std::string str);