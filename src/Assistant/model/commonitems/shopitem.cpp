// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/shopitem.cpp), verbatim.

#include "shopitem.h"

namespace xuoassist {

CShopItem::CShopItem(
    const string &graphic,
    const string &color,
    const string &name,
    const int &count,
    const uint &price,
    const string &comment)
    : m_Graphic(graphic)
    , m_Color(color)
    , m_Name(name)
    , m_Count(count)
    , m_Price(price)
    , m_Comment(comment)
{
}

} // namespace xuoassist
