// MIT License
// Copyright (C) November 2017 Hotride

#include "GumpProperty.h"
#include "../ToolTip.h"
#include "../Managers/MouseManager.h"

CGumpProperty::CGumpProperty(const wstring &text)
    : CGump(GT_PROPERTY, 0, 0, 0)
{
    Timer = g_Ticks + 5000;

    int width = 0;
    g_ToolTip.Create(m_Sprite, text, width, 0);

    m_X = g_MouseManager.Position.X - (m_Sprite.Width + 8);
    if (m_X < 0)
    {
        m_X = 0;
    }

    m_Y = g_MouseManager.Position.Y - (m_Sprite.Height + 8);
    if (m_Y < 0)
    {
        m_Y = 0;
    }

    Add(new CGUIColoredPolygone(0, 0, 0, 0, m_Sprite.Width + 12, m_Sprite.Height + 8, 0x7F000000));
    Add(new CGUIExternalTexture(&m_Sprite, false, 6, 4));
}

void CGumpProperty::PrepareContent()
{
    if (Timer < g_Ticks)
    {
        RemoveMark = true;
    }
}
