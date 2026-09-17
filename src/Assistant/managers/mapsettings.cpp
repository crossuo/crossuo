// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// CMapSettings persistence (GlobalConfig/Map.ini), extracted from assmap.cpp
// so the config-layer code does not sit in the radar/sampler translation
// unit. Single-record INI file: one [Map] section, keys named like the
// former Map.ini attributes.

#include "mapsettings.h"
#include "../config/configio.h"
#include "../config/assistantconfig.h" // GetGlobalConfigPath
#include "../core/assist.h"            // RawStringToBool/BoolToText
#include "../core/textconv.h"          // TextToSerial/SerialToText

#include <cstdio>

namespace xuoassist {

namespace {

// Map.ini is a single-record file: one section, keys named like the former
// Map.ini attributes.
const char *XA_MAP_INI_SECTION = "Map";

// Attribute helpers over the shared INI document (keys are read only when
// present, so a partial file falls back to the in-memory defaults).
void load_bool(const CIniFile &ini, const char *name, bool *value)
{
    if (ini.Has(XA_MAP_INI_SECTION, name))
    {
        *value = CAssistant::RawStringToBool(ini.Get(XA_MAP_INI_SECTION, name));
    }
}

void load_int(const CIniFile &ini, const char *name, int *value)
{
    if (ini.Has(XA_MAP_INI_SECTION, name))
    {
        *value = ini.GetInt(XA_MAP_INI_SECTION, name, 0);
    }
}

// Packed RGBA text form (the ARGB uint32_t round-trips through
// CAssistant::SerialToText/TextToSerial)
void load_color(const CIniFile &ini, const char *name, uint32_t *value)
{
    if (ini.Has(XA_MAP_INI_SECTION, name))
    {
        *value = TextToSerial(ini.Get(XA_MAP_INI_SECTION, name));
    }
}

void set_bool(CIniFile &ini, const char *name, bool value)
{
    ini.Set(XA_MAP_INI_SECTION, name, CAssistant::BoolToText(value));
}

void set_int(CIniFile &ini, const char *name, int value)
{
    ini.SetInt(XA_MAP_INI_SECTION, name, value);
}

void set_color(CIniFile &ini, const char *name, uint32_t value)
{
    ini.Set(XA_MAP_INI_SECTION, name, CAssistant::SerialToText(value));
}

} // namespace

void CMapSettings::Load()
{
    const fs_path path = fs_path_join(CAssistantConfig::GetGlobalConfigPath(), "Map.ini");
    CIniFile ini;
    if (!ini.Load(path))
    {
        return; // absent file = defaults (the original code silently kept defaults too)
    }

    load_int(ini, "window_x", &WindowX);
    load_int(ini, "window_y", &WindowY);
    load_int(ini, "window_width", &WindowWidth);
    load_int(ini, "window_height", &WindowHeight);

    load_bool(ini, "show_statics", &ShowStatics);
    load_bool(ini, "apply_altitude", &ApplyAltitude);
    load_bool(ini, "altitude_only", &AltitudeOnly);
    load_bool(ini, "smooth_image", &SmoothImage);
    load_bool(ini, "display_coordinates", &DisplayCoordinates);
    load_bool(ini, "show_on_login", &ShowOnLogin);
    load_bool(ini, "show_last_corpse", &ShowLastCorpse);
    load_bool(ini, "mark_dead_mobiles", &MarkDeadMobiles);
    load_int(ini, "opacity", &Opacity);
    load_bool(ini, "rotate_map", &RotateMap);

    load_int(ini, "player_font_style", &PlayerFontStyle);
    load_color(ini, "player_color", &PlayerColor);
    load_bool(ini, "player_show_hits", &PlayerShowHits);
    load_bool(ini, "player_show_mana", &PlayerShowMana);
    load_bool(ini, "player_show_stam", &PlayerShowStam);
    load_bool(ini, "player_show_name", &PlayerShowName);

    load_bool(ini, "party_enabled", &PartyEnabled);
    load_int(ini, "party_font_style", &PartyFontStyle);
    load_color(ini, "party_color", &PartyColor);
    load_bool(ini, "party_show_hits", &PartyShowHits);
    load_bool(ini, "party_show_name", &PartyShowName);

    load_bool(ini, "guild_enabled", &GuildEnabled);
    load_int(ini, "guild_font_style", &GuildFontStyle);
    load_color(ini, "guild_color", &GuildColor);
    load_bool(ini, "guild_show_hits", &GuildShowHits);
    load_bool(ini, "guild_show_name", &GuildShowName);

    load_bool(ini, "friends_enabled", &FriendsEnabled);
    load_int(ini, "friends_font_style", &FriendsFontStyle);
    load_color(ini, "friends_color", &FriendsColor);
    load_bool(ini, "friends_show_hits", &FriendsShowHits);
    load_bool(ini, "friends_show_name", &FriendsShowName);

    load_bool(ini, "enemies_enabled", &EnemiesEnabled);
    load_int(ini, "enemies_font_style", &EnemiesFontStyle);
    load_color(ini, "enemies_color", &EnemiesColor);
    load_bool(ini, "enemies_show_hits", &EnemiesShowHits);
    load_bool(ini, "enemies_show_name", &EnemiesShowName);

    load_bool(ini, "other_enabled", &OtherEnabled);
    load_int(ini, "other_font_style", &OtherFontStyle);
    load_color(ini, "other_color", &OtherColor);
    load_bool(ini, "other_show_hits", &OtherShowHits);
    // Format quirk: this key is spelled
    // "other_sho_name" in BOTH write/read directions.
    load_bool(ini, "other_sho_name", &OtherShowName);
}

void CMapSettings::Save()
{
    CIniFile ini;

    set_int(ini, "window_x", WindowX);
    set_int(ini, "window_y", WindowY);
    set_int(ini, "window_width", WindowWidth);
    set_int(ini, "window_height", WindowHeight);

    set_bool(ini, "show_statics", ShowStatics);
    set_bool(ini, "apply_altitude", ApplyAltitude);
    set_bool(ini, "altitude_only", AltitudeOnly);
    set_bool(ini, "smooth_image", SmoothImage);
    set_bool(ini, "display_coordinates", DisplayCoordinates);
    set_bool(ini, "show_on_login", ShowOnLogin);
    set_bool(ini, "show_last_corpse", ShowLastCorpse);
    set_bool(ini, "mark_dead_mobiles", MarkDeadMobiles);
    set_int(ini, "opacity", Opacity);
    set_bool(ini, "rotate_map", RotateMap);

    set_int(ini, "player_font_style", PlayerFontStyle);
    set_color(ini, "player_color", PlayerColor);
    set_bool(ini, "player_show_hits", PlayerShowHits);
    set_bool(ini, "player_show_mana", PlayerShowMana);
    set_bool(ini, "player_show_stam", PlayerShowStam);
    set_bool(ini, "player_show_name", PlayerShowName);

    set_bool(ini, "party_enabled", PartyEnabled);
    set_int(ini, "party_font_style", PartyFontStyle);
    set_color(ini, "party_color", PartyColor);
    set_bool(ini, "party_show_hits", PartyShowHits);
    set_bool(ini, "party_show_name", PartyShowName);

    set_bool(ini, "guild_enabled", GuildEnabled);
    set_int(ini, "guild_font_style", GuildFontStyle);
    set_color(ini, "guild_color", GuildColor);
    set_bool(ini, "guild_show_hits", GuildShowHits);
    set_bool(ini, "guild_show_name", GuildShowName);

    set_bool(ini, "friends_enabled", FriendsEnabled);
    set_int(ini, "friends_font_style", FriendsFontStyle);
    set_color(ini, "friends_color", FriendsColor);
    set_bool(ini, "friends_show_hits", FriendsShowHits);
    set_bool(ini, "friends_show_name", FriendsShowName);

    set_bool(ini, "enemies_enabled", EnemiesEnabled);
    set_int(ini, "enemies_font_style", EnemiesFontStyle);
    set_color(ini, "enemies_color", EnemiesColor);
    set_bool(ini, "enemies_show_hits", EnemiesShowHits);
    set_bool(ini, "enemies_show_name", EnemiesShowName);

    set_bool(ini, "other_enabled", OtherEnabled);
    set_int(ini, "other_font_style", OtherFontStyle);
    set_color(ini, "other_color", OtherColor);
    set_bool(ini, "other_show_hits", OtherShowHits);
    set_bool(ini, "other_sho_name", OtherShowName); // format quirk, see Load

    ini.Save(fs_path_join(CAssistantConfig::GetGlobalConfigPath(), "Map.ini"));
}

} // namespace xuoassist
