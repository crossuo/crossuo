// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The map settings record: every field, default and the exact
// GlobalConfig/Map.ini key set, including the "other_sho_name" key spelling
// for the Other group's show-name flag (a format quirk both write/read
// directions share). In its own header so
// the config layer (and the script harness) can include the record without
// the radar window machinery.

#pragma once

#include "../core/global.h"

namespace xuoassist {

// The map settings record.
class CMapSettings
{
public:
    int WindowX = 0;
    int WindowY = 0;
    int WindowWidth = 0;
    int WindowHeight = 0;
    uint32_t Facet = 0xFFFFFFFF; // 0xFFFFFFFF = current client facet
    bool FollowPlayer = true;
    int Zoom = 2; // the original: index 1..16, scale = index * 0.25
    bool ShowStatics = true;
    bool ApplyAltitude = true;
    bool AltitudeOnly = false;
    bool SmoothImage = true; // menu-disabled in the original; kept for format parity
    int Opacity = 0; // 0=opaque, then 25%/50% steps (map.cpp:73)
    bool DisplayCoordinates = true;
    bool ShowOnLogin = false;
    bool ShowLastCorpse = true;
    bool MarkDeadMobiles = true;
    bool RotateMap = true;

    int PlayerFontStyle = 0;
    uint32_t PlayerColor = 0xFF00FFFF; // rgba(0, 255, 255)
    bool PlayerShowHits = true;
    bool PlayerShowMana = true;
    bool PlayerShowStam = true;
    bool PlayerShowName = true;

    bool PartyEnabled = true;
    int PartyFontStyle = 0;
    uint32_t PartyColor = 0xFF00FF00; // rgb(0, 255, 0)
    bool PartyShowHits = true;
    bool PartyShowName = true;

    bool GuildEnabled = true;
    int GuildFontStyle = 0;
    uint32_t GuildColor = 0xFF00C887; // rgb(135, 200, 0)
    bool GuildShowHits = true;
    bool GuildShowName = true;

    bool FriendsEnabled = true;
    int FriendsFontStyle = 0;
    uint32_t FriendsColor = 0xFFFFFF00; // rgb(255, 255, 0)
    bool FriendsShowHits = true;
    bool FriendsShowName = true;

    bool EnemiesEnabled = true;
    int EnemiesFontStyle = 0;
    uint32_t EnemiesColor = 0xFFFF0000; // rgb(255, 0, 0)
    bool EnemiesShowHits = true;
    bool EnemiesShowName = true;

    bool OtherEnabled = false;
    int OtherFontStyle = 0;
    uint32_t OtherColor = 0xFFC0C0C0; // rgb(192, 192, 192)
    bool OtherShowHits = true;
    bool OtherShowName = true;

    CMapSettings() = default;

    void Copy(const CMapSettings &settings);
    void Load();  // GlobalConfig/Map.ini
    void Save() const;

    // pointer accessors for the settings form (ui/mapsettings.cpp binds the
    // working copy's fields through these)
    int *WindowXPtr() { return &WindowX; }
    int *WindowYPtr() { return &WindowY; }
    int *WindowWidthPtr() { return &WindowWidth; }
    int *WindowHeightPtr() { return &WindowHeight; }
    uint32_t *FacetPtr() { return &Facet; }
    int *ZoomPtr() { return &Zoom; }
    int *OpacityPtr() { return &Opacity; }
#define XA_MAP_FIELD_PTR(name, type) type *name##Ptr() { return &name; }
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
    XA_MAP_FIELD_PTR(PlayerColor, uint32_t)
    XA_MAP_FIELD_PTR(PlayerShowHits, bool)
    XA_MAP_FIELD_PTR(PlayerShowMana, bool)
    XA_MAP_FIELD_PTR(PlayerShowStam, bool)
    XA_MAP_FIELD_PTR(PlayerShowName, bool)
    XA_MAP_FIELD_PTR(PartyEnabled, bool)
    XA_MAP_FIELD_PTR(PartyFontStyle, int)
    XA_MAP_FIELD_PTR(PartyColor, uint32_t)
    XA_MAP_FIELD_PTR(PartyShowHits, bool)
    XA_MAP_FIELD_PTR(PartyShowName, bool)
    XA_MAP_FIELD_PTR(GuildEnabled, bool)
    XA_MAP_FIELD_PTR(GuildFontStyle, int)
    XA_MAP_FIELD_PTR(GuildColor, uint32_t)
    XA_MAP_FIELD_PTR(GuildShowHits, bool)
    XA_MAP_FIELD_PTR(GuildShowName, bool)
    XA_MAP_FIELD_PTR(FriendsEnabled, bool)
    XA_MAP_FIELD_PTR(FriendsFontStyle, int)
    XA_MAP_FIELD_PTR(FriendsColor, uint32_t)
    XA_MAP_FIELD_PTR(FriendsShowHits, bool)
    XA_MAP_FIELD_PTR(FriendsShowName, bool)
    XA_MAP_FIELD_PTR(EnemiesEnabled, bool)
    XA_MAP_FIELD_PTR(EnemiesFontStyle, int)
    XA_MAP_FIELD_PTR(EnemiesColor, uint32_t)
    XA_MAP_FIELD_PTR(EnemiesShowHits, bool)
    XA_MAP_FIELD_PTR(EnemiesShowName, bool)
    XA_MAP_FIELD_PTR(OtherEnabled, bool)
    XA_MAP_FIELD_PTR(OtherFontStyle, int)
    XA_MAP_FIELD_PTR(OtherColor, uint32_t)
    XA_MAP_FIELD_PTR(OtherShowHits, bool)
    XA_MAP_FIELD_PTR(OtherShowName, bool)
#undef XA_MAP_FIELD_PTR
};

extern CMapSettings g_MapSettings;

} // namespace xuoassist
