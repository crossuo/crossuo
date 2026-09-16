// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/menu.h): strings are astr_t.

#pragma once

#include "core/global.h"
#include "menuitem.h"

namespace xuoassist {

class CMenu
{
public:
    uint32_t Serial = 0;
    uint16_t ID = 0;
    bool GrayMenu = false;
    astr_t Name = "";
    bool Replayed = false;
    uint32_t ReplyID = 0;

    CMenu();
    CMenu(const uint32_t &serial, const uint16_t &id, const astr_t &name, const bool &grayMenu);
    virtual ~CMenu();

    MENU_LIST m_Items;
};

} // namespace xuoassist
