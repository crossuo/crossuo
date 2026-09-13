// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameitem.h); quint64 -> uint64_t.

#pragma once

#include "gameobject.h"

namespace xuoassist {

//! Game item class (or corpse)
class CGameItem : public CGameObject
{
    //! Layer the object is placed in
    SETGET(uchar, Layer, 0)
    //! This is a multi proxy object
    SETGET(bool, MultiBody, false)

    SETGET(uint, Price, 0)
    SETGET(bool, NameFromCliloc, false)
    SETGET(uint64_t, TiledataFlags, 0)

public:
    CGameItem(const uint &serial = 0);
    virtual ~CGameItem();

    int GetEquipLayer();

    CGameItem *FindItem(const ushort &graphic, const ushort &color = 0xFFFF);
};

} // namespace xuoassist

