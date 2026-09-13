// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant shell (was assist/assistform.ui/.cpp): window frame, the
// top-level tab bar (§2.1 order Main/Display/Lists/Hotkeys/Macros/Scripts/
// Agents/Skills/Filters/About), the journal pane, the status bar line and the
// pop out/dock title row from Phase 0. Later-phase tabs render placeholders.

#define LOGGER_MODULE Assistant

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include <common/logging/logging.h>

#include "assistanteventsui.h"
#include "widgets.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/packets.h"
#include "../core/global.h"
#include "../model/gameobjects/gameworld.h" // xuoassist::g_World (journal gating)

#include <revision.h> // PRODUCT_VERSION_STR (generated into the build dir)

namespace xuoassist {

// --- shell state -------------------------------------------------------------

namespace {

// Qt default: 650x400, non-resizable (assistform.cpp:124); ImGui keeps
// resizable windows, the size applies on first use only
constexpr int SHELL_DEFAULT_WIDTH = 650;
constexpr int SHELL_DEFAULT_HEIGHT = 400;

const char *const MAIN_TABS[] = {
    "Main", "Display", "Lists", "Hotkeys", "Macros",
    "Scripts", "Agents", "Skills", "Filters", "About",
};
enum MAIN_TAB
{
    MT_MAIN = 0,
    MT_DISPLAY,
    MT_LISTS,
    MT_HOTKEYS,
    MT_MACROS,
    MT_SCRIPTS,
    MT_AGENTS,
    MT_SKILLS,
    MT_FILTERS,
    MT_ABOUT
};

const char *const LIST_TABS[] = { "Types", "Objects", "Find", "Ignore", "Friends", "Enemies" };
const char *const FILTER_TABS[] = { "Speech", "Replaces", "Sound" };
// which phase delivers the remaining tabs (§7)
const char *const PLACEHOLDER_PHASE[] = {
    "",       // Main
    "4",      // Display
    "",       // Lists
    "3",      // Hotkeys
    "3",      // Macros
    "4",      // Scripts
    "",       // Agents (nested: 4)
    "4",      // Skills
    "",       // Filters
    "5",      // About
};

bool s_Initialized = false;
CAssistantEvents *s_PreviousEvents = nullptr; // sink to restore on shutdown
bool s_OptionsDirty = false; // Main/Options pages save on close (Qt semantics)
uint32_t s_SessionStartTicks = 0;

// auto-reconnect countdown (was TabMain::m_ReconnectTimer, tabmain.cpp:722)
int s_ReconnectSeconds = 0;
uint32_t s_ReconnectLastTick = 0;

void mark_options_dirty()
{
    s_OptionsDirty = true;
}

} // namespace

namespace
{

// --- placeholder page ---------------------------------------------------------

void draw_placeholder(const char *title, const char *phase)
{
    ImGui::TextDisabled("%s", title);
    ImGui::TextDisabled("Coming in phase %s.", phase);
}

// --- nested tab bars ----------------------------------------------------------

void draw_lists_tabs()
{
    if (ImGui::BeginTabBar("##lists_tabs", ImGuiTabBarFlags_None))
    {
        for (int i = 0; i < int(sizeof(LIST_TABS) / sizeof(LIST_TABS[0])); i++)
        {
            if (ImGui::BeginTabItem(LIST_TABS[i]))
            {
                TabLists_Draw(i);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void draw_filters_tabs()
{
    if (ImGui::BeginTabBar("##filters_tabs", ImGuiTabBarFlags_None))
    {
        for (int i = 0; i < int(sizeof(FILTER_TABS) / sizeof(FILTER_TABS[0])); i++)
        {
            if (ImGui::BeginTabItem(FILTER_TABS[i]))
            {
                TabFilters_Draw(i);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

void draw_agents_tabs()
{
    // Agents land in phase 4; the nested bar (Dress/Party/Shop) is already final
    if (ImGui::BeginTabBar("##agents_tabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Dress"))
        {
            draw_placeholder("Dress agent", "4");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Party"))
        {
            draw_placeholder("Party agent", "4");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Shop"))
        {
            draw_placeholder("Shop agent", "4");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// --- auto reconnect (TabMain::StartReconnect / on_ReconnectTimerTimer) ---------

void tick_reconnect()
{
    const bool feature = EnabledFeatureReconnector();
    if (!g_AssistantConfig.AutoReconnect() || !feature || g_Connected)
    {
        s_ReconnectSeconds = 0;
        return;
    }
    if (s_ReconnectSeconds <= 0)
    {
        s_ReconnectSeconds = g_AssistantConfig.AutoReconnectSeconds();
        s_ReconnectLastTick = g_Ticks;
        return;
    }
    if (g_Ticks - s_ReconnectLastTick >= 1000u)
    {
        s_ReconnectLastTick = g_Ticks;
        s_ReconnectSeconds--;
        if (s_ReconnectSeconds == 0)
        {
            CPacketReconnect().SendClient();
            s_ReconnectSeconds = g_AssistantConfig.AutoReconnectSeconds();
        }
    }
}

} // namespace

// --- lifecycle ------------------------------------------------------------------

void AssistantUI_Init()
{
    if (s_Initialized)
    {
        return;
    }
    // install the UI events consumer (journal pane feed); handlers unchanged
    s_PreviousEvents = g_AssistantEvents;
    g_AssistantEvents = AssistantUIEvents_Get();
    s_Initialized = true;
    Info(Assistant, "assistant UI installed");
}

void AssistantUI_Tick()
{
    // runs from AssistantWindow_FrameTick (every game frame, visible or not)
    tick_reconnect();
}

void AssistantUI_Shutdown()
{
    if (s_Initialized)
    {
        // flush pending Main/Options page edits (the Qt app saved on close)
        if (s_OptionsDirty)
        {
            g_AssistantConfig.SaveProfile();
            s_OptionsDirty = false;
        }
        // restore the previous events sink
        g_AssistantEvents = s_PreviousEvents;
        s_PreviousEvents = nullptr;
        s_Initialized = false;
    }
    // NOTE: the icon atlas is NOT released here: its texture must stay
    // registered until the ImGui backend shut it down (GL release), and the
    // CPU pixels are freed after DestroyContext - both handled by
    // AssistantWindow_Shutdown right after ui_shutdown.
}

int AssistantUI_ReconnectSecondsLeft()
{
    return s_ReconnectSeconds;
}

void AssistantUI_MarkOptionsDirty()
{
    mark_options_dirty();
}

void AssistantUI_ClearOptionsDirty()
{
    s_OptionsDirty = false;
}

// --- the shell -------------------------------------------------------------------

void AssistantUI_Draw(bool *open, uint32_t sessionStartTicks)
{
    s_SessionStartTicks = sessionStartTicks;
    Icons_Init(); // idempotent; needs the live ImGui context

    ImGui::SetNextWindowSize(
        ImVec2((float)SHELL_DEFAULT_WIDTH, (float)SHELL_DEFAULT_HEIGHT), ImGuiCond_FirstUseEver);

    // assistant windows opacity (Qt setWindowOpacity; plan §6): clamped to the
    // slider range (a hand-edited ini may hold anything) and pushed only
    // around the assistant shell window, so it never leaks into anything else
    // drawn in the game frame
    const float opacity = g_AssistantConfig.WindowsOpacity() / 100.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, opacity < 0.3f ? 0.3f : (opacity > 1.0f ? 1.0f : opacity));

    if (!ImGui::Begin("X:UO Assist", open))
    {
        // collapsed: the window X button already flipped *open for us
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    // header row: pop out of / dock back into the game window (ADR D11)
    const ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    const bool poppedOut = ImGui::GetWindowViewport() != mainViewport;
    if (poppedOut)
    {
        if (ImGui::Button("Dock in game window"))
        {
            ImGui::SetWindowPos(ImVec2(
                mainViewport->Pos.x + 32.0f,
                mainViewport->Pos.y + 32.0f));
        }
    }
    else
    {
        // placing the window outside the main viewport work area makes ImGui
        // spawn a real OS window for it on the next frame
        if (ImGui::Button("Pop out"))
        {
            ImGui::SetWindowPos(ImVec2(
                mainViewport->Pos.x + mainViewport->WorkSize.x + 48.0f,
                mainViewport->Pos.y + 48.0f));
        }
    }

    // the Qt title carried "X:UO Assist v<version> <playerName>"
    ImGui::SameLine();
    ImGui::TextDisabled("v" PRODUCT_VERSION_STR "%s", g_CharacterName.c_str());
    ImGui::SameLine();
    static bool journalOpen = true;
    ImGui::Checkbox("Journal", &journalOpen);
    ImGui::SameLine();
    ImGui::TextDisabled("Alt+U shows/hides the assistant.");
    ImGui::Separator();

    // content: tabs on the left/top, journal pane below, status bar pinned last
    const float footerHeight = ImGui::GetFrameHeightWithSpacing();
    float tabsHeight = ImGui::GetContentRegionAvail().y - footerHeight;
    const float journalHeight = 150.0f;
    if (journalOpen && tabsHeight > journalHeight + footerHeight)
    {
        tabsHeight -= journalHeight;
    }
    else
    {
        journalOpen = false;
    }

    ImGui::BeginChild("##tabs", ImVec2(0.0f, tabsHeight), ImGuiChildFlags_None);
    if (ImGui::BeginTabBar("##main_tabs", ImGuiTabBarFlags_None))
    {
        for (int i = 0; i < int(sizeof(MAIN_TABS) / sizeof(MAIN_TABS[0])); i++)
        {
            if (!ImGui::BeginTabItem(MAIN_TABS[i]))
            {
                continue;
            }
            switch (i)
            {
                case MT_MAIN:
                    TabMain_Draw();
                    break;
                case MT_LISTS:
                    draw_lists_tabs();
                    break;
                case MT_FILTERS:
                    draw_filters_tabs();
                    break;
                case MT_AGENTS:
                    draw_agents_tabs();
                    break;
                default:
                    draw_placeholder(MAIN_TABS[i], PLACEHOLDER_PHASE[i]);
                    break;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    if (journalOpen)
    {
        JournalPane_Draw(journalHeight);
    }

    StatusBar_Draw(s_SessionStartTicks);

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
