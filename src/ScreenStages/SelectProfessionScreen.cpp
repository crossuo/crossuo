// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "SelectProfessionScreen.h"
#include "Config.h"
#include "ConnectionScreen.h"
#include "CreateCharacterScreen.h"
#include "../CrossUO.h"
#include "../GameWindow.h"
#include "../Profession.h"
#include "../Managers/CreateCharacterManager.h"
#include "../Managers/ProfessionManager.h"
#include "../Managers/ScreenEffectManager.h"
#include "xuocore/enumlist.h"

CSelectProfessionScreen g_SelectProfessionScreen;

//the built-in "Advanced" profession node (DescriptionIndex -1) can sit at
//any depth of the prof.txt tree; find it recursively
static CBaseProfession *FindAdvancedProfession(CBaseProfession *start)
{
    for (CBaseProfession *item = start; item != nullptr; item = (CBaseProfession *)item->m_Next)
    {
        if (item->Type == PT_PROFESSION && item->DescriptionIndex == -1)
        {
            return item;
        }

        CBaseProfession *found = FindAdvancedProfession((CBaseProfession *)item->m_Items);
        if (found != nullptr)
        {
            return found;
        }
    }

    return nullptr;
}

CSelectProfessionScreen::CSelectProfessionScreen()
    : CBaseScreen(m_SelectProfessionGump)
{
}

CSelectProfessionScreen::~CSelectProfessionScreen()
= default;

void CSelectProfessionScreen::SetSkillSelection(int val)
{
    m_SkillSelection = val;
    m_Gump.WantUpdateContent = true;
}

void CSelectProfessionScreen::Init()
{
    CBaseScreen::Init();

    g_ProfessionManager.Selected = (CBaseProfession *)g_ProfessionManager.m_Items;
    m_SkillSelection = 0;

    //1.25.32 wizard start (CharStat): the original initialized the stat
    //inputs to 22/21/22 and the InitSkill values to 34/33/33 (the classic
    //"Advanced" preset, skills unpicked), so seed the Advanced profession
    //node with exactly those defaults and reset the wizard state
    if (g_Config.ClientVersion < CV_200)
    {
        CProfession *advanced = (CProfession *)FindAdvancedProfession(
            (CBaseProfession *)g_ProfessionManager.m_Items);

        if (advanced == nullptr)
        {
            //the 1998 install ships no prof.txt (the file appeared with later
            //clients): synthesize the node the CharStat sliders write into.
            //The create-character packet carries no profession field - the
            //original's profession list only seeded stat/skill presets.
            advanced = new CProfession();
            advanced->Name = "Advanced";
            advanced->Type = PT_PROFESSION;
            advanced->DescriptionIndex = -1; //FindAdvancedProfession marker
            g_ProfessionManager.Add(advanced);
        }

        g_ProfessionManager.Selected = advanced;
        advanced->Str = 22;
        advanced->Dex = 21;
        advanced->Int = 22;
        advanced->SetSkillIndex(0, 0xFF);
        advanced->SetSkillIndex(1, 0xFF);
        advanced->SetSkillIndex(2, 0xFF);
        advanced->SetSkillIndex(3, 0xFF);
        advanced->SetSkillValue(0, 34);
        advanced->SetSkillValue(1, 33);
        advanced->SetSkillValue(2, 33);
        advanced->SetSkillValue(3, 0);

        //appearance defaults for the CharCreation step. The 1998 wizard
        //addresses skin/hair/beard on the wire hue ids (skin 1002+,
        //hair/beard 1102+) - the same base the hue sliders and the preview
        //use (GumpScreenCreateCharacter). 1001/1101 are the file's own
        //entry numbering and resolve to a leftover shipped hue / junk, so
        //the doll rendered uncolored until a slider rewrote the id.
        g_CreateCharacterManager.Clear();
        g_CreateCharacterManager.SkinTone = 1002;
        g_CreateCharacterManager.HairColor = 1102;
        g_CreateCharacterManager.BeardColor = 1102;
        g_CreateCharacterManager.ShirtColor = 1;

        g_CreateCharacterScreen.Name = "";
        m_SelectProfessionGump.m_Password->Clear();

        //the wizard boots on the CharStat panel (page 1; the profession
        //scrollchoice popup is page 2)
        m_SelectProfessionGump.Page = 1;
    }

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

void CSelectProfessionScreen::ProcessSmoothAction(uint8_t action)
{
    if (action == 0xFF)
    {
        action = SmoothScreenAction;
    }

    if (action == ID_SMOOTH_SPS_QUIT)
    {
        g_GameWindow.Destroy();
    }
    else if (action == ID_SMOOTH_SPS_GO_SCREEN_CHARACTER)
    {
        g_Game.InitScreen(GS_CHARACTER);
    }
    else if (action == ID_SMOOTH_SPS_GO_SCREEN_GAME_CONNECT)
    {
        g_Game.InitScreen(GS_GAME_CONNECT);
        g_ConnectionScreen.SetType(CST_SELECT_PROFESSOIN);
    }
    else if (action == ID_SMOOTH_SPS_GO_SCREEN_CREATE)
    {
        g_Game.InitScreen(GS_CREATE);
    }
}

void CSelectProfessionScreen::OnTextInput(const TextEvent &ev)
{
    const auto ch = EvChar(ev);
    if (!IsPrintable(ch) || !g_FontManager.IsPrintASCII((uint8_t)ch))
    {
        return;
    }
    if (g_EntryPointer == nullptr || m_SelectProfessionGump.m_1998NameEntry == nullptr ||
        m_SelectProfessionGump.m_1998PasswordFake == nullptr)
    {
        return;
    }

    //the password entry is masked: the visible entry takes an asterisk and
    //the real character goes into the password store (login screen scheme)
    if (g_EntryPointer == &m_SelectProfessionGump.m_1998PasswordFake->m_Entry)
    {
        if (g_EntryPointer->Length() < 16 && g_EntryPointer->Insert(L'*'))
        {
            m_SelectProfessionGump.m_Password->Insert(ch);
        }
    }
    else
    {
        g_EntryPointer->Insert(ch);
        g_CreateCharacterScreen.Name = g_EntryPointer->GetTextA();
    }

    m_Gump.WantRedraw = true;
}

void CSelectProfessionScreen::OnKeyDown(const KeyEvent &ev)
{
    const auto key = EvKey(ev);
    if (g_EntryPointer == nullptr || m_SelectProfessionGump.m_1998NameEntry == nullptr ||
        m_SelectProfessionGump.m_1998PasswordFake == nullptr)
    {
        return;
    }

    switch (key)
    {
        case KEY_RETURN:
        case KEY_RETURN2:
        {
            //RETURN presses OK
            m_Gump.OnButton(300 /*ID_SPS_1998_OK*/);
            break;
        }
        case KEY_TAB:
        {
            if (g_EntryPointer == &m_SelectProfessionGump.m_1998NameEntry->m_Entry)
            {
                g_EntryPointer = &m_SelectProfessionGump.m_1998PasswordFake->m_Entry;
            }
            else
            {
                g_EntryPointer = &m_SelectProfessionGump.m_1998NameEntry->m_Entry;
            }
            break;
        }
        default:
        {
            if (g_EntryPointer == &m_SelectProfessionGump.m_1998PasswordFake->m_Entry)
            {
                m_SelectProfessionGump.m_Password->OnKey(nullptr, key);
            }

            g_EntryPointer->OnKey(nullptr, key);
            if (g_EntryPointer == &m_SelectProfessionGump.m_1998NameEntry->m_Entry)
            {
                g_CreateCharacterScreen.Name = g_EntryPointer->GetTextA();
            }
            break;
        }
    }
    m_Gump.WantRedraw = true;
}
