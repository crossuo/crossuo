// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/ignoreitem.cpp), verbatim.

#include "ignoreitem.h"

namespace xuoassist {

CIgnoreItem::CIgnoreItem()
= default;

CIgnoreItem::CIgnoreItem(const astr_t &serial, const astr_t &graphic, const astr_t &color)
    : CFindItem(graphic, color)
    , Serial(serial)
{
}

CIgnoreItem::~CIgnoreItem()
= default;

bool CIgnoreItem::Found(const uint32_t &serial, const uint16_t &graphic, const uint16_t &color) const
{
    uint32_t currentSerial = 0;
    uint32_t flagsSerial = ParseToken(Serial, currentSerial, false);

    if ((currentSerial != 0u) && TestValues(serial, currentSerial, flagsSerial))
    {
        return true;
    }

    return CFindItem::Found(graphic, color);
}

} // namespace xuoassist
