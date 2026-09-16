// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/menuitem.h): strings are astr_t,
// lists are std::vector.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CMenuItem
{
public:
    uint ID = 0;
    ushort Graphic = 0;
    ushort Color = 0;
    astr_t Name = "";

    CMenuItem();
    CMenuItem(const uint &id, const ushort &graphic, const ushort &color, const astr_t &name);
    virtual ~CMenuItem();
};

typedef vector<CMenuItem> MENU_LIST;

} // namespace xuoassist
