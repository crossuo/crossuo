// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIText.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"

CGUIText::CGUIText(uint16_t color, int x, int y)
    : CBaseGUI(GOT_TEXT, 0, 0, color, x, y)
{
}

CGUIText::~CGUIText()
{
    DEBUG_TRACE_FUNCTION;
    m_Texture.Clear();
}

void CGUIText::CreateTextureA(
    uint8_t font, const string &str, int width, TEXT_ALIGN_TYPE align, uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    g_FontManager.GenerateA(font, m_Texture, str, Color, width, align, flags);
}

void CGUIText::CreateTextureW(
    uint8_t font,
    const wstring &str,
    uint8_t cell,
    int width,
    TEXT_ALIGN_TYPE align,
    uint16_t flags)
{
    DEBUG_TRACE_FUNCTION;
    g_FontManager.GenerateW(font, m_Texture, str, Color, cell, width, align, flags);
}

void CGUIText::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    m_Texture.Draw(m_X, m_Y, checktrans);
}

bool CGUIText::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < m_Texture.Width && y < m_Texture.Height);
}
