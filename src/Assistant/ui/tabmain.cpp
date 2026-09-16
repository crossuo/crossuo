// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// TabMain (was assist/tabmain.ui/.cpp): the 23-option checklist bound to
// g_AssistantConfig.m_Options (the same fields the packet handlers consult)
// and the nested Main/Options pages. Toggles apply immediately like the original
// app's on_lw_Options_clicked -> OptionChanged and persist right away
// (SaveMainOptions); the other page widgets save with the profile on close.

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include "widgets.h"
#include "mapwindow.h"
#include "../config/assistantconfig.h"
#include "../core/world.h" // g_Player
#include "../core/assist.h"
#include "../core/global.h"
#include "Managers/PacketManager.h" // ::g_PacketManager (the light/weather/season apply-cores)

namespace xuoassist {

namespace {

// §2.2 checklist order (CHECKLISTBOX_IDENTIFICATOR)
struct XA_OPTION_ROW
{
    const char *Label;
    int Index;
};

const XA_OPTION_ROW s_Options[] = {
    { "Open corpses", CLBI_CORPSES_AUTOOPEN },
    { "Stealth steps counter", CLBI_STEALTH_STEP_COUNTER },
    { "Sounds filter", CLBI_SOUND_FILTER },
    { "Always light", CLBI_FILTER_LIGHT },
    { "Weather filter", CLBI_FILTER_WEATHER },
    { "Season filter", CLBI_FILTER_SEASON },
    { "No death screen", CLBI_NO_DEATH_SCREEN },
    { "Open doors", CLBI_OPEN_DOORS },
    { "Tracker", CLBI_TRACKER },
    { "Stay on top", CLBI_STAY_ON_TOP },
    { "Fast turn", CLBI_FAST_ROTATION },
    { "Recurse containers search", CLBI_RECURSE_CONTAINERS_SEARCH },
    { "Autostart", CLBI_AUTOSTART },
    { "Ignore stamina check", CLBI_IGNORE_STAMINA_CHECK },
    { "Minimize to tray", CLBI_MINIMIZE_TO_TRAY },
    { "Sound echo", CLBI_SOUND_ECHO },
    { "Animation echo", CLBI_ANIMATION_ECHO },
    { "Effect echo", CLBI_EFFECT_ECHO },
    { "Speech filter", CLBI_FILTER_SPEECH },
    { "Text replace", CLBI_TEXT_REPLACE },
    { "Object Inspector", CLBI_OBJECT_INSPECTOR },
    { "No gray screen", CLBI_NO_GRAY_SCREEN },
    { "Save far objects", CLBI_SAVE_FAR_OBJECTS },
};

// "Assistant messages show:" (tabmain.ui cb_AssistMessagesLevel)
const char *const s_MessagesLevels[] = { "Minimum", "Medium", "Show all" };

// Ported from TabMain::OptionChanged: the checklist
// rows that act on the client immediately when toggled.
void option_changed(int index, bool state)
{
    switch (index)
    {
        case CLBI_FILTER_LIGHT:
        {
            if (!EnabledFeatureLightFilter())
            {
                g_AssistantConfig.SetOption(index, false);
                state = false;
            }
            ::g_PacketManager.SetLightLevel((uchar)(state ? 0 : g_LightLevel));
            break;
        }
        case CLBI_FILTER_WEATHER:
        {
            if (!EnabledFeatureWeatherFilter())
            {
                g_AssistantConfig.SetOption(index, false);
                state = false;
            }
            ::g_PacketManager.SetWeather(g_Weather.Type, g_Weather.Count, g_Weather.Temperature);
            break;
        }
        case CLBI_FILTER_SEASON:
        {
            if (!EnabledFeatureSeasonFilter())
            {
                g_AssistantConfig.SetOption(index, false);
                state = false;
            }
            ::g_PacketManager.SetSeason(g_Season.Index, g_Season.Music);
            break;
        }
        case CLBI_TRACKER:
        {
            if (!EnabledFeatureTracker())
            {
                g_AssistantConfig.SetOption(index, false);
                state = false;
            }
            if (!state)
            {
                g_Assistant.Track(false, 0, 0);
            }
            break;
        }
        case CLBI_STAY_ON_TOP:
        {
            // was AssistForm::UpdateStayOnTop: a host-window flag; the
            // assistant renders in-game and has no window to keep on top
            break;
        }
        case CLBI_FAST_ROTATION:
        {
            if (!EnabledFeatureFastRotation())
            {
                g_AssistantConfig.SetOption(index, false);
                state = false;
            }
            g_SetInt(VKI_FAST_ROTATION, static_cast<int>(state));
            break;
        }
        case CLBI_IGNORE_STAMINA_CHECK:
        {
            if (!EnabledFeatureIgnoreStaminaCheck())
            {
                g_AssistantConfig.SetOption(index, false);
                state = false;
            }
            g_SetInt(VKI_IGNORE_STAMINA_CHECK, static_cast<int>(state));
            break;
        }
        case CLBI_NO_GRAY_SCREEN:
        {
            if (!EnabledFeatureNoDeathScreen())
            {
                g_AssistantConfig.SetOption(index, false);
                state = false;
            }
            if (g_Player != nullptr)
            {
                ushort graphic = 0;
                if (!state)
                {
                    if (g_Player->IsDead() || (g_MorphGraphic == 0u))
                    {
                        graphic = g_Player->Graphic;
                    }
                    else
                    {
                        graphic = g_MorphGraphic;
                    }
                }
                else
                {
                    if (g_Player->IsDead())
                    {
                        graphic = 0x03DB;
                    }
                    else if (g_MorphGraphic != 0u)
                    {
                        graphic = g_MorphGraphic;
                    }
                    else
                    {
                        graphic = g_Player->Graphic;
                    }
                }
                if (graphic != 0u)
                {
                    g_SetInt(VKI_SET_PLAYER_GRAPHIC, graphic);
                }
            }
            break;
        }
        default:
            break;
    }
}

// A color row: use-checkbox + hex line edit + hue preview swatch + "..."
// (the original "..." sent a dye packet; the 0x95 response updates the config).
void font_color_row(const char *label, bool *use, astr_t *color, uint32_t dyeSerial)
{
    // the fixed widget ids (##use/##hex/...) are scoped per row: this is drawn
    // twice ("Font color" / "Characters font color") and they would collide
    ImGui::PushID(label);
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    if (ImGui::Checkbox("##use", use))
    {
        AssistantUI_MarkOptionsDirty();
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Use this color");
    }
    ImGui::SameLine();
    if (HexEditField("##hex", color, 80.0f))
    {
        AssistantUI_MarkOptionsDirty();
    }
    ImGui::SameLine();
    HueSwatch((ushort)CAssistant::TextToGraphic(*color));
    ImGui::SameLine();
    // was on_tb_SelectFontColor_clicked: opens the client's dye gump; the 0x95
    // response writes the color into the config
    ImGui::BeginDisabled(!g_Connected);
    const bool pick = ImGui::Button("...");
    ImGui::EndDisabled();
    if (pick && g_Connected)
    {
        // the 0x95 DyeData handler's gump open, called directly (the former
        // injected 0x95 carried the same fixed 0x0FAB dye-tub graphic)
        ::g_PacketManager.OpenDyeGump(dyeSerial, 0x0FAB);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Pick the color with the server's dye gump");
    }
    ImGui::PopID();
}

void draw_main_page()
{
    // Assistant messages show:
    int level = g_AssistantConfig.GetMessagesLevel();
    ImGui::SetNextItemWidth(120.0f);
    if (ComboGroup("##messages", &level, s_MessagesLevels, 3))
    {
        g_AssistantConfig.m_MessagesLevel = level;
        AssistantUI_MarkOptionsDirty();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Assistant messages show:");

    ImGui::Separator();

    // Font color / Characters font color rows (the checkbox is the original
    // cb_FontColor "use color" state; the hex edit is the le_FontColor field)
    font_color_row(
        "Font color:",
        &g_AssistantConfig.m_UseFontColor,
        &g_AssistantConfig.m_FontColor,
        0xFFFFFFFF);
    font_color_row(
        "Characters font color:",
        &g_AssistantConfig.m_UseCharactersFontColor,
        &g_AssistantConfig.m_CharactersFontColor,
        0xFFFFFFFE);

    ImGui::Separator();

    // Auto reconnect after [N] seconds (left X seconds)
    bool reconnect = g_AssistantConfig.AutoReconnect();
    if (ImGui::Checkbox("Auto reconnect after", &reconnect))
    {
        g_AssistantConfig.m_AutoReconnect = reconnect;
        AssistantUI_MarkOptionsDirty();
    }
    ImGui::SameLine();
    int seconds = g_AssistantConfig.AutoReconnectSeconds();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::InputInt("##reconnect_seconds", &seconds))
    {
        seconds = seconds < 10 ? 10 : (seconds > 9999 ? 9999 : seconds);
        g_AssistantConfig.m_AutoReconnectSeconds = seconds;
        AssistantUI_MarkOptionsDirty();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("seconds");
    if (reconnect && !g_Connected && EnabledFeatureReconnector() &&
        AssistantUI_ReconnectSecondsLeft() > 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(left %d seconds)", AssistantUI_ReconnectSecondsLeft());
    }

    ImGui::Separator();

    // Profiles: combo + Save (was the editable cb_ProfileList + pb_SaveProfile)
    const vector<astr_t> &profiles = g_AssistantConfig.GetProfiles();
    static int currentProfile = -1;
    const astr_t currentName = g_AssistantConfig.ProfileName;
    if (currentProfile < 0 || (currentProfile < (int)profiles.size() && profiles[currentProfile] != currentName))
    {
        currentProfile = -1;
        for (int i = 0; i < (int)profiles.size(); i++)
        {
            if (profiles[i] == currentName)
            {
                currentProfile = i;
                break;
            }
        }
    }
    static char newProfile[64] = { 0 };
    ImGui::TextUnformatted("Profiles:");
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("##profile", currentName.c_str()))
    {
        for (int i = 0; i < (int)profiles.size(); i++)
        {
            if (ImGui::Selectable(profiles[i].c_str(), i == currentProfile))
            {
                // was on_cb_ProfileList_currentIndexChanged: load + remember
                g_AssistantConfig.SetCurrentProfile(profiles[i]);
                g_AssistantConfig.LoadProfile();
                if (g_Connected && g_Player != nullptr)
                {
                    g_AssistantConfig.SaveProfileMap();
                }
                // was LoadCurrentProfile's tail: recompile the display title
                AssistantUI_RecompileDisplayTitle();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::InputTextWithHint("##new_profile", "(new profile name)", newProfile, sizeof(newProfile));
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        // was on_pb_SaveProfile_clicked: save under the (possibly new) name
        if (newProfile[0] != '\0')
        {
            g_AssistantConfig.SetCurrentProfile(newProfile);
            newProfile[0] = '\0';
        }
        g_AssistantConfig.SaveProfile();
        g_AssistantConfig.ScanProfiles();
        g_AssistantConfig.SaveProfileMap();
        g_Assistant.ClientPrint("Configuration saved.");
        AssistantUI_ClearOptionsDirty(); // everything was just written
    }

    ImGui::Separator();

    // Assistant windows opacity: 30-100 (was hs_WindowsOpacity)
    int opacity = g_AssistantConfig.WindowsOpacity();
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::SliderInt("Assistant windows opacity:", &opacity, 30, 100))
    {
        g_AssistantConfig.m_WindowsOpacity = opacity;
        AssistantUI_MarkOptionsDirty();
    }

    ImGui::Separator();

    // Open map (map_icon.png) - the map window
    if (IconButton(XA_ICON_MAP, "Open map"))
    {
        MapWindow_Open();
    }
}

void draw_options_page()
{
    // spinbox ranges/defaults from tabmain.ui (§2.2)
    struct XA_SPIN_ROW
    {
        const char *Label;
        int *Value;
        int Min, Max;
    };
    XA_SPIN_ROW rows[] = {
        { "Use items delay:", &g_AssistantConfig.m_UseItemsDelay, 0, 10000 },
        { "Wait targets delay:", &g_AssistantConfig.m_WaitTargetsDelay, 0, 60000 },
        { "Move items delay:", &g_AssistantConfig.m_MoveItemsDelay, 10, 5000 },
        { "Search objects distance:", &g_AssistantConfig.m_SearchItemsDistance, 0, 4 },
        { "Open corpses distance:", &g_AssistantConfig.m_OpenCorpsesDistance, 0, 4 },
        { "Use objects distance:", &g_AssistantConfig.m_UseItemsDistance, 0, 4 },
        { "keep delay (ms):", &g_AssistantConfig.m_CorpseKeepDelay, 100, 600000 },
    };
    for (int i = 0; i < int(sizeof(rows) / sizeof(rows[0])); i++)
    {
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::InputInt(rows[i].Label, rows[i].Value))
        {
            *rows[i].Value = *rows[i].Value < rows[i].Min
                ? rows[i].Min
                : (*rows[i].Value > rows[i].Max ? rows[i].Max : *rows[i].Value);
            AssistantUI_MarkOptionsDirty();
        }
    }
    if (ImGui::Checkbox("Double click for open doors", &g_AssistantConfig.m_DoubleClickForOpenDoors))
    {
        AssistantUI_MarkOptionsDirty();
    }
}

} // namespace

void TabMain_Draw()
{
    // left: the 23-option checklist
    ImGui::BeginChild("##options_list", ImVec2(190.0f, 0.0f), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Options:");
    ImGui::Separator();
    for (int i = 0; i < int(sizeof(s_Options) / sizeof(s_Options[0])); i++)
    {
        const int index = s_Options[i].Index;
        bool state = g_AssistantConfig.GetOption(index);
        if (ImGui::Checkbox(s_Options[i].Label, &state))
        {
            g_AssistantConfig.SetOption(index, state);
            option_changed(index, state); // immediate client effect (parity with the original)
            g_AssistantConfig.SaveMainOptions();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // right: the nested Main/Options tab (tw_Options)
    ImGui::BeginChild("##options_pages", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    if (ImGui::BeginTabBar("##tw_options", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Main"))
        {
            draw_main_page();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Options"))
        {
            draw_options_page();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
