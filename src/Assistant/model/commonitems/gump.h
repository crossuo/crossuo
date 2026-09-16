// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gump.h): astr_t lists are std::vector<astr_t>.

#pragma once

#include "core/global.h"
#include "gumpitem.h"

namespace xuoassist {

class CGump
{
public:
    uint32_t Serial = 0;
    uint32_t ID = 0;
    int X = 0;
    int Y = 0;
    bool Replayed = false;
    uint32_t ReplyID = 0;
    bool NoMove = false;
    bool NoClose = false;
    bool NoDispose = false;

    CGump();
    CGump(const uint32_t &serial, const uint32_t &id, const int &x, const int &y);
    virtual ~CGump();

    GUMP_LIST m_Items;
    vector<astr_t> m_Text;
};

} // namespace xuoassist
