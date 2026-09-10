// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// GumpVfsStatus implementation. See the header for the drawing contract.
// State kept across frames: one stats copy (refreshed a few times a second,
// never per frame), the last activity tick and the previous byte count for
// the rate estimate. Nothing is drawn from a non-main thread: the fetch and
// demand workers only touch vfs internals, which vfs_hud_stats copies under
// the vfs state lock.

#include "GumpVfsStatus.h"

#include <stdio.h>
#include <string.h>

#include <xuocore/vfs.h>
#include <xuocore/vfs_fetch.h>

#include "../GameWindow.h"
#include "../Globals.h"
#include "../Managers/FontsManager.h"

namespace
{
constexpr uint8_t VFS_HUD_FONT = 3;
constexpr uint16_t VFS_HUD_COLOR = 0x0386;         // restful blue-gray
constexpr uint16_t VFS_HUD_COLOR_WARN = 0x0034;    // placeholders in use
constexpr uint16_t VFS_HUD_COLOR_OFFLINE = 0x0028; // offline badge
constexpr int VFS_HUD_MARGIN = 6;
constexpr int VFS_HUD_REFRESH_MS = 250; // stats poll period
constexpr int VFS_HUD_IDLE_MS = 2000;   // fade-out delay after activity

// human-readable byte count ("1.2 MB"); small fixed buffer, called at the
// poll period only
void vfs_hud_size(uint64_t bytes, char *out, size_t cap)
{
    if (bytes >= uint64_t(1024) * 1024 * 1024)
        snprintf(out, cap, "%.1f GB", double(bytes) / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024 * 1024)
        snprintf(out, cap, "%.1f MB", double(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024)
        snprintf(out, cap, "%.0f KB", double(bytes) / 1024.0);
    else
        snprintf(out, cap, "%llu B", (unsigned long long)bytes);
}
} // namespace

void CGumpVfsStatus::Draw()
{
    if (!vfs_enabled())
        return; // zero cost when streaming is off

    static VfsStats s_stats;
    static uint64_t s_last_poll = 0;
    static uint64_t s_last_activity = 0;
    static uint64_t s_prev_bytes = 0;
    static uint64_t s_bytes_per_sec = 0;

    const uint64_t now = g_Ticks;
    if (s_last_poll == 0 || s_last_poll + VFS_HUD_REFRESH_MS <= now)
    {
        vfs_hud_stats(&s_stats);
        vfs_fetch_stats fetch_stats;
        vfs_fetch_stats_get(&fetch_stats);
        // rate: bytes landed since the previous poll, smoothed towards 1s
        const uint64_t delta =
            s_stats.bytes_done > s_prev_bytes ? s_stats.bytes_done - s_prev_bytes : 0;
        s_bytes_per_sec = (s_bytes_per_sec * 3 + delta * (1000 / VFS_HUD_REFRESH_MS)) / 4;
        s_prev_bytes = s_stats.bytes_done;

        // activity: requests in flight, bytes landing, or placeholders held
        if (fetch_stats.inflight > 0 || delta > 0 || s_stats.stubs_active > 0)
            s_last_activity = now;
        s_last_poll = now;
    }

    // hidden entirely after the idle period (boot progress has its own path)
    if (s_last_activity == 0 || s_last_activity + VFS_HUD_IDLE_MS < now)
        return;

    const bool offline = vfs_get_params().offline;
    char text[192] = {};
    uint16_t color = VFS_HUD_COLOR;
    if (s_stats.stubs_active > 0)
    {
        // warn while part of the world still lives on placeholders
        snprintf(
            text, sizeof(text), "placeholders: %llu", (unsigned long long)s_stats.stubs_active);
        color = VFS_HUD_COLOR_WARN;
    }
    else if (s_stats.bytes_total > 0 && s_stats.bytes_done < s_stats.bytes_total)
    {
        const int percent = int((s_stats.bytes_done * 100) / s_stats.bytes_total);
        if (s_bytes_per_sec > 0)
        {
            char rate[32];
            char total[32];
            vfs_hud_size(s_bytes_per_sec, rate, sizeof(rate));
            vfs_hud_size(s_stats.bytes_total, total, sizeof(total));
            snprintf(text, sizeof(text), "streaming %d%% (%s/s of %s)", percent, rate, total);
        }
        else
        {
            snprintf(text, sizeof(text), "streaming %d%%", percent);
        }
    }
    else if (!offline)
    {
        return; // complete and idle: nothing to say
    }
    if (offline)
    {
        // the offline badge rides along with whatever else is shown
        const size_t used = strlen(text);
        snprintf(text + used, sizeof(text) - used, "%soffline", used != 0 ? " | " : "");
        if (used == 0)
            color = VFS_HUD_COLOR_OFFLINE;
    }

    const int width = g_FontManager.GetWidthA(VFS_HUD_FONT, text);
    const auto size = g_GameWindow.GetSize();
    g_FontManager.DrawA(
        VFS_HUD_FONT, text, color, size.Width - width - VFS_HUD_MARGIN, VFS_HUD_MARGIN);
}
