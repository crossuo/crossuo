// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <common/str.h>
#include "GumpScreenSelectProfession.h"
#include "Gump1998ScrollChoice.h"
#include "../Config.h"
#include "../ToolTip.h"
#include "../Profession.h"
#include "../SelectedObject.h"
#include "../GameWindow.h"
#include "../TextEngine/EntryText.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/FontsManager.h"
#include "../Managers/SkillsManager.h"
#include "../Managers/ProfessionManager.h"
#include "../ScreenStages/SelectProfessionScreen.h"
#include "../ScreenStages/CreateCharacterScreen.h"

enum
{
    ID_SPS_QUIT = 1,
    ID_SPS_ARROW_PREV = 2,
    ID_SPS_ARROW_NEXT = 3,
    ID_SPS_ARROW_BACK_PROFESSION = 4,
    ID_SPS_LABEL_BACK_PROFESSION = 5,
    ID_SPS_HTMLGUMP = 6,

    ID_SPS_STATS_SPHERE = 10,

    ID_SPS_SKILLS_SPHERE = 20,

    ID_SPS_SKILLS_FILED = 30,

    ID_SPS_LABEL = 50,

    ID_SPS_SKILLS_LIST = 100,

    ID_SPS_ERROR_OK = 110,

    //1.25.32 (1998) CharStat controls: disjoint id blocks per role (panel
    //controls vs popup ids - the popup row branch is bounded by the row
    //count, never an open-ended >=)
    ID_SPS_1998_NAME = 300,
    ID_SPS_1998_PASSWORD,
    ID_SPS_1998_STAT,      //+0..+2: the stat sliders
    ID_SPS_1998_PROFESSION,
    ID_SPS_1998_BACK,
    ID_SPS_1998_OK,
    ID_SPS_1998_POPUP_UP = 340,
    ID_SPS_1998_POPUP_DOWN,
    ID_SPS_1998_POPUP_THUMB,
    ID_SPS_1998_POPUP_ROW = 345, //+0..+rowCount-1, bounded in the handlers

    //1.25.32 (1998) InitSkill step (the second CharStat page): disjoint
    //blocks again - pick buttons / value sliders / skill popup / nav
    ID_SPS_1998_SKILLS_BACK = 360,
    ID_SPS_1998_SKILLS_NEXT,
    ID_SPS_1998_SKILL_PICK,    //+0..+2
    ID_SPS_1998_SKILL_SLIDER,  //+0..+2
    ID_SPS_1998_SKILL_POPUP_UP = 370,
    ID_SPS_1998_SKILL_POPUP_DOWN,
    ID_SPS_1998_SKILL_POPUP_THUMB,
    ID_SPS_1998_SKILL_POPUP_ROW = 375, //+0..+skillCount-1
    ID_SPS_1998_MSG_OK = 380,
};

CGumpScreenSelectProfession::CGumpScreenSelectProfession()
    : CGump(GT_NONE, 0, 0, 0)
{
    NoMove = true;
    NoClose = true;

    for (int i = 0; i < 3; i++)
    {
        m_StatsSliders[i] = nullptr;
        m_SkillsSliders[i] = nullptr;
        m_1998StatSliders[i] = nullptr;
        m_1998StatValue[i] = nullptr;
    }

    m_SkillsSliders[3] = nullptr;

    //the real password store; the visible entry carries asterisks only
    m_Password = new CEntryText(16, 0, 300);
}

CGumpScreenSelectProfession::~CGumpScreenSelectProfession()
{
    delete m_Password;
    m_Password = nullptr;
}

void CGumpScreenSelectProfession::UpdateContent()
{
    Clear();

    if (g_ProfessionManager.Selected == nullptr)
    {
        g_GameWindow.ShowMessage("No items in profession manager!", "Profession error!");
        return;
    }

    if (g_Config.ClientVersion >= CV_308Z)
    {
        UpdateContentNew();
    }
    else if (g_Config.ClientVersion < CV_200)
    {
        //1.25.32 (1998) creation wizard step 1: CharStat
        UpdateContent1998();
    }
    else
    {
        UpdateContentOld();
    }
}

//the original CharStat profession picker listed its 14 professions flat;
//the crossuo data is a tree (prof.txt categories -> professions, plus the
//built-in Advanced node), so walk it in document order
static CBaseProfession *Get1998ProfessionByIndex(int index, int &count, CBaseProfession *start)
{
    for (CBaseProfession *item = start; item != nullptr; item = (CBaseProfession *)item->m_Next)
    {
        if (item->Type == PT_PROFESSION && count++ == index)
        {
            return item;
        }

        CBaseProfession *found =
            Get1998ProfessionByIndex(index, count, (CBaseProfession *)item->m_Items);
        if (found != nullptr)
        {
            return found;
        }
    }

    return nullptr;
}

static CBaseProfession *Get1998ProfessionByIndex(int index)
{
    int count = 0;
    return Get1998ProfessionByIndex(index, count, (CBaseProfession *)g_ProfessionManager.m_Items);
}

void CGumpScreenSelectProfession::UpdateContent1998()
{
    //hardening: drop the scrollchoices' cached child pointers (the
    //gump's Clear() frees them) and the slider/value pointers with them
    m_1998ProfessionPopup.Reset();
    m_1998SkillPopup.Reset();
    for (int i = 0; i < 3; i++)
    {
        m_1998StatSliders[i] = nullptr;
        m_1998StatValue[i] = nullptr;
        m_1998StatLeft[i] = nullptr;
        m_1998SkillName[i] = nullptr;
        m_1998SkillSliders[i] = nullptr;
        m_1998SkillValue[i] = nullptr;
    }

    //the wood backdrop (640x480) stays behind every 1998 login-flow screen
    Add(new CGUIGumppic(0x03E8, 0, 0));

    //the duplicate-skills message dialog REPLACES the whole screen (the
    //original hides the InitSkill gump: [this+0x68]=0 at 0x40EA56):
    //backdrop 0x474 at (121,102), the message wrapped+centered in rel
    //x 62..330 at y 46 (font 2 hue 0x386), the green OK 0x47B/0x47C/0x47D
    //at rel (161,190). Added before any page marker so it draws over
    //everything, and the early return leaves both pages empty
    if (m_1998MsgDialog)
    {
        Add(new CGUIGumppic(0x0474, 121, 102));

        CGUIText *text = (CGUIText *)Add(new CGUIText(0x0386, 121 + 62, 102 + 46));
        text->CreateTextureA(2, m_1998MsgText, 330 - 62, TS_CENTER);

        Add(new CGUIButton(ID_SPS_1998_MSG_OK, 0x047B, 0x047C, 0x047D, 121 + 161, 102 + 190));
        return;
    }

    //the CharStat sliders read the profession node; a broken prof.txt (no
    //profession data at all) leaves the tree head selected
    if (g_ProfessionManager.Selected == nullptr ||
        g_ProfessionManager.Selected->Type != PT_PROFESSION)
    {
        g_GameWindow.ShowMessage("No items in profession manager!", "Profession error!");
        return;
    }

    Add(new CGUIPage(1)); //page 1: the CharStat panel

    //right-side paper scroll (sub-gump ctor 0x41A97F: x=0x200 y=0x79).
    //Collapsed profession list: hidden while the popup is open
    if (!m_1998ProfessionPopupOpen)
    {
        Add(new CGUIGumppic(0x04BB, 512, 121));
    }

    //CharStat backdrop (400x269)
    Add(new CGUIGumppic(0x04B0, 120, 102));

    //name entry (max 16, font 1 hue 0x386, insets (7,2)): the box trio
    //state machine (filler / with-text / focused) is swapped in
    //PrepareContent, like the login screen entries
    m_1998NameArt = (CGUIGumppic *)Add(new CGUIGumppic(0x04B3, 241, 131));
    CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
        ID_SPS_1998_NAME, 0x0386, 0x0386, 0x0386, 248, 133, 144, false, 1, TS_LEFT, 0, 16));
    entry->CheckOnSerial = true;
    entry->m_Entry.SetTextA(g_CreateCharacterScreen.Name);
    m_1998NameEntry = entry;

    //password entry (masked): the real text lives in m_Password, the
    //visible entry shows asterisks only
    m_1998PasswordArt = (CGUIGumppic *)Add(new CGUIGumppic(0x04B7, 242, 151));
    entry = (CGUITextEntry *)Add(new CGUITextEntry(
        ID_SPS_1998_PASSWORD, 0x0386, 0x0386, 0x0386, 249, 153, 144, false, 1, TS_LEFT, 0, 16));
    entry->CheckOnSerial = true;
    m_1998PasswordFake = entry;
    m_1998PasswordFake->m_Entry.Clear();
    for (int i = 0; i < (int)m_Password->Length(); i++)
    {
        m_1998PasswordFake->m_Entry.Insert(L'*');
    }

    //the entries are recreated on every rebuild; keep the focus on a
    //living entry (defaults to the name field) - unless the skill step is
    //up, which has no entries at all
    if (Page == 2)
    {
        g_EntryPointer = nullptr;
    }
    else if (
        g_EntryPointer != &m_1998NameEntry->m_Entry &&
        g_EntryPointer != &m_1998PasswordFake->m_Entry)
    {
        g_EntryPointer = &m_1998NameEntry->m_Entry;
    }

    //STR/DEX/INT sliders (knob 0x4B9/0x4BA, travel to x=269; the original
    //track art is not in the report, the generic 0x00D5 ruler is used),
    //max 35 each, defaults 22/21/22; the values are shown in fixed display
    //boxes at x=342
    CProfession *profession = (CProfession *)g_ProfessionManager.Selected;
    static const int statPos[3][2] = { { 179, 220 }, { 179, 250 }, { 179, 280 } };
    static const int statTextPos[3][2] = { { 349, 216 }, { 349, 246 }, { 349, 277 } };
    static const int statLeftPos[3][2] = { { 277, 216 }, { 276, 246 }, { 276, 277 } };
    const int statValue[3] = { profession->Str, profession->Dex, profession->Int };
    for (int i = 0; i < 3; i++)
    {
        m_1998StatSliders[i] = (CGUISlider *)Add(new CGUISlider(
            ID_SPS_1998_STAT + i, 0x04B9, 0x04BA, 0x04BA, 0, false, false, statPos[i][0],
            statPos[i][1], 90, 10, 45, statValue[i]));
        //the backdrop has a number slot on each side of the sliders: the
        //original kept editable stat holders on the left of the tracks and
        //display copies on the right; the right column reads HP / Mana /
        //Stamina top to bottom, so STR feeds hits, INT mana and DEX
        //stamina (the original swapped the middle/bottom copies when
        //filling the display column)
        static const int displayStat[3] = { 0, 2, 1 };
        m_1998StatLeft[i] = (CGUIText *)Add(new CGUIText(0x0386, statLeftPos[i][0], statLeftPos[i][1]));
        m_1998StatLeft[i]->CreateTextureA(1, str_from(statValue[i]));
        m_1998StatValue[i] =
            (CGUIText *)Add(new CGUIText(0x0386, statTextPos[i][0], statTextPos[i][1]));
        m_1998StatValue[i]->CreateTextureA(1, str_from(statValue[displayStat[i]]));
    }

    //profession picker -> scrollchoice popup (page 2); shown only when a
    //real prof.txt tree exists (with the synthesized fallback node the
    //sliders write into being the sole entry, the picker is pointless -
    //the create-character packet carries no profession field)
    if (g_ProfessionManager.GetItemsCount() > 1)
    {
        Add(new CGUIButton(ID_SPS_1998_PROFESSION, 0x04BC, 0x04BD, 0x04BE, 283, 177));
    }

    //BACK -> character list; OK -> validate and open InitSkill
    Add(new CGUIButton(ID_SPS_1998_BACK, 0x04C2, 0x04C3, 0x04C4, 182, 310));
    Add(new CGUIButton(ID_SPS_1998_OK, 0x04BF, 0x04C0, 0x04C1, 406, 310));

    //the profession scrollchoice draws OVER the panel (the original keeps
    //the gump visible; the right-side scroll art is replaced while the
    //list is open)
    if (m_1998ProfessionPopupOpen)
    {
        std::vector<astr_t> names;
        for (int i = 0;; i++)
        {
            CBaseProfession *prof = Get1998ProfessionByIndex(i);
            if (prof == nullptr)
            {
                break;
            }
            names.push_back(prof->Name);
        }
        if (names.size() > 1)
        {
        //the selected profession is highlighted in the list
        int selected = -1;
        for (int i = 0;; i++)
        {
            CBaseProfession *prof = Get1998ProfessionByIndex(i);
            if (prof == nullptr)
            {
                break;
            }
            if (prof == g_ProfessionManager.Selected)
            {
                selected = i;
                break;
            }
        }
        m_1998ProfessionPopup.Create(this, ID_SPS_1998_POPUP_ROW, ID_SPS_1998_POPUP_UP,
            ID_SPS_1998_POPUP_DOWN, ID_SPS_1998_POPUP_THUMB, false, names, 0, selected);
        }
    }

    Add(new CGUIPage(2)); //page 2: the InitSkill panel (the skill picks)

    //InitSkill backdrop (429x273)
    Add(new CGUIGumppic(0x04CE, 98, 101));

    //right-side paper scroll (sub-gump ctor 0x41C1CD: x=0x205 y=0x78).
    //Collapsed skill list: hidden while the popup is open
    if (!m_1998SkillPopupOpen)
    {
        Add(new CGUIGumppic(0x04CF, 517, 120));
    }

    //three pick buttons (open the skill list for the slot) + skill name
    //labels; the values come from the profession presets and are shown in
    //display boxes with a slider each (0..50)
    static const int pickPos[3][2] = { { 190, 153 }, { 188, 198 }, { 188, 242 } };
    static const int namePos[3][2] = { { 229, 150 }, { 229, 195 }, { 229, 240 } };
    static const int valuePos[3][2] = { { 403, 161 }, { 404, 206 }, { 405, 252 } };
    static const int skillSliderPos[3][2] = { { 228, 169 }, { 228, 214 }, { 228, 260 } };
    static const uint16_t pickArt[3][3] = {
        { 0x04D6, 0x04D7, 0x04D8 },
        { 0x04D9, 0x04DA, 0x04DB },
        { 0x04DC, 0x04DD, 0x04DE },
    };
    for (int i = 0; i < 3; i++)
    {
        //the ctor preselects fixed skill ids 0/1/2 in the three slots
        //(0x41B920: [entry+0xE8] = 0/1/2; there is no RNG in the wizard)
        if (profession->GetSkillIndex(i) == 0xFF)
        {
            profession->SetSkillIndex(i, (uint8_t)i);
        }

        Add(new CGUIButton(ID_SPS_1998_SKILL_PICK + i, pickArt[i][0], pickArt[i][1],
            pickArt[i][2], pickPos[i][0], pickPos[i][1]));

        astr_t name = "";
        const int skillID = profession->GetSkillIndex(i);
        if (skillID < (int)g_SkillsManager.Count)
        {
            CSkill *skill = g_SkillsManager.Get(skillID);
            if (skill != nullptr)
            {
                name = skill->Name;
            }
        }
        m_1998SkillName[i] = (CGUIText *)Add(new CGUIText(0x0386, namePos[i][0], namePos[i][1]));
        m_1998SkillName[i]->CreateTextureA(1, name);

        const int value = profession->GetSkillValue(i);
        m_1998SkillValue[i] =
            (CGUIText *)Add(new CGUIText(0x0386, valuePos[i][0], valuePos[i][1]));
        m_1998SkillValue[i]->CreateTextureA(1, str_from(value));

        m_1998SkillSliders[i] = (CGUISlider *)Add(new CGUISlider(ID_SPS_1998_SKILL_SLIDER + i,
            0x04B9, 0x04BA, 0x04BA, 0, false, false, skillSliderPos[i][0], skillSliderPos[i][1],
            165, 0, 50, value));
    }

    //BACK -> CharStat; NEXT -> the appearance step (after the
    //distinct-skills check)
    Add(new CGUIButton(ID_SPS_1998_SKILLS_BACK, 0x04D3, 0x04D4, 0x04D5, 191, 304));
    Add(new CGUIButton(ID_SPS_1998_SKILLS_NEXT, 0x04D0, 0x04D1, 0x04D2, 378, 303));

    //the skill scrollchoice draws over the panel; the right-side scroll
    //art is replaced while the list is open
    if (m_1998SkillPopupOpen)
    {
        std::vector<astr_t> names;
        for (uint32_t i = 0; i < g_SkillsManager.Count; i++)
        {
            CSkill *skill = g_SkillsManager.Get(g_SkillsManager.GetSortedIndex(i));
            if (skill != nullptr)
            {
                names.push_back(skill->Name);
            }
        }

        //the picked skill of the slot that opened the popup is highlighted
        int selected = -1;
        const int pickedSkill = profession->GetSkillIndex(m_1998PickSlot);
        if (pickedSkill != 0xFF)
        {
            for (uint32_t i = 0; i < g_SkillsManager.Count; i++)
            {
                if (g_SkillsManager.GetSortedIndex(i) == pickedSkill)
                {
                    selected = (int)i;
                    break;
                }
            }
        }
        m_1998SkillPopup.Create(this, ID_SPS_1998_SKILL_POPUP_ROW, ID_SPS_1998_SKILL_POPUP_UP,
            ID_SPS_1998_SKILL_POPUP_DOWN, ID_SPS_1998_SKILL_POPUP_THUMB, true, names, 0,
            selected);
    }
    //the starting page (CharStat panel) is set by the stage Init, so that
    //rebuilds don't clobber an open skill list
}

void CGumpScreenSelectProfession::PrepareContent()
{
    //1.25.32 CharStat: mirror the original's entry art state machine - the
    //filler-text box shows while the field is unfocused and empty, the
    //plain box once it holds text, and the bright variant while focused
    //(same scheme as the login screen entries; the base box variants
    //0x4B1/0x4B5 are unused, like the slot art base on the character list)
    if (g_Config.ClientVersion < CV_200 && m_1998NameArt != nullptr)
    {
        const bool nameFocused = g_EntryPointer == &m_1998NameEntry->m_Entry;
        int nameArt = 0x4B3;
        if (nameFocused)
        {
            nameArt = 0x4B4;
        }
        else if (m_1998NameEntry->m_Entry.GetTextA().length() != 0)
        {
            nameArt = 0x4B2;
        }
        if (m_1998NameArt->Graphic != nameArt)
        {
            m_1998NameArt->Graphic = nameArt;
            WantRedraw = true;
        }

        const bool passwordFocused = g_EntryPointer == &m_1998PasswordFake->m_Entry;
        int passwordArt = 0x4B7;
        if (passwordFocused)
        {
            passwordArt = 0x4B8;
        }
        else if (m_Password->Length() != 0)
        {
            passwordArt = 0x4B6;
        }
        if (m_1998PasswordArt->Graphic != passwordArt)
        {
            m_1998PasswordArt->Graphic = passwordArt;
            WantRedraw = true;
        }
    }
}

void CGumpScreenSelectProfession::UpdateContentOld()
{
    CBaseProfession *obj = g_ProfessionManager.Selected;

    if (obj == nullptr)
    {
        g_GameWindow.ShowMessage("No items in profession manager!", "Profession error!");
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        m_StatsSliders[i] = nullptr;
        m_SkillsSliders[i] = nullptr;
    }

    m_SkillsSliders[3] = nullptr;

    Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));
    Add(new CGUIGumppic(0x157C, 0, 0));
    Add(new CGUIGumppic(0x15A0, 0, 4));
    Add(new CGUIButton(ID_SPS_QUIT, 0x1589, 0x158A, 0x158B, 555, 4));
    Add(new CGUIButton(ID_SPS_ARROW_PREV, 0x15A1, 0x15A2, 0x15A3, 586, 445));
    Add(new CGUIResizepic(0, 0x0A28, 80, 80, 546, 352));
    Add(new CGUIGumppic(0x058B, 145, 57));
    Add(new CGUIGumppic(0x0589, 222, 44));
    Add(new CGUIButton(ID_SPS_ARROW_BACK_PROFESSION, 0x119C, 0x119D, 0x119E, 200, 356));

    CGUIHTMLGump *htmlGump =
        new CGUIHTMLGump(ID_SPS_HTMLGUMP, 0x0BB8, 120, 137, 221, 214, true, true);
    Add(htmlGump);

    if (g_SelectProfessionScreen.GetSkillSelection() == 0)
    {
        //!Используем обработку HTML-тэгов при создании текстуры текста
        g_FontManager.SetUseHTML(true);

        CGUIText *text = new CGUIText(0, 3, 3);
        text->CreateTextureW(1, obj->Description, 30, 195);
        htmlGump->Add(text);

        //!Выключаем обработку HTML-тэгов
        g_FontManager.SetUseHTML(false);
    }
    else
    {
        int yPtr = 4; // 20;

        for (uint32_t i = 0; i < g_SkillsManager.Count; i++)
        {
            CSkill *skill = g_SkillsManager.Get(g_SkillsManager.GetSortedIndex(i));

            if (skill == nullptr)
            {
                continue;
            }

            CGUITextEntry *entry = new CGUITextEntry(
                ID_SPS_SKILLS_LIST + (int)i, 1, 0x0035, 0x0035, 3, yPtr, 0, false, 9);
            entry->m_Entry.SetTextA(skill->Name);
            entry->m_Entry.CreateTextureA(9, skill->Name, 1, 0, TS_LEFT, 0);
            entry->CheckOnSerial = true;
            entry->ReadOnly = true;

            htmlGump->Add(new CGUIHitBox(
                ID_SPS_SKILLS_LIST + (int)i, 3, yPtr, 195, entry->m_Entry.m_Texture.Height));
            htmlGump->Add(entry);

            yPtr += entry->m_Entry.m_Texture.Height;
        }
    }

    htmlGump->CalculateDataSize();

    if (obj->Type == PT_CATEGORY) //category
    {
        Add(new CGUIButton(
            ID_SPS_LABEL_BACK_PROFESSION, obj->Gump, obj->Gump, obj->Gump + 1, 231, 53));

        int offsY = 0;

        int index = 0;

        for (CBaseProfession *child = (CBaseProfession *)obj->m_Items; child != nullptr; child = (CBaseProfession *)child->m_Next)
        {
            Add(new CGUIGumppic(0x0589, 500, 100 + offsY));

            Add(new CGUIButton(
                ID_SPS_LABEL + index, child->Gump, child->Gump, child->Gump + 1, 509, 109 + offsY));

            CGUIText *text = (CGUIText *)Add(new CGUIText(0, 350, 135 + offsY));
            text->CreateTextureW(2, wstr_from(child->Name), 30, 185, TS_LEFT, UOFONT_SOLID);

            offsY += 79;

            index++;
        }
    }
    else if (obj->Type == PT_PROFESSION) //profession
    {
        Add(new CGUIButton(
            ID_SPS_LABEL_BACK_PROFESSION, obj->Gump, obj->Gump, obj->Gump + 1, 231, 53));

        CProfession *profession = (CProfession *)obj;
        int statVal[3] = { profession->Str, profession->Dex, profession->Int };

        const astr_t statName[3] = { "Strength", "Dexterity", "Intelligence" };

        int yPtr = 136;

        for (int i = 0; i < 3; i++)
        {
            CGUIText *text = new CGUIText(1, 360, yPtr);
            text->CreateTextureA(1, statName[i]);
            Add(text);

            m_StatsSliders[i] = (CGUISlider *)Add(new CGUISlider(
                ID_SPS_STATS_SPHERE + (int)i,
                0x00D8,
                0x00D8,
                0x00D8,
                0x00D5,
                true,
                false,
                496,
                yPtr,
                95,
                10,
                45,
                statVal[i]));
            m_StatsSliders[i]->DefaultTextOffset = -10;
            m_StatsSliders[i]->SetTextParameters(true, STP_LEFT, 1, 1, false);

            yPtr += 30;
        }

        if (profession->DescriptionIndex >= 0)
        {
            yPtr = 260;

            for (int i = 0; i < 3; i++)
            {
                auto skillID = (uint32_t)profession->GetSkillIndex((int)i);
                if (skillID >= g_SkillsManager.Count)
                {
                    skillID = 0;
                }

                CGUIText *text = new CGUIText(1, 360, yPtr);

                CSkill *skill = g_SkillsManager.Get(skillID);

                if (skill != nullptr)
                {
                    text->CreateTextureA(1, skill->Name, 90, TS_LEFT, UOFONT_FIXED);
                }

                Add(text);

                yPtr += 32;
            }
        }
        else //advanced
        {
            yPtr = 256;

            for (int i = 0; i < 3; i++)
            {
                Add(new CGUIResizepic(ID_SPS_SKILLS_FILED + (int)i, 0x0BB8, 350, yPtr, 105, 25));

                int skillID = profession->GetSkillIndex((uint32_t)i);

                CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
                    ID_SPS_SKILLS_FILED + (int)i,
                    0x0386,
                    0,
                    0x0021,
                    354,
                    yPtr + 5,
                    90,
                    false,
                    9,
                    TS_LEFT,
                    UOFONT_FIXED));

                CSkill *skill = g_SkillsManager.Get(skillID);

                if (skillID >= (int)g_SkillsManager.Count || skill == nullptr)
                {
                    entry->m_Entry.SetTextA("Click here");
                }
                else
                {
                    entry->m_Entry.SetTextA(skill->Name);
                }

                entry->CheckOnSerial = true;
                entry->ReadOnly = true;

                yPtr += 32;
            }
        }

        for (int i = 0; i < 3; i++)
        {
            m_SkillsSliders[i] = (CGUISlider *)Add(new CGUISlider(
                ID_SPS_SKILLS_SPHERE + (int)i,
                0x00D8,
                0x00D8,
                0x00D8,
                0x00D5,
                true,
                false,
                496,
                258 + ((int)i * 32),
                95,
                0,
                50,
                profession->GetSkillValue((int)i)));
            m_SkillsSliders[i]->DefaultTextOffset = -10;
            m_SkillsSliders[i]->SetTextParameters(true, STP_LEFT, 1, 1, false);
        }

        Add(new CGUIButton(ID_SPS_ARROW_NEXT, 0x15A4, 0x15A5, 0x15A6, 610, 445));
    }
}

void CGumpScreenSelectProfession::UpdateContentNew()
{
    CBaseProfession *obj = g_ProfessionManager.Selected;

    Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));
    Add(new CGUIGumppic(0x157C, 0, 0));
    Add(new CGUIGumppic(0x15A0, 0, 4));
    Add(new CGUIButton(ID_SPS_QUIT, 0x1589, 0x158A, 0x158B, 555, 4));
    Add(new CGUIButton(ID_SPS_ARROW_PREV, 0x15A1, 0x15A2, 0x15A3, 586, 445));
    Add(new CGUIResizepic(0, 0x0A28, 100, 80, 470, 372));
    Add(new CGUIGumppic(0x058B, 213, 57));
    Add(new CGUIGumppic(0x0589, 290, 44));

    auto str = g_ClilocManager.Cliloc(g_Language)
                   ->GetA(3000326, false, "Choose a Trade for Your Character");
    CGUIText *text = new CGUIText(0x0386, 120, 126);
    text->CreateTextureA(2, str, 432, TS_CENTER);
    Add(text);

    Add(new CGUIGumppic(obj->Gump, 299, 53));

    if (obj->Type == PT_CATEGORY) //category
    {
        int offsX = 0;
        int offsY = 0;
        int index = 0;

        for (CBaseProfession *child = (CBaseProfession *)obj->m_Items; child != nullptr; child = (CBaseProfession *)child->m_Next)
        {
            Add(new CGUIResizepic(ID_SPS_LABEL + index, 0x0BB8, 145 + offsX, 168 + offsY, 175, 34));

            CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
                ID_SPS_LABEL + index,
                0,
                0,
                0,
                151 + offsX,
                174 + offsY,
                185,
                true,
                2,
                TS_LEFT,
                UOFONT_FIXED | UOFONT_SOLID));
            entry->m_Entry.SetTextA(child->Name); // TODO: Check DrawW - Refactory display strings
            entry->m_Entry.PrepareToDrawW(2, 0, TS_LEFT, UOFONT_FIXED | UOFONT_SOLID);
            entry->CheckOnSerial = true;
            entry->ReadOnly = true;

            Add(new CGUIButton(
                ID_SPS_LABEL + index,
                child->Gump,
                child->Gump,
                child->Gump + 1,
                265 + offsX,
                155 + offsY));

            if (offsX != 0)
            {
                offsX = 0;
                offsY += 70;
            }
            else
            {
                offsX = 195;
            }

            index++;
        }
    }
    else if (obj->Type == PT_PROFESSION) //profession
    {
        CProfession *profession = (CProfession *)obj;
        int statVal[3] = { profession->Str, profession->Dex, profession->Int };
        const astr_t statName[3] = { "Strength", "Dexterity", "Intelligence" };

        int yPtr = 171;

        //Stats
        for (int i = 0; i < 3; i++)
        {
            text = (CGUIText *)Add(new CGUIText(1, 160, yPtr));
            text->CreateTextureA(1, statName[i]);

            m_StatsSliders[i] = (CGUISlider *)Add(new CGUISlider(
                ID_SPS_STATS_SPHERE + (int)i,
                0x00D8,
                0x00D8,
                0x00D8,
                0x00D5,
                true,
                false,
                160,
                yPtr + 20,
                95,
                10,
                50,
                statVal[i]));
            m_StatsSliders[i]->SetTextParameters(true, STP_RIGHT, 1, 1, false);

            yPtr += 80;
        }

        yPtr = 171;

        //Skills

        if (g_SelectProfessionScreen.GetSkillSelection() != 0)
        {
            CGUIHTMLGump *htmlGump = (CGUIHTMLGump *)Add(
                new CGUIHTMLGump(ID_SPS_HTMLGUMP, 0x0BB8, 320, 168, 197, 215, true, true));

            yPtr = 4;

            for (uint32_t i = 0; i < g_SkillsManager.Count; i++)
            {
                CSkill *skill = g_SkillsManager.Get(g_SkillsManager.GetSortedIndex(i));
                if (skill == nullptr)
                {
                    continue;
                }

                CGUITextEntry *entry = new CGUITextEntry(
                    ID_SPS_SKILLS_LIST + (int)i, 1, 0x0035, 0x0035, 2, yPtr, 0, false, 9);
                entry->m_Entry.SetTextA(skill->Name);
                entry->m_Entry.PrepareToDrawA(9, 1);
                entry->CheckOnSerial = true;
                entry->ReadOnly = true;

                htmlGump->Add(new CGUIHitBox(
                    ID_SPS_SKILLS_LIST + (int)i, 2, yPtr, 190, entry->m_Entry.m_Texture.Height));
                htmlGump->Add(entry);

                yPtr += entry->m_Entry.m_Texture.Height;
            }

            htmlGump->CalculateDataSize();
        }
        else
        {
            int skillsCount = 3;
            int skillStep = 80;

            if (g_Config.ClientVersion >= CV_70160)
            {
                yPtr -= 12;
                skillStep = 70;
                skillsCount++;
            }

            for (int i = 0; i < skillsCount; i++)
            {
                Add(new CGUIResizepic(ID_SPS_SKILLS_FILED + (int)i, 0x0BB8, 340, yPtr, 175, 25));

                int skillID = profession->GetSkillIndex((int)i);

                CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
                    ID_SPS_SKILLS_FILED + (int)i,
                    0x0386,
                    0,
                    0x0021,
                    346,
                    yPtr + 5,
                    90,
                    false,
                    9,
                    TS_LEFT,
                    UOFONT_FIXED));

                CSkill *skill = g_SkillsManager.Get(skillID);

                if (skillID >= (int)g_SkillsManager.Count || skill == nullptr)
                {
                    entry->m_Entry.SetTextA("Click here");
                }
                else
                {
                    entry->m_Entry.SetTextA(skill->Name);
                }

                entry->CheckOnSerial = true;
                entry->ReadOnly = true;

                m_SkillsSliders[i] = (CGUISlider *)Add(new CGUISlider(
                    ID_SPS_SKILLS_SPHERE + (int)i,
                    0x00D8,
                    0x00D8,
                    0x00D8,
                    0x00D5,
                    true,
                    false,
                    340,
                    yPtr + 30,
                    95,
                    0,
                    50,
                    profession->GetSkillValue((int)i)));
                m_SkillsSliders[i]->DefaultTextOffset = 60;
                m_SkillsSliders[i]->SetTextParameters(true, STP_RIGHT, 1, 1, false);

                yPtr += skillStep;
            }
        }

        Add(new CGUIButton(ID_SPS_ARROW_NEXT, 0x15A4, 0x15A5, 0x15A6, 610, 445));
    }
}

void CGumpScreenSelectProfession::InitToolTip()
{
    if (!g_ConfigManager.UseToolTips)
    {
        return;
    }

    uint32_t id = g_SelectedObject.Serial;

    switch (id)
    {
        case ID_SPS_QUIT:
        {
            g_ToolTip.Set(L"Quit Ultima Online", 80);
            break;
        }
        case ID_SPS_ARROW_NEXT:
        {
            g_ToolTip.Set(L"Next screen");
            break;
        }
        case ID_SPS_ARROW_PREV:
        {
            g_ToolTip.Set(L"Previous screen");
            break;
        }
        case ID_SPS_ARROW_BACK_PROFESSION:
        case ID_SPS_LABEL_BACK_PROFESSION:
        {
            g_ToolTip.Set(L"Back to select profession category", 150);
            break;
        }
        default:
            break;
    }

    if (id >= ID_SPS_LABEL && id < ID_SPS_SKILLS_LIST)
    {
        CBaseProfession *obj = g_ProfessionManager.Selected;

        int index = 0;

        for (CBaseProfession *child = (CBaseProfession *)obj->m_Items; child != nullptr; child = (CBaseProfession *)child->m_Next)
        {
            if (id == ID_SPS_LABEL + index)
            {
                if (child->DescriptionClilocID != 0u)
                {
                    g_ToolTip.Set(child->DescriptionClilocID, "Description", 350, false);
                }

                break;
            }

            index++;
        }
    }
}

void CGumpScreenSelectProfession::GUMP_BUTTON_EVENT_C
{
    //1.25.32 CharStat + InitSkill controls (disjoint id blocks: popup
    //rows/arrows / panel buttons never share ids)
    if (g_Config.ClientVersion < CV_200)
    {
        //the duplicate-skills message box is modal: OK closes it, every
        //other click is swallowed
        if (m_1998MsgDialog)
        {
            if (serial == ID_SPS_1998_MSG_OK)
            {
                m_1998MsgDialog = false;
                WantRedraw = true;
                WantUpdateContent = true;
            }
            return;
        }

        if (m_1998ProfessionPopup.IsRow(serial))
        {
            //the picked profession carries the stat/skill presets the stat
            //sliders and the InitSkill step read; the pick closes the popup
            //(the original has no close-X)
            CBaseProfession *picked =
                Get1998ProfessionByIndex(serial - ID_SPS_1998_POPUP_ROW);
            if (picked != nullptr)
            {
                g_ProfessionManager.Selected = picked;
            }
            m_1998ProfessionPopupOpen = false;
            WantRedraw = true;
            //the CharStat panel shows the picked profession's stat presets:
            //rebuild it (the popup scroll itself never rebuilds)
            WantUpdateContent = true;
        }
        else if (serial == ID_SPS_1998_POPUP_UP || serial == ID_SPS_1998_POPUP_DOWN)
        {
            //scroll: re-render the windowed rows around the new top
            m_1998ProfessionPopup.SetTop(
                m_1998ProfessionPopup.Top() + (serial == ID_SPS_1998_POPUP_UP ? -1 : 1));
            WantRedraw = true;
        }
        else if (serial == ID_SPS_1998_PROFESSION)
        {
            //the picker toggles the list (the original recreates it)
            m_1998ProfessionPopupOpen = !m_1998ProfessionPopupOpen;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (serial == ID_SPS_1998_BACK)
        {
            g_SelectProfessionScreen.CreateSmoothAction(
                CSelectProfessionScreen::ID_SMOOTH_SPS_GO_SCREEN_CHARACTER);
        }
        else if (serial == ID_SPS_1998_OK)
        {
            if (m_1998NameEntry == nullptr)
            {
                return;
            }

            astr_t name = m_1998NameEntry->m_Entry.GetTextA();
            if (name.length() < 2)
            {
                g_GameWindow.ShowMessage("Enter a name for your character.", "Character creation");
                return;
            }

            //the create-character frame carries the name and the account
            //password (the wizard's password field)
            g_CreateCharacterScreen.Name = name;
            g_Config.Password = m_Password->GetTextA();

            //on to the skill picks (the original's InitSkill gump, now the
            //second page of this gump)
            Page = 2;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (serial == ID_SPS_1998_SKILLS_BACK)
        {
            Page = 1; //back to CharStat
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (serial == ID_SPS_1998_SKILLS_NEXT)
        {
            CProfession *profession = (CProfession *)g_ProfessionManager.Selected;

            //all three picks must be set and distinct (the original refused
            //with "You have two of the same skill.")
            astr_t error = "";
            for (int i = 0; i < 3 && error.empty(); i++)
            {
                if (profession->GetSkillIndex(i) == 0xFF)
                {
                    error = "Pick three skills first.";
                    break;
                }

                for (int j = i + 1; j < 3; j++)
                {
                    if (profession->GetSkillIndex(i) == profession->GetSkillIndex(j))
                    {
                        error = "You have two of the same skill.";
                        break;
                    }
                }
            }

            if (!error.empty())
            {
                //the original shows its message gump and stays; the flag
                //needs a rebuild - the dialog is rendered by
                //UpdateContent1998
                m_1998MsgDialog = true;
                m_1998MsgText = error;
                WantRedraw = true;
                WantUpdateContent = true;
                return;
            }

            //on to the appearance step
            g_SelectProfessionScreen.CreateSmoothAction(
                CSelectProfessionScreen::ID_SMOOTH_SPS_GO_SCREEN_CREATE);
        }
        else if (serial >= ID_SPS_1998_SKILL_PICK && serial < ID_SPS_1998_SKILL_PICK + 3)
        {
            //the pick buttons toggle the skill list for their slot
            const int slot = serial - ID_SPS_1998_SKILL_PICK;
            if (m_1998SkillPopupOpen && m_1998PickSlot == slot)
            {
                m_1998SkillPopupOpen = false;
            }
            else
            {
                m_1998PickSlot = slot;
                m_1998SkillPopupOpen = true;
            }
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (m_1998SkillPopup.IsRow(serial))
        {
            //row i is the i-th sorted skill; the pick updates the panel in
            //place and closes the popup
            const int index = g_SkillsManager.GetSortedIndex(serial - ID_SPS_1998_SKILL_POPUP_ROW);
            CProfession *profession = (CProfession *)g_ProfessionManager.Selected;
            profession->SetSkillIndex(m_1998PickSlot, (uint8_t)index);

            astr_t name = "";
            if (index < (int)g_SkillsManager.Count)
            {
                CSkill *skill = g_SkillsManager.Get(index);
                if (skill != nullptr)
                {
                    name = skill->Name;
                }
            }
            if (m_1998SkillName[m_1998PickSlot] != nullptr)
            {
                m_1998SkillName[m_1998PickSlot]->CreateTextureA(1, name);
            }

            //a pick closes the list (a rebuild - WantRedraw alone leaves
            //the popup items in the visible page)
            m_1998SkillPopupOpen = false;
            WantRedraw = true;
            WantUpdateContent = true;
        }
        else if (
            serial == ID_SPS_1998_SKILL_POPUP_UP || serial == ID_SPS_1998_SKILL_POPUP_DOWN)
        {
            //scroll: re-render the windowed rows around the new top
            m_1998SkillPopup.SetTop(
                m_1998SkillPopup.Top() +
                (serial == ID_SPS_1998_SKILL_POPUP_UP ? -1 : 1));
            WantRedraw = true;
        }

        return;
    }

    CBaseProfession *obj = g_ProfessionManager.Selected;
    CProfession *profession = (CProfession *)obj;

    if (serial == ID_SPS_QUIT)
    { //x button
        g_SelectProfessionScreen.CreateSmoothAction(CSelectProfessionScreen::ID_SMOOTH_SPS_QUIT);
    }
    else if (serial == ID_SPS_ERROR_OK)
    {
        UpdateContentNew();
    }
    else if (serial == ID_SPS_ARROW_PREV) //< button
    {
        if (g_Config.ClientVersion >= CV_308Z &&
            g_ProfessionManager.Selected->Type == PT_PROFESSION &&
            g_ProfessionManager.Selected->DescriptionIndex == -1 /*Advanced*/)
        {
            g_ProfessionManager.Selected =
                g_ProfessionManager.GetParent(g_ProfessionManager.Selected);
            g_SelectProfessionScreen.SetSkillSelection(0);

            return;
        }
        {
            g_SelectProfessionScreen.CreateSmoothAction(
                CSelectProfessionScreen::ID_SMOOTH_SPS_GO_SCREEN_CHARACTER);
        }
    }
    else if (serial == ID_SPS_ARROW_NEXT) //> button
    {
        if (obj->Type == PT_PROFESSION)
        {
            bool passed = true;

            if (profession->DescriptionIndex == -1) //Advanced
            {
                int skillsCount = 3;

                if (g_Config.ClientVersion >= CV_70160)
                {
                    skillsCount++;
                }

                for (int i = 0; i < skillsCount; i++)
                {
                    for (int j = 0; j < skillsCount; j++)
                    {
                        if (i != j)
                        {
                            if (profession->GetSkillIndex((int)i) == 0xFF ||
                                profession->GetSkillIndex((int)i) ==
                                    profession->GetSkillIndex((int)j))
                            {
                                passed = false;

                                g_SelectProfessionScreen.CreateSmoothAction(
                                    CSelectProfessionScreen::ID_SMOOTH_SPS_GO_SCREEN_GAME_CONNECT);

                                break;
                            }
                        }
                    }
                }
            }

            if (passed)
            {
                g_SelectProfessionScreen.CreateSmoothAction(
                    CSelectProfessionScreen::ID_SMOOTH_SPS_GO_SCREEN_CREATE);
            }
        }
    }
    else if (
        serial == ID_SPS_ARROW_BACK_PROFESSION ||
        serial == ID_SPS_LABEL_BACK_PROFESSION) //Arrow < or General Label gump
    {
        g_ProfessionManager.Selected = g_ProfessionManager.GetParent(g_ProfessionManager.Selected);
        g_SelectProfessionScreen.SetSkillSelection(0);

        return;
    }
    else if (obj->Type == PT_CATEGORY)
    {
        int index = 0;

        for (CBaseProfession *child = (CBaseProfession *)obj->m_Items; child != nullptr; child = (CBaseProfession *)child->m_Next)
        {
            if (serial == ID_SPS_LABEL + index)
            {
                if (child->NameClilocID == 1061177 || child->NameClilocID == 1061178)
                {
                    if (!g_Pal_Necro_Creation)
                    {
                        NoFeatureError();
                        return;
                    }
                }
                g_ProfessionManager.Selected = child;
                g_SelectProfessionScreen.SetSkillSelection(0);

                if (g_Config.ClientVersion >= CV_308Z && child->Type == PT_PROFESSION &&
                    child->DescriptionIndex != -1)
                {
                    g_SelectProfessionScreen.CreateSmoothAction(
                        CSelectProfessionScreen::ID_SMOOTH_SPS_GO_SCREEN_CREATE);
                }

                return;
            }

            index++;
        }
    }
    else if (obj->Type == PT_PROFESSION)
    {
        if (serial >= ID_SPS_SKILLS_LIST)
        {
            g_SelectProfessionScreen.SetSkillSelection(
                g_SelectProfessionScreen.GetSkillSelection() - 1);
            int index = serial - ID_SPS_SKILLS_LIST;
            index = g_SkillsManager.GetSortedIndex(index);

            profession->SetSkillIndex(g_SelectProfessionScreen.GetSkillSelection(), index);

            g_SelectProfessionScreen.SetSkillSelection(0);
        }
        else
        {
            int skillsCount = 3;

            if (g_Config.ClientVersion >= CV_70160)
            {
                skillsCount++;
            }

            for (int i = 0; i < skillsCount; i++)
            {
                if (serial == ID_SPS_SKILLS_FILED + (int)i)
                {
                    g_SelectProfessionScreen.SetSkillSelection((int)i + 1);

                    break;
                }
            }
        }
    }
}

void CGumpScreenSelectProfession::NoFeatureError()
{
    Add(new CGUIResizepic(0, 0x0A28, 142, 134, 356, 212));
    Add(new CGUIButton(ID_SPS_ERROR_OK, 0x0481, 0x0482, 0x0483, 306, 304));
    CGUIText *obj = new CGUIText(0x0386, 189, 178);
    obj->CreateTextureA(2, "The server don't support this Age of Shadow feature", 260, TS_CENTER);
    Add(obj);
}

void CGumpScreenSelectProfession::GUMP_SLIDER_CLICK_EVENT_C
{
    OnSliderMove(serial);
}

void CGumpScreenSelectProfession::GUMP_SLIDER_MOVE_EVENT_C
{
    if (m_1998ProfessionPopup.IsThumb(serial))
    {
        //thumb drag: read the slider value back into the row window (the
        //row textures refresh in place)
        m_1998ProfessionPopup.SyncThumb();
        WantRedraw = true;
        return;
    }

    if (m_1998SkillPopup.IsThumb(serial))
    {
        m_1998SkillPopup.SyncThumb();
        WantRedraw = true;
        return;
    }

    //1.25.32 CharStat stat sliders: the original had no sum shuffling on
    //this screen, each stat is an independent slider (max 35); the values
    //go straight into the profession (the create packet reads them there)
    if (serial >= ID_SPS_1998_STAT && serial < ID_SPS_1998_STAT + 3)
    {
        //1.25.32 CharStat stat-linkage (FUN_0041AC00): the three stats share
        //a 65-point pool, each clamped to 10..45. After the dragged stat is
        //set, the drift is split in half (odd point to the second stat) and
        //applied to the other two, with a 45 cap / 10 floor that spills the
        //remainder to the sibling
        const int idx = serial - ID_SPS_1998_STAT;
        const int o1 = (idx + 1) % 3;
        const int o2 = (idx + 2) % 3;
        int value[3] = { m_1998StatSliders[0]->Value, m_1998StatSliders[1]->Value,
            m_1998StatSliders[2]->Value };
        const int total = value[0] + value[1] + value[2];
        if (total < 65)
        {
            int half = (65 - total) / 2;
            int rest = (65 - total) - half;
            if (value[o1] + half > 45)
            {
                const int spill = value[o1] + half - 45;
                value[o1] = 45;
                value[o2] += rest + spill;
            }
            else if (value[o2] + rest > 45)
            {
                const int spill = value[o2] + rest - 45;
                value[o2] = 45;
                value[o1] += half + spill;
            }
            else
            {
                value[o1] += half;
                value[o2] += rest;
            }
        }
        else if (total > 65)
        {
            int half = (total - 65) / 2;
            int rest = (total - 65) - half;
            if (value[o1] - half < 10)
            {
                const int spill = half - (value[o1] - 10);
                value[o1] = 10;
                value[o2] -= rest + spill;
            }
            else if (value[o2] - rest < 10)
            {
                const int spill = rest - (value[o2] - 10);
                value[o2] = 10;
                value[o1] -= half + spill;
            }
            else
            {
                value[o1] -= half;
                value[o2] -= rest;
            }
        }

        CProfession *profession = (CProfession *)g_ProfessionManager.Selected;
        profession->Str = (uint8_t)value[0];
        profession->Dex = (uint8_t)value[1];
        profession->Int = (uint8_t)value[2];

        for (int i = 0; i < 3; i++)
        {
            if (i != idx)
            {
                m_1998StatSliders[i]->Value = value[i];
                m_1998StatSliders[i]->CalculateOffset();
            }
            //left column: STR / DEX / INT; right column: HP / Mana / Stamina
            //(STR, INT, DEX top to bottom)
            static const int displayStat[3] = { 0, 2, 1 };
            m_1998StatLeft[i]->CreateTextureA(1, str_from(value[i]));
            m_1998StatValue[i]->CreateTextureA(1, str_from(value[displayStat[i]]));
        }
        WantRedraw = true;
        return;
    }

    //1.25.32 InitSkill skill-value sliders: the three values share a
    //100-point pool, each clamped to 0..50 - same half/rest drift
    //compensation as the stat sliders, with a 50 cap / 0 floor spilling
    //the remainder to the sibling (0x41C3A0)
    if (serial >= ID_SPS_1998_SKILL_SLIDER && serial < ID_SPS_1998_SKILL_SLIDER + 3)
    {
        const int idx = serial - ID_SPS_1998_SKILL_SLIDER;
        const int o1 = (idx + 1) % 3;
        const int o2 = (idx + 2) % 3;
        int value[3] = { m_1998SkillSliders[0]->Value, m_1998SkillSliders[1]->Value,
            m_1998SkillSliders[2]->Value };
        const int total = value[0] + value[1] + value[2];
        if (total < 100)
        {
            int half = (100 - total) / 2;
            int rest = (100 - total) - half;
            if (value[o1] + half > 50)
            {
                const int spill = value[o1] + half - 50;
                value[o1] = 50;
                value[o2] += rest + spill;
            }
            else if (value[o2] + rest > 50)
            {
                const int spill = value[o2] + rest - 50;
                value[o2] = 50;
                value[o1] += half + spill;
            }
            else
            {
                value[o1] += half;
                value[o2] += rest;
            }
        }
        else if (total > 100)
        {
            int half = (total - 100) / 2;
            int rest = (total - 100) - half;
            if (value[o1] - half < 0)
            {
                const int spill = half - value[o1];
                value[o1] = 0;
                value[o2] -= rest + spill;
            }
            else if (value[o2] - rest < 0)
            {
                const int spill = rest - value[o2];
                value[o2] = 0;
                value[o1] -= half + spill;
            }
            else
            {
                value[o1] -= half;
                value[o2] -= rest;
            }
        }

        CProfession *profession = (CProfession *)g_ProfessionManager.Selected;
        for (int i = 0; i < 3; i++)
        {
            if (i != idx)
            {
                m_1998SkillSliders[i]->Value = value[i];
                m_1998SkillSliders[i]->CalculateOffset();
            }
            profession->SetSkillValue(i, (uint8_t)value[i]);
            m_1998SkillValue[i]->CreateTextureA(1, str_from(value[i]));
        }
        WantRedraw = true;
        return;
    }

    int skillsCount = 3;

    if (g_Config.ClientVersion >= CV_70160)
    {
        skillsCount++;
    }

    //Stats
    if (serial >= ID_SPS_STATS_SPHERE && (int)serial < ID_SPS_STATS_SPHERE + skillsCount)
    {
        if (g_Config.ClientVersion >= CV_308Z)
        {
            if (g_Config.ClientVersion >= CV_70160)
            {
                ShuffleStats(serial - ID_SPS_STATS_SPHERE, 90, 60);
            }
            else
            {
                ShuffleStats(serial - ID_SPS_STATS_SPHERE, 80, 60);
            }
        }
        else
        {
            ShuffleStats(serial - ID_SPS_STATS_SPHERE, 65, 45);
        }
    }

    //Skills
    if (serial >= ID_SPS_SKILLS_SPHERE && (int)serial < ID_SPS_SKILLS_SPHERE + skillsCount)
    {
        ShuffleSkills(serial - ID_SPS_SKILLS_SPHERE);
    }
}

void CGumpScreenSelectProfession::ShuffleStats(int id, int maxSum, int maxVal)
{
    CProfession *profession = (CProfession *)g_ProfessionManager.Selected;
    int stats[3] = { m_StatsSliders[0]->Value, m_StatsSliders[1]->Value, m_StatsSliders[2]->Value };

    int used_stat = id;
    int others_stat[2] = { 0 };

    others_stat[0] = 0;
    if (others_stat[0] == used_stat)
    {
        others_stat[0]++;
    }

    others_stat[1] = others_stat[0] + 1;
    if (others_stat[1] == used_stat)
    {
        others_stat[1]++;

        if (others_stat[1] == 3)
        {
            others_stat[1] = 0;
        }
    }

    stats[used_stat] = m_StatsSliders[id]->Value;

    int stat_sum = maxSum - (stats[0] + stats[1] + stats[2]);

    if (stat_sum > 0) //stat will decrease
    {
        while (stat_sum > 0)
        {
            if (stats[others_stat[0]] < maxVal)
            {
                stat_sum--;
                stats[others_stat[0]]++;
            }
            else if (stats[others_stat[1]] < maxVal)
            {
                stat_sum--;
                stats[others_stat[1]]++;
            }
            else
            {
                break;
            }
        }
    }
    else if (stat_sum < 0) //stat will increase
    {
        while (stat_sum < 0)
        {
            if (stats[others_stat[0]] > 10)
            {
                stat_sum++;
                stats[others_stat[0]]--;
            }
            else if (stats[others_stat[1]] > 10)
            {
                stat_sum++;
                stats[others_stat[1]]--;
            }
            else
            {
                break;
            }
        }
    }

    for (int i = 0; i < 3; i++)
    {
        m_StatsSliders[i]->Value = stats[i];
        m_StatsSliders[i]->CalculateOffset();
        m_StatsSliders[i]->UpdateText();
    }

    profession->Str = stats[0];
    profession->Dex = stats[1];
    profession->Int = stats[2];
}

void CGumpScreenSelectProfession::ShuffleSkills(int id)
{
    CProfession *profession = (CProfession *)g_ProfessionManager.Selected;
    int skills[4] = {
        m_SkillsSliders[0]->Value, m_SkillsSliders[1]->Value, m_SkillsSliders[2]->Value, 0
    };

    int used_skill = id;
    int others_skills[3] = { 0 };

    others_skills[0] = 0;

    if (others_skills[0] == used_skill)
    {
        others_skills[0]++;
    }

    others_skills[1] = others_skills[0] + 1;

    if (others_skills[1] == used_skill)
    {
        others_skills[1]++;
    }

    others_skills[2] = others_skills[1] + 1;

    if (others_skills[2] == used_skill)
    {
        others_skills[2]++;

        if (others_skills[2] == 5)
        {
            others_skills[2] = 0;
        }
    }

    int skillsCount = 3;
    bool use4Skill = false;

    if (g_Config.ClientVersion >= CV_70160)
    {
        use4Skill = true;
        skillsCount++;
        skills[3] = m_SkillsSliders[3]->Value;
    }

    skills[used_skill] = m_SkillsSliders[id]->Value;

    int skills_sum = 100 - (skills[0] + skills[1] + skills[2] + skills[3]);

    if (skills_sum > 0) //skill will decrease
    {
        while (skills_sum > 0)
        {
            if (skills[others_skills[0]] < 50)
            {
                skills_sum--;
                skills[others_skills[0]]++;
            }
            else if (skills[others_skills[1]] < 50)
            {
                skills_sum--;
                skills[others_skills[1]]++;
            }
            else if (use4Skill && skills[others_skills[2]] < 50)
            {
                skills_sum--;
                skills[others_skills[2]]++;
            }
            else
            {
                break;
            }
        }
    }
    else if (skills_sum < 0) //skill will increase
    {
        while (skills_sum < 0)
        {
            if (skills[others_skills[0]] > 0)
            {
                skills_sum++;
                skills[others_skills[0]]--;
            }
            else if (skills[others_skills[1]] > 0)
            {
                skills_sum++;
                skills[others_skills[1]]--;
            }
            else if (use4Skill && skills[others_skills[2]] > 0)
            {
                skills_sum++;
                skills[others_skills[2]]--;
            }
            else
            {
                break;
            }
        }
    }

    for (int i = 0; i < skillsCount; i++)
    {
        m_SkillsSliders[i]->Value = skills[i];
        m_SkillsSliders[i]->CalculateOffset();
        m_SkillsSliders[i]->UpdateText();
    }

    profession->SetSkillValue(0, skills[0]);
    profession->SetSkillValue(1, skills[1]);
    profession->SetSkillValue(2, skills[2]);
}
