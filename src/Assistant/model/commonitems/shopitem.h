// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/shopitem.h): strings are astr_t.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CShopItem
{
public:
    astr_t Graphic = "";
    astr_t Color = "";
    astr_t Name = "";
    int Count = 0;
    uint Price = 0;
    astr_t Comment = "";

    CShopItem() {}
    CShopItem(
        const astr_t &graphic,
        const astr_t &color,
        const astr_t &name,
        const int &count,
        const uint &price,
        const astr_t &comment);
};

} // namespace xuoassist
