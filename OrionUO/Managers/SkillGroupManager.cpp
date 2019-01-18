// MIT License
// Copyright (C) August 2016 Hotride

#include "SkillGroupManager.h"
#include "GumpManager.h"
#include "SkillsManager.h"
#include "ConfigManager.h"
#include "../OrionUO.h"
#include "../SkillGroup.h"
#include "../Gumps/GumpNotify.h"
#include "../ScreenStages/GameBlockedScreen.h"

CSkillGroupManager g_SkillGroupManager;

CSkillGroupManager::CSkillGroupManager()
{
}

CSkillGroupManager::~CSkillGroupManager()
{
}

void CSkillGroupManager::MakeDefault()
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
    if (group->m_Prev == nullptr) //Miscellaneous
    {
        int x = g_ConfigManager.GameWindowX + (g_ConfigManager.GameWindowWidth / 2) - 100;
        int y = g_ConfigManager.GameWindowY + (g_ConfigManager.GameWindowHeight / 2) - 62;

        CGumpNotify *gump = new CGumpNotify(
            x, y, CGumpNotify::ID_GN_STATE_NOTIFICATION, 200, 125, "Cannot delete this group.");

        g_GumpManager.AddGump(gump);

        g_Orion.InitScreen(GS_GAME_BLOCKED);
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

bool CSkillGroupManager::Load(const os_path &path)
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    Clear();

    Wisp::CMappedFile file;

    if (file.Load(path))
    {
        uint8_t version = file.ReadUInt8();

        short count = file.ReadUInt16LE();

        for (int i = 0; i < count; i++)
        {
            uint8_t *next = file.Ptr;
            short size = file.ReadUInt16LE();
            next += size;

            CSkillGroupObject *group = new CSkillGroupObject();

            short length = file.ReadUInt16LE();
            string str = file.ReadString(length);
            group->Name = str;

            short skills = file.ReadUInt16LE();

            for (int j = 0; j < skills; j++)
            {
                uint8_t skill = file.ReadUInt8();

                if (skill != 0xFF)
                {
                    group->Add(skill);
                }
            }

            group->Sort();

            Add(group);

            file.Ptr = next;
        }

        file.Unload();

        result = true;
    }
    else
    {
        MakeDefault();
    }

    return result;
}

void CSkillGroupManager::Save(const os_path &path)
{
    DEBUG_TRACE_FUNCTION;
    Wisp::CBinaryFileWriter writer;

    writer.Open(path);

    writer.WriteUInt8(0); //version

    Count = 0;
    CSkillGroupObject *group = m_Groups;
    while (group != nullptr)
    {
        Count++;
        group = group->m_Next;
    }

    writer.WriteUInt16LE(Count); //Count

    group = m_Groups;

    for (int i = 0; i < Count; i++)
    {
        string str = group->Name;
        size_t len = str.length() + 1;

        short size = (short)len + 2 + 2 + 2 + group->Count;
        writer.WriteUInt16LE(size); //Block size

        writer.WriteUInt16LE((short)len); //Name length
        writer.WriteString(str, 0u);      //Name

        short count = group->Count;

        writer.WriteUInt16LE(count); //Skills count

        for (int j = 0; j < count; j++)
        {
            uint8_t skill = group->GetItem(j);
            writer.WriteUInt8(skill); //Skill
        }

        writer.WriteBuffer();

        group = group->m_Next;
    }

    writer.Close();
}
