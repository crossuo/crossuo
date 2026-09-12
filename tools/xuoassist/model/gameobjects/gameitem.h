// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameitem.h); quint64 -> uint64_t.

#ifndef XUOASSIST_GAMEITEM_H
#define XUOASSIST_GAMEITEM_H

#include "gameobject.h"

namespace xuoassist {

//! Класс игрового предмета (или трупа)
class CGameItem : public CGameObject
{
    //! Слой, в котором расположен объект
    SETGET(uchar, Layer, 0)
    //! Это прокси-объект мульти
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

#endif // XUOASSIST_GAMEITEM_H
