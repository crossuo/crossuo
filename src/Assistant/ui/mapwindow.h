// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant Map window (was assist/map.cpp Map + gui/mapimagewidget.cpp,
// which only forwarded events): one dockable/poppable ImGui window drawing
// the radar texture from managers/assmap.cpp with the draw-object overlays,
// the context menu and the zoom/rotate/follow/pan input.
//
// Coordinate transforms (derived once from the original code, both directions):
//
//   scale  s = zoom * 0.25 in [0.25 .. 4.0]      (Map::UpdateScale)
//   half   = (width/2, height/2)                 window content center
//   d      = world - center                      (wx-cx, wy-cy)
//
//   unrotated:
//       screen = half + d / s                    world->screen
//       world  = center + (screen - half) * s    screen->world
//
//   rotated 45deg isometric basis u=d.x-d.y, v=d.x+d.y:
//       screen = half + (u, v) / s
//       inverse: d = (screen-half) * s;
//                wx = cx + (dx+dy)/2, wy = cy + (dy-dx)/2
//   the drag pan subtracts the same inverse displacement from the center,
//   c' = c - (s(dx+dy)/2, s(dy-dx)/2), so the world point under the cursor
//   follows it exactly (the original 0.707107 pair approximated this step)
//
// The window is viewport-safe: every position comes from ImGui screen-space
// queries (GetCursorScreenPos), never from the main window geometry, so it
// behaves identically docked and popped out (ADR D11). Mouse input is
// consumed through the canvas item (io.WantCaptureMouse covers it before the
// game sees anything), so panning cannot walk the player.

#pragma once

#include "../core/global.h"

#if defined(XUO_ASSISTANT)

namespace xuoassist {

// The window body (assistantui.cpp draws it while the assistant is visible).
// open: visibility flag (the window X button flips it).
void MapWindow_Draw(bool *open);

// TabMain::OpenMap / OIPMT_OPEN_MAP / ShowOnLogin.
void MapWindow_Open();

// The window visibility flag (assistantui.cpp draws the window while the
// assistant is visible, gated by this).
extern bool g_AssistantMapOpen;

// The original Map::OnActionSelect: the context-menu transition.
void MapWindow_OnAction(uint32_t type, uint32_t extra);

// Shutdown (context alive): persists the window geometry into g_MapSettings
// and clears the per-session state (follow center, tracker, overlays).
void MapWindow_Shutdown();

// --- events (assistanteventsui.cpp phase-5 consumers) ------------------------
// OnMapUpdate(serial) / OnMapRedraw(): the overlays are recomputed per frame
// from the mirror, so the window only records dirtiness.
void MapWindow_OnMapUpdate();
void MapWindow_OnMapRedraw();
// OnTrack(state, x, y) - was the UOAM external tracker notify; the assistant
// map draws the point itself now (documented deviation).
void MapWindow_OnTrack(bool state, int x, int y);

// The map settings window (ui/mapsettings.cpp; the "Open display settings"
// context-menu item).
void MapSettings_Open();
void MapSettings_Draw();
void MapSettings_Shutdown();

} // namespace xuoassist

#endif // XUO_ASSISTANT
