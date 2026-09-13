// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/buffitem.h): strings are std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CBuffItem
{
    SETGET(ushort, Graphic, 0)
    SETGET(uint, Timer, 0)
    SETGET(string, Name, "")

public:
    CBuffItem();
    CBuffItem(ushort graphic, uint timer, string name);
    virtual ~CBuffItem();
};

} // namespace xuoassist

