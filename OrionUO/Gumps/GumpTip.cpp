// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpTip.h"

enum
{
    ID_GT_NONE,

    ID_GT_PREV_GUMP,
    ID_GT_REMOVE_GUMP,
    ID_GT_NEXT_GUMP,

    ID_GT_COUNT,
};

CGumpTip::CGumpTip(uint32_t serial, short x, short y, const string &str, bool updates)
    : CGumpBaseScroll(GT_TIP, serial, 0x0820, 250, x, y, true)
    , Updates(updates)
{
    DEBUG_TRACE_FUNCTION;
    Add(new CGUIPage(1));
    Add(new CGUIGumppic((Updates ? 0x09D3 : 0x09CB), 0, 0));

    Add(new CGUIPage(2));

    if (Updates) //Updates text gump
    {
        Add(new CGUIGumppic(0x09D2, 112, 35));

        m_ButtonPrevGump = nullptr;
        m_ButtonNextGump = nullptr;
    }
    else //Tips text gump
    {
        Add(new CGUIGumppic(0x09CA, 127, 33));

        m_ButtonPrevGump = (CGUIButton *)Add(
            new CGUIButton(ID_GT_PREV_GUMP, 0x09CC, 0x09CC, 0x09CC, 35, Height)); //Previous gump
        m_ButtonPrevGump->CheckPolygone = true;
        //m_ButtonRemoveGump = (CGUIButton*)Add(new CGUIButton(ID_GT_REMOVE_GUMP, 0x0823, 0x0823, 0x0823, 18, Height + 34)); //+/- gump
        m_ButtonNextGump = (CGUIButton *)Add(
            new CGUIButton(ID_GT_NEXT_GUMP, 0x09CD, 0x09CD, 0x09CD, 251, Height)); //Next gump
        m_ButtonNextGump->CheckPolygone = true;
    }

    CGUIText *text = (CGUIText *)m_HTMLGump->Add(new CGUIText(0, 3, 3));
    text->CreateTextureA(6, str, 200);
    m_HTMLGump->CalculateDataSize();
}

CGumpTip::~CGumpTip()
{
}

void CGumpTip::UpdateHeight()
{
    DEBUG_TRACE_FUNCTION;
    CGumpBaseScroll::UpdateHeight();

    if (!Updates)
    {
        m_ButtonPrevGump->SetY(Height);
        //m_ButtonRemoveGump->SetY(Height + 34);
        m_ButtonNextGump->SetY(Height);
    }
}

void CGumpTip::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (serial == ID_GBS_BUTTON_MINIMIZE)
    {
        Minimized = true;
        Page = 1;
        WantRedraw = true;
    }
    else if (serial == ID_GT_PREV_GUMP)
    {
        SendTipRequest(0);
    }
    else if (serial == ID_GT_NEXT_GUMP)
    {
        SendTipRequest(1);
    }
}

bool CGumpTip::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    if (Minimized)
    {
        Minimized = false;
        Page = 2;
        WantRedraw = true;

        return true;
    }

    return false;
}

void CGumpTip::SendTipRequest(uint8_t flag)
{
    DEBUG_TRACE_FUNCTION;
    //Отправляем запрос диалога Tip/Updates
    CPacketTipRequest((uint16_t)Serial, flag).Send();

    //Удаляем использованный гамп
    RemoveMark = true;
}
