// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GUIHTMLText.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGUIHTMLText::CGUIHTMLText(
    int index,
    uint8_t font,
    uint16_t color,
    int x,
    int y,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t textFlags,
    int htmlStartColor)
    : CBaseGUI(GOT_HTMLTEXT, 0, 0, color, x, y)
    , TextID(index)
    , HTMLStartColor(htmlStartColor)
    , Text(L"")
    , Font(font)
    , Align(align)
    , TextFlags(textFlags)
    , Width(width)
{
}

CGUIHTMLText::~CGUIHTMLText()
{
    DEBUG_TRACE_FUNCTION;
    m_Texture.Clear();
}

void CGUIHTMLText::CreateTexture(bool backgroundCanBeColored)
{
    DEBUG_TRACE_FUNCTION;
    g_FontManager.SetUseHTML(true, HTMLStartColor, backgroundCanBeColored);

    g_FontManager.GenerateW(Font, m_Texture, Text, Color, 30, Width, Align, TextFlags);

    g_FontManager.SetUseHTML(false);
}

void CGUIHTMLText::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    m_Texture.Draw(m_X, m_Y, checktrans);
}

bool CGUIHTMLText::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < m_Texture.Width && y < m_Texture.Height);
}

