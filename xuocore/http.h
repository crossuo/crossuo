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
// they did not ask for.
bool http_get_range(
    const char *url, const char *agent, uint64_t offset, uint64_t length, http_response *out);

void http_init();
void http_shutdown();
void http_set_user_agent(const char *name);
bool http_get_binary(const char *url, const uint8_t *buf, size_t *size);
bool http_get_binary(const char *url, std::vector<uint8_t> &data);
bool http_get_string(const char *url, std::string &data);
bool http_get_file(const char *url, const char *filename);
std::string http_urlencode(std::string str);