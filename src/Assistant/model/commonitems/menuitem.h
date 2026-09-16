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
    uint32_t ID = 0;
    uint16_t Graphic = 0;
    uint16_t Color = 0;
    astr_t Name = "";

    CMenuItem();
    CMenuItem(const uint32_t &id, const uint16_t &graphic, const uint16_t &color, const astr_t &name);
    virtual ~CMenuItem();
};

typedef vector<CMenuItem> MENU_LIST;

} // namespace xuoassist
