// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/menu.cpp), verbatim.

#include "menu.h"

namespace xuoassist {

CMenu::CMenu()
{
}

CMenu::CMenu(const uint &serial, const ushort &id, const string &name, const bool &grayMenu)
    : m_Serial(serial)
    , m_ID(id)
    , m_GrayMenu(grayMenu)
    , m_Name(name)
{
}

CMenu::~CMenu()
{
}

} // namespace xuoassist
