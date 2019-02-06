﻿// MIT License
// Copyright (C) August 2016 Hotride

#include "ToolTip.h"
#include "SelectedObject.h"
#include "Managers/FontsManager.h"
#include "Managers/ConfigManager.h"
#include "Managers/MouseManager.h"
#include "Managers/ClilocManager.h"

CToolTip g_ToolTip;

CToolTip::CToolTip()
{
}

CToolTip::~CToolTip()
{
    DEBUG_TRACE_FUNCTION;
    Reset();
}

void CToolTip::Reset()
{
    DEBUG_TRACE_FUNCTION;
    Texture.Clear();
    m_Object = nullptr;
}

void CToolTip::CreateTextTexture(
    CGLTextTexture &texture, const wstring &str, int &width, int minWidth)
{
    g_FontManager.SetUseHTML(true);
    g_FontManager.RecalculateWidthByInfo = true;

    texture.Clear();

    uint8_t font = (uint8_t)g_ConfigManager.ToolTipsTextFont;

    if (width == 0)
    {
        width = g_FontManager.GetWidthW(font, str);

        if (width > 600)
        {
            width = 600;
        }

        width = g_FontManager.GetWidthExW(font, str, width, TS_CENTER, UOFONT_BLACK_BORDER);

        if (width > 600)
        {
            width = 600;
        }
    }

    if (width < minWidth)
    {
        width = minWidth;
    }

    g_FontManager.GenerateW(
        font,
        texture,
        str,
        g_ConfigManager.ToolTipsTextColor,
        5,
        width,
        TS_CENTER,
        UOFONT_BLACK_BORDER);

    g_FontManager.RecalculateWidthByInfo = false;
    g_FontManager.SetUseHTML(false);
}

void CToolTip::Set(const wstring &str, int maxWidth)
{
    DEBUG_TRACE_FUNCTION;
    if (str.length() == 0u)
    {
        return;
    }

    Use = !(Timer > g_Ticks);

    CRenderObject *object = g_SelectedObject.Object;

    if (object == m_Object || object == nullptr)
    { //Уже забиндено или нет объекта для бинда
        return;
    }

    Timer = g_Ticks + g_ConfigManager.ToolTipsDelay;
    m_Object = object;
    Use = false;
    Data = str;
    ClilocID = 0;
    MaxWidth = maxWidth;

    Position.X = 0;
    Position.Y = 0;

    CreateTextTexture(Texture, Data, MaxWidth, 0);
}

void CToolTip::Set(int clilocID, const string &str, int maxWidth, bool toCamelCase)
{
    DEBUG_TRACE_FUNCTION;
    Set(g_ClilocManager.Cliloc(g_Language)->GetW(clilocID, toCamelCase, str), maxWidth);

    ClilocID = clilocID;
}

void CToolTip::Draw(int cursorWidth, int cursorHeight)
{
    DEBUG_TRACE_FUNCTION;
    if (!Use /*|| !g_ConfigManager.UseToolTips*/)
    {
        return;
    }

    if (Texture.Empty())
    {
        CreateTextTexture(Texture, Data, MaxWidth, 0);
    }

    if (!Texture.Empty())
    {
        int x = Position.X;
        int y = Position.Y;

        if (x == 0)
        {
            x = g_MouseManager.Position.X - (Texture.Width + 8);
        }

        if (y == 0)
        {
            y = g_MouseManager.Position.Y - (Texture.Height + 8);
        }

        if (y < 0)
        {
            y = Position.Y;
            //y = g_MouseManager.Position.Y + cursorHeight;
        }

        if (x < 0)
        {
            x = Position.X;
            //x = g_MouseManager.Position.X + cursorWidth;
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glColor4f(0.0f, 0.0f, 0.0f, 0.5f);

        g_GL.DrawPolygone(x, y, Texture.Width + 8, Texture.Height + 8);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        glDisable(GL_BLEND);

        g_GL_Draw(Texture, x + 6, y + 4);
    }

    Use = false;
}
