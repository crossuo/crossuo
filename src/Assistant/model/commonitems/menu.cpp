// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/menu.cpp), verbatim.

#include "menu.h"

namespace xuoassist {

CMenu::CMenu()
= default;

CMenu::CMenu(const uint32_t &serial, const uint16_t &id, const astr_t &name, const bool &grayMenu)
    : Serial(serial)
    , ID(id)
    , GrayMenu(grayMenu)
    , Name(name)
{
}

CMenu::~CMenu()
= default;

} // namespace xuoassist
