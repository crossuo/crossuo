// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/vendorshopitem.h): QString -> std::string.

#ifndef XUOASSIST_VENDORSHOPITEM_H
#define XUOASSIST_VENDORSHOPITEM_H

#include "core/global.h"

namespace xuoassist {

class CVendorShopItem
{
    SETGET(uint, Serial, 0)
    SETGET(ushort, Graphic, 0)
    SETGET(ushort, Color, 0)
    SETGET(string, Name, "")
    SETGET(int, Count, 0)
    SETGET(uint, Price, 0)
    SETGET(bool, NameFromCliloc, false)

public:
    CVendorShopItem() {}
};

} // namespace xuoassist

#endif // XUOASSIST_VENDORSHOPITEM_H
