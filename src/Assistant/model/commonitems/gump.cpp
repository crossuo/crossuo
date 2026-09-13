// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gump.cpp), verbatim.

#include "gump.h"

namespace xuoassist {

CGump::CGump()
{
}

CGump::CGump(const uint &serial, const uint &id, const int &x, const int &y)
    : m_Serial(serial)
    , m_ID(id)
    , m_X(x)
    , m_Y(y)
{
}

CGump::~CGump()
{
}

} // namespace xuoassist
