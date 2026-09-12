// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gump.h): QStringList -> std::vector<std::string>.

#ifndef XUOASSIST_GUMP_H
#define XUOASSIST_GUMP_H

#include "core/global.h"
#include "gumpitem.h"

namespace xuoassist {

class CGump
{
    SETGET(uint, Serial, 0)
    SETGET(uint, ID, 0)
    SETGET(int, X, 0)
    SETGET(int, Y, 0)
    SETGET(bool, Replayed, false)
    SETGET(uint, ReplyID, 0)
    SETGET(bool, NoMove, false)
    SETGET(bool, NoClose, false)
    SETGET(bool, NoDispose, false)

public:
    CGump();
    CGump(const uint &serial, const uint &id, const int &x, const int &y);
    virtual ~CGump();

    GUMP_LIST m_Items;
    vector<string> m_Text;
};

} // namespace xuoassist

#endif // XUOASSIST_GUMP_H
