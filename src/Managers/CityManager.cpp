// MIT License
// Copyright (C) August 2016 Hotride

#include "CityManager.h"
#include "ClilocManager.h"
#include "../Application.h"
#include <xuocore/mappedfile.h>
#include <common/str.h>

CCityManager g_CityManager;

CCity::CCity(const std::string &name, const std::wstring &description)
    : Name(name)
    , Description(description)
{
}

CCity::~CCity()
{
}

CCityManager::CCityManager()
{
}

void CCityManager::Init()
{
    DEBUG_TRACE_FUNCTION;

    CMappedFile file;
    if (file.Load(g_App.UOFilesPath("citytext.enu")))
    {
        uint8_t *end = file.Ptr + file.Size;
        while (file.Ptr < end)
        {
            if (memcmp(&file.Ptr[0], "END\0", 4) == 0)
            {
                file.Move(4);

                uint8_t *startBlock = file.Ptr + 4;
                uint8_t *ptrBlock = startBlock;
                std::string name{};
                while (ptrBlock < end)
                {
                    if (*ptrBlock == '<')
                    {
                        size_t len = ptrBlock - startBlock;
                        name.resize(len);
                        memcpy(&name[0], &startBlock[0], len);
                        break;
                    }
                    ptrBlock++;
                }

                std::string text{};
                while (file.Ptr < end)
                {
                    auto str = file.ReadString();
                    if (text.length() != 0u)
                    {
                        text += "\n\n";
                    }

                    text += str;
                    if (*file.Ptr == 0x2E || (memcmp(&file.Ptr[0], "END\0", 4) == 0))
                    {
                        break;
                    }
                }
                m_CityList.push_back(CCity(name, ToWString(text)));
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
        static const std::string cityNames[9] = { "Yew",      "Minoc",      "Britain",
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

CCity CCityManager::GetCity(const std::string &name)
{
    DEBUG_TRACE_FUNCTION;

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
    DEBUG_TRACE_FUNCTION;
    m_CityList.clear();
}
