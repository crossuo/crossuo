// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/ignoreitem.h): strings are astr_t.

#pragma once

#include "core/global.h"
#include "finditem.h"

namespace xuoassist {

class CIgnoreItem : public CFindItem
{
public:
    astr_t Serial = "";

    CIgnoreItem();
    CIgnoreItem(const astr_t &serial, const astr_t &graphic, const astr_t &color);
    virtual ~CIgnoreItem();

    bool Found(const uint &serial, const ushort &graphic, const ushort &color) const;
};

} // namespace xuoassist
