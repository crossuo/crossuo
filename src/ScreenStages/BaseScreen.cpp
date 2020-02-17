// MIT License
// Copyright (C) August 2016 Hotride

#include "BaseScreen.h"
#include "../SelectedObject.h"
#include "../Managers/MouseManager.h"
#include "../Managers/ScreenEffectManager.h"
#include "ScreenshotBuilder.h"

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
#ifdef NEW_RENDERER_ENABLED
    Render_ResetCmdList(g_renderCmdList, Render_DefaultState());
    RenderAdd_FlushState(g_renderCmdList);
    RenderAdd_ClearRT(g_renderCmdList, ClearRTCmd{});
#else
    // TODO renderer BeginDraw exists only to set Drawing to true, as it's used by
    // Gump-something; investigate it and remove/move it elsewhere so
    // we can get rid of glEngine
    g_GL.BeginDraw();
#endif

    if (DrawSmoothMonitor() != 0)
    {
        return;
    }

#ifndef NEW_RENDERER_ENABLED
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
    // FlushState command above already sets color to white with alpha 100%
#endif
    m_Gump.Draw();
    InitToolTip();
    DrawSmoothMonitorEffect();
    g_MouseManager.Draw(CursorGraphic);

    RenderDraw_Execute(g_renderCmdList);

#ifdef NEW_RENDERER_ENABLED
    Render_SwapBuffers();
    g_ScreenshotBuilder.GPUDataReady();
#else
    g_GL.EndDraw();
#endif
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
        ProcessSmoothAction();
#ifdef NEW_RENDERER_ENABLED
        Render_SwapBuffers();
        g_ScreenshotBuilder.GPUDataReady();
#else
        g_GL.EndDraw();
#endif
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
