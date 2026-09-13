// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/shopitem.h): strings are std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CShopItem
{
    SETGET(string, Graphic, "")
    SETGET(string, Color, "")
    SETGET(string, Name, "")
    SETGET(int, Count, 0)
    SETGET(uint, Price, 0)
    SETGET(string, Comment, "")

public:
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

