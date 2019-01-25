// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpBulletinBoard.h"
#include "GumpBulletinBoardItem.h"
#include "../PressedObject.h"
#include "../Managers/GumpManager.h"
#include "../Network/Packets.h"
#include "../GameObjects/GamePlayer.h"

enum
{
    ID_GBB_NONE,

    ID_GBB_POST_MESSAGE,
    ID_GBB_MESSAGE,
    ID_GBS_HTMLGUMP,

    ID_GBS_COUNT,
};

CGumpBulletinBoard::CGumpBulletinBoard(uint32_t serial, short x, short y, const string &name)
    : CGump(GT_BULLETIN_BOARD, serial, x, y)
{
    DEBUG_TRACE_FUNCTION;
    Add(new CGUIGumppic(0x087A, 0, 0));

    CGUIText *text = (CGUIText *)Add(new CGUIText(0x0386, 159, 36));
    text->CreateTextureA(2, name, 170, TS_CENTER);

    Add(new CGUIHitBox(ID_GBB_POST_MESSAGE, 15, 170, 80, 80, true));

    m_HTMLGump =
        (CGUIHTMLGump *)Add(new CGUIHTMLGump(ID_GBS_HTMLGUMP, 0, 127, 159, 241, 195, false, true));

    if (m_HTMLGump->m_Background != nullptr)
    {
        m_HTMLGump->m_Background->Height -= 30;
    }

    if (m_HTMLGump->m_Scissor != nullptr)
    {
        m_HTMLGump->m_Scissor->Height -= 30;
    }

    CGUIHTMLButton *button = m_HTMLGump->m_ButtonUp;

    if (button != nullptr)
    {
        button->Graphic = 0x0824;
        button->GraphicSelected = 0x0824;
        button->GraphicPressed = 0x0824;
        button->SelectOnly = true;
        button->CheckPolygone = true;
        button->SetY(button->GetY() - 14);
    }

    button = m_HTMLGump->m_ButtonDown;

    if (button != nullptr)
    {
        button->Graphic = 0x0825;
        button->GraphicSelected = 0x0825;
        button->GraphicPressed = 0x0825;
        button->SelectOnly = true;
        button->CheckPolygone = true;
        button->SetY(button->GetY() - 14);
    }

    CGUIHTMLSlider *slider = m_HTMLGump->m_Slider;

    if (slider != nullptr)
    {
        slider->Graphic = 0x001F;
        slider->GraphicSelected = 0x001F;
        slider->GraphicPressed = 0x001F;
        slider->BackgroundGraphic = 0;
        slider->SetY(slider->GetY() - 14);
    }
}

CGumpBulletinBoard::~CGumpBulletinBoard()
{
    DEBUG_TRACE_FUNCTION;
    g_GumpManager.CloseGump(0xFFFFFFFF, Serial, GT_BULLETIN_BOARD_ITEM);
}

void CGumpBulletinBoard::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (serial == ID_GBB_POST_MESSAGE)
    {
        CGumpBulletinBoardItem *gump = new CGumpBulletinBoardItem(
            0, 0, 0, 0, Serial, ToWString(g_Player->GetName()), {}, L"Date/Time", {});

        g_GumpManager.AddGump(gump);
    }
}

bool CGumpBulletinBoard::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    if (g_PressedObject.LeftObject != nullptr && g_PressedObject.LeftObject->IsGUI())
    {
        CBaseGUI *gui = (CBaseGUI *)g_PressedObject.LeftObject;

        if (gui->Type == GOT_BB_OBJECT)
        {
            CPacketBulletinBoardRequestMessage(Serial, gui->Serial).Send();

            result = true;
        }
    }

    return result;
}
