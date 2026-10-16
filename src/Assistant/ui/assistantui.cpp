// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant shell: window frame, the

#define LOGGER_MODULE Assistant

#include "assistantui.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>

#include <common/logging/logging.h>

#include <Wisp/WispWindow.h> // Wisp::g_WispWindow (the display caption)

#include "assistanteventsui.h"
#include "widgets.h"
#include "tabhotkeys.h"
#include "tabmacros.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/dressagent.h"
#include "../core/shopagent.h"
#include "../core/global.h"
#include "../../AssetViewer/AssetViewer.h" // AssetViewer_Toggle (the Assets button)
#include "../managers/displaymanager.h"
#include "../macros/macroexecutor.h"
#include "../script/commandqueue.h"
#include "../script/scriptfile.h" // the file-engine sandbox root
#include "../script/scriptgump.h" // the script text window draw
#include "../script/scripthandle.h"
#include "CrossUO.h" // ::g_Game (StartReconnect)
#include "scripteditor.h"
#include "tabscripts.h"
#include "mapwindow.h"
#include "inspector.h"
#include "textdialog.h"
#include "../managers/assmap.h"
#include "Managers/CorpseManager.h" // ::g_CorpseManager (corpse auto-open)

#include <revision.h> // PRODUCT_VERSION_STR (generated into the build dir)

namespace xuoassist {

// the script editor window flag (declared in assistantui.h inside this
// namespace - keep the definition in it too, or the compiler warns about a
// missing matching extern declaration)
bool g_AssistantScriptEditorOpen = false;

// --- shell state -------------------------------------------------------------

namespace {

// the default: 650x400, non-resizable; ImGui keeps
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
// which module delivers the remaining tabs
const char *const PLACEHOLDER_PHASE[] = {
    "",  // Main
    "",  // Display
    "",  // Lists
    "",  // Hotkeys
    "",  // Macros
    "",  // Scripts
    "",  // Agents (nested: Dress/Party/Shop)
    "",  // Skills
    "",  // Filters
    "",  // About
};

bool s_Initialized = false;
CAssistantEvents *s_PreviousEvents = nullptr; // sink to restore on shutdown
bool s_OptionsDirty = false; // Main/Options pages save on close (original semantics)
uint32_t s_SessionStartTicks = 0;

// auto-reconnect countdown
int s_ReconnectSeconds = 0;
uint32_t s_ReconnectLastTick = 0;

// the display caption: the last astr_t we applied to
astr_t s_LastWindowTitle;
uint32_t s_LastStepTick = 0;

// the Autostart script run (
uint32_t s_AutostartAt = 0;

// the display caption: compose the plain textual title and apply it to the
// client window when it changed
void apply_display_title()
{
    g_DisplayManager.Redraw(g_AssistantConfig.DisplayEnabled());

    // apply to the OS window on change (
    const astr_t &title = g_DisplayManager.GetTitleText();
    if (title != s_LastWindowTitle)
    {
        s_LastWindowTitle = title;
        if (Wisp::g_WispWindow != nullptr)
        {
            Wisp::g_WispWindow->SetTitle(title);
        }
    }
}

// the AssistantUI_Tick half of the caption: consume the dirty requests, then
// compose+apply. The {x}/{y}/{z} tags set g_UpdateDisplayOnStep at compile
// time - those refresh at 4 Hz; everything else is change-driven (event or
// edit).
void tick_display_title()
{
    const bool forced = TabDisplay_ConsumeTitleDirty();
    if (!forced && !g_UpdateDisplayOnStep)
    {
        return;
    }
    if (!forced && (g_Ticks - s_LastStepTick) < 250u)
    {
        return;
    }
    s_LastStepTick = g_Ticks;
    apply_display_title();
}

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
            char childId[32];
            snprintf(childId, sizeof(childId), "##lists_sub%d", i);
            if (BeginSubTab(LIST_TABS[i], childId))
            {
                TabLists_Draw(i);
                EndSubTab();
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
            char childId[32];
            snprintf(childId, sizeof(childId), "##filters_sub%d", i);
            if (BeginSubTab(FILTER_TABS[i], childId))
            {
                TabFilters_Draw(i);
                EndSubTab();
            }
        }
        ImGui::EndTabBar();
    }
}

void draw_agents_tabs()
{
    // the nested bar (Dress/Party/Shop), agent pages
    if (ImGui::BeginTabBar("##agents_tabs", ImGuiTabBarFlags_None))
    {
        if (BeginSubTab("Dress", "##agents_dress"))
        {
            TabDress_Draw();
            EndSubTab();
        }
        if (BeginSubTab("Party", "##agents_party"))
        {
            TabParty_Draw();
            EndSubTab();
        }
        if (BeginSubTab("Shop", "##agents_shop"))
        {
            TabShop_Draw();
            EndSubTab();
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
            g_Game.StartReconnect();
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
    // pin the file-engine sandbox to the client dir (the config root: the
    // client cwd == binary dir) once, on the main thread
    ScriptFile_SetSandboxRoot(fs_path_str(g_AssistantConfig.GetRootPath()));
    s_Initialized = true;
    // the profile list for the combo (was AssistForm::Init's scan); the
    // serial-mapped profile is selected + loaded at login (LoadProfileBySerial)
    g_AssistantConfig.ScanProfiles();
    // compile the display title data from the just-loaded profile (was
    // AssistForm::LoadCurrentProfile -> g_TabDisplay->RecompileData)
    // and paint the initial caption
    AssistantUI_RecompileDisplayTitle();
    // the map settings (loaded before the window opens, so the
    g_MapSettings.Load();
    Info(Assistant, "assistant UI installed");
}

void AssistantUI_RecompileDisplayTitle()
{
    g_DisplayManager.RecompileData(g_AssistantConfig.DisplayTitleData());
    TabDisplay_RequestTitleRedraw();
}

void AssistantUI_RequestClientClose()
{
    // CWindow::Destroy pushes the SDL_QUIT that runs the normal shutdown path
    if (Wisp::g_WispWindow != nullptr)
    {
        Wisp::g_WispWindow->Destroy();
    }
}

void AssistantUI_QueueAutostart()
{
    // the checkbox check stays in the EnterWorld path (
    s_AutostartAt = g_Ticks + 1000;
}

void AssistantUI_Tick()
{
    // runs from AssistantWindow_FrameTick (every game frame, visible or not)
    // the deferred Autostart script run
    if (s_AutostartAt != 0u && g_Ticks >= s_AutostartAt)
    {
        s_AutostartAt = 0;
        // runs the editor buffer as the "Autostart" script
        ScriptManager_Get()->Run("Autostart", ScriptEditor_Text());
    }
    // the script runtime first: the marshaled commands must run on the main
    // thread before anything else consumes game state this frame (plan:
    // queue -> agents -> draw)
    ScriptQueue_Drain();
    ScriptManager_Get()->Tick();
    tick_reconnect();
    // macro playback (frame-tick driven): a
    g_MacroExecutor.Tick();
    // corpse auto-open: the client manager holds
    if (g_AssistantConfig.CorpsesAutoopen())
    {
        if (::g_CorpseManager.CheckCorpses(
                g_AssistantConfig.OpenCorpsesDistance(),
                g_AssistantConfig.UseItemsDelay(),
                g_LastUseObjectTimer))
        {
            g_LastUseObjectTimer = g_Ticks;
        }
    }
    // the dress sequence state machine
    g_DressAgent.Tick();
    // the shop engine progress countdown
    g_ShopAgent.Tick();
    // the display caption painter
    tick_display_title();
}

void AssistantUI_Shutdown()
{
    // disarm a pending Autostart run
    s_AutostartAt = 0;
    // the scripts stop first (bounded join; never hang the client), then the
    // queue is shut down so late pushes fail fast (
    ScriptManager_Get()->StopAll(2000);
    ScriptQueue_Shutdown();
    // one more brief join pass: with pushes failing fast the stragglers fall
    // through their pending calls and become joinable (zombie-eligible ones
    // were detached by the first pass; this catches threads that only needed
    // the queue closed)
    ScriptManager_Get()->StopAll(250);
    // disarm any from-target request armed before shutdown (CancelPendingFrom
    // Targets): otherwise the stale g_TargetHandler would swallow the first
    // legit 0x6C after a re-init
    CancelPendingFromTargets();
    // the phase 5 windows: persist the map settings + geometry (delta: the
    // original app only wrote Map.ini on the settings form Apply) and clear the
    // inspector/text-dialog state
    MapWindow_Shutdown();
    Inspector_Shutdown();
    TextDialog_Shutdown();
    g_MapSettings.Save();
    // stop the agents
    g_DressAgent.Abort();
    g_ShopAgent.Reset();
    TabShop_CloseEditor();
    if (s_Initialized)
    {
        // flush pending Main/Options page edits (saved on close)
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
    // NOTE: the UI-owned textures (icon atlas, map radar, About logo) are NOT
    // released here: they must stay registered until the ImGui backend shuts
    // them down (GL release), and the CPU pixels are freed after
    // DestroyContext - both handled by AssistantWindow_Shutdown right after
    // ui_shutdown via AssistantUI_AfterImGuiShutdown.
}

void AssistantUI_AfterImGuiShutdown()
{
    // called right after ui_shutdown (the ImGui context is gone): free the
    // CPU-side pixels of the UI-owned textures; their GL names were reclaimed
    // by the backend teardown inside ui_shutdown (RefCount still 1)
    Icons_Shutdown();
    AssMap_AfterImGuiShutdown();
    TabAbout_AfterImGuiShutdown();
}

int AssistantUI_ReconnectSecondsLeft()
{
    return s_ReconnectSeconds;
}

// The script text window (the g_TextDialog port, scriptgump.h): a small
// ImGui window fed by the main-thread scriptgump.cpp state (the ops arrive
// marshaled through SC_TEXT_WINDOW). Position/size come from the engine's
// SetPos/SetSize ops; the window only closes through TextWindow.Close (the
// script owns it).
void ScriptTextWindow_Draw()
{
    if (!ScriptTextWindow_Opened())
    {
        return;
    }
    int x = 40;
    int y = 40;
    int width = 400;
    int height = 300;
    ScriptTextWindow_Rect(&x, &y, &width, &height);
    ImGui::SetNextWindowPos(ImVec2((float)x, (float)y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Script text window", nullptr))
    {
        ImGui::End();
        return;
    }
    const vector<astr_t> &lines = ScriptTextWindow_Lines();
    for (const astr_t &line : lines)
    {
        ImGui::TextUnformatted(line.c_str(), line.c_str() + line.size());
    }
    // keep the newest line visible (the dialog scrolled to the bottom)
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::End();
}

void CancelPendingFromTargets()
{
    TabHotkeys_CancelPendingFromTargets();
    TabLists_CancelPendingFromTargets();
    TabDress_CancelPendingFromTargets();
    TabParty_CancelPendingFromTargets();
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

    // closed: submit nothing. Begin() does not skip the window when *open is
    // false, so without this early-out the shell (and everything drawn in this
    // pass) would keep rendering while hidden. The frame pipeline in
    // AssistantWindow_DrawOverlay keeps running so ImGui still processes the
    // close and tears down popped-out platform windows.
    if (open != nullptr && !*open)
    {
        return;
    }

    Icons_Init(); // idempotent; needs the live ImGui context

    ImGui::SetNextWindowSize(
        ImVec2((float)SHELL_DEFAULT_WIDTH, (float)SHELL_DEFAULT_HEIGHT), ImGuiCond_FirstUseEver);

    // assistant windows opacity: clamped to the
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

    // the title carries "X:UO Assist v<version> <playerName>"
    ImGui::SameLine();
    ImGui::TextDisabled("v" PRODUCT_VERSION_STR "%s", astr_t(g_SelectedCharName).c_str());
    ImGui::SameLine();
    static bool journalOpen = true;
    ImGui::Checkbox("Journal", &journalOpen);
    ImGui::SameLine();
    if (ImGui::Button("Assets"))
    {
        AssetViewer_Toggle();
    }
    ImGui::Separator();

    // content: tabs on the left/top, journal pane below, status bar pinned last.
    // The tab bar stays fixed; only each tab's content scrolls (its child).
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

    if (ImGui::BeginTabBar("##main_tabs", ImGuiTabBarFlags_None))
    {
        for (int i = 0; i < int(sizeof(MAIN_TABS) / sizeof(MAIN_TABS[0])); i++)
        {
            if (!ImGui::BeginTabItem(MAIN_TABS[i]))
            {
                continue;
            }
            // the child takes the height left over by the tab bar and holds
            // the scroll region for this tab's content
            float contentHeight = tabsHeight - ImGui::GetFrameHeightWithSpacing();
            if (contentHeight < 1.0f)
            {
                contentHeight = 1.0f;
            }
            ImGui::BeginChild(MAIN_TABS[i], ImVec2(0.0f, contentHeight), ImGuiChildFlags_None);
            switch (i)
            {
                case MT_MAIN:
                    TabMain_Draw();
                    break;
                case MT_DISPLAY:
                    TabDisplay_Draw();
                    break;
                case MT_LISTS:
                    draw_lists_tabs();
                    break;
                case MT_HOTKEYS:
                    TabHotkeys_Draw();
                    break;
                case MT_MACROS:
                    TabMacros_Draw();
                    break;
                case MT_SCRIPTS:
                    TabScripts_Draw();
                    break;
                case MT_FILTERS:
                    draw_filters_tabs();
                    break;
                case MT_AGENTS:
                    draw_agents_tabs();
                    break;
                case MT_SKILLS:
                    TabSkills_Draw();
                    break;
                case MT_ABOUT:
                    TabAbout_Draw();
                    break;
                default:
                    draw_placeholder(MAIN_TABS[i], PLACEHOLDER_PHASE[i]);
                    break;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (journalOpen)
    {
        JournalPane_Draw(journalHeight);
    }

    StatusBar_Draw(s_SessionStartTicks);

    ImGui::End();
    ImGui::PopStyleVar();

    // the pop-out assistant windows (hotkeys form, speech-only control
    // forms): separate ImGui windows gated by this draw call, i.e. by the
    // AssistantWindow visibility like everything else
    TabHotkeys_DrawControlForms();
    // the script editor window (ScriptEditorDialog): one shared buffer with
    // the script runtime; drawn only while open
    ScriptEditorWindow_Draw(&g_AssistantScriptEditorOpen);
    // the shopping progress popup: agent-owned window drawn next to the other
    // always-on UI so it survives leaving the Shop sub-tab (still only while
    // the assistant window is drawn; the popup itself early-outs when the
    // agent is idle or the "Show shopping progress bar" option is off)
    TabShop_DrawProgress();
    // the script text window (the g_TextDialog port): small ImGui window
    // driven by scriptgump.cpp state. Documented caveat: it lives inside this
    // draw pass, so it hides with the assistant shell (the lines stay; the
    // packet handlers keep running - only the pixels are gated)
    ScriptTextWindow_Draw();
    // the secondary windows: the Map (radar + overlays + context menu), its
    // settings form, the Object Inspector sections and the Text Dialog -
    // each gated by its own open flag, viewport-safe like the shell
    MapWindow_Draw(&g_AssistantMapOpen);
    MapSettings_Draw();
    InspectorWindow_Draw();
    TextDialog_Draw();
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
