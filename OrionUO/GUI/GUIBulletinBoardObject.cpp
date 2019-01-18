// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIBulletinBoardObject.h"
#include "../Config.h"
#include "../OrionUO.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"

CGUIBulletinBoardObject::CGUIBulletinBoardObject(int serial, int x, int y, const wstring &text)
    : CBaseGUI(GOT_BB_OBJECT, serial, 0, 0, x, y)
    , Text(text)
{
    DEBUG_TRACE_FUNCTION;
    MoveOnDrag = true;

    if (g_Config.ClientVersion >= CV_305D)
    {
        g_FontManager.GenerateW(1, m_Texture, text, 0);
    }
    else
    {
        g_FontManager.GenerateA(9, m_Texture, ToString(text), 0x0386);
    }
}

CGUIBulletinBoardObject::~CGUIBulletinBoardObject()
{
    DEBUG_TRACE_FUNCTION;
    m_Texture.Clear();
}

void CGUIBulletinBoardObject::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.ExecuteGump(0x1523);
}

void CGUIBulletinBoardObject::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = g_Orion.ExecuteGump(0x1523);

    if (th != nullptr)
    {
        th->Draw(m_X, m_Y, checktrans);
    }

    m_Texture.Draw(m_X + 23, m_Y + 1);
}

bool CGUIBulletinBoardObject::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < 230 && y < 18);
}
