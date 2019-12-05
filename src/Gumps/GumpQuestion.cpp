// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpQuestion.h"
#include "../CrossUO.h"
#include "../ScreenStages/GameScreen.h"

CGumpQuestion::CGumpQuestion(uint32_t serial, short x, short y, uint8_t variant)
    : CGump(GT_QUESTION, serial, x, y)
    , Variant(variant)
{
    Blocked = true;
    g_GrayMenuCount++;

    Add(new CGUIGumppic(0x0816, 0, 0));

    CGUIText *obj = (CGUIText *)Add(new CGUIText(0x0386, 33, 30));

    if (Variant == ID_GQ_STATE_QUIT)
    {
        obj->CreateTextureA(1, "Quit\nUltima Online?");
    }
    else if (Variant == ID_GQ_STATE_ATTACK_REQUEST)
    {
        obj->CreateTextureA(1, "This may flag\nyou criminal!");
    }

    Add(new CGUIButton(ID_GQ_BUTTON_CANCEL, 0x0817, 0x0819, 0x0818, 37, 75));
    Add(new CGUIButton(ID_GQ_BUTTON_OKAY, 0x081A, 0x081C, 0x081B, 100, 75));
}

CGumpQuestion::~CGumpQuestion()
{
}

void CGumpQuestion::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GQ_BUTTON_CANCEL)
    { //Button cancel
        RemoveMark = true;
    }
    else if (serial == ID_GQ_BUTTON_OKAY) //Button okay
    {
        if (Variant == ID_GQ_STATE_QUIT)
        {
            g_GameScreen.CreateSmoothAction(CGameScreen::ID_SMOOTH_GS_LOGOUT);
        }
        else if (Variant == ID_GQ_STATE_ATTACK_REQUEST)
        {
            g_Game.AttackReq(ID);
            RemoveMark = true;
        }
    }
}
