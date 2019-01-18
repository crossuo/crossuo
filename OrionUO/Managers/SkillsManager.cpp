// MIT License
// Copyright (C) Obtober 2017 Hotride

#include "SkillsManager.h"
#include "FileManager.h"
#include "../plugin/mulstruct.h"

CSkillsManager g_SkillsManager;

CSkill::CSkill(bool haveButton, const string &name)
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

    //LOG("Skill loaded (button:%i): %s\n", m_Button, m_Name.c_str());
}

bool CSkillsManager::Load()
{
    DEBUG_TRACE_FUNCTION;
    if ((g_FileManager.m_SkillsIdx.Size == 0u) || (g_FileManager.m_SkillsMul.Size == 0u) ||
        (Count != 0u))
    {
        return false;
    }

    Wisp::CMappedFile &idx = g_FileManager.m_SkillsIdx;
    Wisp::CMappedFile &mul = g_FileManager.m_SkillsMul;

    while (!idx.IsEOF())
    {
        SKILLS_IDX_BLOCK *idxBlock = (SKILLS_IDX_BLOCK *)idx.Ptr;
        idx.Move(sizeof(SKILLS_IDX_BLOCK));

        if ((idxBlock->Size != 0u) && idxBlock->Position != 0xFFFFFFFF &&
            idxBlock->Size != 0xFFFFFFFF)
        {
            mul.Ptr = mul.Start + idxBlock->Position;

            bool haveButton = (mul.ReadUInt8() != 0);

            Add(CSkill(haveButton, mul.ReadString(idxBlock->Size - 1)));
        }
    }

    LOG("Skills count: %i\n", Count);

    if (Count < 2 || Count > 100)
    {
        Count = 0;

        return false;
    }

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

bool CSkillsManager::CompareName(const string &str1, const string &str2)
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
    vector<uint8_t> bufTable(Count, 0xFF);

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
