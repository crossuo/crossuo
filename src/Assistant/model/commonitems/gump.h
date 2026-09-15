// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gump.h): string lists are std::vector<std::string>.

#pragma once

#include "core/global.h"
#include "gumpitem.h"

namespace xuoassist {

class CGump
{
public:
    uint Serial = 0;
    uint ID = 0;
    int X = 0;
    int Y = 0;
    bool Replayed = false;
    uint ReplyID = 0;
    bool NoMove = false;
    bool NoClose = false;
    bool NoDispose = false;

    CGump();
    CGump(const uint &serial, const uint &id, const int &x, const int &y);
    virtual ~CGump();

    GUMP_LIST m_Items;
    vector<string> m_Text;
};

} // namespace xuoassist

