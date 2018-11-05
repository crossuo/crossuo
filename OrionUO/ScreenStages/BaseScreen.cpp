// MIT License
// Copyright (C) August 2016 Hotride

CBaseScreen *g_CurrentScreen = nullptr;

CBaseScreen::CBaseScreen(CGump &gump)
    : CBaseQueue()
    , m_Gump(gump)
{
}

void CBaseScreen::Render()
{
    DEBUG_TRACE_FUNCTION;

    g_GL.BeginDraw();
    if (DrawSmoothMonitor())
        return;

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    m_Gump.Draw();
    InitToolTip();
    DrawSmoothMonitorEffect();
    g_MouseManager.Draw(CursorGraphic);
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
    if (g_ScreenEffectManager.Process() && SmoothScreenAction)
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

void CBaseScreen::CreateSmoothAction(BYTE action)
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
