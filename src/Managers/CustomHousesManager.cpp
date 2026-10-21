// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#include "CustomHousesManager.h"
#include <xuocore/mappedfile.h>
#include <xuocore/text_parser.h>
#include <common/str.h>
#include <string>
#include "../Gumps/GumpCustomHouse.h"
#include "../GameObjects/GameItem.h"

CustomHousesManager g_CustomHousesManager;

void CCustomHouse::Paste(CGameItem *foundation)
{
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
    Clear();
}

void CustomHousesManager::Clear()
{
    for (auto it = m_Items.begin(); it != m_Items.end(); ++it)
    {
        CCustomHouse *house = it->second;
        delete house;
        it->second = nullptr;
    }
}

CCustomHouse *CustomHousesManager::Get(int serial)
{
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
    if (house != nullptr)
    {
        m_Items[house->Serial] = house;
    }
}

void CustomHousesManager::Load(const fs_path &path)
{
    Clear();

    if (!fs_path_exists(path))
    {
        return;
    }

    TextFileParser file(path, " =", "#;", "\"");

    CCustomHouse *house = nullptr;
    while (!file.IsEOF())
    {
        auto v = file.ReadTokens();
        if (v.empty())
        {
            continue;
        }

        if (v[0] == "House" && v.size() >= 2)
        {
            auto f = TextSplit(v[1], ',');
            if (f.size() < 2)
            {
                continue;
            }
            const uint32_t serial = TextToUInt(f[0]);
            const uint32_t revision = TextToUInt(f[1]);
            if (serial == 0u)
            {
                house = nullptr;
                continue;
            }

            house = Get(serial);
            Info(Client, "load house from cache file: 0x%08X 0x%08X", serial, revision);
            if (house == nullptr)
            {
                house = new CCustomHouse(serial, revision);
                Add(house);
            }
            else
            {
                house->Revision = revision;
            }
        }
        else if (v[0] == "HouseItem" && house != nullptr && v.size() >= 2)
        {
            auto f = TextSplit(v[1], ',');
            if (f.size() >= 4)
            {
                house->m_Items.push_back(
                    CBuildObject(
                        (uint16_t)str_to_int(f[0]),
                        (char)str_to_int(f[1]),
                        (char)str_to_int(f[2]),
                        (char)str_to_int(f[3])));
            }
        }
    }
}

void CustomHousesManager::Save(TextFileWriter &writer)
{
    writer.WriteRaw("[custom-houses]");

    for (auto it = m_Items.begin(); it != m_Items.end(); ++it)
    {
        CCustomHouse *house = it->second;
        if (house == nullptr || house->m_Items.empty())
        {
            continue;
        }

        writer.WriteRaw(
            "House=" + std::to_string(house->Serial) + "," + std::to_string(house->Revision));

        for (const CBuildObject &item : house->m_Items)
        {
            writer.WriteRaw(
                "HouseItem=" + str_from((int)item.Graphic) + "," + str_from((int)item.X) + "," +
                str_from((int)item.Y) + "," + str_from((int)item.Z));
        }
    }
}
