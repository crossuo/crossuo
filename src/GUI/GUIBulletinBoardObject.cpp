// MIT License
// Copyright (C) August 2016 Hotride

#include <common/str.h>
#include "GUIBulletinBoardObject.h"
#include "../Config.h"
#include "../CrossUO.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"
#include "../Utility/PerfMarker.h"

CGUIBulletinBoardObject::CGUIBulletinBoardObject(int serial, int x, int y, const std::wstring &text)
    : CBaseGUI(GOT_BB_OBJECT, serial, 0, 0, x, y)
    , Text(text)
{
    MoveOnDrag = true;

    if (g_Config.ClientVersion >= CV_305D)
    {
        g_FontManager.GenerateW(1, m_Texture, text, 0);
    }
    else
    {
        g_FontManager.GenerateA(9, m_Texture, str_from(text), 0x0386);
    }
}

CGUIBulletinBoardObject::~CGUIBulletinBoardObject()
{
    m_Texture.Clear();
}

void CGUIBulletinBoardObject::PrepareTextures()
{
    g_Game.ExecuteGump(0x1523);
}

void CGUIBulletinBoardObject::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    auto spr = g_Game.ExecuteGump(0x1523);
    if (spr != nullptr && spr->Texture != nullptr)
    {
        spr->Texture->Draw(m_X, m_Y, checktrans);
    }
    m_Texture.Draw(m_X + 23, m_Y + 1);
}

bool CGUIBulletinBoardObject::Select()
{
    const int x = g_MouseManager.Position.X - m_X;
    const int y = g_MouseManager.Position.Y - m_Y;
    return (x >= 0 && y >= 0 && x < 230 && y < 18);
}
