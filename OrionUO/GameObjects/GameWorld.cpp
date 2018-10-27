// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GameWorld.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGameWorld *g_World = nullptr;

CGameWorld::CGameWorld(int serial)
{
    DEBUG_TRACE_FUNCTION;
    CreatePlayer(serial);
}

CGameWorld::~CGameWorld()
{
    DEBUG_TRACE_FUNCTION;
    RemovePlayer();

    CGameObject *obj = m_Items;
    while (obj != nullptr)
    {
        CGameObject *next = (CGameObject *)obj->m_Next;
        RemoveObject(obj);
        //delete obj;
        obj = next;
    }

    m_Map.clear();

    m_Items = nullptr;
}

void CGameWorld::ResetObjectHandlesState()
{
    QFOR(obj, m_Items, CGameObject *)
    obj->ClosedObjectHandle = false;
}

void CGameWorld::ProcessSound(int ticks, CGameCharacter *gc)
{
    DEBUG_TRACE_FUNCTION;
    if (g_ConfigManager.FootstepsSound && gc->IsHuman() && !gc->Hidden())
    {
        if (!gc->m_Steps.empty() && gc->LastStepSoundTime < ticks)
        {
            int incID = gc->StepSoundOffset;
            int soundID = 0x012B;
            int delaySound = 400;

            if (gc->FindLayer(OL_MOUNT) != nullptr)
            {
                if (gc->m_Steps.back().Direction & 0x80)
                {
                    soundID = 0x0129;
                    delaySound = 150;
                }
                else
                {
                    incID = 0;
                    delaySound = 350;
                }
            }

            if (g_ConfigManager.StandartCharactersAnimationDelay)
                delaySound = delaySound * 13 / 10;
            soundID += incID;
            gc->StepSoundOffset = (incID + 1) % 2;

            g_Orion.PlaySoundEffect(soundID, g_SoundManager.GetVolumeValue()); //0x0129 - 0x0134

            gc->LastStepSoundTime = ticks + delaySound;
        }
    }
}

/*!
Обработка анимации всех персонажей
@return 
*/
void CGameWorld::ProcessAnimation()
{
    DEBUG_TRACE_FUNCTION;
    int delay =
        (g_ConfigManager.StandartCharactersAnimationDelay ? ORIGINAL_CHARACTERS_ANIMATION_DELAY :
                                                            ORION_CHARACTERS_ANIMATION_DELAY);
    g_AnimCharactersDelayValue = (float)delay;
    deque<CGameObject *> toRemove;

    QFOR(obj, m_Items, CGameObject *)
    {
        if (obj->NPC)
        {
            CGameCharacter *gc = obj->GameCharacterPtr();
            uchar dir = 0;
            gc->UpdateAnimationInfo(dir, true);

            ProcessSound(g_Ticks, gc);

            if (gc->LastAnimationChangeTime < g_Ticks && !gc->NoIterateAnimIndex())
            {
                char frameIndex = gc->AnimIndex;

                if (gc->AnimationFromServer && !gc->AnimationDirection)
                    frameIndex--;
                else
                    frameIndex++;

                ushort id = gc->GetMountAnimation();
                int animGroup = gc->GetAnimationGroup(id);
                gc->ProcessGargoyleAnims(animGroup);

                CGameItem *mount = gc->FindLayer(OL_MOUNT);
                if (mount != nullptr)
                {
                    switch (animGroup)
                    {
                        case PAG_FIDGET_1:
                        case PAG_FIDGET_2:
                        case PAG_FIDGET_3:
                        {
                            id = mount->GetMountAnimation();
                            animGroup = gc->GetAnimationGroup(id);
                            break;
                        }
                        default:
                            break;
                    }
                }

                bool mirror = false;

                g_AnimationManager.GetAnimDirection(dir, mirror);

                int currentDelay = delay;

                if (id < MAX_ANIMATIONS_DATA_INDEX_COUNT && dir < 5)
                {
                    CTextureAnimationDirection &direction =
                        g_AnimationManager.m_DataIndex[id].m_Groups[animGroup].m_Direction[dir];
                    g_AnimationManager.AnimID = id;
                    g_AnimationManager.AnimGroup = animGroup;
                    g_AnimationManager.Direction = dir;
                    if (direction.FrameCount == 0)
                        g_AnimationManager.LoadDirectionGroup(direction);

                    if (direction.Address != 0 || direction.IsUOP)
                    {
                        direction.LastAccessTime = g_Ticks;
                        int fc = direction.FrameCount;

                        if (gc->AnimationFromServer)
                        {
                            currentDelay += currentDelay * (int)(gc->AnimationInterval + 1);

                            if (!gc->AnimationFrameCount)
                                gc->AnimationFrameCount = fc;
                            else
                                fc = gc->AnimationFrameCount;

                            if (gc->AnimationDirection) //forward
                            {
                                if (frameIndex >= fc)
                                {
                                    frameIndex = 0;

                                    if (gc->AnimationRepeat)
                                    {
                                        uchar repCount = gc->AnimationRepeatMode;

                                        if (repCount == 2)
                                        {
                                            repCount--;
                                            gc->AnimationRepeatMode = repCount;
                                        }
                                        else if (repCount == 1)
                                            gc->SetAnimation(0xFF);
                                    }
                                    else
                                        gc->SetAnimation(0xFF);
                                }
                            }
                            else //backward
                            {
                                if (frameIndex < 0)
                                {
                                    if (!fc)
                                        frameIndex = 0;
                                    else
                                        frameIndex = fc - 1;

                                    if (gc->AnimationRepeat)
                                    {
                                        uchar repCount = gc->AnimationRepeatMode;

                                        if (repCount == 2)
                                        {
                                            repCount--;
                                            gc->AnimationRepeatMode = repCount;
                                        }
                                        else if (repCount == 1)
                                            gc->SetAnimation(0xFF);
                                    }
                                    else
                                        gc->SetAnimation(0xFF);
                                }
                            }
                        }
                        else
                        {
                            if (frameIndex >= fc)
                            {
                                frameIndex = 0;

                                if (obj->Serial & 0x80000000)
                                    toRemove.push_back(obj);
                            }
                        }

                        gc->AnimIndex = frameIndex;
                    }
                    else if (obj->Serial & 0x80000000)
                        toRemove.push_back(obj);
                }
                else if (obj->Serial & 0x80000000)
                    toRemove.push_back(obj);

                gc->LastAnimationChangeTime = g_Ticks + currentDelay;
            }
        }
        else if (obj->IsCorpse())
        {
            CGameItem *gi = (CGameItem *)obj;
            uchar dir = gi->Layer;

            if (obj->LastAnimationChangeTime < g_Ticks)
            {
                char frameIndex = obj->AnimIndex + 1;

                ushort id = obj->GetMountAnimation();

                bool mirror = false;

                g_AnimationManager.GetAnimDirection(dir, mirror);

                if (id < MAX_ANIMATIONS_DATA_INDEX_COUNT && dir < 5)
                {
                    int animGroup = g_AnimationManager.GetDieGroupIndex(id, gi->UsedLayer);

                    CTextureAnimationDirection &direction =
                        g_AnimationManager.m_DataIndex[id].m_Groups[animGroup].m_Direction[dir];
                    g_AnimationManager.AnimID = id;
                    g_AnimationManager.AnimGroup = animGroup;
                    g_AnimationManager.Direction = dir;
                    if (direction.FrameCount == 0)
                        g_AnimationManager.LoadDirectionGroup(direction);

                    if (direction.Address != 0 || direction.IsUOP)
                    {
                        direction.LastAccessTime = g_Ticks;
                        int fc = direction.FrameCount;

                        if (frameIndex >= fc)
                        {
                            frameIndex = fc - 1;
                        }

                        obj->AnimIndex = frameIndex;
                    }
                }

                obj->LastAnimationChangeTime = g_Ticks + delay;
            }
        }

        obj->UpdateEffects();
    }

    if (!toRemove.empty())
    {
        for (CGameObject *obj : toRemove)
        {
            g_CorpseManager.Remove(0, obj->Serial);

            RemoveObject(obj);
        }

        g_GameScreen.RenderListInitalized = false;
    }
}

/*!
Создать игрока
@param [__in] serial Серийник игрока
@return 
*/
void CGameWorld::CreatePlayer(int serial)
{
    DEBUG_TRACE_FUNCTION;
    RemovePlayer();

    g_PlayerSerial = serial;
    g_Player = new CPlayer(serial);

    m_Map[serial] = g_Player;

    if (m_Items != nullptr)
        m_Items->Add(g_Player);
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
    DEBUG_TRACE_FUNCTION;
    if (g_Player != nullptr)
    {
        RemoveFromContainer(g_Player);
        m_Map[g_Player->Serial] = nullptr;
        m_Map.erase(g_Player->Serial);
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
void CGameWorld::SetPlayer(int serial)
{
    DEBUG_TRACE_FUNCTION;
    if (serial != g_Player->Serial)
        CreatePlayer(serial);
}

/*!
Создать (или взять, если уже существует) игровой предмет
@param [__in] serial Серийник предмета
@return Ссылка на предмет
*/
CGameItem *CGameWorld::GetWorldItem(int serial)
{
    DEBUG_TRACE_FUNCTION;
    WORLD_MAP::iterator i = m_Map.find(serial);

    if (i == m_Map.end() || (*i).second == nullptr)
    {
        CGameItem *obj = new CGameItem(serial);

        m_Map[serial] = obj;

        if (m_Items != nullptr)
            m_Items->AddObject(obj);
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
CGameCharacter *CGameWorld::GetWorldCharacter(int serial)
{
    DEBUG_TRACE_FUNCTION;
    WORLD_MAP::iterator i = m_Map.find(serial);

    if (i == m_Map.end() || (*i).second == nullptr)
    {
        CGameCharacter *obj = new CGameCharacter(serial);

        m_Map[serial] = obj;

        if (m_Items != nullptr)
            m_Items->AddObject(obj);
        else
        {
            m_Items = obj;
            m_Items->m_Next = nullptr;
            m_Items->m_Prev = nullptr;
        }

        return obj;
    }

    return i->second->GameCharacterPtr();
}

/*!
Найти игровой объект в памяти
@param [__in] serial Серийник объекта
@return Ссылка на объект или nullptr
*/
CGameObject *CGameWorld::FindWorldObject(int serial)
{
    DEBUG_TRACE_FUNCTION;
    CGameObject *result = nullptr;

    WORLD_MAP::iterator i = m_Map.find(serial);
    if (i != m_Map.end())
        result = (*i).second;

    return result;
}

/*!
Найти игровой предмет в памяти
@param [__in] serial Серийник предмета
@return Ссылка на предмет или nullptr
*/
CGameItem *CGameWorld::FindWorldItem(int serial)
{
    DEBUG_TRACE_FUNCTION;
    CGameItem *result = nullptr;

    WORLD_MAP::iterator i = m_Map.find(serial);
    if (i != m_Map.end() && !((*i).second)->NPC)
        result = (CGameItem *)(*i).second;

    return result;
}

/*!
Найти игрового персонажа в памяти
@param [__in] serial Серийник персонажа
@return Ссылка а персонажа или nullptr
*/
CGameCharacter *CGameWorld::FindWorldCharacter(int serial)
{
    DEBUG_TRACE_FUNCTION;
    CGameCharacter *result = nullptr;

    WORLD_MAP::iterator i = m_Map.find(serial);
    if (i != m_Map.end() && ((*i).second)->NPC)
        result = i->second->GameCharacterPtr();

    return result;
}

void CGameWorld::ReplaceObject(CGameObject *obj, int newSerial)
{
    DEBUG_TRACE_FUNCTION;

    m_Map[obj->Serial] = nullptr;
    m_Map.erase(obj->Serial);

    QFOR(item, obj->m_Items, CGameObject *)
    item->Container = newSerial;

    m_Map[newSerial] = obj;
    obj->Serial = newSerial;
}

/*!
Удалить объект из памяти
@param [__in] obj Ссылка на объект
@return 
*/
void CGameWorld::RemoveObject(CGameObject *obj)
{
    DEBUG_TRACE_FUNCTION;
    RemoveFromContainer(obj);

    uint serial = obj->Serial;
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
    DEBUG_TRACE_FUNCTION;
    uint containerSerial = obj->Container;

    if (containerSerial != 0xFFFFFFFF)
    {
        if (containerSerial < 0x40000000)
            g_GumpManager.UpdateContent(containerSerial, 0, GT_PAPERDOLL);
        else
            g_GumpManager.UpdateContent(containerSerial, 0, GT_CONTAINER);

        CGameObject *container = FindWorldObject(containerSerial);

        if (container != nullptr)
            container->Reject(obj);
        else
            obj->Container = 0xFFFFFFFF;
    }
    else
    {
        g_GameScreen.RenderListInitalized = false;

        if (m_Items != nullptr)
        {
            if (m_Items == obj)
            {
                m_Items = (CGameObject *)m_Items->m_Next;
                if (m_Items != nullptr)
                    m_Items->m_Prev = nullptr;
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
                    else //WTF???
                        obj->m_Next->m_Prev = nullptr;
                }
                else if (obj->m_Prev != nullptr)
                    obj->m_Prev->m_Next = nullptr;
            }
        }
    }

    obj->m_Next = nullptr;
    obj->m_Prev = nullptr;
    obj->RemoveRender();
}

/*!
Очистить указанный контейнер
@param [__in] obj Ссылка на объект (контейнер)
@return 
*/
void CGameWorld::ClearContainer(CGameObject *obj)
{
    DEBUG_TRACE_FUNCTION;
    if (!obj->Empty())
        obj->Clear();
}

/*!
Положить в контейнер
@param [__in] obj Ссылка на объект
@param [__in] container Ссылка на контейнер
@return 
*/
void CGameWorld::PutContainer(CGameObject *obj, CGameObject *container)
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
    if (obj == nullptr)
        return;

    if (obj->Container == 0xFFFFFFFF)
        g_MapManager.AddRender(obj);

    if (obj->m_Next == nullptr)
        return;

    if (obj->Container == 0xFFFFFFFF)
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
        CGameObject *container = FindWorldObject(obj->Container);

        if (container == nullptr)
            return;

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

/*!
Поиск объекта
@param [__in] serialStart Начальный серийник для поиска
@param [__in] scanDistance Дистанция поиска
@param [__in] scanType Тип объектов поиска
@param [__in] scanMode Режим поиска
@return Ссылка на найденный объект или nullptr
*/
CGameObject *CGameWorld::SearchWorldObject(
    int serialStart, int scanDistance, SCAN_TYPE_OBJECT scanType, SCAN_MODE_OBJECT scanMode)
{
    DEBUG_TRACE_FUNCTION;
    CGameObject *result = nullptr;

    CGameObject *start = FindWorldObject(serialStart);

    int count = 2;
    int startI = 0;

    if (scanMode == SMO_PREV)
    {
        if (start == nullptr || start->m_Prev == nullptr)
        {
            start = m_Items;
            startI = 1;
        }
        else
            start = (CGameObject *)start->m_Prev;
    }
    else
    {
        if (start == nullptr || start->m_Next == nullptr)
        {
            start = m_Items;
            startI = 1;
        }
        else
            start = (CGameObject *)start->m_Next;
    }

    if (start != nullptr)
    {
        CGameObject *obj = start;
        int distance = 100500;
        CGameObject *distanceResult = nullptr;

        for (int i = startI; i < count && result == nullptr; i++)
        {
            if (i)
            {
                obj = m_Items;

                if (scanMode == SMO_PREV)
                {
                    while (obj != nullptr && obj->m_Next != nullptr)
                        obj = (CGameObject *)obj->m_Next;
                }
            }

            while (obj != nullptr && result == nullptr)
            {
                int dist = GetDistance(obj, g_Player);

                if (obj->Serial != serialStart && dist <= scanDistance)
                {
                    bool condition = false;

                    if (scanType == STO_OBJECTS)
                        condition = (!obj->NPC && !((CGameItem *)obj)->MultiBody);
                    else if (obj->NPC && !obj->IsPlayer())
                    {
                        if (scanType == STO_HOSTILE)
                        {
                            CGameCharacter *gc = obj->GameCharacterPtr();

                            condition =
                                (gc->Notoriety >= NT_SOMEONE_GRAY && gc->Notoriety <= NT_MURDERER);
                        }
                        else if (scanType == STO_PARTY)
                            condition = g_Party.Contains(obj->Serial);
                        //else if (scanType == STO_FOLLOWERS)
                        //	condition = false;
                        else //if (scanType == STO_MOBILES)
                            condition = true;
                    }

                    if (condition)
                    {
                        if (scanMode == SMO_NEAREST)
                        {
                            if (dist < distance)
                            {
                                distance = dist;
                                distanceResult = obj;
                            }
                        }
                        else
                        {
                            result = obj;

                            break;
                        }
                    }
                }

                if (scanMode == SMO_PREV)
                    obj = (CGameObject *)obj->m_Prev;
                else
                    obj = (CGameObject *)obj->m_Next;
            }
        }

        if (distanceResult != nullptr)
            result = distanceResult;
    }

    return result;
}

void CGameWorld::UpdateGameObject(
    int serial,
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
    LOG("UpdateGameObject 0x%08lX:0x%04X 0x%04X (%i) %d:%d:%d %i\n",
        serial,
        graphic,
        color,
        count,
        x,
        y,
        z,
        direction);

    CGameCharacter *character = nullptr;
    CGameItem *item = nullptr;
    CGameObject *obj = FindWorldObject(serial);

    if (g_ObjectInHand.Enabled && g_ObjectInHand.Serial == serial)
    {
        if (g_ObjectInHand.Container && g_ObjectInHand.Container != 0xFFFFFFFF)
        {
            if (!g_ObjectInHand.Layer)
                g_GumpManager.UpdateContent(g_ObjectInHand.Container, 0, GT_CONTAINER);
            else
                g_GumpManager.UpdateContent(g_ObjectInHand.Container, 0, GT_PAPERDOLL);
        }

        g_ObjectInHand.UpdatedInWorld = true;
    }

    bool created = false;

    if (obj == nullptr)
    {
        created = true;
        LOG("created ");

        if (!(serial & 0x40000000) && updateType != 3)
        {
            character = GetWorldCharacter(serial);

            if (character == nullptr)
            {
                LOG("No memory?\n");
                return;
            }

            obj = character;
            character->Graphic = graphic + graphicIncrement;
            character->OnGraphicChange(1000);
            character->Direction = direction;
            character->Color = g_ColorManager.FixColor(color, (color & 0x8000));
            character->SetX(x);
            character->SetY(y);
            character->SetZ(z);
            character->SetFlags(flags);
        }
        else
        {
            item = GetWorldItem(serial);

            if (item == nullptr)
            {
                LOG("No memory?\n");
                return;
            }

            obj = item;
        }
    }
    else
    {
        LOG("updated ");

        if (obj->Container != 0xFFFFFFFF)
        {
            RemoveFromContainer(obj);
            obj->Container = 0xFFFFFFFF;
            m_Items->AddObject(obj);
        }

        if (obj->NPC)
            character = (CGameCharacter *)obj;
        else
            item = (CGameItem *)obj;
    }

    if (obj == nullptr)
        return;

    obj->MapIndex = g_CurrentMap;

    if (!obj->NPC)
    {
        if (graphic != 0x2006)
            graphic += graphicIncrement;

        if (updateType == UGOT_MULTI)
        {
            item->MultiBody = true;
            item->WantUpdateMulti = ((graphic & 0x3FFF) != obj->Graphic) || (obj->GetX() != x) ||
                                    (obj->GetY() != y) || (obj->GetZ() != z);

            item->Graphic = graphic & 0x3FFF;
        }
        else
        {
            item->MultiBody = false;

            item->Graphic = graphic;
        }

        if (item->Dragged)
        {
            g_GumpManager.CloseGump(serial, 0, GT_DRAG);
            item->Dragged = false;
        }

        item->SetX(x);
        item->SetY(y);
        item->SetZ(z);
        item->LightID = direction;

        if (graphic == 0x2006)
            item->Layer = direction;

        item->Color = g_ColorManager.FixColor(color, (color & 0x8000));

        if (!count)
            count = 1;

        item->Count = count;
        item->SetFlags(flags);

        item->OnGraphicChange(direction);

        LOG("serial:0x%08X graphic:0x%04X color:0x%04X count:%i xyz:%d,%d,%d light:%i flags:0x%02X\n",
            obj->Serial,
            obj->Graphic,
            obj->Color,
            item->Count,
            obj->GetX(),
            obj->GetY(),
            obj->GetZ(),
            direction,
            obj->GetFlags());
    }
    else
    {
        graphic += graphicIncrement;

        bool found = false;

        if (character->m_Steps.size() != MAX_STEPS_COUNT)
        {
            //if (character->Graphic == graphic && character->Flags == flags)
            {
                if (!character->m_Steps.empty())
                {
                    CWalkData &wd = character->m_Steps.back();

                    if (wd.X == x && wd.Y == y && wd.Z == z && wd.Direction == direction)
                    {
                        found = true;
                    }
                }
                else if (
                    character->GetX() == x && character->GetY() == y && character->GetZ() == z &&
                    character->Direction == direction)
                {
                    found = true;
                }
            }

            if (!found)
            {
                if (character->m_Steps.empty())
                    character->LastStepTime = g_Ticks;

                character->m_Steps.push_back(
                    CWalkData(x, y, z, direction, graphic & 0x3FFF, flags));
                found = true;
            }
        }

        if (!found)
        {
            character->SetX(x);
            character->SetY(y);
            character->SetZ(z);
            character->Direction = direction;

            character->m_Steps.clear();

            character->OffsetX = 0;
            character->OffsetY = 0;
            character->OffsetZ = 0;
        }

        character->Graphic = graphic & 0x3FFF;
        character->Color = g_ColorManager.FixColor(color, (color & 0x8000));
        character->SetFlags(flags);

        LOG("NPC serial:0x%08X graphic:0x%04X color:0x%04X xyz:%d,%d,%d flags:0x%02X direction:%d notoriety:%d\n",
            obj->Serial,
            obj->Graphic,
            obj->Color,
            obj->GetX(),
            obj->GetY(),
            obj->GetZ(),
            obj->GetFlags(),
            character->Direction,
            character->Notoriety);
    }

    if (created && g_ConfigManager.ShowIncomingNames && !obj->Clicked && !obj->GetName().length())
    {
        if (obj->NPC || obj->IsCorpse())
            g_Orion.Click(obj->Serial);
    }

    MoveToTop(obj);
}

void CGameWorld::UpdatePlayer(
    int serial,
    ushort graphic,
    uchar graphicIncrement,
    ushort color,
    uchar flags,
    int x,
    int y,
    ushort serverID,
    uchar direction,
    char z)
{
    if (serial == g_PlayerSerial)
    {
        g_Player->CloseBank();

        g_Walker.WalkingFailed = false;

        g_Player->SetX(x);
        g_Player->SetY(y);
        g_Player->SetZ(z);

        g_RemoveRangeXY.X = x;
        g_RemoveRangeXY.Y = y;

        UOI_PLAYER_XYZ_DATA xyzData = { g_RemoveRangeXY.X, g_RemoveRangeXY.Y, 0 };
        g_PluginManager.WindowProc(
            g_OrionWindow.Handle, UOMSG_UPDATE_REMOVE_POS, (WPARAM)&xyzData, 0);

        g_GameScreen.UpdateDrawPos = true;

        bool oldDead = g_Player->Dead();
        ushort oldGraphic = g_Player->Graphic;

        g_Player->Graphic = graphic;
        g_Player->OnGraphicChange();

        g_Player->Direction = direction;
        g_Player->Color = g_ColorManager.FixColor(color);

        //UpdatePlayerCoordinates(x, y, z, serverID);

        g_Player->SetFlags(flags);

        g_Walker.DenyWalk(-1, -1, -1, -1);
        g_Weather.Reset();

        if (oldGraphic && oldGraphic != g_Player->Graphic)
        {
            if (g_Player->Dead())
                g_Target.Reset();
        }

        if (oldDead != g_Player->Dead())
        {
            if (g_Player->Dead())
                g_Orion.ChangeSeason(ST_DESOLATION, DEATH_MUSIC_INDEX);
            else
                g_Orion.ChangeSeason(g_OldSeason, g_OldSeasonMusic);
        }

        g_Walker.ResendPacketSended = false;

        g_GumpManager.RemoveRangedGumps();

        MoveToTop(g_Player);
    }
}

void CGameWorld::UpdateItemInContainer(CGameObject *obj, CGameObject *container, int x, int y)
{
    obj->SetX(x);
    obj->SetY(y);
    PutContainer(obj, container);

    uint containerSerial = container->Serial;

    CGump *gump = g_GumpManager.UpdateContent(containerSerial, 0, GT_BULLETIN_BOARD);

    if (gump != nullptr) //Message board item
        CPacketBulletinBoardRequestMessageSummary(containerSerial, obj->Serial).Send();
    else
    {
        gump = g_GumpManager.UpdateContent(containerSerial, 0, GT_SPELLBOOK);

        if (gump == nullptr)
        {
            gump = g_GumpManager.UpdateContent(containerSerial, 0, GT_CONTAINER);

            if (gump != nullptr && gump->GumpType == GT_CONTAINER)
                ((CGumpContainer *)gump)->UpdateItemCoordinates(obj);
        }

        if (gump != nullptr && !container->NPC)
        {
            ((CGameItem *)container)->Opened = true;
        }
    }

    CGameObject *top = container->GetTopObject();

    if (top != nullptr)
    {
        top = top->FindSecureTradeBox();

        if (top != nullptr)
            g_GumpManager.UpdateContent(0, top->Serial, GT_TRADE);
    }
}

void CGameWorld::UpdateContainedItem(
    int serial,
    ushort graphic,
    uchar graphicIncrement,
    ushort count,
    int x,
    int y,
    int containerSerial,
    ushort color)
{
    if (g_ObjectInHand.Serial == serial && g_ObjectInHand.Dropped)
        g_ObjectInHand.Clear();

    CGameObject *container = FindWorldObject(containerSerial);

    if (container == nullptr)
        return;

    CGameObject *obj = FindWorldObject(serial);

    if (obj != nullptr && (!container->IsCorpse() || ((CGameItem *)obj)->Layer == OL_NONE))
    {
        RemoveObject(obj);
        obj = nullptr;
    }

    if (obj == nullptr)
    {
        if (serial & 0x40000000)
            obj = GetWorldItem(serial);
        else
            obj = GetWorldCharacter(serial);
    }

    if (obj == nullptr)
    {
        LOG("No memory?\n");
        return;
    }

    obj->MapIndex = g_CurrentMap;

    obj->Graphic = graphic + graphicIncrement;
    obj->OnGraphicChange();
    obj->Color = g_ColorManager.FixColor(color, (color & 0x8000));

    if (!count)
        count = 1;

    obj->Count = count;

    UpdateItemInContainer(obj, container, x, y);

    MoveToTop(obj);

    LOG("\t|0x%08X<0x%08X:%04X*%d (%d,%d) %04X\n",
        containerSerial,
        serial,
        graphic + graphicIncrement,
        count,
        x,
        y,
        color);
}

/*!
Дамп предметов, хранящихся в памяти
@param [__in] nCount Количество отступов
@param [__in_opt] serial Серийник родителя
@return
*/
void CGameWorld::Dump(uchar tCount, uint serial)
{
    DEBUG_TRACE_FUNCTION;
    LOG("World Dump:\n\n");

    CGameObject *obj = m_Items;

    if (serial != 0xFFFFFFFF)
    {
        obj = FindWorldObject(serial);
        if (obj != nullptr)
            obj = (CGameObject *)obj->m_Items;
    }

    while (obj != nullptr)
    {
        if (obj->Container == serial)
        {
            if (obj->Serial == g_Player->Serial)
                LOG("---Player---\n");

            for (int i = 0; i < tCount; i++)
                LOG("\t");

            LOG("%s%08X:%04X[%04X](%%02X)*%i\tin 0x%08X XYZ=%i,%i,%i on Map %i\n",
                (obj->NPC ? "NPC: " : "Item: "),
                obj->Serial,
                obj->Graphic,
                obj->Color,
                /*obj->Layer,*/ obj->Count,
                obj->Container,
                obj->GetX(),
                obj->GetY(),
                obj->GetZ(),
                obj->MapIndex);

            if (obj->m_Items != nullptr)
                Dump(tCount + 1, obj->Container);
        }

        obj = (CGameObject *)obj->m_Next;
    }
}
