// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant Map window: one dockable/poppable ImGui window drawing the
// radar texture from managers/assmap.cpp with the draw-object overlays, the
// context menu and the zoom/rotate/follow/pan input.
// Coordinate transforms (both directions):

#if defined(XUO_ASSISTANT)

#include "../core/global.h"


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
// OnTrack(state, x, y) - the external tracker point; the assistant
void MapWindow_OnTrack(bool state, int x, int y);

// The map settings window (ui/mapsettings.cpp; the "Open display settings"
// context-menu item).
void MapSettings_Open();
void MapSettings_Draw();
void MapSettings_Shutdown();

} // namespace xuoassist

#endif // XUO_ASSISTANT
