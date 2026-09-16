// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/menuitem.cpp), verbatim.

#include "menuitem.h"

namespace xuoassist {

CMenuItem::CMenuItem()
{
}

CMenuItem::CMenuItem(const uint32_t &id, const uint16_t &graphic, const uint16_t &color, const astr_t &name)
    : ID(id)
    , Graphic(graphic)
    , Color(color)
    , Name(name)
{
}

CMenuItem::~CMenuItem()
{
}

} // namespace xuoassist
