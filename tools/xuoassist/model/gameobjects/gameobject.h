// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameobject.h): QString -> std::string,
// QList -> std::vector, QStringList -> std::vector<std::string>.

#ifndef XUOASSIST_GAMEOBJECT_H
#define XUOASSIST_GAMEOBJECT_H

#include "../basequeue.h"
#include "core/global.h"
#include "../commonitems/ignoreitem.h"

namespace xuoassist {

//! Класс игрового объекта
class CGameObject : public CBaseQueueItem
{
    //! Серийник объекта
    SETGET(uint, Serial, 0)
    //! Индекс картинки объекта
    SETGET(ushort, Graphic, 0)
    //! Цвет объекта
    SETGET(ushort, Color, 0)
    //! Координата X в мире
    SETGET(short, X, 0)
    //! Координата Y в мире
    SETGET(short, Y, 0)
    //! Координата Z в мире
    SETGET(char, Z, 0)
    //! Серийник контейнера, содержащего объект (0xFFFFFFFF - объект лежит в мире)
    SETGET(uint, Container, 0xFFFFFFFF)
    //! Карта объекта
    SETGET(uchar, MapIndex, 0)
    //! Количество
    SETGET(uint, Count, 0)
    //! Флаги от сервера
    SETGETE(uchar, Flags, 0, OnChangeFlags)
    //! Имя
    SETGET(string, Name, "")
    //! НПС или предмет
    SETGET(bool, NPC, false)
    //! Игнорировать предмет при поиске
    SETGET(bool, Ignored, false)
    SETGET(bool, SA_Poisoned, false)

public:
    CGameObject(const uint &serial = 0);
    virtual ~CGameObject();

    void AddObject(CGameObject *obj);

    void AddItem(CGameObject *obj);

    void Reject(CGameObject *obj);

    void Clear();
    void ClearUnequipped();

    bool Frozen() { return (m_Flags & 0x01); }
    bool Poisoned();
    bool Flying();
    bool YellowHits() { return (m_Flags & 0x08); }
    bool IgnoreCharacters() { return (m_Flags & 0x10); }
    bool Locked() { return !(m_Flags & 0x20); }
    bool InWarMode() { return (m_Flags & 0x40); }
    bool Hidden() { return (m_Flags & 0x80); }

    virtual bool IsHuman() { return false; }
    virtual bool IsPlayer() { return false; }
    bool IsCorpse() { return (m_Graphic == 0x2006); }

    CGameObject *GetTopObject();

    class CGameItem *FindLayer(const int &layer);

    int CountItems(const CFindItem &findListItem, const bool &recurse = true);

    int CountItems(
        const ushort &graphic = 0xFFFF, const ushort &color = 0xFFFF, const bool &recurse = true);

    CGameObject *FindType(
        const CFindItem &findListItem,
        const vector<CIgnoreItem> &ignoreList,
        const uchar &flags,
        const bool &recurseSearch,
        vector<string> &list);
};

int GetDistance(CGameObject *obj1, CGameObject *obj2);
int GetDistance(const int &x1, const int &y1, const int &x2, const int &y2);

} // namespace xuoassist

#endif // XUOASSIST_GAMEOBJECT_H
