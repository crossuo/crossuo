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
    uint Serial = 0;
    ushort ID = 0;
    bool GrayMenu = false;
    astr_t Name = "";
    bool Replayed = false;
    uint ReplyID = 0;

    CMenu();
    CMenu(const uint &serial, const ushort &id, const astr_t &name, const bool &grayMenu);
    virtual ~CMenu();

    MENU_LIST m_Items;
};

} // namespace xuoassist
