// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameworld.cpp). UI-poking call sites
// (g_Map, g_TabMain, g_TabDisplay, g_ObjectInspectorForm, g_CorpseManager,
// g_UOMapManager) are marked PORT-TODO and land with their modules in later
// rounds; all model state mutations are verbatim. LOG() goes through the client
// logger; g_Timer.elapsed() became g_Ticks.

#include "gameworld.h"
#include "gameplayer.h"
#include "core/global.h"
#include "core/assistantevents.h"
#include <common/logging/logging.h>

namespace xuoassist {

CGameWorld *g_World = nullptr;

CGameWorld::CGameWorld(const uint &serial)
{
    m_Items = nullptr;
    CreatePlayer(serial);
}

CGameWorld::~CGameWorld()
{
    RemovePlayer();

    CGameObject *obj = m_Items;
    while (obj != nullptr)
    {
        CGameObject *next = (CGameObject *)obj->m_Next;
        RemoveObject(obj);
        // delete obj;
        obj = next;
    }

    m_Map.clear();

    m_Items = nullptr;
}

/*!
Создать игрока
@param [__in] serial Серийник игрока
@return
*/
void CGameWorld::CreatePlayer(const uint &serial)
{
    RemovePlayer();

    g_PlayerSerial = serial;
    g_Player = new CPlayer(serial);

    m_Map[serial] = g_Player;

    if (m_Items != nullptr)
    {
        m_Items->Add(g_Player);
    }
    else
    {
        m_Items = g_Player;
        m_Items->m_Next = nullptr;
        m_Items->m_Prev = nullptr;
    }
}

/*!
Удалить игрока
@return
*/
void CGameWorld::RemovePlayer()
{
    if (g_Player != nullptr)
    {
        RemoveFromContainer(g_Player);
        m_Map[g_Player->GetSerial()] = nullptr;
        m_Map.erase(g_Player->GetSerial());
        delete g_Player;
        g_Player = nullptr;
        g_PlayerSerial = 0;
    }
}

/*!
Установить текущего чара с указанным серийником как основного
@param [__in] serial Серийник нового игрока
@return
*/
void CGameWorld::SetPlayer(const uint &serial)
{
    if (serial != g_Player->GetSerial())
    {
        CreatePlayer(serial);
    }
}

void CGameWorld::RemoveRangedObjects()
{
    int objectsRange = GetInt(VKI_VIEW_RANGE);

    CGameObject *go = m_Items;

    while (go != nullptr)
    {
        CGameObject *next = (CGameObject *)go->m_Next;

        if (go->GetContainer() == 0xFFFFFFFF && !go->IsPlayer())
        {
            if (GetDistance(g_RemoveRangeXY.x(), g_RemoveRangeXY.y(), go->GetX(), go->GetY()) >
                objectsRange)
            {
                // PORT-TODO(party): g_PartyManager.SaveMemberInfo for party
                // members once managers/partymanager is ported.

                RemoveObject(go);
            }
        }

        go = next;
    }
}

void CGameWorld::IgnoreReset()
{
    for (WORLD_MAP::iterator i = m_Map.begin(); i != m_Map.end(); ++i)
    {
        i->second->SetIgnored(false);
    }
}

/*!
Создать (или взять, если уже существует) игровой предмет
@param [__in] serial Серийник предмета
@return Ссылка на предмет
*/
CGameItem *CGameWorld::GetWorldItem(const uint &serial)
{
    WORLD_MAP::iterator i = m_Map.find(serial);

    if (i == m_Map.end() || (*i).second == nullptr)
    {
        CGameItem *obj = new CGameItem(serial);

        m_Map[serial] = obj;

        if (m_Items != nullptr)
        {
            m_Items->AddObject(obj);
        }
        else
        {
            m_Items = obj;
            m_Items->m_Next = nullptr;
            m_Items->m_Prev = nullptr;
        }

        return obj;
    }

    return (CGameItem *)(*i).second;
}

/*!
Создать (или взять, если уже существует) игрового персонажа
@param [__in] serial Серийник персонажа
@return Ссылка на персонажа
*/
CGameCharacter *CGameWorld::GetWorldCharacter(const uint &serial)
{
    WORLD_MAP::iterator i = m_Map.find(serial);

    if (i == m_Map.end() || (*i).second == nullptr)
    {
        CGameCharacter *obj = new CGameCharacter(serial);

        m_Map[serial] = obj;

        if (m_Items != nullptr)
        {
            m_Items->AddObject(obj);
        }
        else
        {
            m_Items = obj;
            m_Items->m_Next = nullptr;
            m_Items->m_Prev = nullptr;
        }

        return obj;
    }

    return (CGameCharacter *)i->second;
}

/*!
Найти игровой объект в памяти
@param [__in] serial Серийник объекта
@return Ссылка на объект или nullptr
*/
CGameObject *CGameWorld::FindWorldObject(const uint &serial)
{
    CGameObject *result = nullptr;

    WORLD_MAP::iterator i = m_Map.find(serial);
    if (i != m_Map.end())
    {
        result = (*i).second;
    }

    return result;
}

/*!
Найти игровой предмет в памяти
@param [__in] serial Серийник предмета
@return Ссылка на предмет или nullptr
*/
CGameItem *CGameWorld::FindWorldItem(const uint &serial)
{
    CGameItem *result = nullptr;

    WORLD_MAP::iterator i = m_Map.find(serial);
    if (i != m_Map.end() && !((*i).second)->GetNPC())
    {
        result = (CGameItem *)(*i).second;
    }

    return result;
}

/*!
Найти игрового персонажа в памяти
@param [__in] serial Серийник персонажа
@return Ссылка а персонажа или nullptr
*/
CGameCharacter *CGameWorld::FindWorldCharacter(const uint &serial)
{
    CGameCharacter *result = nullptr;

    WORLD_MAP::iterator i = m_Map.find(serial);
    if (i != m_Map.end() && ((*i).second)->GetNPC())
    {
        result = (CGameCharacter *)i->second;
    }

    return result;
}

/*!
Удалить объект из памяти
@param [__in] obj Ссылка на объект
@return
*/
void CGameWorld::RemoveObject(CGameObject *obj)
{
    RemoveFromContainer(obj);

    uint serial = obj->GetSerial();
    m_Map[serial] = nullptr;
    m_Map.erase(serial);
    delete obj;
}

/*!
Вынуть объект из контейнера
@param [__in] obj Ссылка на объект
@return
*/
void CGameWorld::RemoveFromContainer(CGameObject *obj)
{
    if (obj->GetContainer() != 0xFFFFFFFF)
    {
        CGameObject *container = FindWorldObject(obj->GetContainer());
        if (container != nullptr)
        {
            container->Reject(obj);
        }
        else
        {
            obj->SetContainer(0xFFFFFFFF);
        }
    }
    else
    {
        if (m_Items != nullptr)
        {
            if (m_Items->GetSerial() == obj->GetSerial())
            {
                m_Items = (CGameObject *)m_Items->m_Next;
                if (m_Items != nullptr)
                {
                    m_Items->m_Prev = nullptr;
                }
            }
            else
            {
                if (obj->m_Next != nullptr)
                {
                    if (obj->m_Prev != nullptr)
                    {
                        obj->m_Prev->m_Next = obj->m_Next;
                        obj->m_Next->m_Prev = obj->m_Prev;
                    }
                    else
                    { // WTF???
                        obj->m_Next->m_Prev = nullptr;
                    }
                }
                else if (obj->m_Prev != nullptr)
                {
                    obj->m_Prev->m_Next = nullptr;
                }
            }
        }
    }

    obj->m_Next = nullptr;
    obj->m_Prev = nullptr;
}

/*!
Очистить указанный контейнер
@param [__in] obj Ссылка на объект (контейнер)
@return
*/
void CGameWorld::ClearContainer(CGameObject *obj)
{
    if (!obj->Empty())
    {
        obj->Clear();
    }
}

/*!
Положить в контейнер
@param [__in] obj Ссылка на объект
@param [__in] container Ссылка на контейнер
@return
*/
void CGameWorld::PutContainer(CGameObject *obj, CGameObject *container)
{
    RemoveFromContainer(obj);
    container->AddItem(obj);
}

/*!
Поднять объект вверх в очереди
@param [__in] obj Ссылка на объект
@return
*/
void CGameWorld::MoveToTop(CGameObject *obj)
{
    if (obj == nullptr)
    {
        return;
    }

    if (obj->m_Next == nullptr)
    {
        return;
    }

    if (obj->GetContainer() == 0xFFFFFFFF)
    {
        if (obj->m_Prev == nullptr)
        {
            m_Items = (CGameObject *)obj->m_Next;
            m_Items->m_Prev = nullptr;

            CGameObject *item = m_Items;

            while (item != nullptr)
            {
                if (item->m_Next == nullptr)
                {
                    item->m_Next = obj;
                    obj->m_Prev = item;
                    obj->m_Next = nullptr;

                    break;
                }

                item = (CGameObject *)item->m_Next;
            }
        }
        else
        {
            CGameObject *item = (CGameObject *)obj->m_Next;

            obj->m_Prev->m_Next = obj->m_Next;
            obj->m_Next->m_Prev = obj->m_Prev;

            while (item != nullptr)
            {
                if (item->m_Next == nullptr)
                {
                    item->m_Next = obj;
                    obj->m_Prev = item;
                    obj->m_Next = nullptr;

                    break;
                }

                item = (CGameObject *)item->m_Next;
            }
        }
    }
    else
    {
        CGameObject *container = FindWorldObject(obj->GetContainer());

        if (container == nullptr)
        {
            return;
        }

        if (obj->m_Prev == nullptr)
        {
            container->m_Items = obj->m_Next;
            container->m_Items->m_Prev = nullptr;

            CGameObject *item = (CGameObject *)container->m_Items;

            while (item != nullptr)
            {
                if (item->m_Next == nullptr)
                {
                    item->m_Next = obj;
                    obj->m_Prev = item;
                    obj->m_Next = nullptr;

                    break;
                }

                item = (CGameObject *)item->m_Next;
            }
        }
        else
        {
            CGameObject *item = (CGameObject *)obj->m_Next;

            obj->m_Prev->m_Next = obj->m_Next;
            obj->m_Next->m_Prev = obj->m_Prev;

            while (item != nullptr)
            {
                if (item->m_Next == nullptr)
                {
                    item->m_Next = obj;
                    obj->m_Prev = item;
                    obj->m_Next = nullptr;

                    break;
                }

                item = (CGameObject *)item->m_Next;
            }
        }
    }
}

void CGameWorld::UpdateGameObject(
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
    ushort a13)
{
    (void)a11;
    (void)a13;

    CGameCharacter *character = nullptr;
    CGameItem *item = nullptr;
    CGameObject *obj = FindWorldObject(serial);

    if (obj == nullptr)
    {
        if (((serial & 0x40000000) == 0u) && updateType != 3)
        {
            character = GetWorldCharacter(serial);

            if (character == nullptr)
            {
                Warning(Assistant, "No memory?");
                return;
            }

            obj = character;
            character->SetGraphic(graphic + graphicIncrement);
            // character->OnGraphicChange(1000);
            character->SetDirection(direction);
            character->SetColor(FixColor(color, (color & 0x8000)));
            character->SetX(x);
            character->SetY(y);
            character->SetZ(z);
            character->SetFlags(flags);

            // was g_Map->CheckForUpdate(serial) (radar): the map window round
            // consumes it through the events seam
            g_AssistantEvents->OnMapUpdate(serial);
        }
        else
        {
            item = GetWorldItem(serial);

            if (item == nullptr)
            {
                Warning(Assistant, "No memory?");
                return;
            }

            if (graphic == 0x2006)
            {
                g_LastCorpseObject = serial;

                // PORT-TODO(corpse): corpse auto-open bookkeeping
                // (g_TabMain->CorpsesAutoopen() + g_CorpseManager.Add(...,
                // g_StartTimer + CorpseKeepDelay())) once tabmain/corpsemanager
                // are ported.
            }

            obj = item;
        }
    }
    else
    {
        if (obj->GetContainer() != 0xFFFFFFFF)
        {
            RemoveFromContainer(obj);
            obj->SetContainer(0xFFFFFFFF);
            m_Items->AddObject(obj);
        }

        if (obj->GetNPC())
        {
            character = (CGameCharacter *)obj;
        }
        else
        {
            item = (CGameItem *)obj;
        }
    }

    if (obj == nullptr)
    {
        return;
    }

    obj->SetMapIndex(g_CurrentMap);

    if (!obj->GetNPC())
    {
        if (graphic != 0x2006)
        {
            graphic += graphicIncrement;
        }

        if (updateType == UGOT_MULTI)
        {
            item->SetMultiBody(true);

            item->SetGraphic(graphic & 0x3FFF);

            // DEVIATION (phase 5): was g_UOMapManager.UOAMnotifyHouse(x, y,
            // graphic) - a Windows-only WM_USER notify to the EXTERNAL UO Auto
            // Map app (the Qt build compiled it out on non-Windows too); there
            // is no HWND message host in the in-client port. Houses appear on
            // the map window through the live world overlays instead.
        }
        else
        {
            item->SetMultiBody(false);

            item->SetGraphic(graphic);
            item->SetTiledataFlags(g_GetStaticFlags(graphic));
        }

        item->SetX(x);
        item->SetY(y);
        item->SetZ(z);

        if (graphic == 0x2006)
        {
            item->SetLayer(direction);
        }

        item->SetColor(FixColor(color, (color & 0x8000)));

        if (count < 1)
        {
            item->SetCount(1);
        }
        else
        {
            item->SetCount(count);
        }

        item->SetFlags(flags);

        // item->OnGraphicChange(direction);
    }
    else
    {
        graphic += graphicIncrement;

        {
            character->SetGraphic(graphic & 0x3FFF);
            character->SetX(x);
            character->SetY(y);
            character->SetZ(z);
            character->SetDirection(direction);
            character->SetColor(FixColor(color, (color & 0x8000)));
            character->SetFlags(flags);

            // was g_Map->CheckForUpdate(serial) (radar): the map window round
            // consumes it through the events seam
            g_AssistantEvents->OnMapUpdate(serial);
        }
    }

    // was g_ObjectInspectorForm->CheckUpdate(obj): the inspector round
    // consumes it through the events seam
    g_AssistantEvents->OnInspectorUpdate(obj->GetSerial());

    MoveToTop(obj);
}

void CGameWorld::UpdatePlayer(
    const uint &serial,
    const ushort &graphic,
    const uchar &graphicIncrement,
    const ushort &color,
    const uchar &flags,
    const int &x,
    const int &y,
    const ushort &serverID,
    const uchar &direction,
    const char &z)
{
    (void)graphicIncrement;
    (void)serverID;

    if (serial == g_PlayerSerial)
    {
        g_Player->SetX(x);
        g_Player->SetY(y);
        g_Player->SetZ(z);

        g_Player->SetGraphic(graphic);
        // g_Player->OnGraphicChange();

        g_Player->SetDirection(direction);
        g_Player->SetColor(FixColor(color, 0));

        g_Player->SetFlags(flags);

        // was g_ObjectInspectorForm->CheckUpdate(g_Player) + the radar
        // SetWantRedraw poke: consumed through the events seam
        g_AssistantEvents->OnInspectorUpdate(g_Player->GetSerial());
        g_AssistantEvents->OnMapRedraw();

        MoveToTop(g_Player);
    }
}

void CGameWorld::UpdateContainedItem(
    const uint &serial,
    const ushort &graphic,
    const uchar &graphicIncrement,
    const ushort &count,
    const int &x,
    const int &y,
    const uint &containerSerial,
    const ushort &color)
{
    CGameObject *container = FindWorldObject(containerSerial);

    if (container == nullptr)
    {
        return;
    }

    CGameObject *obj = FindWorldObject(serial);

    if (obj != nullptr && (!container->IsCorpse() || ((CGameItem *)obj)->GetLayer() == OL_NONE))
    {
        RemoveObject(obj);
        obj = nullptr;
    }

    if (obj == nullptr)
    {
        if ((serial & 0x40000000) != 0u)
        {
            obj = GetWorldItem(serial);
        }
        else
        {
            obj = GetWorldCharacter(serial);
        }
    }

    if (obj == nullptr)
    {
        Warning(Assistant, "No memory?");
        return;
    }

    obj->SetMapIndex(g_CurrentMap);

    obj->SetGraphic(graphic + graphicIncrement);

    if (!obj->GetNPC())
    {
        ((CGameItem *)obj)->SetTiledataFlags(g_GetStaticFlags(graphic + graphicIncrement));
    }
    obj->SetColor(FixColor(color, (color & 0x8000)));

    if (count < 1)
    {
        obj->SetCount(1);
    }
    else
    {
        obj->SetCount(count);
    }

    obj->SetX(x);
    obj->SetY(y);
    PutContainer(obj, container);

    // was g_ObjectInspectorForm->CheckUpdate(obj): the inspector round
    // consumes it through the events seam
    g_AssistantEvents->OnInspectorUpdate(obj->GetSerial());

    MoveToTop(obj);

    // PORT-TODO(display): g_TabDisplay->CheckRedrawTitle(obj) when the object
    // belongs to the player (obj->GetTopObject() == g_Player) once the display
    // module is ported.
}

ushort CGameWorld::FixColor(const ushort &color, const ushort &defaultColor)
{
    ushort fixedColor = color & 0x3FFF;

    if (fixedColor != 0u)
    {
        if (fixedColor >= 0x0BB8)
        {
            fixedColor = 1;
        }

        fixedColor |= (color & 0xC000);
    }
    else
    {
        fixedColor = defaultColor;
    }

    return fixedColor;
}

/*!
Дамп предметов, хранящихся в памяти
@param [__in] nCount Количество отступов
@param [__in_opt] serial Серийник родителя
@return
*/
void CGameWorld::Dump(uchar tCount, uint serial)
{
    // PORT-TODO(debug): the Qt app compiled this out unless CLOGGER was enabled;
    // re-enable as a client-logger dump when the debug tooling round needs it.
    (void)tCount;
    (void)serial;
}

} // namespace xuoassist
