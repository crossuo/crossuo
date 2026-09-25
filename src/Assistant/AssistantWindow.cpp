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
#include <external/gfx/imgui/imgui_internal.h> // RegisterUserTexture
#include <external/gfx/ui.h>
#include <xuocore/enumlist.h>
#include <xuocore/uodata.h> // ExeFilePath
#include <vector>
#include "GLEngine/GLTexture.h" // CGLTexture (the cursor mirror source)
#include "Managers/MouseManager.h" // g_MouseManager (game cursor draw geometry)
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

// --- UO cursor overlay mirror -----------------------------------------------
//
// The client renders the UO cursor in the game render pass (CGameScreen::Render
// -> g_MouseManager.Draw), which lands BEHIND the ImGui overlay drawn after it
// - and inside the game window the OS cursor is suppressed by the per-frame
// cursor policy below, so with the overlay up nothing visible pointed at the
// assistant UI. While the overlay is visible and the mouse is inside the game
// window, the cursor sprite is mirrored into an ImGui-owned texture and drawn
// on the foreground draw list at the exact rect CMouseManager::Draw uses.
//
// The mirror reads the client texture with assistant_cursor_gl_read during the
// assistant draw pass: that pass runs inside the game GL context (before
// ui_draw switches around popped-out windows) so the client texture is
// directly readable; the previous GL_TEXTURE_2D and framebuffer bindings are
// restored. Note the Felucca hue
// (CMouseManager::Draw tints ids >= 0x206A with 0x0033 on non-zero facets) is a
// shader-time tint: the mirrored pixels are the uncolorized art.
static ImTextureData *s_CursorTex = nullptr;
static std::vector<ImTextureData *> s_CursorRetired;
static uint16_t s_CursorGraphic = 0; // art id currently mirrored

// true when the mouse is over one of the assistant's popped-out platform
// windows (the desktop cursor rules there, both for the OS cursor policy and
// for the game-cursor overlay)
static bool mouse_over_assistant_popout()
{
    const ImGuiIO &io = ImGui::GetIO();
    const ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    const ImVector<ImGuiViewport *> &viewports = ImGui::GetPlatformIO().Viewports;
    for (int i = 0; i < viewports.Size; i++)
    {
        const ImGuiViewport *vp = viewports[i];
        if (vp == mainViewport)
            continue; // the game window: the UO cursor rules there
        if (io.MousePos.x >= vp->Pos.x && io.MousePos.y >= vp->Pos.y &&
            io.MousePos.x < vp->Pos.x + vp->Size.x && io.MousePos.y < vp->Pos.y + vp->Size.y)
        {
            return true;
        }
    }
    return false;
}

// reaps mirror retirements the backend finished destroying (the radar map's
// resize sequence): only ever holds entries on a cursor size change
static void assistant_cursor_reap()
{
    for (size_t i = 0; i < s_CursorRetired.size();)
    {
        ImTextureData *tex = s_CursorRetired[i];
        if (tex->Status == ImTextureStatus_Destroyed)
        {
            ImGui::UnregisterUserTexture(tex);
            delete tex;
            s_CursorRetired.erase(s_CursorRetired.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

// the ImGui-owned mirror texture, (re)created at the cursor sprite size
static ImTextureData *assistant_cursor_texture(int w, int h)
{
    if (s_CursorTex != nullptr && (s_CursorTex->Width != w || s_CursorTex->Height != h))
    {
        // size change: queue the texture for destroy and register a replacement
        // (the atlas lifecycle sequence: it stays registered until the backend
        // destroys it, then assistant_cursor_reap unregisters + frees it)
        s_CursorTex->WantDestroyNextFrame = true;
        s_CursorRetired.push_back(s_CursorTex);
        s_CursorTex = nullptr;
    }

    if (s_CursorTex == nullptr)
    {
        s_CursorTex = new ImTextureData();
        s_CursorTex->Create(ImTextureFormat_RGBA32, w, h);
        s_CursorTex->SetStatus(ImTextureStatus_WantCreate);
        s_CursorTex->UseColors = true;
        memset(s_CursorTex->GetPixels(), 0, (size_t)s_CursorTex->GetSizeInBytes());
        ImGui::RegisterUserTexture(s_CursorTex);
    }
    return s_CursorTex;
}

// reads a GL texture into dst (w*h RGBA8 rows) on desktop GL and GLES2 alike
// (glGetTexImage does not exist in OpenGL ES): temporary FBO + glReadPixels,
// with RGBA/UNSIGNED_BYTE - the one combination ES2 guarantees readable; the
// previously bound FBO and texture are saved and restored
static void assistant_cursor_gl_read(GLuint glName, int w, int h, void *dst)
{
    GLint previousTex = 0;
    GLint previousFbo = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glName, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
    {
        // row order: glReadPixels returns the FBO bottom row first, i.e.
        // texture v=0 - the same order glGetTexImage handed back
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFbo);
    glDeleteFramebuffers(1, &fbo);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTex);
}

// copies the client cursor sprite pixels into the mirror (RGBA8, first row =
// top: the client uploads v=0 at the top of its quads and the FBO readback in
// assistant_cursor_gl_read returns v=0 first too, so no flip vs ImGui's UV
// convention)
static void assistant_cursor_upload(ImTextureData *tex, GLuint glName)
{
    assistant_cursor_gl_read(glName, tex->Width, tex->Height, tex->GetPixels());

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // first frame of this texture: the create upload carries the pixels -
        // do NOT downgrade the status (the backend asserts if a texture is
        // never created)
        return;
    }
    ImTextureRect full;
    full.x = 0;
    full.y = 0;
    full.w = (unsigned short)tex->Width;
    full.h = (unsigned short)tex->Height;
    tex->Updates.clear();
    tex->Updates.push_back(full);
    tex->SetStatus(ImTextureStatus_WantUpdates);
}

// draws the mirrored cursor on the ImGui foreground list; mirrors the client's
// own draw condition: CGameScreen::Render draws the cursor only at GS_GAME
// (the blocked screen draws none), for the id g_MouseManager.GetGameCursor()
// resolves, and CMouseManager::Draw skips ids without sprite/texture
static void assistant_cursor_draw()
{
    // draw ONLY while the mouse is over assistant UI inside the game viewport:
    // with only assistant windows in the context WantCaptureMouse is exactly
    // "over an assistant window", and pop-outs are excluded by the viewport
    // check below (they are separate OS windows in desktop space). Over the
    // bare game world the client's own cursor render is the visible cursor -
    // mirroring there produced a second cursor.
    if (!s_visible || g_GameState != GS_GAME || !ImGui::GetIO().WantCaptureMouse ||
        mouse_over_assistant_popout())
    {
        g_MouseManager.Hide(false);
        return;
    }

    g_MouseManager.Hide();

    int x = 0, y = 0, w = 0, h = 0;
    CGLTexture *texture = nullptr;
    const uint16_t id = 0x206A + 7;//g_MouseManager.GetGameCursor();
    if (!g_MouseManager.GetCursorDrawRect(id, &x, &y, &w, &h, &texture))
    {
        return;
    }

    // refresh the pixels only when the cursor art (or its size) changed
    if (s_CursorGraphic != id || s_CursorTex == nullptr || s_CursorTex->Width != w ||
        s_CursorTex->Height != h)
    {
        assistant_cursor_reap();
        ImTextureData *tex = assistant_cursor_texture(w, h);
        assistant_cursor_upload(tex, (GLuint)texture->Texture);
        s_CursorGraphic = id;
    }
    else
    {
        assistant_cursor_reap();
    }

    // the foreground list renders above every ImGui window: exactly where the
    // cursor belongs; coordinates are game-window pixels (SDL_GetMouseState
    // space), 1:1 with g_MouseManager.Position - no HiDPI scaling anywhere
    ImGui::GetForegroundDrawList()->AddImage(
        s_CursorTex->GetTexRef(), ImVec2((float)x, (float)y), ImVec2((float)(x + w), (float)(y + h)));
}

// the post-shutdown half of the cursor mirror texture lifecycle (runs from
// AssistantWindow_Shutdown right AFTER ui_shutdown/DestroyContext, next to
// AssistantUI_AfterImGuiShutdown): the GL name was reclaimed by the backend
// teardown inside ui_shutdown (the texture stayed registered, RefCount == 1);
// only now may the CPU-side pixels be freed
static void assistant_cursor_after_imgui_shutdown()
{
    delete s_CursorTex;
    s_CursorTex = nullptr;
    for (ImTextureData *tex : s_CursorRetired)
    {
        delete tex;
    }
    s_CursorRetired.clear();
    s_CursorGraphic = 0;
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

// Sole owner of the SDL text-input state while the assistant is initialized,
// called once per frame from AssistantWindow_DrawOverlay right after ui_newframe
// (the assistant ImGui backend runs first; its SDL2 backend only touches
// SDL_Start/StopTextInput on Android, so this has the final say everywhere).
// Precedence: assistant text field > game console > stop.
// - io.WantTextInput (an assistant InputText is active): keep text input
//   started. The game console receives nothing meanwhile: the event pump in
//   Main.cpp hands SDL_TEXTINPUT to ImGui (AssistantWindow_ProcessEvent
//   consumes it) whenever the assistant wants text, so nothing is typed twice.
// - game stage (GS_GAME/GS_GAME_BLOCKED): the game console (CGameScreen::
//   OnTextInput -> g_EntryPointer->Insert) and text-entry gumps (including
//   CGumpTextEntryDialog on the blocked screen, via CGumpManager::OnTextInput)
//   are fed EXCLUSIVELY from SDL_TEXTINPUT events (WispWindow OnWindowProc),
//   so keep text input started even when the assistant does not want text.
//   The client never toggles the state itself: SDL leaves it enabled from
//   video init, and stopping it here whenever an assistant field lost focus is
//   exactly what muted the game console (no SDL_TEXTINPUT -> cannot talk).
// - otherwise: stopped.
static void sync_text_input(bool assistantWantsText, bool gameStage)
{
    const bool wantText = assistantWantsText || gameStage;
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
    // from AssistantWindow_DrawOverlay (in-game render pass) or from the
    // ToggleAssistant game macro (AssistantWindow_Toggle); both run on the
    // main thread with the game GL context current at the in-game stage
    SDL_Window *window =
        Wisp::g_WispWindow != nullptr ? Wisp::g_WispWindow->m_window : nullptr;
    SDL_GLContext context = SDL_GL_GetCurrentContext();
#if defined(XUO_EMSC)
    // the emscripten WebGL context is current but SDL_GL_GetCurrentContext
    // returns null (SDL does not track it). The GL is functional - rendering
    // works - so skip the null pointer check and let ImGui initialize with
    // the existing context.
    if (window == nullptr && context == nullptr)
    {
        // proceed: the GL is current, ImGui just needs to be set up
    }
    else if (context == nullptr)
    {
        Error(Assistant, "could not initialize: no game window/context");
        return false;
    }
#else
    if (window == nullptr || context == nullptr)
    {
        Error(Assistant, "could not initialize: no game window/context");
        return false;
    }
#endif

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
    // the ImGui SDL2 backend toggles the OS cursor from NewFrame via
    // ImGui_ImplSDL2_UpdateMouseCursor unless ImGuiConfigFlags_NoMouseCursorChange
    // is set; the game draws its own UO software cursor, so the backend must
    // never touch the process cursor - AssistantWindow_DrawOverlay enforces the
    // cursor policy instead
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    s_initialized = true;
    s_sessionStartTicks = g_Ticks;
    xuoassist::AssistantUI_Init(); // journal pane events consumer
    // icon atlas lifecycle is bound to the ImGui context: registered after it
    // exists, released after it is destroyed (see AssistantWindow_Shutdown)
    xuoassist::Icons_Init();
    Info(Assistant, "initialized (ini: %s)", s_iniFile);
    return true;
}

bool AssistantWindow_Show(bool show)
{
    if (!at_game_stage())
        return false;
    if (!s_initialized && !init())
    {
        // log once: the macro falls through to the game instead, and retrying
        // every toggle attempt must not spam the log
        static bool warned = false;
        if (!warned)
        {
            Error(Assistant, "toggle failed: assistant not initialized");
            warned = true;
        }
        return false;
    }
    s_visible = show;
    if (!s_visible)
    {
        // disarm a pending capture field: with the shell hidden nothing would
        // commit it, and the engine would keep eating keys into the buffer
        xuoassist::g_Hotkeys.CancelCapture();
        // hand the cursor back to the client policy (the game window baseline is
        // a hidden OS cursor); the backend no longer re-hides it for us since
        // init() set ImGuiConfigFlags_NoMouseCursorChange
        SDL_ShowCursor(SDL_DISABLE);
    }
    return true;
}

bool AssistantWindow_Toggle()
{
    return AssistantWindow_Show(!AssistantWindow_IsVisible());
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
    // Note: the window toggle is no longer a hardcoded key here - it is the
    // ToggleAssistant game macro (Options -> Macros), run by the macro
    // executor through AssistantWindow_Toggle.
    if (!s_initialized)
        return false; // pop-out windows exist only while initialized

    // ImGui's SDL2 backend must see every event, including the ones belonging
    // to its own platform windows
    ui_process_event(s_ctx, &ev);

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
    // Cursor policy while the overlay is visible: the OS cursor stays hidden
    // inside the game window - the game's UO software cursor is the cursor,
    // including over assistant windows docked in the game viewport - and the
    // desktop cursor shows only over the assistant's own popped-out platform
    // windows. Enforced per frame after NewFrame, where io.MousePos and the
    // viewport list are valid; init() set ImGuiConfigFlags_NoMouseCursorChange
    // so the SDL2 backend never fights this. When the overlay is not visible
    // the assistant leaves the cursor alone entirely: the client owns it.
    if (s_visible)
    {
        // io.MousePos and the viewport list are valid after NewFrame
        const bool overAssistantPopOut = mouse_over_assistant_popout();
        SDL_ShowCursor(overAssistantPopOut ? SDL_ENABLE : SDL_DISABLE);
    }
    xuoassist::AssistantUI_Draw(&s_visible, s_sessionStartTicks);
    // the on-top UO cursor: after every assistant window submission (the
    // foreground list draws above windows anyway), before ui_draw renders the
    // frame; only while visible (see assistant_cursor_draw)
    assistant_cursor_draw();
    ui_draw(s_ctx);

    // belt and braces: ui_draw restores the game context, nothing should have
    // been able to change it since
    if (SDL_GL_GetCurrentWindow() != s_assistantWin.window)
        SDL_GL_MakeCurrent(s_assistantWin.window, s_assistantWin.context);

    sync_text_input(ImGui::GetIO().WantTextInput, at_game_stage());
}

void AssistantWindow_Shutdown()
{
    if (!s_initialized)
        return;

    // leaving the game stage: restore the state the client shipped with (text
    // input enabled from SDL video init) instead of stopping it - the main-menu
    // stage still types through SDL_TEXTINPUT (login/account fields and, on
    // embedded-launcher builds, the launcher's ImGui text fields)
    sync_text_input(true, false);
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
    // the cursor mirror follows the same lifecycle (CPU pixels after the GL
    // names are gone)
    assistant_cursor_after_imgui_shutdown();
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
