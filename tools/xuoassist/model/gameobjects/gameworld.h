// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameworld.h): WORLD_MAP unchanged
// (std::map), the serial-graph semantics are kept exactly.

#ifndef XUOASSIST_GAMEWORLD_H
#define XUOASSIST_GAMEWORLD_H

#include "gameobject.h"
#include "gameitem.h"
#include "gamecharacter.h"
#include <map>

namespace xuoassist {

typedef std::map<uint, CGameObject *> WORLD_MAP;

//! Класс игрового мира
class CGameWorld
{
private:
    /*!
	Создать игрока
	@param [__in] serial Серийник игрока
	@return
	*/
    void CreatePlayer(const uint &serial);

    /*!
	Удалить игрока
	@return
	*/
    void RemovePlayer();

public:
    CGameWorld(const uint &serial);
    ~CGameWorld();

    //! Предметы в памяти
    WORLD_MAP m_Map;

    //! Предметы в мире
    CGameObject *m_Items;

    void IgnoreReset();

    void RemoveRangedObjects();

    /*!
	Установить текущего чара с указанным серийником как основного
	@param [__in] serial Серийник нового игрока
	@return
	*/
    void SetPlayer(const uint &serial);

    /*!
	Создать (или взять, если уже существует) игровой предмет
	@param [__in] serial Серийник предмета
	@return Ссылка на предмет
	*/
    CGameItem *GetWorldItem(const uint &serial);

    /*!
	Создать (или взять, если уже существует) игрового персонажа
	@param [__in] serial Серийник персонажа
	@return Ссылка на персонажа
	*/
    CGameCharacter *GetWorldCharacter(const uint &serial);

    /*!
	Найти игровой объект в памяти
	@param [__in] serial Серийник объекта
	@return Ссылка на объект или nullptr
	*/
    CGameObject *FindWorldObject(const uint &serial);

    /*!
	Найти игровой предмет в памяти
	@param [__in] serial Серийник предмета
	@return Ссылка на предмет или nullptr
	*/
    CGameItem *FindWorldItem(const uint &serial);

    /*!
	Найти игрового персонажа в памяти
	@param [__in] serial Серийник персонажа
	@return Ссылка а персонажа или nullptr
	*/
    CGameCharacter *FindWorldCharacter(const uint &serial);

    /*!
	Удалить объект из памяти
	@param [__in] obj Ссылка на объект
	@return
	*/
    void RemoveObject(CGameObject *obj);

    /*!
	Вынуть объект из контейнера
	@param [__in] obj Ссылка на объект
	@return
	*/
    void RemoveFromContainer(CGameObject *obj);

    /*!
	Очистить указанный контейнер
	@param [__in] obj Ссылка на объект (контейнер)
	@return
	*/
    void ClearContainer(CGameObject *obj);

    /*!
	Положить в контейнер
	@param [__in] obj Ссылка на объект
	@param [__in] containerSerial Серийник контейнера
	@return
	*/
    void PutContainer(CGameObject *obj, const uint &containerSerial)
    {
        CGameObject *cnt = FindWorldObject(containerSerial);
        if (cnt != nullptr)
            PutContainer(obj, cnt);
    }

    /*!
	Положить в контейнер
	@param [__in] obj Ссылка на объект
	@param [__in] container Ссылка на контейнер
	@return
	*/
    void PutContainer(CGameObject *obj, CGameObject *container);

    /*!
	Одеть предмет
	@param [__in] obj Ссылка на предмет
	@param [__in] containerSerial Серийник контейнера
	@param [__in] layer Слой, в который одеть предмет
	@return
	*/
    void PutEquipment(CGameItem *obj, const uint &containerSerial, const int &layer)
    {
        CGameObject *cnt = FindWorldObject(containerSerial);
        if (cnt != nullptr)
            PutEquipment(obj, cnt, layer);
    }

    /*!
	Одеть предмет
	@param [__in] obj Ссылка на объект
	@param [__in] container Ссылка на контейнер
	@param [__in] layer Слой, в который одеть предмет
	@return
	*/
    void PutEquipment(CGameItem *obj, CGameObject *container, const int &layer)
    {
        PutContainer(obj, container);
        obj->SetLayer(layer);
    }

    /*!
	Поднять объект вверх в очереди
	@param [__in] obj Ссылка на объект
	@return
	*/
    void MoveToTop(CGameObject *obj);

    void UpdateContainedItem(
        const uint &serial,
        const ushort &graphic,
        const uchar &graphicIncrement,
        const ushort &count,
        const int &x,
        const int &y,
        const uint &containerSerial,
        const ushort &color);

    void UpdateGameObject(
        uint serial,
        ushort graphic,
        uchar graphicIncrement,
        int count,
        int x,
        int y,
        char z,
        uchar direction,
        ushort color,
        uchar flags,
        int a11,
        UPDATE_GAME_OBJECT_TYPE updateType,
        ushort a13);

    void UpdatePlayer(
        const uint &serial,
        const ushort &graphic,
        const uchar &graphicIncrement,
        const ushort &color,
        const uchar &flags,
        const int &x,
        const int &y,
        const ushort &serverID,
        const uchar &direction,
        const char &z);

    ushort FixColor(const ushort &color, const ushort &defaultColor);

    /*!
	Дамп предметов, хранящихся в памяти
	@param [__in] nCount Количество отступов
	@param [__in_opt] serial Серийник родителя
	@return
	*/
    void Dump(uchar nCount = 0, uint serial = 0xFFFFFFFF);
};

//! Указатель на мир
extern CGameWorld *g_World;

} // namespace xuoassist

#endif // XUOASSIST_GAMEWORLD_H
