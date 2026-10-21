// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "CityManager.h"
#include "ClilocManager.h"
#include <xuocore/uodata.h>
#include <xuocore/mappedfile.h>
#include <common/str.h>
#include <common/utils.h> // countof
#include "../Globals.h"   // g_Language

CCityManager g_CityManager;

CCity::CCity(const astr_t &name, const wstr_t &description)
    : Name(name)
    , Description(description)
{
}

CCity::~CCity()
= default;

CCityManager::CCityManager()
= default;

void CCityManager::Init()
{
    CMappedFile file;
    if (file.Load(UOFilePath("citytext.enu")))
    {
        // the walk runs on logical positions and window peeks (platform
        // neutral: WindowPtr/WindowByte/Pos resolve through the mapping on
        // native and through the bounded file-backed window on web)
        const size_t end = file.Size;
        while (file.Pos() < end)
        {
            const uint8_t *tag = file.WindowPtr(4);
            if (tag != nullptr && memcmp(tag, "END\0", 4) == 0)
            {
                file.Move(4);

                const size_t startBlock = file.Pos() + 4;
                size_t ptrBlock = startBlock;
                astr_t name{};
                while (ptrBlock < end)
                {
                    if (file.Seek(ptrBlock) && file.WindowByte() == '<')
                    {
                        size_t len = ptrBlock - startBlock;
                        name.resize(len);
                        file.Seek(startBlock);
                        file.ReadDataLE((uint8_t *)&name[0], len);
                        break;
                    }
                    ptrBlock++;
                }

                astr_t text{};
                while (file.Pos() < end)
                {
                    auto str = file.ReadString();
                    if (text.length() != 0u)
                    {
                        text += "\n\n";
                    }

                    text += str;
                    const uint8_t *endTag = file.WindowPtr(4);
                    if (file.WindowByte() == 0x2E ||
                        (endTag != nullptr && memcmp(endTag, "END\0", 4) == 0))
                    {
                        break;
                    }
                }
                m_CityList.push_back(CCity(name, wstr_from(text)));
            }
            else
            {
                file.Move(1);
            }
        }
        file.Unload();
    }
    else
    {
        static const astr_t cityNames[9] = { "Yew",      "Minoc",      "Britain",
                                             "Moonglow", "Trinsic",    "Magincia",
                                             "Jhelom",   "Skara Brae", "Vesper" };

        CCliloc *cliloc = g_ClilocManager.Cliloc(g_Language);
        if (cliloc != nullptr)
        {
            for (int i = 0; i < (int)countof(cityNames); i++)
            {
                m_CityList.push_back(CCity(cityNames[i], cliloc->GetW(1075072 + i)));
            }
        }
    }
}

CCityManager::~CCityManager()
{
    Clear();
}

CCity CCityManager::GetCity(const astr_t &name)
{
    for (auto &city : m_CityList)
    {
        if (city.Name == name)
        {
            return city;
        }
    }
    return CCity();
}

void CCityManager::Clear()
{
    m_CityList.clear();
}
