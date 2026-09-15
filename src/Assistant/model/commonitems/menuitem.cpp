// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/menuitem.cpp), verbatim.

#include "menuitem.h"

namespace xuoassist {

CMenuItem::CMenuItem()
{
}

CMenuItem::CMenuItem(const uint &id, const ushort &graphic, const ushort &color, const string &name)
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
