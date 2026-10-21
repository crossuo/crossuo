// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <utility>

#include "GumpNotify.h"
#include "../TextEngine/GameConsole.h"
#include "../Managers/ConfigManager.h"
#include "../ScreenStages/GameScreen.h"

CGumpNotify::CGumpNotify(short x, short y, uint8_t variant, short width, short height, astr_t text)
    : CGump(GT_NOTIFY, 0, x, y)
    , Variant(variant)
    , Width(width)
    , Height(height)
    , Text(std::move(text))
{
    Blocked = true;
    g_GrayMenuCount++;

    Add(new CGUIResizepic(0, 0x0A28, 0, 0, Width, Height));

    CGUIText *obj = (CGUIText *)Add(new CGUIText(0x0386, 40, 45));
    obj->CreateTextureA(1, Text, Width - 90);

    Add(new CGUIButton(ID_GN_BUTTON_OK, 0x0481, 0x0482, 0x0483, (Width / 2) - 13, Height - 45));
}

CGumpNotify::~CGumpNotify()
= default;

void CGumpNotify::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GN_BUTTON_OK)
    {
        Process();
    }
}

void CGumpNotify::OnKeyDown(const KeyEvent &ev)
{
    const auto key = EvKey(ev);
    if (key != KEY_RETURN && key != KEY_RETURN2)
    {
        return;
    }

    if (g_ConfigManager.GetConsoleNeedEnter())
    {
        g_EntryPointer = nullptr;
    }
    else
    {
        g_EntryPointer = &g_GameConsole;
    }

    Process();
}

void CGumpNotify::Process()
{
    if (Variant == ID_GN_STATE_LOGOUT)
    {
        // process directly: this dialog shows while the BLOCKED screen is
        // active (disconnect), and a deferred action parked on the game
        // screen would never run - its render pass is not running
        g_GameScreen.ProcessSmoothAction(CGameScreen::ID_SMOOTH_GS_LOGOUT);
    }
    else if (Variant == ID_GN_STATE_NOTIFICATION)
    {
        RemoveMark = true;
    }
}
