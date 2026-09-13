// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/buffitem.cpp), verbatim.

#include "buffitem.h"

namespace xuoassist {

CBuffItem::CBuffItem()
{
}

CBuffItem::CBuffItem(ushort graphic, uint timer, string name)
    : m_Graphic(graphic)
    , m_Timer(timer)
    , m_Name(name)
{
}

CBuffItem::~CBuffItem()
{
}

} // namespace xuoassist
