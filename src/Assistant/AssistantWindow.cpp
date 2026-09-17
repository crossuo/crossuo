// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Assistant

#include "AssistantWindow.h"

#if defined(XUO_ASSISTANT)

#include <string.h>

#include <common/fs.h>
#include <common/logging/logging.h>
#include <external/gfx/gfx.h>
#include <external/gfx/imgui/imgui.h>
#include <external/gfx/ui.h>
#include <xuocore/enumlist.h>
#include <xuocore/uodata.h> // ExeFilePath
#include "ui/assistantui.h" // AssistantUI_Draw/Init/Tick/Shutdown (xuoassist)
#include "ui/widgets.h"     // Icons_Init/Icons_Shutdown (icon atlas lifecycle)
#include "core/global.h"    // g_Connected (xuoassist)
#include "core/hotkeys.h"   // the hotkey engine
#include "macros/macroexecutor.h" // g_MacroExecutor.Reset (disconnect reset)
#include "Globals.h"        // g_GameState, g_Ticks
#include "Wisp/WispWindow.h" // Wisp::g_WispWindow

// The assistant wraps the game window and the game GL context: it never creates
// a window or a GL context of its own. Popped-out platform windows are created,
// rendered and destroyed by the ImGui SDL2 backend inside ui_draw.
static win_context s_assistantWin;
static ui_context s_ctx;
static bool s_initialized = false;
static bool s_visible = false;
static uint32_t s_sessionStartTicks = 0;
static bool s_textInputActive = false;
// io.IniFilename keeps the pointer handed over in win_context.inifile, so the
// buffer must outlive the ImGui context: it lives for the process lifetime.
static char s_iniFile[512] = {};

// the assistant is a gameplay tool: it exists exclusively while the client is
// at the in-game stage (ADR D10)
static bool at_game_stage()
{
    return g_GameState == GS_GAME || g_GameState == GS_GAME_BLOCKED;
}

static bool event_belongs_to_game(const SDL_Event &ev)
{
    // mirrors WispWindow.cpp OnWindowProc: per-event window ids decide who owns
    // the event; anything not addressed to the game window belongs to an
    // assistant pop-out platform window.
    const uint32_t gameId = s_assistantWin.window != nullptr
                                ? SDL_GetWindowID(s_assistantWin.window)
                                : 0;
    if (gameId == 0)
        return true;
    switch (ev.type)
    {
        case SDL_WINDOWEVENT:
            return ev.window.windowID == gameId;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            return ev.key.windowID == gameId;
        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:
            return ev.text.windowID == gameId;
        case SDL_MOUSEMOTION:
            return ev.motion.windowID == gameId;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            return ev.button.windowID == gameId;
        case SDL_MOUSEWHEEL:
            return ev.wheel.windowID == gameId;
        default:
            return true; // window-less events (SDL_QUIT, ...) go to the game
    }
}

// The client never enables SDL text input (the game console and gumps are fed
// from raw key events via CGameScreen::OnTextInput), so SDL_TEXTINPUT never
// reaches ImGui either; the vendored SDL2 backend only starts it on Android.
// Leaving it enabled for good would make the game receive text events as well
// and every character would be typed twice. Enable it only while ImGui actually
// wants text input and restore the previous state otherwise.
static void sync_text_input(bool wantText)
{
    if (wantText == s_textInputActive)
        return;
    s_textInputActive = wantText;
    if (wantText)
    {
        SDL_StartTextInput();
    }
    else
    {
        SDL_StopTextInput();
    }
}

static bool init()
{
    if (s_initialized)
        return true;

    // the game owns the GL context on the main thread: initialization may run
    // from AssistantWindow_DrawOverlay (in-game render pass) or from the Alt+U
    // toggle in AssistantWindow_ProcessEvent; both run on the main thread with
    // the game GL context current at the in-game stage
    SDL_Window *window =
        Wisp::g_WispWindow != nullptr ? Wisp::g_WispWindow->m_window : nullptr;
    SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (window == nullptr || context == nullptr)
    {
        Error(Assistant, "could not initialize: no game window/context");
        return false;
    }

    memset(&s_assistantWin, 0, sizeof(s_assistantWin));
    s_assistantWin.title = "X:UO Assist";
    s_assistantWin.window = window;
    s_assistantWin.context = context;
    SDL_GetWindowSize(window, &s_assistantWin.width, &s_assistantWin.height);
    // fs_path_ascii hands out a pointer into a temporary fs_path: hold the
    // string in an astr_t that outlives the copy into s_iniFile below
    const astr_t ini = fs_path_str(ExeFilePath("xuoassist.ini"));
    const size_t iniLength = ini.length();
    const size_t iniMax = sizeof(s_iniFile) - 1;
    const size_t iniSize = iniLength < iniMax ? iniLength : iniMax;
    memcpy(s_iniFile, ini.c_str(), iniSize);
    s_iniFile[iniSize] = '\0';
    s_assistantWin.inifile = s_iniFile;

    // docking + multi-viewport (pop-out, ADR D11): ui_draw saves/restores the
    // game context around the platform windows it renders (external/gfx/ui.cpp)
    s_ctx = ui_init(s_assistantWin, /*enable_viewports=*/true);
    if (ImGui::GetCurrentContext() == nullptr)
    {
        // ui_init created only ImGui/backend resources: ui_shutdown destroys
        // them; the SDL window and GL context belong to the game and stay
        ui_shutdown(s_ctx);
        s_ctx = ui_context();
        memset(&s_assistantWin, 0, sizeof(s_assistantWin));
        Error(Assistant, "initialization failed: no ImGui context");
        return false;
    }
    // the launcher color scheme is the final style: applied after ui_init (its
    // StyleColorsDark and the viewport WindowBg tweak run first) so the
    // assistant renders with the same theme as the launcher
    ui_style_launcher();
    s_initialized = true;
    s_sessionStartTicks = g_Ticks;
    xuoassist::AssistantUI_Init(); // journal pane events consumer
    // icon atlas lifecycle is bound to the ImGui context: registered after it
    // exists, released after it is destroyed (see AssistantWindow_Shutdown)
    xuoassist::Icons_Init();
    Info(Assistant, "initialized (ini: %s)", s_iniFile);
    return true;
}

bool AssistantWindow_Toggle()
{
    if (!at_game_stage())
        return false;
    if (!s_initialized && !init())
    {
        // log once: the key falls through to the game instead, and retrying
        // every Alt+U must not spam the log
        static bool warned = false;
        if (!warned)
        {
            Error(Assistant, "toggle failed: assistant not initialized");
            warned = true;
        }
        return false;
    }
    s_visible = !s_visible;
    if (!s_visible)
    {
        // disarm a pending capture field: with the shell hidden nothing would
        // commit it, and the engine would keep eating keys into the buffer
        xuoassist::g_Hotkeys.CancelCapture();
    }
    return true;
}

void AssistantWindow_FrameTick()
{
    // per-frame assistant logic that must run while visible or not
    if (s_initialized)
        xuoassist::AssistantUI_Tick(); // auto-reconnect countdown (was TabMain::m_ReconnectTimer)

    // disconnect stage-gate: when the client leaves the in-game stage the assistant
    // drops its connected flag and resets its own state; running here instead
    // of per packet means a stage change is noticed within one frame
    if (xuoassist::g_Connected && !at_game_stage())
    {
        xuoassist::g_MacroExecutor.Reset();
        xuoassist::g_Connected = false;
    }

    // in-game only (ADR D10): logout, disconnect or back-to-main-menu shuts the
    // assistant down, no matter whether its windows are docked or popped out;
    // the client quit path is covered by the shutdown hook in Run()
    if (s_initialized && !at_game_stage())
        AssistantWindow_Shutdown();
}

bool AssistantWindow_ProcessEvent(const SDL_Event &ev)
{
    const bool isToggleKey = ev.type == SDL_KEYDOWN && !ev.key.repeat &&
                             ev.key.keysym.sym == SDLK_u &&
                             (ev.key.keysym.mod & KMOD_ALT) != 0;

    if (!s_initialized)
    {
        // the hotkey exists only in game; anywhere else - and whenever the
        // toggle fails (init error) - it falls through to the client untouched
        if (!isToggleKey || !at_game_stage())
            return false;
        return AssistantWindow_Toggle(); // true: consumed, false: fall through
    }

    // ImGui's SDL2 backend must see every event, including the ones belonging
    // to its own platform windows
    ui_process_event(s_ctx, &ev);

    if (isToggleKey)
    {
        if (at_game_stage() && !ImGui::GetIO().WantTextInput)
        {
            AssistantWindow_Toggle();
        }
        return true;
    }

    if (!event_belongs_to_game(ev))
        return true; // assistant pop-out platform window

    const ImGuiIO &io = ImGui::GetIO();

    // The capture field of the hotkeys form ("listening"): it must see the
    // next raw key/wheel/mouse event even when the game would. Gated on
    // !WantTextInput so a busy text field always wins.
    if (xuoassist::Hotkeys_IsCapturing() && !io.WantTextInput &&
        xuoassist::Hotkeys_ProcessCapture(ev))
    {
        return true;
    }

    // Hidden: keep ImGui fed but let the game play normally - EXCEPT the
    // hotkey engine, which keeps working while the assistant is hidden
    // (Pass-keys and the On/Off switch govern it, see core/hotkeys.cpp).
    if (!s_visible)
        return xuoassist::Hotkeys_ProcessGameEvent(ev);

    switch (ev.type)
    {
        case SDL_MOUSEMOTION:
            return io.WantCaptureMouse;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // the engine only consumes middle/X buttons with a matching
            // binding (the plain buttons always pass)
            if (io.WantCaptureMouse)
                return true;
            return xuoassist::Hotkeys_ProcessGameEvent(ev);
        case SDL_MOUSEWHEEL:
            if (io.WantCaptureMouse)
                return true;
            return xuoassist::Hotkeys_ProcessGameEvent(ev);
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
        case SDL_TEXTEDITING:
            // ImGui text fields always win: typing in the assistant never
            // fires a binding
            if (io.WantCaptureKeyboard || io.WantTextInput)
                return true;
            return xuoassist::Hotkeys_ProcessGameEvent(ev);
        default:
            return false;
    }
}

void AssistantWindow_DrawOverlay()
{
    if (!at_game_stage())
        return; // never draws outside the game stage (FrameTick owns shutdown)
    if (!s_initialized && !init())
        return;

    // the game render pass owns the context: ImGui renders the main viewport
    // into it, and ui_draw saves/restores the game context around the popped
    // out platform windows
    // NOTE: when the game window is minimized CGameScreen::Render is not called
    // at all, so this pass (and any popped-out window) freezes until restore;
    // acceptable for now
    // The pipeline keeps running while hidden: the shell X (and the OS close
    // button of a popped-out window) only REPORTS the click through p_open -
    // AssistantUI_Draw stops submitting the window when the flag is false, but
    // the frames must keep flowing so ImGui processes the close and destroys
    // the popped-out platform windows. Bailing out here left them frozen on
    // screen: the assistant could not be closed via its X button.
    ui_newframe(s_ctx);
    xuoassist::AssistantUI_Draw(&s_visible, s_sessionStartTicks);
    ui_draw(s_ctx);

    // belt and braces: ui_draw restores the game context, nothing should have
    // been able to change it since
    if (SDL_GL_GetCurrentWindow() != s_assistantWin.window)
        SDL_GL_MakeCurrent(s_assistantWin.window, s_assistantWin.context);

    sync_text_input(ImGui::GetIO().WantTextInput);
}

void AssistantWindow_Shutdown()
{
    if (!s_initialized)
        return;

    sync_text_input(false);
    // flush pending edits + uninstall the UI events consumer while the ImGui
    // context (and the registered icon texture) still exist. The atlas stays
    // registered with RefCount == 1 so that ui_shutdown below is the one to
    // release its GL texture (ImGui_ImplOpenGL3_Shutdown -> DestroyDeviceObjects
    // destroys every registered texture whose RefCount is 1,
    // imgui_impl_opengl3.cpp:1023-1031); freeing it earlier would leave a
    // dangling ImTextureData in PlatformIO.Textures (UAF) and leak the GL name
    xuoassist::AssistantUI_Shutdown();
    // destroys the ImGui context and every platform window it created; must
    // not SDL_Quit: the client keeps running
    ui_shutdown(s_ctx);
    // only now, with the context gone and the GL names released, may the
    // CPU-side pixels be freed (icon atlas + map radar + About logo;
    // idempotent, deletes the ImTextureData objects)
    xuoassist::AssistantUI_AfterImGuiShutdown();
    s_initialized = false;
    s_visible = false;
    s_ctx = ui_context();
    memset(&s_assistantWin, 0, sizeof(s_assistantWin));
    // the game draws its own software cursor and the ImGui SDL2 backend toggles
    // the process cursor on mouse focus changes: make sure it stays hidden
    SDL_ShowCursor(SDL_DISABLE);
    Info(Assistant, "shutdown");
}

bool AssistantWindow_IsVisible()
{
    return s_initialized && s_visible;
}

#endif // XUO_ASSISTANT
