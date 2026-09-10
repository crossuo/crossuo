// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// Shared local fixture server harness (tiny python http server with Range
// support + per-path hit counters). Used by the fetch tests and the
// update tests; see http_server.py.

#pragma once

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "test_main.h"

#include <common/fs.h>
#include <external/process.h>
#include <xuocore/http.h>
#include <xuocore/vfs.h>

#ifndef VFS_SERVER_SCRIPT
#define VFS_SERVER_SCRIPT "tests/vfs/http_server.py"
#endif

// keeps the vfs module state clean even when an assertion returns early: the
// background tier and the fetch workers must not leak into the next test
struct test_vfs_shutdown
{
    ~test_vfs_shutdown() { vfs_shutdown(); }
};

// mutes the vfs log for the noise-heavy network paths
struct fetch_log_mute
{
    eLogSystem saved;
    fetch_log_mute()
        : saved(g_LogEnabled)
    {
        g_LogEnabled = eLogSystem::LogSystemNone;
    }
    ~fetch_log_mute() { g_LogEnabled = saved; }
};

struct test_server
{
    process_s process = {};
    bool running = false;
    int port = 0;
};

// `extra` (optional) is appended verbatim to the server command line, e.g.
// "--coop-coep" for the cross-origin-isolation tests
static inline bool
test_server_start_ex(test_server &srv, const fs_path &root, bool no_range, const char *extra);

static inline bool test_server_start(test_server &srv, const fs_path &root, bool no_range)
{
    return test_server_start_ex(srv, root, no_range, nullptr);
}

static inline bool
test_server_start_ex(test_server &srv, const fs_path &root, bool no_range, const char *extra)
{
    char root_arg[FS_MAX_PATH];
    snprintf(root_arg, sizeof(root_arg), "%s", fs_path_ascii(root));
    char no_range_arg[] = "--no-range";

    for (const char *python : { "python3", "python" })
    {
        // execvp stops at the first NULL element: the optional arguments are
        // packed in order, without holes
        const char *argv[8];
        int argc = 0;
        argv[argc++] = python;
        argv[argc++] = VFS_SERVER_SCRIPT;
        argv[argc++] = "--root";
        argv[argc++] = root_arg;
        if (no_range)
            argv[argc++] = no_range_arg;
        if (extra)
            argv[argc++] = extra;
        argv[argc++] = nullptr;
        memset(&srv.process, 0, sizeof(srv.process));
        if (process_create(argv, process_option_inherit_environment, &srv.process) != 0)
        {
            continue; // interpreter not present: try the next name
        }
        char line[128] = {};
        if (!fgets(line, sizeof(line), process_stdout(&srv.process)))
        {
            process_destroy(&srv.process);
            continue;
        }
        if (sscanf(line, "port %d", &srv.port) != 1 || srv.port <= 0)
        {
            process_destroy(&srv.process);
            continue;
        }
        srv.running = true;
        return true;
    }
    vfs_test_fail(__FILE__, __LINE__, "could not start the fixture server (python3/python)");
    return false;
}

static inline std::string test_server_url(const test_server &srv, const char *path)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "http://127.0.0.1:%d%s", srv.port, path);
    return std::string(buf);
}

static inline void test_server_stop(test_server &srv)
{
    if (!srv.running)
        return;
    const auto url = test_server_url(srv, "/shutdown");
    std::vector<uint8_t> data;
    http_get_binary(url.c_str(), data);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    // closes the pipes; closing stdin also shuts the server down
    process_destroy(&srv.process);
    srv.running = false;
}

// GET /hit?path=... -> plain per-path request count
static inline uint64_t test_server_hits(test_server &srv, const char *path)
{
    char url[256];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/hit?path=%s", srv.port, path);
    std::string data;
    if (!http_get_string(url, data))
        return uint64_t(-1);
    return strtoull(data.c_str(), nullptr, 10);
}

// total requests served, from the /hits JSON summary
static inline uint64_t test_server_total_hits(test_server &srv)
{
    const auto url = test_server_url(srv, "/hits");
    std::string data;
    if (!http_get_string(url.c_str(), data))
        return uint64_t(-1);
    const size_t at = data.find("\"total\":");
    if (at == std::string::npos)
        return uint64_t(-1);
    return strtoull(data.c_str() + at + 8, nullptr, 10);
}
