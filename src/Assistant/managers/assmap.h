// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant map module: map settings (CMapSettings), overlay markers and
// the client-API-backed radar texture feeding ui/mapwindow.cpp.
//
// Data access: the client already has the map/static files mapped, patched
// and indexed (g_MapManager + g_MapSize/g_MapBlockSize), so this module reads
// RADAR cells through the client API instead of a second file mapping:
//   - g_MapManager.GetIndex(facet, bx, by) + the RADAR_MAP_BLOCK merge loop
//     (parameterized by facet: the client's own GetRadarMapBlock is pinned to
//     GetActualMap());
//   - the radar color pipeline of src/Gumps/GumpMinimap.cpp
//     (season graphic -> radarcol -> hue);
//   - g_MapSize / g_MapBlockSize for the per-facet dimensions.
// No .mul/.uop is read twice; multis are drawn from live world data (no
// multimap.rle use).

#pragma once

#include "../core/world.h" // ::CGameCharacter (the client's map mobile)

#include "../core/global.h"
#include "mapsettings.h"
#include <external/gfx/imgui/imgui.h>

#if defined(XUO_ASSISTANT)

namespace xuoassist {

// Map character types (names kept for parity)
enum CLIENT_MAP_CHARACTER_TYPE
{
    OMCT_PLAYER = 0,
    OMCT_PARTY_MEMBER,
    OMCT_GUILD_MEMBER,
    OMCT_FRIEND,
    OMCT_ENEMY,
    OMCT_OTHER,
    OMCT_CORPSE,
    OMCT_INVALID
};

// The context-menu action ids (the ImGui menu reuses them)
enum CLIENT_MAP_ACTION
{
    OMA_FACET = 1,
    OMA_FOLLOW_CHARACTER,
    OMA_ZOOM,
    OMA_SHOW_STATICS,
    OMA_APPLY_ALTITUDE,
    OMA_ALTITUDE_ONLY,
    OMA_SMOOTH_IMAGE,
    OMA_TRANSPARENT_WINDOW,
    OMA_DISPLAY_COORDINATES,
    OMA_SHOW_ON_LOGIN,
    OMA_OPEN_SETTINGS,
    OMA_ROTATE_MAP
};

// One map overlay marker: the character pointer
// stays valid for the frame it was collected on.
struct XA_MAP_DRAW_OBJECT
{
    CLIENT_MAP_CHARACTER_TYPE Type{ OMCT_INVALID };
    int X{ 0 };
    int Y{ 0 };
    astr_t Name{ "" };
    CGameCharacter *Ptr{ nullptr };
};


extern const char *g_FacetName[6];

// The draw scale from the stored zoom index:
// scale = zoom * 0.25, clamped to [0.25 .. 4.0].
float MapSettings_Scale();

// The character-type classification with the friends/enemies lists read
// from the config module.
CLIENT_MAP_CHARACTER_TYPE AssMap_GetCharacterType(const uint32_t &serial);

// The view the radar texture is built for. CX/CY = world coordinates at the
// window center (the panned/followed position).
struct XA_MAP_VIEW
{
    int Map{ 0 };
    int CX{ 0 };
    int CY{ 0 };
    float Scale{ 1.0f };
    bool Rotated{ false };
};

// --- radar texture (client-API backed) ---------------------------------------
//
// AssMap_RadarTexture returns the shared ImTextureRef for the given view,
// rebuilding the CPU pixels when the (view, settings) content hash changed
// and the throttle (>= 150 ms) allows.
// Draw it with ImGui::Image / draw list AddImage; it is invalid until the
// client map data is loaded (return value may be a null ref).
//
// Texture lifecycle notes:
//   - create/upload run on the main thread from the map window draw;
//   - the texture stays registered with RefCount == 1 so ui_shutdown's
//     backend teardown reclaims the GL name;
//   - AssMap_AfterImGuiShutdown frees the CPU pixels AFTER the context is
//     gone (called from AssistantWindow next to Icons_Shutdown).
ImTextureRef AssMap_RadarTexture(int width, int height, const XA_MAP_VIEW &view);

// Marks the cached pixels stale (the next texture call rebuilds). Used on
// facet/statics/altitude changes even when the content hash would agree.
void AssMap_Invalidate();

// While the ImGui context is alive (shutdown ordering).
void AssMap_Shutdown();
// Frees the CPU-side pixels; AFTER ui_shutdown/DestroyContext.
void AssMap_AfterImGuiShutdown();

} // namespace xuoassist

#endif // XUO_ASSISTANT
