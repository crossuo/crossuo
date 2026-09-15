// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/vendorshopitem.h): strings are std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CVendorShopItem
{
public:
    uint Serial = 0;
    ushort Graphic = 0;
    ushort Color = 0;
    string Name = "";
    int Count = 0;
    uint Price = 0;
    bool NameFromCliloc = false;

    CVendorShopItem() {}
};

} // namespace xuoassist

