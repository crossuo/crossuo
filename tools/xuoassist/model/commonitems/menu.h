// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/menu.h): QString -> std::string.

#ifndef XUOASSIST_MENU_H
#define XUOASSIST_MENU_H

#include "core/global.h"
#include "menuitem.h"

namespace xuoassist {

class CMenu
{
    SETGET(uint, Serial, 0)
    SETGET(ushort, ID, 0)
    SETGET(bool, GrayMenu, false)
    SETGET(string, Name, "")
    SETGET(bool, Replayed, false)
    SETGET(uint, ReplyID, 0)

public:
    CMenu();
    CMenu(const uint &serial, const ushort &id, const string &name, const bool &grayMenu);
    virtual ~CMenu();

    MENU_LIST m_Items;
};

} // namespace xuoassist

#endif // XUOASSIST_MENU_H
