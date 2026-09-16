// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/vendorshopitem.h): strings are astr_t.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CVendorShopItem
{
public:
    uint32_t Serial = 0;
    uint16_t Graphic = 0;
    uint16_t Color = 0;
    astr_t Name = "";
    int Count = 0;
    uint32_t Price = 0;
    bool NameFromCliloc = false;

    CVendorShopItem() {}
};

} // namespace xuoassist
