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

    static const uint32_t s_renderCmdListSize = 192 * 1024;
    m_RenderCmdListData = malloc(s_renderCmdListSize);
    assert(m_RenderCmdListData);

// leave this on to execute ogl commands as they're added to the list,
// instead of deferring their execution until RenderDraw_Execute is called
#define NEWRENDERER_IMMEDIATEMODE
#ifdef NEWRENDERER_IMMEDIATEMODE
    const bool immediateMode = true;
#else
    const bool immediateMode = false;
#endif
    m_RenderCmdList = Render_CmdList(
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

    Render_ResetCmdList(&m_RenderCmdList, Render_DefaultState());
    RenderAdd_FlushState(&m_RenderCmdList);

    g_GL.BeginDraw();
    if (DrawSmoothMonitor() != 0)
    {
        return;
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    m_Gump.Draw();
    InitToolTip();
    DrawSmoothMonitorEffect();
    g_MouseManager.Draw(CursorGraphic);

    // turning this off while immediateMode is on
    // RenderDraw_Execute(&m_RenderCmdList);
    g_GL.EndDraw();
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
