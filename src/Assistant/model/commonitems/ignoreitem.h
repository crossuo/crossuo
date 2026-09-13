// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/ignoreitem.h): strings are std::string.

#pragma once

#include "core/global.h"
#include "finditem.h"

namespace xuoassist {

class CIgnoreItem : public CFindItem
{
    SETGETC(string, Serial, "")

public:
    CIgnoreItem();
    CIgnoreItem(const string &serial, const string &graphic, const string &color);
    virtual ~CIgnoreItem();

    bool Found(const uint &serial, const ushort &graphic, const ushort &color) const;
};

} // namespace xuoassist

