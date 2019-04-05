// MIT License
// Copyright (C) September 2017 Hotride

#include "CustomHousesManager.h"
#include "../api/mappedfile.h"
#include "../Gumps/GumpCustomHouse.h"
#include "../GameObjects/GameItem.h"

CustomHousesManager g_CustomHousesManager;

void CCustomHouse::Paste(CGameItem *foundation)
{
    DEBUG_TRACE_FUNCTION;
    if (foundation == nullptr)
    {
        return;
    }

    foundation->ClearCustomHouseMultis(0);
    int z = foundation->GetZ();

    for (const CBuildObject &item : m_Items)
    {
        foundation->AddMulti(item.Graphic, 0, item.X, item.Y, item.Z + z, true);
    }

    if (g_CustomHouseGump != nullptr && g_CustomHouseGump->Serial == Serial)
    {
        g_CustomHouseGump->WantUpdateContent = true;
        g_CustomHouseGump->GenerateFloorPlace();
    }
}

CustomHousesManager::~CustomHousesManager()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

void CustomHousesManager::Clear()
{
    DEBUG_TRACE_FUNCTION;
    for (auto it = m_Items.begin(); it != m_Items.end(); ++it)
    {
        CCustomHouse *house = it->second;
        delete house;
        it->second = nullptr;
    }
}

CCustomHouse *CustomHousesManager::Get(int serial)
{
    DEBUG_TRACE_FUNCTION;
    for (auto it = m_Items.begin(); it != m_Items.end(); ++it)
    {
        CCustomHouse *house = it->second;
        if (house != nullptr && house->Serial == serial)
        {
            return it->second;
        }
    }

    return nullptr;
}

void CustomHousesManager::Add(CCustomHouse *house)
{
    DEBUG_TRACE_FUNCTION;
    if (house != nullptr)
    {
        m_Items[house->Serial] = house;
    }
}

void CustomHousesManager::Load(const os_path &path)
{
    DEBUG_TRACE_FUNCTION;
    Clear();

    CMappedFile file;

    if (file.Load(path) && (file.Size != 0u))
    {
        const uint8_t _version = file.ReadUInt8();
        (void)_version;

        int count = file.ReadInt32LE();

        for (int i = 0; i < count; i++)
        {
            uint32_t serial = file.ReadUInt32LE();

            if (serial == 0u)
            {
                break;
            }

            uint32_t revision = file.ReadUInt32LE();
            int itemsCount = file.ReadInt32LE();

            CCustomHouse *house = Get(serial);

            Info(Client, "load house from cache file: 0x%08X 0x%08X", serial, revision);
            if (house == nullptr)
            {
                house = new CCustomHouse(serial, revision);
                g_CustomHousesManager.Add(house);
            }
            else
            {
                house->Revision = revision;
            }

            for (int j = 0; j < itemsCount; j++)
            {
                uint16_t graphic = file.ReadUInt16LE();
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
    Wisp::CBinaryFileWriter writer;

    writer.Open(path);

    writer.WriteInt8(1); //version
    writer.WriteBuffer();

    int count = 0;

    for (auto it = m_Items.begin(); it != m_Items.end(); ++it)
    {
        CCustomHouse *house = it->second;
        if (house != nullptr && (static_cast<unsigned int>(!house->m_Items.empty()) != 0u))
        {
            count++;
        }
    }

    writer.WriteInt32LE(count);
    writer.WriteBuffer();

    for (auto it = m_Items.begin(); it != m_Items.end(); ++it)
    {
        CCustomHouse *house = it->second;
        if (house == nullptr || house->m_Items.empty())
        {
            continue;
        }

        writer.WriteUInt32LE(house->Serial);
        writer.WriteUInt32LE(house->Revision);
        writer.WriteInt32LE((uint32_t)house->m_Items.size());
        writer.WriteBuffer();

        for (const CBuildObject &item : house->m_Items)
        {
            writer.WriteUInt16LE(item.Graphic);
            writer.WriteInt8(item.X);
            writer.WriteInt8(item.Y);
            writer.WriteInt8(item.Z);
            writer.WriteBuffer();
        }
    }

    writer.WriteUInt32LE(0); //EOF
    writer.WriteBuffer();

    writer.Close();
}
