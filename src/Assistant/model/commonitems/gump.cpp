// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gump.cpp), verbatim.

#include "gump.h"

namespace xuoassist {

CGump::CGump()
{
}

CGump::CGump(const uint32_t &serial, const uint32_t &id, const int &x, const int &y)
    : Serial(serial)
    , ID(id)
    , X(x)
    , Y(y)
{
}

CGump::~CGump()
{
}

} // namespace xuoassist
