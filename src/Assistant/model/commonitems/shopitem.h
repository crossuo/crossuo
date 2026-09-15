// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/shopitem.h): strings are std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CShopItem
{
public:
    string Graphic = "";
    string Color = "";
    string Name = "";
    int Count = 0;
    uint Price = 0;
    string Comment = "";

    CShopItem() {}
    CShopItem(
        const string &graphic,
        const string &color,
        const string &name,
        const int &count,
        const uint &price,
        const string &comment);
};

} // namespace xuoassist

