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

#include <Wisp/WispWindow.h> // Wisp::g_WispWindow (the display caption)

#include "assistanteventsui.h"
#include "widgets.h"
#include "tabhotkeys.h"
#include "tabmacros.h"
#include "../config/assistantconfig.h"
#include "../core/assist.h"
#include "../core/dressagent.h"
#include "../core/shopagent.h"
#include "../core/packets.h"
#include "../core/global.h"
#include "../managers/displaymanager.h"
#include "../macros/macroexecutor.h"
#include "../script/commandqueue.h"
#include "../script/scriptfile.h" // the file-engine sandbox root
#include "../script/scriptgump.h" // the script text window draw
#include "../script/scripthandle.h"
#include "scripteditor.h"
#include "tabscripts.h"
#include "mapwindow.h"
#include "inspector.h"
#include "textdialog.h"
#include "../managers/assmap.h"
#include "../model/gameobjects/gameworld.h" // xuoassist::g_World (journal gating)

#include <revision.h> // PRODUCT_VERSION_STR (generated into the build dir)

namespace xuoassist {

// the script editor window flag (declared in assistantui.h inside this
// namespace - keep the definition in it too, or the compiler warns about a
// missing matching extern declaration)
bool g_AssistantScriptEditorOpen = false;

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
    "",  // Main
    "",  // Display (phase 4a)
    "",  // Lists
    "",  // Hotkeys (phase 3)
    "",  // Macros (phase 3)
    "",  // Scripts (phase 4b round 1)
    "",  // Agents (nested: Dress/Party/Shop, phase 4a)
    "",  // Skills (phase 4a)
    "",  // Filters
    "",  // About (phase 5)
};

bool s_Initialized = false;
CAssistantEvents *s_PreviousEvents = nullptr; // sink to restore on shutdown
bool s_OptionsDirty = false; // Main/Options pages save on close (Qt semantics)
uint32_t s_SessionStartTicks = 0;

// auto-reconnect countdown (was TabMain::m_ReconnectTimer, tabmain.cpp:722)
int s_ReconnectSeconds = 0;
uint32_t s_ReconnectLastTick = 0;

// the display caption (was the Qt title bar): the last string we applied to
// the OS window, so SetTitle only runs on change
string s_LastWindowTitle;
uint32_t s_LastStepTick = 0;

// was TabDisplay::RedrawTitle: the config radio -> the display manager mode
void apply_display_title()
{
    REDRAW_DISPLAY_MODE mode = RDM_DISABLED;
    switch (g_AssistantConfig.DisplayType())
    {
        case 1:
            mode = RDM_TEXTUAL;
            break;
        case 2:
            mode = RDM_COLORED_TEXTUAL;
            break;
        case 3:
            mode = RDM_COLORED_GRAPHICAL;
            break;
        default:
            break;
    }

    g_DisplayManager.Redraw(
        mode,
        g_AssistantConfig.ApplyNotorietyColorOnName(),
        g_AssistantConfig.HightFont(),
        g_AssistantConfig.HighlightReagents(),
        true,
        g_AssistantConfig.DisplayBackgroundColor());

    // apply to the OS window on change (was the GDI painter writing the
    // caption; the client's own writes - player name change etc. - are
    // overwritten here exactly like the Qt title bar was repainted)
    const string &title = g_DisplayManager.GetTitleText();
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
    // the nested bar (Dress/Party/Shop) per §2.1, pages from phase 4a
    if (ImGui::BeginTabBar("##agents_tabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Dress"))
        {
            TabDress_Draw();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Party"))
        {
            TabParty_Draw();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Shop"))
        {
            TabShop_Draw();
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
    // pin the file-engine sandbox to the client dir (the config root: the
    // client cwd == binary dir) once, on the main thread
    ScriptFile_SetSandboxRoot(fs_path_str(g_AssistantConfig.GetRootPath()));
    s_Initialized = true;
    // compile the display title data from the just-loaded profile (was
    // AssistForm::LoadCurrentProfile -> g_TabDisplay->RecompilleData,
    // assistform.cpp:463) and paint the initial caption
    AssistantUI_RecompileDisplayTitle();
    // the map settings (was the Qt app init order: CMapSettings::Load ran
    // once at startup, mapsettingsform.cpp:72)
    g_MapSettings.Load();
    Info(Assistant, "assistant UI installed");
}

void AssistantUI_RecompileDisplayTitle()
{
    g_DisplayManager.RecompilleData(
        g_AssistantConfig.DisplayTitleData(), g_AssistantConfig.DisplayTextColor());
    TabDisplay_RequestTitleRedraw();
}

void AssistantUI_Tick()
{
    // runs from AssistantWindow_FrameTick (every game frame, visible or not)
    // the script runtime first: the marshaled commands must run on the main
    // thread before anything else consumes game state this frame (plan:
    // queue -> agents -> draw)
    ScriptQueue_Drain();
    ScriptManager_Get()->Tick();
    tick_reconnect();
    // macro playback (was the TabMacros 10 ms QTimer -> ProcessMacro): a
    // headless frame tick keeps running while the assistant window is hidden
    g_MacroExecutor.Tick();
    // the dress sequence state machine (was the Qt g_EquipScriptName script)
    g_DressAgent.Tick();
    // the shop engine progress countdown (was the ShopProgressForm QTimer)
    g_ShopAgent.Tick();
    // the display caption (was the win32 GDI title-bar painter)
    tick_display_title();
}

void AssistantUI_Shutdown()
{
    // the scripts stop first (bounded join; never hang the client), then the
    // queue is shut down so late pushes fail fast (the Qt app tore the tab
    // widgets down with the window and the queued signals died with it)
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
    // Qt app only wrote Map.xml on the settings form Apply) and clear the
    // inspector/text-dialog state
    MapWindow_Shutdown();
    Inspector_Shutdown();
    TextDialog_Shutdown();
    g_MapSettings.Save();
    // stop the agents (was the Qt app tearing down the tab widgets + dialogs)
    g_DressAgent.Abort();
    g_ShopAgent.Reset();
    TabShop_CloseEditor();
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
// script owns it, like the Qt dialog's script-driven lifecycle).
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
    const vector<string> &lines = ScriptTextWindow_Lines();
    for (const string &line : lines)
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
    TabDisplay_CancelPendingFromTargets();
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
    // the phase 5 windows: the Map (radar + overlays + context menu), its
    // settings form, the Object Inspector sections and the Text Dialog -
    // each gated by its own open flag, viewport-safe like the shell
    MapWindow_Draw(&g_AssistantMapOpen);
    MapSettings_Draw();
    InspectorWindow_Draw();
    TextDialog_Draw();
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
