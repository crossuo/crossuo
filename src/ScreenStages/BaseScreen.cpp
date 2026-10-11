// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "BaseScreen.h"
#include "../SelectedObject.h"
#include "../Managers/MouseManager.h"
#include "VfsStatusHud.h"
#include "../Managers/ScreenEffectManager.h"
#include "ScreenshotBuilder.h"
#include "../Renderer/RenderAPI.h"

CBaseScreen *g_CurrentScreen = nullptr;

CBaseScreen::CBaseScreen(CGump &gump)
    : m_Gump(gump)
{
}

CBaseScreen::~CBaseScreen()
{
}

void CBaseScreen::Init()
{
}

void CBaseScreen::Render()
{
#if defined(USE_GL3) || defined(USE_GLES)
    Render_ResetCmdList(g_renderCmdList, Render_DefaultState());
    g_FontManager.ClearTextSpritePool(); // Clear previous frame's text sprites
    RenderAdd_FlushState(g_renderCmdList);
    RenderAdd_ClearRT(g_renderCmdList, ClearRTCmd{});
#else
    Render_GL1BeginDraw();
#endif // #if defined(USE_GL3) || defined(USE_GLES)

    if (DrawSmoothMonitor() != 0)
    {
        return;
    }

    // FlushState command above already sets color to white with alpha 100%
    m_Gump.Draw();
#if defined(USE_GL3) || defined(USE_GLES)
    DebugScreen();
#endif
    InitToolTip();
    DrawSmoothMonitorEffect();
    CVfsStatusHud::Draw(); // streamed-install HUD, top-right (gone when idle)
    g_MouseManager.Draw(CursorGraphic);
#if defined(USE_GL3) || defined(USE_GLES)
    RenderDebug_ProcessFrame(g_renderCmdList);
    RenderDraw_Execute(g_renderCmdList);
    // swap (which submits the leftover batched text/cursor quads) must run
    // BEFORE the sprite pool is cleared, or the flush binds deleted textures
    Render_SwapBuffers();
    g_FontManager.ClearTextSpritePool(); // Clear text sprites (GPU is done with them)
    g_ScreenshotBuilder.GPUDataReady();
#else
    Render_GL1EndDraw();
#endif // #if defined(USE_GL3) || defined(USE_GLES)
}

void CBaseScreen::SelectObject()
{
    g_SelectedObject.Clear();
    CRenderObject *selected = m_Gump.Select();
    if (selected != nullptr)
    {
        g_SelectedObject.Init(selected, &m_Gump);
    }

    if (g_SelectedObject.Object != g_LastSelectedObject.Object)
    {
        if (g_SelectedObject.Object != nullptr)
        {
            g_SelectedObject.Object->OnMouseEnter();
        }

        if (g_LastSelectedObject.Object != nullptr)
        {
            g_LastSelectedObject.Object->OnMouseExit();
        }
    }

    g_LastSelectedObject.Init(g_SelectedObject);
}

int CBaseScreen::DrawSmoothMonitor()
{
    if ((g_ScreenEffectManager.Process() != 0) && (SmoothScreenAction != 0u))
    {
        // Consume the queued action BEFORE dispatching it. The action handler
        // may call InitScreen (CGame::Connect -> InitScreen(GS_MAIN_CONNECT)),
        // but that screen swap - and therefore the SmoothScreenAction = 0 in
        // the new screen's Init() - only lands after this frame. Without
        // clearing here, every frame that still reports effect progress
        // re-enters this branch and re-fires the SAME action: on the web boot
        // that meant CGame::Connect() ran twice, dialing the login server a
        // second time (the double /ws bridge) and tearing down the connection
        // carrying the in-flight 0x8C relay handshake. Native's frame pacing
        // usually hides it, the web boot does not.
        const uint8_t action = SmoothScreenAction;
        SmoothScreenAction = 0;
        ProcessSmoothAction(action);
        Render_SwapBuffers();
        g_ScreenshotBuilder.GPUDataReady();
        return 1;
    }
    return 0;
}

void CBaseScreen::DrawSmoothMonitorEffect()
{
    g_ScreenEffectManager.Draw();
}

void CBaseScreen::CreateSmoothAction(uint8_t action)
{
    if (g_ScreenEffectManager.UseSunset())
    {
        SmoothScreenAction = action;
    }
    else
    {
        ProcessSmoothAction(action);
    }
}
