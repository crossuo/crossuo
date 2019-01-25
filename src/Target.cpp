// MIT License
// Copyright (C) August 2016 Hotride

#include "Target.h"
#include "Config.h"
#include "CrossUO.h"
#include "Multi.h"
#include "api/mulstruct.h"
#include "api/commoninterfaces.h"
#include "Network/Packets.h"
#include "Managers/FileManager.h"
#include "Managers/MouseManager.h"
#include "Managers/MapManager.h"
#include "Managers/CustomHousesManager.h"
#include "GameObjects/GameCharacter.h"
#include "GameObjects/CustomHouseMultiObject.h"
#include "Gumps/GumpCustomHouse.h"

CTarget g_Target;

CTarget::CTarget()
{
    //Чистимся
    memset(m_Data, 0, sizeof(m_Data));
    memset(m_LastData, 0, sizeof(m_LastData));
}

void CTarget::Reset()
{
    DEBUG_TRACE_FUNCTION;
    //Чистимся
    memset(m_Data, 0, sizeof(m_Data));
    memset(m_LastData, 0, sizeof(m_LastData));

    if (m_Multi != nullptr)
    {
        delete m_Multi;
        m_Multi = nullptr;
    }

    Type = 0;
    CursorType = 0;
    CursorID = 0;
    Targeting = false;
    MultiGraphic = 0;
}

void CTarget::RequestFromCustomHouse()
{
    Type = 2;
    CursorID = 0;
    CursorType = 0;
    Targeting = true;
    MultiGraphic = 0;

    if (g_CustomHouseGump != nullptr)
    {
        g_CustomHouseGump->Erasing = false;
        g_CustomHouseGump->SeekTile = false;
        g_CustomHouseGump->SelectedGraphic = 0;
        g_CustomHouseGump->CombinedStair = false;
        g_CustomHouseGump->WantUpdateContent = true;
    }
}

void CTarget::SetLastTargetObject(int serial)
{
    Type = 0;
    pack32(m_LastData + 7, serial);
}

void CTarget::SetData(Wisp::CDataReader &reader)
{
    DEBUG_TRACE_FUNCTION;
    //Копируем буффер
    memcpy(&m_Data[0], reader.Start, reader.Size);

    //И устанавливаем соответствующие значения
    Type = reader.ReadUInt8();
    CursorID = reader.ReadUInt32BE();
    CursorType = reader.ReadUInt8();
    Targeting = (CursorType < 3);
    MultiGraphic = 0;
}

void CTarget::SetMultiData(Wisp::CDataReader &reader)
{
    DEBUG_TRACE_FUNCTION;
    //Устанавливаем соответствующие значения
    Type = 1;
    CursorType = 0;
    Targeting = true;
    CursorID = reader.ReadUInt32BE(1);

    //Копируем буффер
    memset(&m_Data[0], 0, 19);
    m_Data[0] = 0x6C;
    m_Data[1] = 1;                           //Таргет на ландшафт
    memcpy(m_Data + 2, reader.Start + 2, 4); //Копируем ID курсора (ID дида)

    reader.ResetPtr();
    reader.Move(18);
    MultiGraphic = reader.ReadUInt16BE() + 1;
    MultiX = reader.ReadUInt16BE();
    MultiY = reader.ReadUInt16BE();
}

void CTarget::SendTargetObject(int serial)
{
    DEBUG_TRACE_FUNCTION;
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    //Пишем серийник объекта, на который ткнули прицелом, остальное - затираем
    pack32(m_Data + 7, serial);
    m_Data[1] = 0;

    CGameObject *obj = (g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr);

    if (obj != nullptr)
    {
        pack16(m_Data + 11, obj->GetX());
        pack16(m_Data + 13, obj->GetY());
        m_Data[15] = 0xFF;
        m_Data[16] = obj->GetZ();
        pack16(m_Data + 17, obj->Graphic);
    }
    else
    {
        pack32(m_Data + 11, 0);
        pack32(m_Data + 15, 0);
    }

    if (serial != g_PlayerSerial)
    {
        g_LastTargetObject = serial;

        //Скопируем для LastTarget
        memcpy(m_LastData, m_Data, sizeof(m_Data));

        if (obj != nullptr && obj->NPC && ((CGameCharacter *)obj)->MaxHits == 0)
        {
            CPacketStatusRequest(serial).Send();
        }
    }

    SendTarget();
}

void CTarget::SendTargetTile(uint16_t tileID, short x, short y, char z)
{
    DEBUG_TRACE_FUNCTION;
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    m_Data[1] = 1;

    //Пишем координаты и индекс тайла, на который ткнули, остальное трем
    pack32(m_Data + 7, 0);
    pack16(m_Data + 11, x);
    pack16(m_Data + 13, y);

    //m_Data[15] = 0xFF;
    //m_Data[16] = z;
    pack16(m_Data + 15, (short)z);
    pack16(m_Data + 17, tileID);

    //Скопируем для LastTarget
    memcpy(m_LastData, m_Data, sizeof(m_Data));

    SendTarget();
}

void CTarget::SendCancelTarget()
{
    DEBUG_TRACE_FUNCTION;
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    //Уходят только нули
    pack32(m_Data + 7, 0);
    pack32(m_Data + 11, 0xFFFFFFFF);
    pack32(m_Data + 15, 0);

    SendTarget();

    if (g_CustomHouseGump != nullptr)
    {
        g_CustomHouseGump->Erasing = false;
        g_CustomHouseGump->SeekTile = false;
        g_CustomHouseGump->SelectedGraphic = 0;
        g_CustomHouseGump->CombinedStair = false;
        g_CustomHouseGump->WantUpdateContent = true;
    }
}

void CTarget::Plugin_SendTargetObject(int serial)
{
    DEBUG_TRACE_FUNCTION;
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    //Пишем серийник объекта, на который ткнули прицелом, остальное - затираем
    pack32(m_Data + 7, serial);
    m_Data[1] = 0;

    CGameObject *obj = (g_World != nullptr ? g_World->FindWorldObject(serial) : nullptr);

    if (obj != nullptr)
    {
        pack16(m_Data + 11, obj->GetX());
        pack16(m_Data + 13, obj->GetY());
        m_Data[15] = 0xFF;
        m_Data[16] = obj->GetZ();
        pack16(m_Data + 17, obj->Graphic);
    }
    else
    {
        pack32(m_Data + 11, 0);
        pack32(m_Data + 15, 0);
    }

    if (serial != g_PlayerSerial)
    {
        g_LastTargetObject = serial;

        //Скопируем для LastTarget
        memcpy(m_LastData, m_Data, sizeof(m_Data));

        if (obj != nullptr && obj->NPC && ((CGameCharacter *)obj)->MaxHits == 0)
        {
            CPacketStatusRequest packet(serial);
            UOMsg_Send(packet.Data().data(), packet.Data().size());
        }
    }

    Plugin_SendTarget();
}

void CTarget::Plugin_SendTargetTile(uint16_t tileID, short x, short y, char z)
{
    DEBUG_TRACE_FUNCTION;
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    m_Data[1] = 1;

    //Пишем координаты и индекс тайла, на который ткнули, остальное трем
    pack32(m_Data + 7, 0);
    pack16(m_Data + 11, x);
    pack16(m_Data + 13, y);

    //m_Data[15] = 0xFF;
    //m_Data[16] = z;
    pack16(m_Data + 15, (short)z);
    pack16(m_Data + 17, tileID);

    //Скопируем для LastTarget
    memcpy(m_LastData, m_Data, sizeof(m_Data));

    Plugin_SendTarget();
}

void CTarget::Plugin_SendCancelTarget()
{
    DEBUG_TRACE_FUNCTION;
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    //Уходят только нули
    pack32(m_Data + 7, 0);
    pack32(m_Data + 11, 0xFFFFFFFF);
    pack32(m_Data + 15, 0);

    Plugin_SendTarget();
}

void CTarget::SendLastTarget()
{
    DEBUG_TRACE_FUNCTION;
    if (!Targeting)
    {
        return; //Если в клиенте нет таргета - выход
    }

    //Восстановим пакет последнего актуального таргета
    memcpy(m_Data, m_LastData, sizeof(m_Data));
    m_Data[0] = 0x6C;
    m_Data[1] = Type;
    m_Data[6] = CursorType;
    pack32(m_Data + 2, CursorID);

    SendTarget();
}

void CTarget::SendTarget()
{
    DEBUG_TRACE_FUNCTION;

    if (Type != 2)
    {
        g_Game.Send(m_Data, sizeof(m_Data));
    }

    memset(m_Data, 0, sizeof(m_Data));
    Targeting = false;
    MultiGraphic = 0;

    g_MouseManager.CancelDoubleClick = true;
}

void CTarget::Plugin_SendTarget()
{
    DEBUG_TRACE_FUNCTION;

    UOMsg_Send(m_Data, sizeof(m_Data));
    memset(m_Data, 0, sizeof(m_Data));
    Targeting = false;
    MultiGraphic = 0;

    g_MouseManager.CancelDoubleClick = true;
}

void CTarget::UnloadMulti()
{
    DEBUG_TRACE_FUNCTION;
    if (m_Multi != nullptr)
    {
        delete m_Multi;
        m_Multi = nullptr;
    }
}

void CTarget::LoadMulti(int offsetX, int offsetY, char offsetZ)
{
    DEBUG_TRACE_FUNCTION;
    UnloadMulti();

    CIndexMulti &index = g_Game.m_MultiDataIndex[MultiGraphic - 1];

    int count = (int)index.Count;

    if (index.UopBlock != nullptr)
    {
        vector<uint8_t> data = g_FileManager.m_MultiCollection.GetData(*index.UopBlock);

        if (data.empty())
        {
            return;
        }

        Wisp::CDataReader reader(&data[0], data.size());
        reader.Move(8); //ID + Count

        for (int i = 0; i < count; i++)
        {
            uint16_t graphic = reader.ReadUInt16LE();
            short x = reader.ReadInt16LE();
            short y = reader.ReadInt16LE();
            short z = reader.ReadInt16LE();
            uint16_t flags = reader.ReadUInt16LE();
            uint32_t clilocsCount = reader.ReadUInt32LE();

            if (clilocsCount != 0u)
            {
                reader.Move(clilocsCount * 4);
            }

            CMultiObject *mo =
                new CMultiObject(graphic, x + offsetX, y + offsetY, (char)z + (char)offsetZ, 2);
            g_MapManager.AddRender(mo);
            AddMultiObject(mo);
        }
    }
    else if (index.Address != 0)
    {
        int itemOffset = sizeof(MULTI_BLOCK);

        if (g_Config.ClientVersion >= CV_7090)
        {
            itemOffset = sizeof(MULTI_BLOCK_NEW);
        }

        for (int j = 0; j < count; j++)
        {
            MULTI_BLOCK *pmb = (MULTI_BLOCK *)(index.Address + (j * itemOffset));

            CMultiObject *mo = new CMultiObject(
                pmb->ID, offsetX + pmb->X, offsetY + pmb->Y, offsetZ + (char)pmb->Z, 2);
            g_MapManager.AddRender(mo);
            AddMultiObject(mo);
        }
    }
}

void CTarget::AddMultiObject(CMultiObject *obj)
{
    DEBUG_TRACE_FUNCTION;
    if (m_Multi == nullptr)
    {
        m_Multi = new CMulti(obj->GetX(), obj->GetY());
        m_Multi->m_Next = nullptr;
        m_Multi->m_Prev = nullptr;
        m_Multi->m_Items = obj;
        obj->m_Next = nullptr;
        obj->m_Prev = nullptr;
    }
    else
    {
        CMulti *multi = GetMultiAtXY(obj->GetX(), obj->GetY());

        if (multi != nullptr)
        {
            QFOR(multiobj, multi->m_Items, CMultiObject *)
            {
                if (obj->GetZ() < multiobj->GetZ())
                {
                    if (multiobj->m_Prev == nullptr)
                    {
                        multi->Insert(multiobj->m_Prev, obj);
                    }
                    else
                    {
                        multi->Insert(multiobj, obj);
                    }

                    return;
                }

                if (multiobj->m_Next == nullptr)
                {
                    multiobj->m_Next = obj;
                    obj->m_Prev = multiobj;
                    obj->m_Next = nullptr;

                    return;
                }
            }

            //Если пришли сюда - что-то пошло не так
        }
        else
        {
            // FIXME: potential leak, validate
            CMulti *newmulti = new CMulti(obj->GetX(), obj->GetY());
            newmulti->m_Next = nullptr;
            newmulti->m_Items = obj;
            obj->m_Next = nullptr;
            obj->m_Prev = nullptr;

            multi = m_Multi;

            while (multi != nullptr)
            {
                if (multi->m_Next == nullptr)
                {
                    multi->m_Next = newmulti;
                    newmulti->m_Prev = multi;
                    break;
                }

                multi = (CMulti *)multi->m_Next;
            }
        }
    }
}

CMulti *CTarget::GetMultiAtXY(short x, short y)
{
    DEBUG_TRACE_FUNCTION;
    CMulti *multi = m_Multi;

    while (multi != nullptr)
    {
        if (multi->X == x && multi->Y == y)
        {
            break;
        }

        multi = (CMulti *)multi->m_Next;
    }

    return multi;
}
