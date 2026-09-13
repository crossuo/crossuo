// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/ignoreitem.h): QString -> std::string.

#ifndef XUOASSIST_IGNOREITEM_H
#define XUOASSIST_IGNOREITEM_H

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

#endif // XUOASSIST_IGNOREITEM_H
