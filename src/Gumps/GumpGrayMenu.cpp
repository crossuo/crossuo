// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpGrayMenu.h"
#include "../CrossUO.h"
#include "../ScreenStages/GameBlockedScreen.h"
#include "../Network/Packets.h"

CGumpGrayMenu::CGumpGrayMenu(uint32_t serial, uint32_t id, short x, short y)
    : CGump(GT_GRAY_MENU, serial, x, y)
{
    DEBUG_TRACE_FUNCTION;
    NoMove = true;
    Blocked = true;

    ID = id;

    if (g_GrayMenuCount == 0)
    {
        g_Game.InitScreen(GS_GAME_BLOCKED);
        g_GameBlockedScreen.Code = 1;
    }

    g_GrayMenuCount++;
}

CGumpGrayMenu::~CGumpGrayMenu()
{
}

void CGumpGrayMenu::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (serial == ID_GGM_CANCEL)
    {
        SendMenuResponse(0);
    }
    else if (serial == ID_GGM_CONTINUE)
    {
        QFOR(item, m_Items, CBaseGUI *)
        {
            if (item->Type == GOT_RADIO)
            {
                if (((CGUIRadio *)item)->Checked)
                {
                    SendMenuResponse(item->Serial);

                    return;
                }
            }
        }

        //Ничего не выбрали
        g_Game.CreateTextMessage(
            TT_SYSTEM, 0xFFFFFFFF, 3, 0x0021, "You must choose the section of menu.");
    }
}

void CGumpGrayMenu::SendMenuResponse(int index)
{
    DEBUG_TRACE_FUNCTION;
    //Ответ на меню
    CPacketGrayMenuResponse(this, index).Send();

    //Удаляем использованный гамп
    RemoveMark = true;
}
