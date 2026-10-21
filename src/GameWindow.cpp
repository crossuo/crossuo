// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <SDL_timer.h>
#include <cmath>
#include <cstdio>
#include <common/utils.h> // countof
#include <external/gfx/gfx.h> // win_gfx_context_attrbutes
#include "icon_client.h"

#if defined(XUO_EMSC)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif


#include "GameWindow.h"
#include "CrossUO.h"
#include "ServerList.h"
#include "PressedObject.h"
#include "SelectedObject.h"
#include "ScreenshotBuilder.h"
#include "ClickObject.h"
#include "Target.h"
#include "Managers/ConfigManager.h"
#include "Managers/ScreenEffectManager.h"
#include "Managers/SoundManager.h"
#include "Managers/GumpManager.h"
#include "Managers/PacketManager.h"
#include "Managers/ConnectionManager.h"
#include "Managers/MouseManager.h"
#include "Network/Packets.h"
#include "ScreenStages/MainScreen.h"
#include "ScreenStages/ServerScreen.h"
#include "Gumps/Gump.h"
#include "Walker/PathFinder.h"
#include "Splash.h"


CGameWindow g_GameWindow;

CGameWindow::CGameWindow()
    : m_iRenderDelay(0)
{
}

CGameWindow::~CGameWindow()
{
}

void CGameWindow::SetRenderTimerDelay(int delay)
{
    m_iRenderDelay = delay;
}

bool CGameWindow::OnCreate()
{
    if (!Render_Init(m_window))
    {
        Error(Client, "error initializing OpenGL");
        ShowMessage("Error initializing OpenGL", "Error");
        return false;
    }

    HACKRender_SetViewParams(SetViewParamsCmd{
        0, 0, m_Size.Width, m_Size.Height, m_Size.Width, m_Size.Height, -150, 150 });

    // Present the embedded splash image as soon as there is a GL context: it
    // covers the window while the data files are loaded and the first screen is
    // built, and the presented frame stays on screen for as long as the loading
    // code below does not swap.
    if (Splash_Init())
    {
        Splash_Render();
        Info(Client, "splash screen displayed");
    }

    if (!g_Game.Install())
    {
        return false;
    }

    g_GumpManager.RedrawAll();
    return true;
}

void CGameWindow::OnDestroy()
{
    g_SoundManager.Free();
    g_Game.Uninstall();
}

void CGameWindow::OnResize()
{
    RenderAdd_SetViewParams(
        g_renderCmdList,
        SetViewParamsCmd{
            0, 0, m_Size.Width, m_Size.Height, m_Size.Width, m_Size.Height, -150, 150 });
    g_GumpManager.RedrawAll();
}

void CGameWindow::EmulateOnLeftMouseButtonDown()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_PressedObject.InitLeft(g_SelectedObject);
        if (g_SelectedObject.Object != nullptr || g_GameState == GS_GAME)
        {
            g_MouseManager.LeftDropPosition = g_MouseManager.Position;
            g_CurrentScreen->OnLeftMouseButtonDown();
        }
    }
}

int CGameWindow::GetRenderDelay()
{
    return m_iRenderDelay;
}

void CGameWindow::OnLeftMouseButtonDown()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_GeneratedMouseDown = false;
        if (g_Target.MultiGraphic == 0)
        {
            g_CurrentScreen->SelectObject();
        }
        g_PressedObject.InitLeft(g_SelectedObject);
        if (g_SelectedObject.Object != nullptr || g_GameState == GS_GAME)
        {
            g_CurrentScreen->OnLeftMouseButtonDown();
        }
    }
}

void CGameWindow::OnLeftMouseButtonUp()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        if (g_Target.MultiGraphic == 0)
        {
            g_CurrentScreen->SelectObject();
        }
        //if ((g_SelectedObject.Object() != nullptr && g_SelectedObject.Object() == g_PressedObject.LeftObject && g_SelectedObject.Serial) || g_GameState >= GS_GAME)
        if ((g_SelectedObject.Object != nullptr && (g_SelectedObject.Serial != 0u)) ||
            g_GameState >= GS_GAME)
        {
            g_CurrentScreen->OnLeftMouseButtonUp();
            if (g_MovingFromMouse && g_PressedObject.LeftGump == nullptr)
            {
                g_AutoMoving = true;
            }
        }

        if (g_PressedObject.LeftObject != nullptr && g_PressedObject.LeftObject->IsGUI() &&
            ((CBaseGUI *)g_PressedObject.LeftObject)->Type == GOT_COMBOBOX &&
            g_PressedObject.LeftGump != nullptr)
        {
            g_PressedObject.LeftGump->WantRedraw = true;
        }

        g_PressedObject.ClearLeft();
    }
}

bool CGameWindow::OnLeftMouseButtonDoubleClick()
{
    bool result = false;

    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_PressedObject.InitLeft(g_SelectedObject);
        result = (g_SelectedObject.Object != nullptr &&
                  g_SelectedObject.Object == g_PressedObject.LeftObject) &&
                 g_CurrentScreen->OnLeftMouseButtonDoubleClick();

        if (result)
        {
            g_PressedObject.ClearLeft();
            g_ClickObject.Clear();
        }
    }

    return result;
}

void CGameWindow::OnRightMouseButtonDown()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        if (g_Target.MultiGraphic == 0)
        {
            g_CurrentScreen->SelectObject();
        }
        g_PressedObject.InitRight(g_SelectedObject);
        g_CurrentScreen->OnRightMouseButtonDown();
        if (g_SelectedObject.Gump == nullptr &&
            !(g_MouseManager.Position.X < g_ConfigManager.GameWindowX ||
              g_MouseManager.Position.Y < g_ConfigManager.GameWindowY ||
              g_MouseManager.Position.X >
                  (g_ConfigManager.GameWindowX + g_ConfigManager.GameWindowWidth) ||
              g_MouseManager.Position.Y >
                  (g_ConfigManager.GameWindowY + g_ConfigManager.GameWindowHeight)))
        {
            g_MovingFromMouse = true;
            g_AutoMoving = false;
        }
    }
}

void CGameWindow::OnRightMouseButtonUp()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        if (g_Target.MultiGraphic == 0)
        {
            g_CurrentScreen->SelectObject();
        }
        if ((g_SelectedObject.Object != nullptr &&
             g_SelectedObject.Object == g_PressedObject.RightObject &&
             (g_SelectedObject.Serial != 0u)) ||
            g_GameState >= GS_GAME)
        {
            g_CurrentScreen->OnRightMouseButtonUp();
        }
        g_MovingFromMouse = false;
        g_PressedObject.ClearRight();
    }
}

bool CGameWindow::OnRightMouseButtonDoubleClick()
{
    bool result = false;
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_PressedObject.InitRight(g_SelectedObject);
        result = (g_SelectedObject.Object != nullptr &&
                  g_SelectedObject.Object == g_PressedObject.RightObject) &&
                 g_CurrentScreen->OnRightMouseButtonDoubleClick();

        if (result)
        {
            g_PressedObject.ClearRight();
        }
    }

    return result;
}

void CGameWindow::OnMidMouseButtonDown()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnMidMouseButtonDown();
    }
}

void CGameWindow::OnMidMouseButtonUp()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnMidMouseButtonUp();
    }
}

bool CGameWindow::OnMidMouseButtonDoubleClick()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        return g_CurrentScreen->OnMidMouseButtonDoubleClick();
    }

    return false;
}

void CGameWindow::OnMidMouseButtonScroll(bool up)
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_CurrentScreen->OnMidMouseButtonScroll(up);
    }
}

void CGameWindow::OnDragging()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnDragging();
    }
}

void CGameWindow::OnActivate()
{
    g_Game.ResumeSound();
    SetRenderTimerDelay(g_FrameDelay[WINDOW_ACTIVE]);
}

void CGameWindow::OnDeactivate()
{
    if (!g_ConfigManager.BackgroundSound)
    {
        g_Game.PauseSound();
    }

    if (g_ConfigManager.GetReduceFPSUnactiveWindow())
    {
        SetRenderTimerDelay(g_FrameDelay[WINDOW_INACTIVE]);
    }
}

void CGameWindow::OnTextInput(const TextEvent &ev)
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnTextInput(ev);
    }
}

void CGameWindow::OnKeyDown(const KeyEvent &ev)
{
    const auto key = EvKey(ev);
    const bool acceptKey = true;
    if (acceptKey && g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
#if __APPLE__
#define CMD_KEY g_CmdPressed
#else
#define CMD_KEY g_CtrlPressed
#endif
        if (CMD_KEY && key == SDLK_v && g_EntryPointer != nullptr)
        {
            if (g_GameState == GS_MAIN)
            {
                g_MainScreen.Paste();
            }
            else
            {
                g_EntryPointer->Paste();
            }
        }

        g_CurrentScreen->OnKeyDown(ev);
    }
}

void CGameWindow::OnKeyUp(const KeyEvent &ev)
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnKeyUp(ev);
    }

    const auto key = EvKey(ev);
    if (key == KEY_PRINTSCREEN)
    {
        g_ScreenshotBuilder.SaveScreen();
    }
}

void CGameWindow::OnTimer(uint32_t id)
{
    if (id == FASTLOGIN_TIMER_ID)
    {
        RemoveTimer(FASTLOGIN_TIMER_ID);
        g_Game.Connect();
    }
    else if (id == TOUCH_HOLD_TIMER_ID)
    {
        OnTouchHoldTimer();
    }
}

bool CGameWindow::OnUserMessages(const UserEvent &ev)
{
    switch (ev.code)
    {

        case CGameWindow::MessageID:
        {
            OnTimer(checked_cast<uint32_t>(ev.data1));
            break;
        }

        default:
        {
        }
        break;
    }

    return true;
}

void CGameWindow::SetWindowResizable(const bool windowResizable) const
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_SetWindowResizable(m_window, windowResizable ? SDL_TRUE : SDL_FALSE);
#endif
}

void CGameWindow::RestoreWindow() const
{
    SDL_RestoreWindow(m_window);
}

#if defined(XUO_EMSC)
#include <emscripten.h>
#include <emscripten/html5.h>
#else
bool WebIsTouchDevice()
{
    return false;
}
#endif

// client-side diagnostics: post one-line events to the serving proxy's
// /client-log (shell.html installs Module.clientLog) so touch/gesture/size
// behavior on real devices can be diagnosed from the server log
static void TouchPost(const char *line)
{
#if defined(XUO_EMSC)
    MAIN_THREAD_EM_ASM({ if (Module.clientLog) Module.clientLog(UTF8ToString($0)); }, line);
#else
    (void)line;
#endif
}

// log to the client log AND the proxy diagnostics channel
#define TOUCH_LOG(...)                                                                             \
    do                                                                                             \
    {                                                                                              \
        char touch_buf[256];                                                                       \
        snprintf(touch_buf, sizeof(touch_buf), __VA_ARGS__);                                       \
        Info(Client, "touch: %s", touch_buf);                                                      \
        TouchPost(touch_buf);                                                                      \
    } while (0)



#if defined(XUO_EMSC)
// the browser document is the web "desktop": until the game maximizes, the
// canvas keeps the authored resolution (640x480, like the native default
// window); MaximizeWindow (CGameScreen::Init, on entering the world) flips
// the canvas to track the viewport, like a native window filling the screen
static bool s_webFillViewport = false;

static CSize web_viewport_size()
{
    // DOM access is main-thread only and main() runs on a pthread worker
    // (PROXY_TO_PTHREAD): run the JS on the main browser thread
    const int w = MAIN_THREAD_EM_ASM_INT({ return window.innerWidth; });
    const int h = MAIN_THREAD_EM_ASM_INT({ return window.innerHeight; });
    return CSize(w > 0 ? w : 640, h > 0 ? h : 480);
}

static EM_BOOL web_on_window_resize(int eventType, const EmscriptenUiEvent *event, void *userData)
{
    if (s_webFillViewport && g_GameWindow.m_window != nullptr)
    {
        const CSize s = web_viewport_size();
        char buf[128];
        snprintf(buf, sizeof(buf), "size viewport-resize %dx%d fill=1", s.Width, s.Height);
        TouchPost(buf);
        g_GameWindow.SetSize(s);
    }
    return EM_FALSE;
}

// device reports touch input (phone/tablet browsers, touch-screen laptops);
// the page (shell.html) independently gates its browser-gesture blocking on
// the same test. Shared with game logic that must behave differently on
// touch (e.g. world scaling is always available, no desktop option needed)
bool WebIsTouchDevice()
{
    return MAIN_THREAD_EM_ASM_INT(
               {
                   return (navigator.maxTouchPoints > 0 || 'ontouchstart' in window) ? 1 : 0;
               }) !=
           0;
}
#endif

// ---------------------------------------------------------------------------
// touch gestures (SDL finger events). SDL's own touch->mouse synthesis stays
// ENABLED: it drives the cursor position and the left button from the
// PRIMARY finger through the real event path, so g_MouseManager (which reads
// SDL_GetMouseState) stays consistent - taps, drags and left-holds work
// unchanged. Raw SDL_PushEvent'ed mouse events would NOT update SDL's
// internal state, which is why the cursor and ProcessWalking broke when the
// gesture layer tried to own everything itself.
// The gesture layer below only adds what SDL cannot do:
//   press + hold (stationary, ~650ms)        -> RIGHT press-hold (movement,
//                                               like desktop right-drag)
//   two-finger pinch                         -> mouse wheel steps (zoom)
//   quick two-finger tap                     -> right click
// ---------------------------------------------------------------------------
struct WebTouchFinger
{
    bool active = false;
    SDL_FingerID id = 0;
    float x = 0.0f;
    float y = 0.0f;
};

static const int WEB_TOUCH_MAX_FINGERS = 8;
static WebTouchFinger s_touchFingers[WEB_TOUCH_MAX_FINGERS];
static int s_touchCount = 0;
// primary finger = the one driving the mouse cursor
static bool s_touchPrimaryValid = false;
static SDL_FingerID s_touchPrimary = 0;
static bool s_touchRightHold = false;
static float s_touchTravel = 0.0f;
static uint32_t s_touchDownTicks = 0;
// short-term finger movement (decayed per motion event): hold conversion
// gates on this instead of accumulated travel, so slow fingertip drift over
// a long hold never blocks the conversion
static float s_touchRecentDrift = 0.0f;
// multi-touch state
static bool s_touchPinch = false;
static float s_touchPinchDist = 0.0f;
static float s_touchPinchTravel = 0.0f;
static uint32_t s_touchPinchStart = 0;

// tap/hold thresholds (pixels / milliseconds). The move slop is generous:
// fingertips jitter far more than a mouse (10px was disarming the hold
// timer almost immediately on tablets)
static const float WEB_TOUCH_MOVE_SLOP = 22.0f;
static const uint32_t WEB_TOUCH_HOLD_MS = 400;
static const uint32_t WEB_TOUCH_TAP_MS = 500;
// total travel budget for the two-finger tap: two fingertips jitter, the
// budget is shared (16px single-finger style was too strict on tablets)
static const float WEB_TOUCH_TAP_TRAVEL = 28.0f;
// the hold timer is armed on primary-down and disarmed on lift/drag; the
// flag (not just SDL timer removal) guards against an ALREADY-QUEUED timer
// event converting the NEXT tap into an instant right-hold
static bool s_touchHoldArmed = false;

static WebTouchFinger *TouchFind(SDL_FingerID id)
{
    for (int i = 0; i < WEB_TOUCH_MAX_FINGERS; i++)
    {
        if (s_touchFingers[i].active && s_touchFingers[i].id == id)
        {
            return &s_touchFingers[i];
        }
    }
    return nullptr;
}

static WebTouchFinger *TouchAcquire(SDL_FingerID id)
{
    for (int i = 0; i < WEB_TOUCH_MAX_FINGERS; i++)
    {
        if (!s_touchFingers[i].active)
        {
            s_touchFingers[i].active = true;
            s_touchFingers[i].id = id;
            return &s_touchFingers[i];
        }
    }
    return nullptr;
}

static WebTouchFinger *TouchOther(const WebTouchFinger *finger)
{
    for (int i = 0; i < WEB_TOUCH_MAX_FINGERS; i++)
    {
        if (&s_touchFingers[i] != finger && s_touchFingers[i].active)
        {
            return &s_touchFingers[i];
        }
    }
    return nullptr;
}

static float TouchDist(const WebTouchFinger *a, const WebTouchFinger *b)
{
    const float dx = a->x - b->x;
    const float dy = a->y - b->y;
    return std::sqrt(dx * dx + dy * dy);
}

// the gesture layer owns the cursor position for right clicks: g_MouseManager
// mirrors SDL's mouse state (primary finger only), so write the position
// directly instead of calling Update()
static void TouchRightClick(int x, int y)
{
    TOUCH_LOG("right click at %d,%d", x, y);
    g_MouseManager.Position = CPoint2Di(x, y);
    g_MouseManager.Capture();
    g_MouseManager.RightButtonPressed = true;
    g_MouseManager.CancelDoubleClick = true;
    g_GameWindow.OnRightMouseButtonDown();
    g_GameWindow.OnRightMouseButtonUp();
    g_MouseManager.RightButtonPressed = false;
    g_MouseManager.Release();
    g_MouseManager.LastRightButtonClickTimer = SDL_GetTicks();
}

// the right button is not synthesized by SDL, so it goes in as a raw pushed
// event; that is fine because right-button logic only reads the
// g_MouseManager flags the client handlers maintain, never SDL's state
static void TouchSynthButton(Uint8 button, bool down, int x, int y)
{
    TOUCH_LOG(
        "synth %s button %d at %d,%d", down ? "DOWN" : "UP", (int)button, x, y);
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    ev.button.windowID = SDL_GetWindowID(g_GameWindow.m_window);
    ev.button.button = button;
    ev.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.button.clicks = 1;
    ev.button.x = x;
    ev.button.y = y;
    SDL_PushEvent(&ev);
}

static void TouchDisarmHold()
{
    s_touchHoldArmed = false;
    g_GameWindow.RemoveTimer(TOUCH_HOLD_TIMER_ID);
}

// stationary press for WEB_TOUCH_HOLD_MS turns into a right-button
// press-hold (the desktop "hold right + move" walk control)
// convert the pending left press into a right press-hold (the desktop
// "hold right + move" walk control). Called from the hold timer and from
// motion: gates on RECENT drift, so a still finger converts no matter how
// far it drifted overall, while an actively moving finger never converts
// (item drags keep working)
void CGameWindow::TouchTryConvertHold()
{
    if (!s_touchHoldArmed)
    {
        return; // not waiting (stale queued timer, already converted, ...)
    }
    if (!s_touchPrimaryValid || s_touchPinch || s_touchRightHold)
    {
        return;
    }
    if (!g_MouseManager.LeftButtonPressed)
    {
        return; // SDL's synthesized left press is gone; not a plain hold
    }
    if (g_SelectedObject.Gump != nullptr)
    {
        // press started on a gump: the left press IS the gump drag, keep it
        // (converting would rip the drag away and the later right-up would
        // act as a right click on the gump - e.g. closing it)
        return;
    }
    if (s_touchRecentDrift >= WEB_TOUCH_MOVE_SLOP)
    {
        return; // the finger is moving right now: still a left drag
    }
    const WebTouchFinger *finger = TouchFind(s_touchPrimary);
    if (finger == nullptr)
    {
        return;
    }
    s_touchHoldArmed = false;
    const int x = (int)finger->x;
    const int y = (int)finger->y;
    TOUCH_LOG("hold -> right-hold at %d,%d (drift=%.1f)", x, y, s_touchRecentDrift);
    TouchSynthButton(SDL_BUTTON_LEFT, false, x, y);
    TouchSynthButton(SDL_BUTTON_RIGHT, true, x, y);
    s_touchRightHold = true;
    // movement (ProcessWalking) is gated on the cursor being inside the
    // game window rect and OnRightMouseButtonDown arming g_MovingFromMouse;
    // log the geometry so a dead hold-to-move is diagnosable from the log
    TOUCH_LOG(
        "game window x=%d y=%d w=%d h=%d, cursor=%d,%d", g_ConfigManager.GameWindowX,
        g_ConfigManager.GameWindowY, g_ConfigManager.GameWindowWidth,
        g_ConfigManager.GameWindowHeight, g_MouseManager.Position.X, g_MouseManager.Position.Y);
}

void CGameWindow::OnTouchHoldTimer()
{
    RemoveTimer(TOUCH_HOLD_TIMER_ID);
    TOUCH_LOG(
        "hold timer fired (primary=%d pinch=%d rightHold=%d left=%d drift=%.1f)",
        s_touchPrimaryValid ? 1 : 0, s_touchPinch ? 1 : 0, s_touchRightHold ? 1 : 0,
        g_MouseManager.LeftButtonPressed ? 1 : 0, s_touchRecentDrift);
    TouchTryConvertHold();
}

bool CGameWindow::OnTouchFinger(const SDL_TouchFingerEvent &ev)
{
    bool isPrimary = s_touchPrimaryValid && ev.fingerId == s_touchPrimary;
    WebTouchFinger *finger = TouchFind(ev.fingerId);
    const float x = ev.x * m_Size.Width;
    const float y = ev.y * m_Size.Height;

    switch (ev.type)
    {
        case SDL_FINGERDOWN:
        {
            TOUCH_LOG(
                "raw down id=%llu at %d,%d (count=%d primary=%d)",
                (unsigned long long)ev.fingerId, (int)x, (int)y, s_touchCount,
                s_touchPrimaryValid ? 1 : 0);
            if (finger == nullptr)
            {
                finger = TouchAcquire(ev.fingerId);
                if (finger == nullptr)
                {
                    return true;
                }
                s_touchCount++;
            }
            finger->x = x;
            finger->y = y;

            if (s_touchCount == 2)
            {
                // multi-touch gesture start: cancel the primary finger's
                // pending left press / hold so pinching never drags
                TouchDisarmHold();
                if (s_touchRightHold)
                {
                    TouchSynthButton(SDL_BUTTON_RIGHT, false, (int)x, (int)y);
                    s_touchRightHold = false;
                }
                else if (g_MouseManager.LeftButtonPressed)
                {
                    // cancel the synthesized left press so the gesture does
                    // not leave a drag behind
                    TouchSynthButton(SDL_BUTTON_LEFT, false, (int)x, (int)y);
                }
                WebTouchFinger *first = TouchOther(finger);
                s_touchPinch = true;
                s_touchPinchDist = first != nullptr ? TouchDist(first, finger) : 0.0f;
                s_touchPinchTravel = 0.0f;
                s_touchPinchStart = SDL_GetTicks();
                TOUCH_LOG(
                    "gesture start (2 fingers), base spread %.0fpx", s_touchPinchDist);
            }
            else if (s_touchCount == 1 && !s_touchPrimaryValid)
            {
                // primary finger down: SDL synthesizes cursor motion + the
                // left press (tap/drag); the hold timer may convert the
                // press into a right press-hold later
                s_touchPrimaryValid = true;
                s_touchPrimary = ev.fingerId;
                isPrimary = true;
                s_touchTravel = 0.0f;
                s_touchRecentDrift = 0.0f;
                s_touchDownTicks = SDL_GetTicks();
                s_touchHoldArmed = true;
#if defined(XUO_EMSC)
                if (s_webFillViewport &&
                    (g_ConfigManager.GameWindowX != 0 || g_ConfigManager.GameWindowY != 0 ||
                     g_ConfigManager.GameWindowWidth != m_Size.Width ||
                     g_ConfigManager.GameWindowHeight != m_Size.Height))
                {
                    // the saved per-account config can restore a stale game
                    // window rect after the maximize sync (config loads at
                    // login complete, after GameScreen::Init maximized);
                    // re-sync at interaction time so zoom/movement/walking
                    // gates always match the maximized canvas
                    g_ConfigManager.GameWindowX = 0;
                    g_ConfigManager.GameWindowY = 0;
                    g_ConfigManager.GameWindowWidth = m_Size.Width;
                    g_ConfigManager.GameWindowHeight = m_Size.Height;
                    if (!g_ConfigManager.GetUseScaling())
                    {
                        g_ConfigManager.SetUseScaling(true);
                    }
                    TOUCH_LOG(
                        "game window rect re-synced to %dx%d", m_Size.Width, m_Size.Height);
                }
#endif
                CreateTimer(TOUCH_HOLD_TIMER_ID, WEB_TOUCH_HOLD_MS);
                TOUCH_LOG(
                    "primary down %d,%d (hold armed) size=%dx%d", (int)x, (int)y, m_Size.Width,
                    m_Size.Height);
            }
            else if (s_touchCount >= 2)
            {
                TOUCH_LOG(
                    "finger down #%d id=%llu at %d,%d", s_touchCount,
                    (unsigned long long)ev.fingerId, (int)x, (int)y);
            }
        }
        break;

        case SDL_FINGERMOTION:
        {
            if (finger == nullptr)
            {
                return true;
            }
            const float dx = x - finger->x;
            const float dy = y - finger->y;
            const float moved = std::sqrt(dx * dx + dy * dy);
            finger->x = x;
            finger->y = y;

            if (isPrimary)
            {
                s_touchTravel += moved;
                s_touchRecentDrift = s_touchRecentDrift * 0.6f + moved;
                if (s_touchHoldArmed && s_touchRecentDrift < WEB_TOUCH_MOVE_SLOP)
                {
                    // finger essentially still with the hold time already
                    // elapsed: convert now (the one-shot timer handles the
                    // stationary case; this handles drift-then-still)
                    const uint32_t held = SDL_GetTicks() - s_touchDownTicks;
                    if (held >= WEB_TOUCH_HOLD_MS)
                    {
                        TouchTryConvertHold();
                    }
                }
            }
            else
            {
                s_touchPinchTravel += moved;
            }

            if (s_touchPinch && s_touchCount == 2)
            {
                WebTouchFinger *first = TouchOther(finger);
                const float dist = first != nullptr ? TouchDist(first, finger) : 0.0f;
                static uint32_t s_pinchLogTicks = 0;
                const uint32_t now = SDL_GetTicks();
                if (now - s_pinchLogTicks > 250)
                {
                    s_pinchLogTicks = now;
                    TOUCH_LOG(
                        "pinch motion spread=%.0f (base=%.0f)", dist, s_touchPinchDist);
                }
                if (s_touchPinchDist > 0.0f && dist > 0.0f && !WebIsTouchDevice())
                {
                    // one wheel step per ~15% of spread change. On touch
                    // devices zoom belongs to the BROWSER (page pinch zoom,
                    // see shell.html touch-action) - game-scale wheel steps
                    // here would double-zoom on top of it
                    while (dist > s_touchPinchDist * 1.15f)
                    {
                        TOUCH_LOG("pinch zoom in dist=%.0f", dist);
                        OnMidMouseButtonScroll(true);
                        s_touchPinchDist *= 1.15f;
                    }
                    while (dist < s_touchPinchDist / 1.15f)
                    {
                        TOUCH_LOG("pinch zoom out dist=%.0f", dist);
                        OnMidMouseButtonScroll(false);
                        s_touchPinchDist /= 1.15f;
                    }
                }
            }
        }
        break;

        case SDL_FINGERUP:
        {
            TOUCH_LOG(
                "raw up id=%llu at %d,%d (count=%d primary=%d)", (unsigned long long)ev.fingerId,
                (int)x, (int)y, s_touchCount, s_touchPrimaryValid ? 1 : 0);
            if (finger == nullptr)
            {
                return true;
            }
            const bool wasPrimary = isPrimary || s_touchCount == 1;
            finger->active = false;
            s_touchCount--;

            if (s_touchCount == 0)
            {
                if (s_touchPinch)
                {
                    // last-resort tap evaluation (both fingers lifted in one
                    // event batch); the normal path is the first-lift check
                    // in the count==1 branch below
                    const uint32_t elapsed = SDL_GetTicks() - s_touchPinchStart;
                    if (elapsed < WEB_TOUCH_TAP_MS && s_touchPinchTravel < WEB_TOUCH_TAP_TRAVEL)
                    {
                        TOUCH_LOG("two-finger tap -> right click (last lift)");
                        TouchRightClick((int)x, (int)y);
                    }
                    s_touchPinch = false;
                }
                else if (wasPrimary)
                {
                    TOUCH_LOG(
                        "primary up at %d,%d (rightHold=%d, travel=%.1f)", (int)x, (int)y,
                        s_touchRightHold ? 1 : 0, s_touchTravel);
                    TouchDisarmHold();
                    if (s_touchRightHold)
                    {
                        TouchSynthButton(SDL_BUTTON_RIGHT, false, (int)x, (int)y);
                        s_touchRightHold = false;
                    }
                    // auto-move (walk-on after a left click during a right
                    // drag) is a desktop affordance: on touch every tap is a
                    // left click and it would latch walking forever - the
                    // lift must always release the movement
                    g_AutoMoving = false;
                }
                s_touchPrimaryValid = false;
                s_touchPrimary = 0;
            }
            else if (s_touchCount == 1 && s_touchPinch)
            {
                // one finger went up mid-gesture. Evaluate the two-finger tap
                // HERE: this first lift is the last moment the gesture state
                // is intact (waiting for the second lift would clear the
                // pinch flag first and the tap would never fire)
                const uint32_t elapsed = SDL_GetTicks() - s_touchPinchStart;
                if (elapsed < WEB_TOUCH_TAP_MS && s_touchPinchTravel < WEB_TOUCH_TAP_TRAVEL)
                {
                    TOUCH_LOG("two-finger tap -> right click");
                    TouchRightClick((int)x, (int)y);
                }
                s_touchPinch = false;
                if (wasPrimary)
                {
                    // the primary finger left the pinch; the remaining finger
                    // no longer drives the cursor until it is re-pressed
                    s_touchPrimaryValid = false;
                    s_touchPrimary = 0;
                }
            }
        }
        break;

        default:
            break;
    }

    return true; // all finger events are consumed by the gesture layer
}



void CGameWindow::SetMinSize(int width, int height)
{
    m_MinSize.Width = width;
    m_MinSize.Height = height;
}
void CGameWindow::SetMaxSize(int width, int height)
{
    m_MaxSize.Width = width;
    m_MaxSize.Height = height;
}

void CGameWindow::MaximizeWindow()
{
#if defined(XUO_EMSC)
    // native maximizes into the desktop; on web the desktop is the browser
    // document. SetSize drives SDL, which resizes the canvas drawing buffer
    // and sends the resize event the game already handles (same as native)
    s_webFillViewport = true;
    SetSize(web_viewport_size());
    return;
#endif
    SDL_RestoreWindow(m_window);
    SDL_MaximizeWindow(m_window);
}

void CGameWindow::SetSize(const CSize &size)
{
#if defined(XUO_EMSC)
    // catch degenerate sizes (a 0x0 canvas renders as a thin line/nothing)
    if (size.Width < 50 || size.Height < 50)
    {
        TOUCH_LOG("SIZE-REJECT %dx%d", size.Width, size.Height);
        return;
    }
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "size set %dx%d fill=%d", size.Width, size.Height,
                 s_webFillViewport ? 1 : 0);
        TouchPost(buf);
    }
#endif
    SDL_SetWindowSize(m_window, size.Width, size.Height);
    m_Size = size;
#if defined(XUO_EMSC)
    if (s_webFillViewport)
    {
        // the maximized canvas IS the game window: the world render and the
        // hit tests (zoom, movement, walking all gate on this rect) must
        // track it - the saved per-account rect can never match the dynamic
        // mobile viewport
        g_ConfigManager.GameWindowX = 0;
        g_ConfigManager.GameWindowY = 0;
        g_ConfigManager.GameWindowWidth = size.Width;
        g_ConfigManager.GameWindowHeight = size.Height;
        // world zoom (pinch) renders through g_GlobalScale only when the
        // desktop UseScaling option is on; force it on for touch
        if (!g_ConfigManager.GetUseScaling())
        {
            g_ConfigManager.SetUseScaling(true);
        }
    }
#endif
}

void CGameWindow::SetPositionSize(int x, int y, int width, int height)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    int borderH;
    SDL_GetWindowBordersSize(m_window, &borderH, nullptr, nullptr, nullptr);
#else
    int borderH = 0;
#endif
    SDL_RestoreWindow(m_window);
    SDL_SetWindowPosition(m_window, x, std::max(y, borderH));
    SDL_SetWindowSize(m_window, width, height);
}

void CGameWindow::GetPositionSize(int *x, int *y, int *width, int *height)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    int borderH;
    SDL_GetWindowBordersSize(m_window, &borderH, nullptr, nullptr, nullptr);
#else
    int borderH = 0;
#endif
    SDL_GetWindowPosition(m_window, x, y);
    *y = std::max(*y, borderH);
    SDL_GetWindowSize(m_window, width, height);
}

bool CGameWindow::Create(const char *title, bool showCursor, int width, int height)
{
#ifdef OGL_DEBUGCONTEXT_ENABLED
    auto debugContext = true;
#else
    auto debugContext = false;
#endif
    if (debugContext)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    }
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        return false;
    }

    m_Size.Width = width;
    m_Size.Height = height;
    Uint32 window_flags = SDL_WINDOW_RESIZABLE;
#if !defined(XUO_EMSC)
    // on web the window is a plain event/size source: the WebGL2 context is
    // created and bound directly on the canvas by the renderer
    window_flags |= SDL_WINDOW_OPENGL;
#endif
    m_window = SDL_CreateWindow(title, 0, 0, width, height, window_flags);
    if (m_window == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Coult not create window: %s\n", SDL_GetError());
        return false;
    }

    uint32_t rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    int shift = (g_icon.bytes_per_pixel == 3) ? 8 : 0;
    rmask = 0xff000000 >> shift;
    gmask = 0x00ff0000 >> shift;
    bmask = 0x0000ff00 >> shift;
    amask = 0x000000ff >> shift;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = (g_icon.bytes_per_pixel == 3) ? 0 : 0xff000000;
#endif

    SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(
        (void *)g_icon.pixel_data,
        g_icon.width,
        g_icon.height,
        g_icon.bytes_per_pixel * 8,
        g_icon.bytes_per_pixel * g_icon.width,
        rmask,
        gmask,
        bmask,
        amask);
    SDL_SetWindowIcon(m_window, icon);
    SDL_FreeSurface(icon);

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(m_window, &info) != 0u)
    {
        Info(Client, "SDL %d.%d.%d", info.version.major, info.version.minor, info.version.patch);

        const char *subsystem = "Unknown";
        switch (info.subsystem)
        {
            case SDL_SYSWM_UNKNOWN:
                break;
#if SDL_VERSION_ATLEAST(2, 0, 5)
            case SDL_SYSWM_OS2:
                subsystem = "IBM OS/2";
                break;
#endif
            case SDL_SYSWM_WINDOWS:
                subsystem = "Microsoft Windows(TM)";
                break;
            case SDL_SYSWM_X11:
                subsystem = "X Window System";
                break;
#if SDL_VERSION_ATLEAST(2, 0, 3)
            case SDL_SYSWM_WINRT:
                subsystem = "WinRT";
                break;
#endif
            case SDL_SYSWM_DIRECTFB:
                subsystem = "DirectFB";
                break;
            case SDL_SYSWM_COCOA:
                subsystem = "Apple OS X";
                break;
            case SDL_SYSWM_UIKIT:
                subsystem = "UIKit";
                break;
#if SDL_VERSION_ATLEAST(2, 0, 2)
            case SDL_SYSWM_WAYLAND:
                subsystem = "Wayland";
                break;
            case SDL_SYSWM_MIR:
                subsystem = "Mir";
                break;
#endif
#if SDL_VERSION_ATLEAST(2, 0, 4)
            case SDL_SYSWM_ANDROID:
                subsystem = "Android";
                break;
#endif
#if SDL_VERSION_ATLEAST(2, 0, 5)
            case SDL_SYSWM_VIVANTE:
                subsystem = "Vivante";
                break;
#endif
            default:
                break;
        }

        Info(Client, "System: %s", subsystem);
#if defined(XUO_WINDOWS)
#endif
    }
    //SDL_SetWindowGrab(m_window, SDL_TRUE);
    SDL_RaiseWindow(m_window);
    SDL_ShowCursor(static_cast<int>(showCursor));

#if defined(XUO_EMSC)
    // keep filling the viewport after the game maximized (browser window
    // resize, devtools, device rotation); registered on the main browser
    // thread, the callback is delivered back here like SDL's own handlers
    emscripten_set_resize_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, web_on_window_resize);
    Info(Client, "Web touch device: %s", WebIsTouchDevice() ? "yes" : "no");
#endif

    return OnCreate();
}

void CGameWindow::Destroy()
{
    if (m_window != nullptr)
    {
        PushEvent(SDL_QUIT, nullptr, nullptr);
    }
}

void CGameWindow::ShowMessage(const astr_t &text, const astr_t &title)
{
    SDL_Log("%s: %s\n", title.c_str(), text.c_str());
}

void CGameWindow::ShowMessage(const wstr_t &text, const wstr_t &title)
{
    SDL_Log("%ls: %ls\n", title.c_str(), text.c_str());
}

bool CGameWindow::OnWindowProc(SDL_Event &ev)
{
    // only handle events belonging to this window; other windows in the
    // process (like the embedded launcher) have their own event handling.
    // Events without a window id (like SDL_QUIT) remain process-global.
    const auto id = SDL_GetWindowID(m_window);
    switch (ev.type)
    {
        case SDL_WINDOWEVENT:
            if (ev.window.windowID != id)
                return false;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (ev.key.windowID != id)
                return false;
            break;
        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:
            if (ev.text.windowID != id)
                return false;
            break;
        case SDL_MOUSEMOTION:
            if (ev.motion.windowID != id)
                return false;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (ev.button.windowID != id)
                return false;
            break;
        case SDL_MOUSEWHEEL:
            if (ev.wheel.windowID != id)
                return false;
            break;
            // NOTE: SDL finger events are deliberately NOT windowID-filtered:
            // on touch-only devices (phones/tablets) there is no mouse focus,
            // so SDL tags finger events with windowID 0 and a strict filter
            // would silently drop every touch gesture
        default:
            break;
    }

    switch (ev.type)
    {
        case SDL_WINDOWEVENT:
        {
            switch (ev.window.event)
            {
                case SDL_WINDOWEVENT_CLOSE:
                {
                    OnDestroy();
                    return true;
                }
                break;

                case SDL_WINDOWEVENT_SHOWN:
                {
                    OnActivate(); // Sound + FPS
                }
                break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_RESIZED:
                {
#if defined(XUO_EMSC)
                    if (ev.window.data1 < 50 || ev.window.data2 < 50)
                    {
                        TOUCH_LOG(
                            "SIZE-DEGENERATE windowevent %dx%d", ev.window.data1,
                            ev.window.data2);
                    }
#endif
                    m_Size = { ev.window.data1, ev.window.data2 };
                    OnResize();
                }
                break;

                case SDL_WINDOWEVENT_MAXIMIZED:
                {
                    OnResize();
                }
                break;

                case SDL_WINDOWEVENT_RESTORED:
                {
                    int x, y, borderH = 0;
#if SDL_VERSION_ATLEAST(2, 0, 5)
                    SDL_GetWindowBordersSize(m_window, &borderH, nullptr, nullptr, nullptr);
#endif
                    SDL_GetWindowPosition(m_window, &x, &y);
                    SDL_SetWindowPosition(m_window, x, std::max(y, borderH));
                    OnResize();
                }
                break;

                case SDL_WINDOWEVENT_HIDDEN:
                {
                    OnDeactivate(); // Sound + FPS
                }
                break;

                default:
                    break;
            }
        }
        break;

        case SDL_QUIT:
        {
            OnDestroy();
            return true;
            break;
        }

        case SDL_KEYDOWN:
        {
            OnKeyDown(ev.key);
        }
        break;

        case SDL_KEYUP:
        {
            OnKeyUp(ev.key);
        }
        break;

        // https://wiki.libsdl.org/Tutorials/TextInput
        case SDL_TEXTINPUT: // WM_CHAR
        {
            OnTextInput(ev.text);
        }
        break;

        case SDL_MOUSEMOTION:
        {
            g_MouseManager.Update();
            if (g_MouseManager.Dragging)
            {
                OnDragging();
            }
        }
        break;

        case SDL_MOUSEWHEEL:
        {
            g_MouseManager.Update();

            const bool isUp = ev.wheel.y < 0;
            OnMidMouseButtonScroll(isUp);
        }
        break;

        case SDL_USEREVENT:
        {
            if (ev.user.code == SDL_QUIT)
            {
                OnDestroy();
                return true;
            }
            OnUserMessages(ev.user);
        }
        break;

        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
        {
            g_MouseManager.Update();
            const bool isDown = ev.type == SDL_MOUSEBUTTONDOWN;
            auto &mouse = ev.button;
            switch (mouse.button)
            {
                case SDL_BUTTON_LEFT:
                    if (isDown)
                    {
                        g_MouseManager.Capture();
                        g_MouseManager.LeftButtonPressed = true;
                        g_MouseManager.LeftDropPosition = g_MouseManager.Position;
                        g_MouseManager.CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (g_MouseManager.LastLeftButtonClickTimer +
                                g_MouseManager.DoubleClickDelay >=
                            ticks)
                        {
                            g_MouseManager.LastLeftButtonClickTimer = 0;
                            if (!OnLeftMouseButtonDoubleClick())
                            {
                                OnLeftMouseButtonDown();
                            }
                            else
                            {
                                g_MouseManager.LastLeftButtonClickTimer = 0xFFFFFFFF;
                            }
                            break;
                        }

                        OnLeftMouseButtonDown();

                        if (g_MouseManager.CancelDoubleClick)
                        {
                            g_MouseManager.LastLeftButtonClickTimer = 0;
                        }
                        else
                        {
                            g_MouseManager.LastLeftButtonClickTimer = ticks;
                        }
                        break;
                    }
                    else
                    {
                        if (g_MouseManager.LastLeftButtonClickTimer != 0xFFFFFFFF)
                        {
                            OnLeftMouseButtonUp();
                        }
                        g_MouseManager.LeftButtonPressed = false;
                        g_MouseManager.Release();
                    }
                    break;

                case SDL_BUTTON_MIDDLE:
                    if (isDown)
                    {
                        g_MouseManager.Capture();
                        g_MouseManager.MidButtonPressed = true;
                        g_MouseManager.CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (g_MouseManager.LastMidButtonClickTimer +
                                g_MouseManager.DoubleClickDelay >=
                            ticks)
                        {
                            if (!OnMidMouseButtonDoubleClick())
                            {
                                OnMidMouseButtonDown();
                            }

                            g_MouseManager.LastMidButtonClickTimer = 0;
                            break;
                        }

                        OnMidMouseButtonDown();

                        if (g_MouseManager.CancelDoubleClick)
                        {
                            g_MouseManager.LastMidButtonClickTimer = 0;
                        }
                        else
                        {
                            g_MouseManager.LastMidButtonClickTimer = ticks;
                        }
                    }
                    else
                    {
                        OnMidMouseButtonUp();
                        g_MouseManager.MidButtonPressed = false;
                        g_MouseManager.Release();
                    }
                    break;

                case SDL_BUTTON_RIGHT:
                    if (isDown)
                    {
                        g_MouseManager.Capture();
                        g_MouseManager.RightButtonPressed = true;
                        g_MouseManager.CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (g_MouseManager.LastRightButtonClickTimer +
                                g_MouseManager.DoubleClickDelay >=
                            ticks)
                        {
                            g_MouseManager.LastRightButtonClickTimer = 0;
                            if (!OnRightMouseButtonDoubleClick())
                            {
                                OnRightMouseButtonDown();
                            }
                            else
                            {
                                g_MouseManager.LastRightButtonClickTimer = 0xFFFFFFFF;
                            }
                            break;
                        }

                        OnRightMouseButtonDown();

                        if (g_MouseManager.CancelDoubleClick)
                        {
                            g_MouseManager.LastRightButtonClickTimer = 0;
                        }
                        else
                        {
                            g_MouseManager.LastRightButtonClickTimer = ticks;
                        }
                    }
                    else
                    {
                        if (g_MouseManager.LastRightButtonClickTimer != 0xFFFFFFFF)
                        {
                            OnRightMouseButtonUp();
                        }
                        g_MouseManager.RightButtonPressed = false;
                        g_MouseManager.Release();
                    }
                    break;

                case SDL_BUTTON_X2:
                    break;

                default:
                    break;
            }
        }
        break;

        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION:
        {
            OnTouchFinger(ev.tfinger);
        }
        break;

        default:
            break;
    }

    return false;
}

bool CGameWindow::IsActive() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

void CGameWindow::SetTitle(const astr_t &text) const
{
    SDL_SetWindowTitle(m_window, text.c_str());
}

bool CGameWindow::IsMinimizedWindow() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
}

bool CGameWindow::IsMaximizedWindow() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
}

static SDL_TimerID timer_table[TOUCH_HOLD_TIMER_ID] = {};

static uint32_t TimerCallbackEventPusher(uint32_t interval, void *param)
{
    PUSH_EVENT(CGameWindow::MessageID, param, nullptr);
    return interval;
}

void CGameWindow::CreateTimer(uint32_t id, int delay)
{
    assert(id > 0 && id < countof(timer_table));
    if (timer_table[id - 1] != 0)
    {
        return;
    }

    auto handle = SDL_AddTimer(delay, TimerCallbackEventPusher, (void *)(intptr_t)(id));
    timer_table[id - 1] = handle;
}

void CGameWindow::RemoveTimer(uint32_t id)
{
    assert(id < countof(timer_table));
    if (timer_table[id - 1] != 0)
    {
        SDL_RemoveTimer(timer_table[id - 1]);
    }
    timer_table[id - 1] = 0;
}

void CGameWindow::Raise()
{
    SDL_RaiseWindow(g_GameWindow.m_window);
}

uint32_t CGameWindow::PushEvent(uint32_t id, void *data1, void *data2)
{
    SDL_UserEvent userevent;
    userevent.type = SDL_USEREVENT;
    userevent.code = id;
    userevent.data1 = data1;
    userevent.data2 = data2;

    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user = userevent;
    SDL_PushEvent(&event);

    return 0;
}

void GetDisplaySize(int *x, int *y)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_Rect r;
    SDL_GetDisplayUsableBounds(0, &r);
    if (x != nullptr)
    {
        *x = r.w;
    }
    if (y != nullptr)
    {
        *y = r.h;
    }
#else
    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);
    if (x != nullptr)
    {
        *x = dm.w;
    }
    if (y != nullptr)
    {
        *y = dm.h;
    }
#endif
}
