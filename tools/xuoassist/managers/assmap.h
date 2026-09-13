// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant map module (was assist/mapsettingsform.h CMapSettings +
// assist/map.h CMapDrawObject, ported per the Phase 5 round; the radar
// rendering itself lives in ui/mapwindow.cpp).
//
// Dedup decisions (plan §4.4): the Qt app loaded map/static/multimap files
// through its own FileManager + mmap and kept a whole-facet 16-bit pixel
// buffer (map.cpp:220-440). The client already has the files mapped, patched
// and indexed (g_MapManager + g_MapSize/g_MapBlockSize), so this module reads
// RADAR cells through the client API instead:
//   - g_MapManager.GetIndex(facet, bx, by) + the RADAR_MAP_BLOCK merge loop
//     (port of src/Managers/MapManager.cpp:272-318, parameterized by facet:
//     the client's own GetRadarMapBlock is pinned to GetActualMap());
//   - the radar color pipeline of src/Gumps/GumpMinimap.cpp:186-193
//     (season graphic -> radarcol -> hue), which supersedes the Qt
//     FileManager::GetRadarcolData;
//   - g_MapSize / g_MapBlockSize for the per-facet dimensions.
// No .mul/.uop is read twice and the multimap (.rle) path is not ported (the
// client draws multis from live world data; Qt used multimap.rle only as a
// fallback for external UOAM hosting).

#ifndef XUOASSIST_ASSMAP_H
#define XUOASSIST_ASSMAP_H

#include "../core/global.h"
#include <external/gfx/imgui/imgui.h>

#if defined(XUO_ASSISTANT)

namespace xuoassist {

// Qt map.h:18-28 (names kept for parity)
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

// Qt map.h:30-44: the context-menu action ids (the ImGui menu reuses them)
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

// One map overlay marker (Qt CMapDrawObject, map.h:53-75; QString -> string,
// the CGameCharacter* stays a mirror pointer valid for the frame only).
struct XA_MAP_DRAW_OBJECT
{
    CLIENT_MAP_CHARACTER_TYPE Type{ OMCT_INVALID };
    int X{ 0 };
    int Y{ 0 };
    string Name{ "" };
    class CGameCharacter *Ptr{ nullptr };
};

// The map settings record (Qt CMapSettings, mapsettingsform.h:33-125): every
// field, default and the exact GlobalConfig/Map.xml attribute set are kept
// (CMapSettings::Load/Save, mapsettingsform.cpp:72-434), including the Qt
// writer's "other_sho_name" attribute spelling for the Other group's
// show-name flag (a format quirk both Qt directions shared - kept so the
// file stays byte-compatible).
class CMapSettings
{
    SETGET(int, WindowX, 0)
    SETGET(int, WindowY, 0)
    SETGET(int, WindowWidth, 0)
    SETGET(int, WindowHeight, 0)
    SETGET(uint, Facet, 0xFFFFFFFF) // 0xFFFFFFFF = current client facet
    SETGET(bool, FollowPlayer, true)
    SETGET(int, Zoom, 2) // Qt: index 1..16, scale = index * 0.25
    SETGET(bool, ShowStatics, true)
    SETGET(bool, ApplyAltitude, true)
    SETGET(bool, AltitudeOnly, false)
    SETGET(bool, SmoothImage, true) // menu-disabled in Qt; kept for format parity
    SETGET(int, Opacity, 0)         // 0=opaque, then 25%/50% steps (map.cpp:73)
    SETGET(bool, DisplayCoordinates, true)
    SETGET(bool, ShowOnLogin, false)
    SETGET(bool, ShowLastCorpse, true)
    SETGET(bool, MarkDeadMobiles, true)
    SETGET(bool, RotateMap, true)

    SETGET(int, PlayerFontStyle, 0)
    SETGET(uint, PlayerColor, 0xFF00FFFF)     // QColor(0, 255, 255).rgba()
    SETGET(bool, PlayerShowHits, true)
    SETGET(bool, PlayerShowMana, true)
    SETGET(bool, PlayerShowStam, true)
    SETGET(bool, PlayerShowName, true)

    SETGET(bool, PartyEnabled, true)
    SETGET(int, PartyFontStyle, 0)
    SETGET(uint, PartyColor, 0xFF00FF00)      // QColor(0, 255, 0)
    SETGET(bool, PartyShowHits, true)
    SETGET(bool, PartyShowName, true)

    SETGET(bool, GuildEnabled, true)
    SETGET(int, GuildFontStyle, 0)
    SETGET(uint, GuildColor, 0xFF00C887)      // QColor(135, 200, 0)
    SETGET(bool, GuildShowHits, true)
    SETGET(bool, GuildShowName, true)

    SETGET(bool, FriendsEnabled, true)
    SETGET(int, FriendsFontStyle, 0)
    SETGET(uint, FriendsColor, 0xFFFFFF00)    // QColor(255, 255, 0)
    SETGET(bool, FriendsShowHits, true)
    SETGET(bool, FriendsShowName, true)

    SETGET(bool, EnemiesEnabled, true)
    SETGET(int, EnemiesFontStyle, 0)
    SETGET(uint, EnemiesColor, 0xFFFF0000)    // QColor(255, 0, 0)
    SETGET(bool, EnemiesShowHits, true)
    SETGET(bool, EnemiesShowName, true)

    SETGET(bool, OtherEnabled, false)
    SETGET(int, OtherFontStyle, 0)
    SETGET(uint, OtherColor, 0xFFC0C0C0)      // QColor(192, 192, 192)
    SETGET(bool, OtherShowHits, true)
    SETGET(bool, OtherShowName, true)

public:
    CMapSettings() {}

    void Copy(const CMapSettings &settings);
    void Load();  // GlobalConfig/Map.xml (format unchanged, plan §4.3)
    void Save();

    // pointer accessors for the settings form (ui/mapsettings.cpp binds the
    // working copy's fields directly; the SETGET members stay protected)
    int *WindowXPtr() { return &m_WindowX; }
    int *WindowYPtr() { return &m_WindowY; }
    int *WindowWidthPtr() { return &m_WindowWidth; }
    int *WindowHeightPtr() { return &m_WindowHeight; }
    uint *FacetPtr() { return &m_Facet; }
    int *ZoomPtr() { return &m_Zoom; }
    int *OpacityPtr() { return &m_Opacity; }
#define XA_MAP_FIELD_PTR(name, type) type *name##Ptr() { return &m_##name; }
    XA_MAP_FIELD_PTR(FollowPlayer, bool)
    XA_MAP_FIELD_PTR(ShowStatics, bool)
    XA_MAP_FIELD_PTR(ApplyAltitude, bool)
    XA_MAP_FIELD_PTR(AltitudeOnly, bool)
    XA_MAP_FIELD_PTR(SmoothImage, bool)
    XA_MAP_FIELD_PTR(DisplayCoordinates, bool)
    XA_MAP_FIELD_PTR(ShowOnLogin, bool)
    XA_MAP_FIELD_PTR(ShowLastCorpse, bool)
    XA_MAP_FIELD_PTR(MarkDeadMobiles, bool)
    XA_MAP_FIELD_PTR(RotateMap, bool)
    XA_MAP_FIELD_PTR(PlayerFontStyle, int)
    XA_MAP_FIELD_PTR(PlayerColor, uint)
    XA_MAP_FIELD_PTR(PlayerShowHits, bool)
    XA_MAP_FIELD_PTR(PlayerShowMana, bool)
    XA_MAP_FIELD_PTR(PlayerShowStam, bool)
    XA_MAP_FIELD_PTR(PlayerShowName, bool)
    XA_MAP_FIELD_PTR(PartyEnabled, bool)
    XA_MAP_FIELD_PTR(PartyFontStyle, int)
    XA_MAP_FIELD_PTR(PartyColor, uint)
    XA_MAP_FIELD_PTR(PartyShowHits, bool)
    XA_MAP_FIELD_PTR(PartyShowName, bool)
    XA_MAP_FIELD_PTR(GuildEnabled, bool)
    XA_MAP_FIELD_PTR(GuildFontStyle, int)
    XA_MAP_FIELD_PTR(GuildColor, uint)
    XA_MAP_FIELD_PTR(GuildShowHits, bool)
    XA_MAP_FIELD_PTR(GuildShowName, bool)
    XA_MAP_FIELD_PTR(FriendsEnabled, bool)
    XA_MAP_FIELD_PTR(FriendsFontStyle, int)
    XA_MAP_FIELD_PTR(FriendsColor, uint)
    XA_MAP_FIELD_PTR(FriendsShowHits, bool)
    XA_MAP_FIELD_PTR(FriendsShowName, bool)
    XA_MAP_FIELD_PTR(EnemiesEnabled, bool)
    XA_MAP_FIELD_PTR(EnemiesFontStyle, int)
    XA_MAP_FIELD_PTR(EnemiesColor, uint)
    XA_MAP_FIELD_PTR(EnemiesShowHits, bool)
    XA_MAP_FIELD_PTR(EnemiesShowName, bool)
    XA_MAP_FIELD_PTR(OtherEnabled, bool)
    XA_MAP_FIELD_PTR(OtherFontStyle, int)
    XA_MAP_FIELD_PTR(OtherColor, uint)
    XA_MAP_FIELD_PTR(OtherShowHits, bool)
    XA_MAP_FIELD_PTR(OtherShowName, bool)
#undef XA_MAP_FIELD_PTR
};

extern CMapSettings g_MapSettings;

extern const char *g_FacetName[6];

// The draw scale from the stored zoom index (Qt Map::UpdateScale,
// map.cpp:1602-1614): scale = zoom * 0.25, clamped to [0.25 .. 4.0] (the Qt
// clamp mapped >2.0 onto 4.0 - preserved).
float MapSettings_Scale();

// Qt Map::GetCharacterType (map.cpp:189-218) with the friends/enemies lists
// read from the config module (was g_TabListsFriends/Enemies).
CLIENT_MAP_CHARACTER_TYPE AssMap_GetCharacterType(const uint &serial);

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

// --- radar texture (client-API backed; plan §5.6) ---------------------------
//
// AssMap_RadarTexture returns the shared ImTextureRef for the given view,
// rebuilding the CPU pixels when the (view, settings) content hash changed
// and the throttle (>= 150 ms, the Qt 100 ms redraw timer rhythm) allows.
// Draw it with ImGui::Image / draw list AddImage; it is invalid until the
// client map data is loaded (return value may be a null ref).
//
// Texture lifecycle (Phase 2 atlas lesson):
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
#endif // XUOASSIST_ASSMAP_H
