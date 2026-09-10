// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_record: CSV appender for the deployment asset-access recorder. See
// vfs_record.h.

#define LOGGER_MODULE Vfs

#include "vfs_record.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <mutex>

#include "common.h"
#include <common/logging/logging.h>

static std::mutex s_mutex;
static FILE *s_file = nullptr;
static fs_path s_path;
static astr_t s_scenario = "default";
static std::chrono::steady_clock::time_point s_started;

void vfs_record_configure(const fs_path &path, const char *scenario)
{
    std::unique_lock<std::mutex> lock(s_mutex);
    if (fs_path_empty(path))
        return;
    s_path = path;
    s_scenario = scenario && *scenario ? scenario : "default";
    s_started = std::chrono::steady_clock::now();
    // a recording run always produces its (possibly header-only) CSV
    fs_path_create(fs_directory(s_path));
    s_file = fopen(fs_path_ascii(s_path), "wb");
    if (!s_file)
    {
        LOG_ERROR("could not open record file: %s", fs_path_ascii(s_path));
        s_path = fs_path{};
        return;
    }
    fprintf(s_file, "hash,pack,ul,tier_hint,first_access_ms,scenario\n");
}

bool vfs_record_enabled()
{
    std::unique_lock<std::mutex> lock(s_mutex);
    return !fs_path_empty(s_path);
}

// RFC4180: quote fields containing comma, quote or newline, doubling embedded
// quotes. Pack names come from the product manifest, so they are treated as
// untrusted input; plain fields are written as-is (the header stays bare).
static void vfs_record_field(FILE *fp, const char *s)
{
    if (!strpbrk(s, ",\"\r\n"))
    {
        fputs(s, fp);
        return;
    }
    fputc('"', fp);
    for (const char *p = s; *p; ++p)
    {
        if (*p == '"')
            fputc('"', fp);
        fputc(*p, fp);
    }
    fputc('"', fp);
}

void vfs_record_asset(uint64_t uop_hash, const astr_t &pack, uint64_t ul, uint8_t tier_hint)
{
    std::unique_lock<std::mutex> lock(s_mutex);
    if (fs_path_empty(s_path) || !s_file)
        return;
    const uint64_t ms = uint64_t(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - s_started)
            .count());
    fprintf(s_file, "%016" PRIx64 ",", uop_hash);
    vfs_record_field(s_file, pack.c_str());
    fprintf(s_file, ",%" PRIu64 ",%u,%" PRIu64 ",", ul, tier_hint, ms);
    vfs_record_field(s_file, s_scenario.c_str());
    fputc('\n', s_file);
}

void vfs_record_close()
{
    std::unique_lock<std::mutex> lock(s_mutex);
    if (s_file)
    {
        fflush(s_file);
        fclose(s_file);
        s_file = nullptr;
    }
    s_path = fs_path{};
}
