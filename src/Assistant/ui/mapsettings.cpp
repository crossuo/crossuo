// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The Map Settings window: the character-display groups (Player/Party/Guild/
// Friends/Other mobiles/Enemies with font-style combo + color + show flags)
// and the main settings toggles, editing a working CMapSettings copy and
// committing through Map::ChangeSettings semantics = g_MapSettings.Copy +
// Save.
// Deltas: the 10-entry font-style combo keeps the g_MapFontStyle names/
// indices (the persisted value stays meaningful) but renders with the single
// assistant UI font; the "Map example" pane is not reproduced.

#include "mapwindow.h"

#if defined(XUO_ASSISTANT)

#include <common/logging/logging.h>

#include "../config/assistantconfig.h"
#include "../managers/assmap.h"
#include "widgets.h"

namespace xuoassist {

namespace {

bool s_Open = false;
CMapSettings s_Settings; // the working copy (was MapSettingsForm::m_Settings)

constexpr int MAP_FONT_STYLES_COUNT = 10; // the original mapsettingsform.h count
const char *const FONT_STYLES[MAP_FONT_STYLES_COUNT] = { "Small 1", "Small 2", "Small 3",
                                                         "Small 4", "Small 5", "Big 1",
                                                         "Big 2",   "Big 3",   "Big 4",
                                                         "Big 5" };

ImU32 RgbaToCol(uint32_t rgba)
{
    return IM_COL32((rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF, (rgba >> 24) & 0xFF);
}

uint32_t ColToRgba(ImU32 col)
{
    return (((col & 0xFF000000u)) | (((col >> 16) & 0xFF) << 16) | (((col >> 8) & 0xFF) << 8) |
            (col & 0xFF));
}

bool StyleCombo(const char *id, int *value)
{
    // clamp: a hand-edited Map.ini may hold anything
    int index = (*value >= 0 && *value < MAP_FONT_STYLES_COUNT) ? *value : 0;
    bool changed = ComboGroup(id, &index, FONT_STYLES, MAP_FONT_STYLES_COUNT);
    if (changed)
    {
        *value = index;
    }
    return changed;
}

bool ColorField(const char *id, uint32_t *rgba)
{
    ImU32 col = RgbaToCol(*rgba);
    if (ColorSwatch(id, &col))
    {
        *rgba = ColToRgba(col);
        return true;
    }
    return false;
}

// one character-display group. The original gb_* boxes were checkable groupboxes:
// the checkbox gates the fields (children render disabled while unchecked,
// like a disabled groupbox).
void CharacterGroup(
    const char *label,
    bool *enabled,
    bool hasEnabled,
    int *fontStyle,
    uint32_t *color,
    bool *showName,
    bool *showHits)
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
    ImGui::PopStyleColor();

    if (hasEnabled)
    {
        ImGui::SameLine();
        ImGui::Checkbox(label, enabled);
    }

    const bool fieldsEnabled = !hasEnabled || *enabled;
    if (!fieldsEnabled)
    {
        ImGui::BeginDisabled();
    }
    ImGui::Indent();
    ImGui::TextUnformatted("Font style:");
    ImGui::SetNextItemWidth(120.0f);
    char id[64];
    snprintf(id, sizeof(id), "##%s_font", label);
    StyleCombo(id, fontStyle);
    ImGui::SameLine();
    ImGui::TextUnformatted("Color:");
    snprintf(id, sizeof(id), "##%s_color", label);
    ColorField(id, color);
    if (showName != nullptr)
    {
        snprintf(id, sizeof(id), "##%s_name", label);
        ImGui::Checkbox(id, showName);
        ImGui::SameLine();
        ImGui::TextUnformatted("Show name");
    }
    if (showHits != nullptr)
    {
        snprintf(id, sizeof(id), "##%s_hits", label);
        ImGui::Checkbox(id, showHits);
        ImGui::SameLine();
        ImGui::TextUnformatted("Show hits");
    }
    ImGui::Unindent();
    if (!fieldsEnabled)
    {
        ImGui::EndDisabled();
    }
}

} // namespace

void MapSettings_Open()
{
    s_Settings = g_MapSettings; // the working copy the dialog edits
    s_Open = true;
}

void MapSettings_Shutdown()
{
    // form teardown with a pending edit: it is discarded
    s_Open = false;
}

void MapSettings_Draw()
{
    if (!s_Open)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(590.0f, 630.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Map Settings", &s_Open))
    {
        ImGui::End();
        return;
    }

    ImGui::BeginChild("##map_settings_scroll", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()));

    if (ImGui::CollapsingHeader("Characters display settings:", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        // Player: never disabled, full Show status group (mapsettingsform.ui)
        CharacterGroup(
            "Player",
            nullptr,
            false,
            s_Settings.PlayerFontStylePtr(),
            s_Settings.PlayerColorPtr(),
            s_Settings.PlayerShowNamePtr(),
            s_Settings.PlayerShowHitsPtr());
        ImGui::TextUnformatted("Show status:");
        ImGui::SameLine();
        ImGui::Checkbox("Mana##player", s_Settings.PlayerShowManaPtr());
        ImGui::SameLine();
        ImGui::Checkbox("Stam##player", s_Settings.PlayerShowStamPtr());

        CharacterGroup(
            "Party members:",
            s_Settings.PartyEnabledPtr(),
            true,
            s_Settings.PartyFontStylePtr(),
            s_Settings.PartyColorPtr(),
            s_Settings.PartyShowNamePtr(),
            s_Settings.PartyShowHitsPtr());
        CharacterGroup(
            "Guild members:",
            s_Settings.GuildEnabledPtr(),
            true,
            s_Settings.GuildFontStylePtr(),
            s_Settings.GuildColorPtr(),
            s_Settings.GuildShowNamePtr(),
            s_Settings.GuildShowHitsPtr());
        CharacterGroup(
            "Friends",
            s_Settings.FriendsEnabledPtr(),
            true,
            s_Settings.FriendsFontStylePtr(),
            s_Settings.FriendsColorPtr(),
            s_Settings.FriendsShowNamePtr(),
            s_Settings.FriendsShowHitsPtr());
        CharacterGroup(
            "Other mobiles",
            s_Settings.OtherEnabledPtr(),
            true,
            s_Settings.OtherFontStylePtr(),
            s_Settings.OtherColorPtr(),
            s_Settings.OtherShowNamePtr(),
            s_Settings.OtherShowHitsPtr());
        CharacterGroup(
            "Enemies",
            s_Settings.EnemiesEnabledPtr(),
            true,
            s_Settings.EnemiesFontStylePtr(),
            s_Settings.EnemiesColorPtr(),
            s_Settings.EnemiesShowNamePtr(),
            s_Settings.EnemiesShowHitsPtr());
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Main settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Checkbox("Show on log in", s_Settings.ShowOnLoginPtr());
        ImGui::Checkbox("Show coordinates", s_Settings.DisplayCoordinatesPtr());
        // the checkbox ships disabled (
        ImGui::BeginDisabled();
        ImGui::Checkbox("Smooth image", s_Settings.SmoothImagePtr());
        ImGui::EndDisabled();
        ImGui::Checkbox("Show altitude map", s_Settings.AltitudeOnlyPtr());
        ImGui::Checkbox("Show statics", s_Settings.ShowStaticsPtr());
        ImGui::Checkbox("Apply altitude", s_Settings.ApplyAltitudePtr());
        ImGui::Checkbox("Show last corpse", s_Settings.ShowLastCorpsePtr());
        ImGui::Checkbox("Mark dead mobiles", s_Settings.MarkDeadMobilesPtr());
        ImGui::Checkbox("Rotate map", s_Settings.RotateMapPtr());
        ImGui::Unindent();
    }

    ImGui::EndChild();

    // the Close / Apply changes row (mapsettingsform.ui pb_Close/pb_ApplyChanges)
    if (ImGui::Button("Close"))
    {
        s_Open = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply changes"))
    {
        // copy + save + redraw
        g_MapSettings.Copy(s_Settings);
        g_MapSettings.Save();
        AssMap_Invalidate();
    }

    ImGui::End();
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
