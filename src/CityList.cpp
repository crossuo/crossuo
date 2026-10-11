// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "CityList.h"
#include "Managers/CityManager.h"
#include <xuocore/mappedfile.h>
#include <xuocore/uodata.h>
#include <common/utils.h> // countof

CCityList g_CityList;

CCityItem::CCityItem()
    : m_City("", {})
{
}

CCityItem::~CCityItem()
{
}

void CCityItem::InitCity()
{
    m_City = g_CityManager.GetCity(Name);
}

CCityItemNew::CCityItemNew()
    : CCityItem()
{
}

CCityItemNew::~CCityItemNew()
{
}

CCityList::CCityList()
{
}

CCityList::~CCityList()
{
    Clear();
}

CCityItem *CCityList::GetCity(int index)
{
    if (index < (int)m_CityList.size())
    {
        return m_CityList[index];
    }

    return nullptr;
}

void CCityList::Clear()
{
    for (auto i = m_CityList.begin(); i != m_CityList.end(); ++i)
    {
        delete *i;
    }

    m_CityList.clear();
}

//1.25.32 (1998) start-location table. The original client (FUN_0046B1D0)
//reads regions.mul at startup: a stream of records with two 40-byte names
//(city + building) and seven u16 coordinates. There is no record count -
//the reader stops at EOF (or 0x2000 entries). The first record's leading
//u16 is the format tag: 0xFACE selects the "new" record (city name,
//7x u16, u32, building name, 2x u16, u8); anything else the shorter
//legacy record (single name, 7x u16, u32 - the building name stays empty).
size_t CCityList::LoadRegionsOrDefault()
{
    if (!m_CityList.empty())
    {
        return m_CityList.size();
    }

    CMappedFile file;
    if (file.Load(UOFilePath("regions.mul")))
    {
        const size_t end = file.Size;
        bool newFormat = false;
        if (end >= 2)
        {
            file.Seek(0);
            newFormat = file.ReadUInt16LE() == 0xFACE;
        }

        //legacy record: 40 (city) + 14 (coords) + 4 = 58 bytes
        //new record: legacy + 40 (building) + 4 + 1 = 103 bytes
        const size_t recordSize = newFormat ? 103u : 58u;
        file.Seek(0);

        while (file.Pos() + recordSize <= end)
        {
            CCityItem *city = new CCityItem();
            city->LocationIndex = uint8_t(m_CityList.size());

            uint8_t name[41] = { 0 };
            file.ReadDataLE(name, 40);
            city->Name = (const char *)name;

            //7x u16 world coordinates (unused by CityChoice)
            for (int i = 0; i < 7; i++)
            {
                file.ReadUInt16LE();
            }

            file.ReadUInt32LE(); //extra

            if (newFormat)
            {
                uint8_t building[41] = { 0 };
                file.ReadDataLE(building, 40);
                city->Area = (const char *)building;
                file.ReadUInt16LE();
                file.ReadUInt16LE();
                file.Move(1);
            }

            if (!city->Name.empty())
            {
                city->InitCity();
                m_CityList.push_back(city);
            }
            else
            {
                delete city;
            }
        }

        file.Unload();
    }

    if (m_CityList.empty())
    {
        //regions.mul absent (the shipped 1.25.32 data lacks it): seed the
        //classic start-location table the original shows - city and
        //building, in the client's order
        static const struct
        {
            const char *name;
            const char *area;
        } defaults[] = {
            { "Minoc", "Tavern" },
            { "Magincia", "Docks" },
            { "Vesper", "Ironwood Inn" },
            { "Trinsic", "West Gate" },
            { "Jhelom", "Docks" },
            { "Yew", "Center" },
            { "Skara Brae", "Docks" },
            { "Britain", "Sweet Dreams Inn" },
            { "Moonglow", "Docks" },
        };

        for (int i = 0; i < (int)countof(defaults); i++)
        {
            CCityItem *city = new CCityItem();
            city->LocationIndex = uint8_t(i);
            city->Name = defaults[i].name;
            city->Area = defaults[i].area;
            city->InitCity();
            m_CityList.push_back(city);
        }
    }

    return m_CityList.size();
}
