// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// CVfsStatusHud: the streamed-install download HUD. A minor, non-intrusive
// one-line indicator in the top-right corner: download activity (rate +
// done/total), placeholders-in-use warning and the offline badge. It fades
// out after a short idle time and costs nothing when streaming is disabled
// (one flag check per frame).
//
// This is deliberately NOT a gump and is not registered in the gump manager:
// the HUD is passive, non-interactive and visible on every screen (login
// through game), so each screen draws it directly at the end of its Render,
// right before the frame is presented. Always the main thread, never the
// fetch/demand workers (vfs_hud_stats copies the stats under the vfs state
// lock).

#pragma once

class CVfsStatusHud
{
public:
    // per-frame draw from a screen's Render (main thread only), before the
    // frame is presented
    static void Draw();
};
