// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "SkillGroupManager.h"
#include "GumpManager.h"
#include "SkillsManager.h"
#include "ConfigManager.h"
#include <cstdint>
#include "../CrossUO.h"
#include "../SkillGroup.h"
#include "../Gumps/GumpNotify.h"
#include "../ScreenStages/GameBlockedScreen.h"
#include "xuocore/enumlist.h"
#include <xuocore/text_parser.h>

CSkillGroupManager g_SkillGroupManager;

CSkillGroupManager::CSkillGroupManager()
= default;

CSkillGroupManager::~CSkillGroupManager()
= default;

void CSkillGroupManager::MakeDefault()
{
    Clear();

    MakeDefaultMiscellaneous();
    MakeDefaultCombat();
    MakeDefaultTradeSkills();
    MakeDefaultMagic();
    MakeDefaultWilderness();
    MakeDefaultThieving();
    MakeDefaultBard();

    CSkillGroupObject *group = m_Groups;

    while (group != nullptr)
    {
        group->Sort();

        group = group->m_Next;
    }
}

void CSkillGroupManager::MakeDefaultMiscellaneous()
{
    CSkillGroupObject *group = new CSkillGroupObject();
    group->Name = "Miscellaneous";
    group->Add(4);
    group->Add(6);
    group->Add(10);
    group->Add(12);
    group->Add(19);
    group->Add(3);
    group->Add(36);

    Add(group);
}

void CSkillGroupManager::MakeDefaultCombat()
{
    int cnt = g_SkillsManager.Count;

    CSkillGroupObject *group = new CSkillGroupObject();
    group->Name = "Combat";
    group->Add(1);
    group->Add(31);
    group->Add(42);
    group->Add(17);
    group->Add(41);
    group->Add(5);
    group->Add(40);
    group->Add(27);

    if (cnt > 57)
    {
        group->Add(57); //Throving
    }
    group->Add(43);

    if (cnt > 50)
    {
        group->Add(50); //Focus
    }
    if (cnt > 51)
    {
        group->Add(51); //Chivalry
    }
    if (cnt > 52)
    {
        group->Add(52); //Bushido
    }
    if (cnt > 53)
    {
        group->Add(53); //Ninjitsu
    }

    Add(group);
}

void CSkillGroupManager::MakeDefaultTradeSkills()
{
    CSkillGroupObject *group = new CSkillGroupObject();
    group->Name = "Trade Skills";
    group->Add(0);
    group->Add(7);
    group->Add(8);
    group->Add(11);
    group->Add(13);
    group->Add(23);
    group->Add(44);
    group->Add(45);
    group->Add(34);
    group->Add(37);

    Add(group);
}

void CSkillGroupManager::MakeDefaultMagic()
{
    int cnt = g_SkillsManager.Count;

    CSkillGroupObject *group = new CSkillGroupObject();
    group->Name = "Magic";
    group->Add(16);
    if (cnt > 56)
    {
        group->Add(56); //Imbuing
    }
    group->Add(25);
    group->Add(46);
    if (cnt > 55)
    {
        group->Add(55); //Mysticism
    }
    group->Add(26);
    if (cnt > 54)
    {
        group->Add(54); //Spellweaving
    }
    group->Add(32);
    if (cnt > 49)
    {
        group->Add(49); //Necromancy
    }

    Add(group);
}

void CSkillGroupManager::MakeDefaultWilderness()
{
    CSkillGroupObject *group = new CSkillGroupObject();
    group->Name = "Wilderness";
    group->Add(2);
    group->Add(35);
    group->Add(18);
    group->Add(20);
    group->Add(38);
    group->Add(39);

    Add(group);
}

void CSkillGroupManager::MakeDefaultThieving()
{
    CSkillGroupObject *group = new CSkillGroupObject();
    group->Name = "Thieving";
    group->Add(14);
    group->Add(21);
    group->Add(24);
    group->Add(30);
    group->Add(48);
    group->Add(28);
    group->Add(33);
    group->Add(47);

    Add(group);
}

void CSkillGroupManager::MakeDefaultBard()
{
    CSkillGroupObject *group = new CSkillGroupObject();
    group->Name = "Bard";
    group->Add(15);
    group->Add(29);
    group->Add(9);
    group->Add(22);

    Add(group);
}

void CSkillGroupManager::Clear()
{
    CSkillGroupObject *item = m_Groups;

    while (item != nullptr)
    {
        CSkillGroupObject *next = item->m_Next;

        delete item;

        item = next;
    }

    Count = 0;
    m_Groups = nullptr;
}

void CSkillGroupManager::Add(CSkillGroupObject *group)
{
    if (m_Groups == nullptr)
    {
        m_Groups = group;
        m_Groups->m_Next = nullptr;
        m_Groups->m_Prev = nullptr;
        Count = 1;

        return;
    }

    CSkillGroupObject *item = m_Groups;

    while (item->m_Next != nullptr)
    {
        item = item->m_Next;
    }

    item->m_Next = group;
    group->m_Next = nullptr;
    group->m_Prev = item;

    Count++;
}

bool CSkillGroupManager::Remove(CSkillGroupObject *group)
{
    if (group->m_Prev == nullptr) //Miscellaneous
    {
        int x = g_ConfigManager.GameWindowX + (g_ConfigManager.GameWindowWidth / 2) - 100;
        int y = g_ConfigManager.GameWindowY + (g_ConfigManager.GameWindowHeight / 2) - 62;

        CGumpNotify *gump = new CGumpNotify(
            x, y, CGumpNotify::ID_GN_STATE_NOTIFICATION, 200, 125, "Cannot delete this group.");

        g_GumpManager.AddGump(gump);

        g_Game.InitScreen(GS_GAME_BLOCKED);
        g_GameBlockedScreen.Code = 4;

        return false;
    }

    Count--;

    if (Count < 0)
    {
        Count = 0;
    }

    if (group->m_Next != nullptr)
    {
        group->m_Next->m_Prev = group->m_Prev;
    }

    group->m_Prev->m_Next = group->m_Next;

    group->TransferTo(m_Groups);
    delete group;

    return true;
}

bool CSkillGroupManager::Load(const fs_path &path)
{
    Clear();

    if (!fs_path_exists(path))
    {
        MakeDefault();
        return false;
    }

    TextFileParser file(path, " =", "#;", "\"");

    CSkillGroupObject *group = nullptr;
    bool found = false;
    while (!file.IsEOF())
    {
        auto v = file.ReadTokens();
        if (v.empty())
        {
            continue;
        }

        if (v[0] == "Group" && v.size() >= 2)
        {
            group = new CSkillGroupObject();
            group->Name = TextUnquote(v[1]);
            Add(group);
            found = true;
        }
        else if (v[0] == "Skills" && group != nullptr && v.size() >= 2)
        {
            for (const auto &part : TextSplit(v[1], ','))
            {
                const int skill = str_to_int(part);
                if (skill >= 0 && skill != 0xFF)
                {
                    group->Add((uint8_t)skill);
                }
            }
            group->Sort();
            group = nullptr;
        }
    }

    if (!found)
    {
        MakeDefault();
        return false;
    }

    return true;
}

void CSkillGroupManager::Save(TextFileWriter &writer) const
{
    writer.WriteRaw("[skill-groups]");

    CSkillGroupObject *group = m_Groups;
    while (group != nullptr)
    {
        writer.WriteStringQuoted("Group", group->Name);

        astr_t skills;
        for (int j = 0; j < group->Count; j++)
        {
            if (j != 0)
            {
                skills += ",";
            }
            skills += str_from((int)group->GetItem(j));
        }
        writer.WriteString("Skills", skills);

        group = group->m_Next;
    }
}
