// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// GumpVfsStatus: the streamed-install download HUD (VUOI plan §11, resolved
// decision #3). A minor, non-intrusive one-line indicator in the top-right
// corner: download activity (rate + done/total), placeholders-in-use warning
// and the offline badge. It fades out after a short idle time and costs
// nothing when streaming is disabled (one flag check per frame).
//
// This is deliberately NOT registered in the gump manager: the HUD is
// passive, non-interactive and visible on every screen (login through game),
// so it draws directly from the per-frame hook in CGame::Process - always on
// the main thread, never from the fetch/demand workers (vfs_hud_stats copies
// the stats under the vfs state lock).

#pragma once

class CGumpVfsStatus
{
public:
    // per-frame draw from the game loop (main thread only)
    static void Draw();
};
