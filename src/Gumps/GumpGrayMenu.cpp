// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpGrayMenu.h"
#include "../CrossUO.h"
#include "../ScreenStages/GameBlockedScreen.h"
#include "../Network/Packets.h"
#if defined(XUO_ASSISTANT)
#include <managers/menumanager.h> // xuoassist::g_MenuManager (menu-choice tap)
#endif

CGumpGrayMenu::CGumpGrayMenu(uint32_t serial, uint32_t id, short x, short y)
    : CGump(GT_GRAY_MENU, serial, x, y)
{
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
    if (serial == ID_GGM_CANCEL)
    {
        SendMenuResponse(0);
    }
    else if (serial == ID_GGM_CONTINUE)
    {
        for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next)
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
    //Ответ на меню
    CPacketGrayMenuResponse(this, index).Send();
#if defined(XUO_ASSISTANT)
    // the gray-menu answer reports to the same assistant menu manager
    xuoassist::g_MenuManager.OnMenuChoiceSent(Serial, ID, index);
#endif

    //Удаляем использованный гамп
    RemoveMark = true;
}
