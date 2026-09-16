// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/shopitem.cpp), verbatim.

#include "shopitem.h"

namespace xuoassist {

CShopItem::CShopItem(
    const astr_t &graphic,
    const astr_t &color,
    const astr_t &name,
    const int &count,
    const uint32_t &price,
    const astr_t &comment)
    : Graphic(graphic)
    , Color(color)
    , Name(name)
    , Count(count)
    , Price(price)
    , Comment(comment)
{
}

} // namespace xuoassist
