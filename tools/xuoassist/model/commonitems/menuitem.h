// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/menuitem.h): QString -> std::string,
// QList -> std::vector.

#ifndef XUOASSIST_MENUITEM_H
#define XUOASSIST_MENUITEM_H

#include "core/global.h"

namespace xuoassist {

class CMenuItem
{
    SETGET(uint, ID, 0)
    SETGET(ushort, Graphic, 0)
    SETGET(ushort, Color, 0)
    SETGET(string, Name, "")

public:
    CMenuItem();
    CMenuItem(const uint &id, const ushort &graphic, const ushort &color, const string &name);
    virtual ~CMenuItem();
};

typedef vector<CMenuItem> MENU_LIST;

} // namespace xuoassist

#endif // XUOASSIST_MENUITEM_H
