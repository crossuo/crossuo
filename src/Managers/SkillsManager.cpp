// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#include <algorithm>
#include "SkillsManager.h"
#include <common/logging/logging.h>
#include <xuocore/uodata.h>
#include <xuocore/mulstruct.h>

CSkillsManager g_SkillsManager;

CSkill::CSkill(bool haveButton, astr_t name)
    : Button(haveButton)
{
    if (name.length() != 0u)
    {
        Name = name;
    }
    else
    {
        Name = "NoNameSkill";
    }
}

bool CSkillsManager::Init()
{
    if (Count != 0)
    {
        return false;
    }

    for (const auto &skill : g_FileManager.m_Skills)
    {
        Info(Client, "%d Skill: %s", Count, skill.Name.c_str());
        Add(CSkill(skill.Iteractive, skill.Name));
    }

    Info(Client, "skills count: %d", Count);
    Info(Client, "sorting skills");
    Sort();
    return true;
}

void CSkillsManager::Add(const CSkill &skill)
{
    m_Skills.push_back(skill);
    Count++;
}

void CSkillsManager::Clear()
{
    Count = 0;
    SkillsTotal = 0.0f;
    m_Skills.clear();
    m_SortedTable.clear();
}

CSkill *CSkillsManager::Get(uint32_t index)
{
    if (index < Count)
    {
        return &m_Skills[index];
    }
    return nullptr;
}

bool CSkillsManager::CompareName(const astr_t &str1, const astr_t &str2)
{
    //Вычисляем минимальную длину строки для сравнения
    const auto len = (int)std::min(str1.length(), str2.length());

    bool result = false;

    //Пройдемся по всем символам этой строки, сравнивая их друг с другом
    for (int i = 0; i < len; i++)
    {
        char c1 = str1.at(i);
        char c2 = str2.at(i);

        if (c1 < c2)
        {
            return true;
        }
        if (c1 > c2)
        {
            return false;
        }
    }

    //Вернем что получилось
    return result;
}

void CSkillsManager::Sort()
{
    m_SortedTable.resize(Count, 0xFF);
    std::vector<uint8_t> bufTable(Count, 0xFF);

    //Установим первый элемент нулем и количество обработанных навыков - 1
    int parsed = 1;
    bufTable[0] = 0;

    //Пройдемся по всем нвыкам (кроме первого)
    for (uint32_t i = 1; i < Count; i++)
    {
        //Пройдемся по обработанным
        for (int j = 0; j < parsed; j++)
        {
            //Если можно вставить в текущую позицию -
            if (CompareName(m_Skills[bufTable[j]].Name, m_Skills[i].Name))
            {
                //Запомним индекс навыка
                uint8_t buf = bufTable[j];
                //Перезапишем
                bufTable[j] = (uint8_t)i;

                //К следующему навыку
                j++;

                //Посмотрим остальные обработанные и перезапишем индекс при необходимости
                for (; j < parsed; j++)
                {
                    uint8_t ptr = bufTable[j];
                    bufTable[j] = buf;
                    buf = ptr;
                }

                //Запишем индекс в текущий обработанный
                bufTable[parsed] = buf;

                //Увеличиваем счетчик
                parsed++;

                break;
            }
        }
    }

    for (int i = 0, j = parsed - 1; i < parsed; i++, j--)
    {
        m_SortedTable[i] = bufTable[j];
    }
}

int CSkillsManager::GetSortedIndex(uint32_t index)
{
    if (index < Count)
    {
        return m_SortedTable[index];
    }
    return -1;
}

void CSkillsManager::UpdateSkillsSum()
{
    SkillsTotal = 0.0f;
    for (const CSkill &skill : m_Skills)
    {
        SkillsTotal += skill.Value;
    }
}
