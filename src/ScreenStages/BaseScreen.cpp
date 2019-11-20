// MIT License
// Copyright (C) August 2016 Hotride

#include "BaseScreen.h"
#include "../SelectedObject.h"
#include "../Managers/MouseManager.h"
#include "../Managers/ScreenEffectManager.h"

CBaseScreen *g_CurrentScreen = nullptr;
RenderCmdList *g_renderCmdList = nullptr;

void CBaseScreen::InitRenderList()
{
    assert(!m_RenderCmdListData);
    if (m_RenderCmdListData)
    {
        return;
    }

    static const uint32_t s_renderCmdListSize = 512 * 1024;
    m_RenderCmdListData = malloc(s_renderCmdListSize);
    assert(m_RenderCmdListData);

    // comment this out to enable delayed render cmds (commands are pushed to the GPU when RenderDraw_Execute is called)
    // don't comment this out until text resources lifetime isn't fixed (see CFontsManager::DrawA)
    const bool immediateMode = true;
    // const bool immediateMode = false;
    m_RenderCmdList = RenderCmdList(
        m_RenderCmdListData, s_renderCmdListSize, Render_DefaultState(), immediateMode);

    g_renderCmdList = &m_RenderCmdList;
}

CBaseScreen::CBaseScreen(CGump &gump)
    : m_Gump(gump)
    , m_RenderCmdList()
    , m_RenderCmdListData(nullptr)
{
}

CBaseScreen::~CBaseScreen()
{
    if (m_RenderCmdListData)
    {
        free(m_RenderCmdListData);
        m_RenderCmdListData = nullptr;
    }

    g_renderCmdList = nullptr;
}

void CBaseScreen::Init()
{
    if (!m_RenderCmdListData)
    {
        InitRenderList();
    }
}

void CBaseScreen::Render()
{
    DEBUG_TRACE_FUNCTION;

#ifdef NEW_RENDERER_ENABLED
    Render_ResetCmdList(&m_RenderCmdList, Render_DefaultState());
    RenderAdd_FlushState(&m_RenderCmdList);
#endif

    // TODO renderer BeginDraw exists only to set Drawing to true, as it's used by
    // Gump-something; investigate it and remove/move it elsewhere so
    // we can get rid of glEngine
    g_GL.BeginDraw();
#ifdef NEW_RENDERER_ENABLED
    RenderAdd_ClearRT(&m_RenderCmdList, &ClearRTCmd());
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

    RenderDraw_Execute(&m_RenderCmdList);

    g_GL.EndDraw();

#ifdef NEW_RENDERER_ENABLED
    Render_SwapBuffers();
    g_ScreenshotBuilder.GPUDataReady();
#endif
}

void CBaseScreen::SelectObject()
{
    DEBUG_TRACE_FUNCTION;

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
    DEBUG_TRACE_FUNCTION;
    if ((g_ScreenEffectManager.Process() != 0) && (SmoothScreenAction != 0u))
    {
        ProcessSmoothAction();
        g_GL.EndDraw();

#ifdef NEW_RENDERER_ENABLED
        Render_SwapBuffers();
        g_ScreenshotBuilder.GPUDataReady();
#endif
        return 1;
    }
    return 0;
}

void CBaseScreen::DrawSmoothMonitorEffect()
{
    DEBUG_TRACE_FUNCTION;
    g_ScreenEffectManager.Draw();
}

void CBaseScreen::CreateSmoothAction(uint8_t action)
{
    DEBUG_TRACE_FUNCTION;
    if (g_ScreenEffectManager.UseSunset())
    {
        SmoothScreenAction = action;
    }
    else
    {
        ProcessSmoothAction(action);
    }
}
