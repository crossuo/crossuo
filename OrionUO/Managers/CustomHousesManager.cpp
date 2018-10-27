// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** CustomHousesManager.cpp
**
** Copyright (C) September 2017 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CustomHousesManager g_CustomHousesManager;

void CCustomHouse::Paste(CGameItem *foundation)
{
    DEBUG_TRACE_FUNCTION;
    if (foundation == nullptr)
        return;

    foundation->ClearCustomHouseMultis(0);
    int z = foundation->GetZ();

    for (const CBuildObject &item : m_Items)
        foundation->AddMulti(item.Graphic, 0, item.X, item.Y, item.Z + z, true);

    if (g_CustomHouseGump != nullptr && g_CustomHouseGump->Serial == Serial)
    {
        g_CustomHouseGump->WantUpdateContent = true;
        g_CustomHouseGump->GenerateFloorPlace();
    }
}

//--------------------------------CustomHousesManager-------------------------------

CustomHousesManager::~CustomHousesManager()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

void CustomHousesManager::Clear()
{
    DEBUG_TRACE_FUNCTION;
    for (unordered_map<uint, CCustomHouse *>::iterator i = m_Items.begin(); i != m_Items.end(); ++i)
    {
        CCustomHouse *house = i->second;
        delete house;
        i->second = nullptr;
    }
}

CCustomHouse *CustomHousesManager::Get(int serial)
{
    DEBUG_TRACE_FUNCTION;
    for (unordered_map<uint, CCustomHouse *>::iterator i = m_Items.begin(); i != m_Items.end(); ++i)
    {
        CCustomHouse *house = i->second;

        if (house != nullptr && house->Serial == serial)
            return i->second;
    }

    return nullptr;
}

void CustomHousesManager::Add(CCustomHouse *house)
{
    DEBUG_TRACE_FUNCTION;
    if (house != nullptr)
        m_Items[house->Serial] = house;
}

void CustomHousesManager::Load(const os_path &path)
{
    DEBUG_TRACE_FUNCTION;
    Clear();

    Wisp::CMappedFile file;

    if (file.Load(path) && file.Size)
    {
        uchar version = file.ReadUInt8();

        int count = file.ReadInt32LE();

        for (int i = 0; i < count; i++)
        {
            uint serial = file.ReadUInt32LE();

            if (!serial)
                break;

            uint revision = file.ReadUInt32LE();
            int itemsCount = file.ReadInt32LE();

            CCustomHouse *house = Get(serial);

            LOG("Load house from cache file: 0x%08X 0x%08X\n", serial, revision);

            if (house == nullptr)
            {
                house = new CCustomHouse(serial, revision);
                g_CustomHousesManager.Add(house);
            }
            else
                house->Revision = revision;

            for (int j = 0; j < itemsCount; j++)
            {
                ushort graphic = file.ReadUInt16LE();
                char x = file.ReadInt8();
                char y = file.ReadInt8();
                char z = file.ReadInt8();

                house->m_Items.push_back(CBuildObject(graphic, x, y, z));
            }
        }

        file.Unload();
    }
}

void CustomHousesManager::Save(const os_path &path)
{
    DEBUG_TRACE_FUNCTION;
    Wisp::CBinaryFileWritter writter;

    writter.Open(path);

    writter.WriteInt8(1); //version
    writter.WriteBuffer();

    int count = 0;

    for (unordered_map<uint, CCustomHouse *>::iterator i = m_Items.begin(); i != m_Items.end(); ++i)
    {
        CCustomHouse *house = i->second;

        if (house != nullptr && house->m_Items.size())
            count++;
    }

    writter.WriteInt32LE(count);
    writter.WriteBuffer();

    for (unordered_map<uint, CCustomHouse *>::iterator i = m_Items.begin(); i != m_Items.end(); ++i)
    {
        CCustomHouse *house = i->second;

        if (house == nullptr || !house->m_Items.size())
            continue;

        writter.WriteUInt32LE(house->Serial);
        writter.WriteUInt32LE(house->Revision);
        writter.WriteInt32LE((uint)house->m_Items.size());
        writter.WriteBuffer();

        for (const CBuildObject &item : house->m_Items)
        {
            writter.WriteUInt16LE(item.Graphic);
            writter.WriteInt8(item.X);
            writter.WriteInt8(item.Y);
            writter.WriteInt8(item.Z);
            writter.WriteBuffer();
        }
    }

    writter.WriteUInt32LE(0); //EOF
    writter.WriteBuffer();

    writter.Close();
}

