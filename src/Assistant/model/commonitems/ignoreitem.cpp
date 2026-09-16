// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/ignoreitem.cpp), verbatim.

#include "ignoreitem.h"

namespace xuoassist {

CIgnoreItem::CIgnoreItem()
{
}

CIgnoreItem::CIgnoreItem(const astr_t &serial, const astr_t &graphic, const astr_t &color)
    : CFindItem(graphic, color)
    , Serial(serial)
{
}

CIgnoreItem::~CIgnoreItem()
{
}

bool CIgnoreItem::Found(const uint &serial, const ushort &graphic, const ushort &color) const
{
    uint currentSerial = 0;
    uint flagsSerial = ParseToken(Serial, currentSerial, false);

    if ((currentSerial != 0u) && TestValues(serial, currentSerial, flagsSerial))
    {
        return true;
    }

    return CFindItem::Found(graphic, color);
}

} // namespace xuoassist
