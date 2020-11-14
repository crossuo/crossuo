// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#pragma once

#include <stdint.h>
#include <unordered_map>
#include <common/fs.h>
#include <xuocore/mappedfile.h>

class CGameItem;
class CMultiObject;

class CBuildObject
{
public:
    uint16_t Graphic = 0;
    char X = 0;
    char Y = 0;
    char Z = 0;

    CBuildObject() {}
    CBuildObject(uint16_t graphic, char x, char y, char z)
        : Graphic(graphic)
        , X(x)
        , Y(y)
        , Z(z)
    {
    }
    ~CBuildObject() {}
};

class CCustomHouse
{
public:
    uint32_t Serial = 0;
    uint32_t Revision = 0;

    CCustomHouse() {}
    CCustomHouse(int serial, int revision)
        : Serial(serial)
        , Revision(revision)
    {
    }
    ~CCustomHouse() {}

    std::vector<CBuildObject> m_Items;

    void Paste(CGameItem *foundation);
};

class CustomHousesManager : public CDataReader
{
    std::unordered_map<uint32_t, CCustomHouse *> m_Items;

public:
    CustomHousesManager() {}
    ~CustomHousesManager();

    void Clear();

    CCustomHouse *Get(int serial);

    void Add(CCustomHouse *house);

    void Load(const fs_path &path);
    void Save(const fs_path &path);
};

extern CustomHousesManager g_CustomHousesManager;
