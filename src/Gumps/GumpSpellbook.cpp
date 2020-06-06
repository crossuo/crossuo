// MIT License
// Copyright (C) September 2016 Hotride

#include <cmath> // ceilf
#include <algorithm>
#include "GumpSpellbook.h"
#include "GumpSpell.h"
#include "../Config.h"
#include "../ToolTip.h"
#include "../CrossUO.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../ClickObject.h"
#include "../GameWindow.h"
#include "../Managers/MouseManager.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/GumpManager.h"
#include "../GameObjects/GamePlayer.h"
#include "../GameObjects/GameWorld.h"

void InitSpells()
{
    if (g_Config.ClientVersion >= CV_305D)
    {
        CGumpSpellbook::m_SpellReagents1[4] = "Sulfurous ash";                 // Magic Arrow
        CGumpSpellbook::m_SpellReagents1[17] = "Black pearl";                  // Fireball
        CGumpSpellbook::m_SpellReagents1[29] = "Mandrake root\nSulfurous ash"; // Lightning
        CGumpSpellbook::m_SpellReagents1[42] = "Bloodmoss\nMandrake root";     // Explosion
    }
}

enum
{
    ID_GSB_BUTTON_PREV = 1,
    ID_GSB_BUTTON_NEXT = 2,
    ID_GSB_BUTTON_CIRCLE_1_2 = 3,
    ID_GSB_BUTTON_CIRCLE_3_4 = 4,
    ID_GSB_BUTTON_CIRCLE_5_6 = 5,
    ID_GSB_BUTTON_CIRCLE_7_8 = 6,
    ID_GSB_BUTTON_MINIMIZE = 7,
    ID_GSB_LOCK_MOVING = 8,
    ID_GSB_SPELL_ICON_LEFT = 100,
    ID_GSB_SPELL_ICON_RIGHT = 1000,
};

static astr_t s_SpellCircleName[] = { "First Circle",   "Second Circle", "Third Circle",
                                      "Fourth Circle",  "Fifth Circle",  "Sixth Circle",
                                      "Seventh Circle", "Eighth Circle" };

static astr_t MasterGroupNameById(int id)
{
    switch (id)
    {
        default:
        case 1:
        case 2:
            return "Provocation";
        case 3:
        case 4:
            return "Peacemaking";
        case 5:
        case 6:
            return "Discordance";
        case 7:
        case 8:
            return "Magery";
        case 9:
        case 10:
            return "Mysticism";
        case 11:
        case 12:
            return "Necromancy";
        case 13:
        case 14:
            return "Spellweaving";
        case 16:
        case 17:
            return "Bushido";
        case 19:
        case 20:
            return "Chivalry";
        case 21:
        case 22:
            return "Ninjitsu";
        case 23:
        case 24:
            return "Archery";
        case 25:
        case 26:
            return "Fencing";
        case 27:
        case 28:
            return "Mace Fighting";
        case 29:
        case 30:
            return "Swordmanship";
        case 31:
        case 32:
            return "Throwing";
        case 34:
        case 35:
        case 36:
            return "Parrying";
        case 37:
        case 38:
        case 39:
            return "Poisoning";
        case 40:
        case 41:
        case 42:
            return "Wrestling";
        case 43:
        case 44:
        case 45:
            return "Animal Taming";
        case 15:
        case 18:
        case 33:
            return "Passive";
    }
}

CGumpSpellbook::CGumpSpellbook(uint32_t serial, int x, int y)
    : CGump(GT_SPELLBOOK, serial, x, y)
{
    Draw2Page = 1;
    memset(&m_Spells[0], 0, sizeof(m_Spells));
}

CGumpSpellbook::~CGumpSpellbook()
{
}

void CGumpSpellbook::UpdateGraphic(uint16_t parentGraphic)
{
    SPELLBOOK_TYPE bookType = BookType;

    switch (parentGraphic)
    {
        case 0x0efa:
        {
            BookType = ST_MAGERY;
            break;
        }
        case 0x2253:
        {
            BookType = ST_NECROMANCY;
            break;
        }
        case 0x2252:
        {
            BookType = ST_CHIVALRY;
            break;
        }
        case 0x238c:
        {
            if ((g_ClientFlag & CLF_SAMURAI_NINJA) != 0)
            {
                BookType = ST_BUSHIDO;
            }
            break;
        }
        case 0x23a0:
        {
            if ((g_ClientFlag & CLF_SAMURAI_NINJA) != 0)
            {
                BookType = ST_NINJITSU;
            }
            break;
        }
        case 0x2d50:
        {
            BookType = ST_SPELLWEAVING;
            break;
        }
        case 0x2d9d:
        {
            BookType = ST_MYSTICISM;
            break;
        }
        case 0x225a:
        case 0x225b:
        {
            BookType = ST_MASTERY;
            break;
        }
        default:
        {
            BookType = ST_MAGERY;
            break;
        }
    }

    if (bookType != BookType)
    {
        WantUpdateContent = true;
    }
}

void CGumpSpellbook::InitToolTip()
{
    if (Minimized)
    {
        g_ToolTip.Set(L"Double click to maximize book gump");
        return;
    }

    int indexPagesCount = 0;
    int tooltipOffset = 0;
    GetTooltipBookInfo(indexPagesCount, tooltipOffset);
    if (Page >= indexPagesCount)
    {
        uint32_t serial = g_SelectedObject.Serial;
        if (serial >= (uint32_t)ID_GSB_SPELL_ICON_LEFT)
        {
            if (serial >= (uint32_t)ID_GSB_SPELL_ICON_RIGHT)
            {
                serial -= ID_GSB_SPELL_ICON_RIGHT;
            }
            else
            {
                serial -= ID_GSB_SPELL_ICON_LEFT;
            }

            if (BookType == ST_MASTERY)
            {
                tooltipOffset = serial < 6 ? 1115689 : 1155932;
            }
            g_ToolTip.Set(
                g_ClilocManager.Cliloc(g_Language)->GetW(tooltipOffset + serial, true), 150);
        }
    }
}

void CGumpSpellbook::PrepareContent()
{
    int maxSpellsCount = 0;
    int spellsOnPage = 0;
    int indexPagesCount = 0;
    int spellIndexOffset = 0;
    uint16_t graphic = 0;
    uint16_t minimizedGraphic = 0;
    uint16_t iconStartGraphic = 0;

    GetSummaryBookInfo(
        maxSpellsCount,
        indexPagesCount,
        spellsOnPage,
        spellIndexOffset,
        graphic,
        minimizedGraphic,
        iconStartGraphic);

    if (g_PressedObject.LeftGump == this && Page >= indexPagesCount &&
        g_PressedObject.LeftSerial >= (uint32_t)ID_GSB_SPELL_ICON_LEFT)
    {
        CPoint2Di offset = g_MouseManager.LeftDroppedOffset();
        if ((abs(offset.X) >= DRAG_PIXEL_RANGE || abs(offset.Y) >= DRAG_PIXEL_RANGE) ||
            (g_MouseManager.LastLeftButtonClickTimer + g_MouseManager.DoubleClickDelay < g_Ticks))
        {
            int index = g_PressedObject.LeftSerial;
            if (index < ID_GSB_SPELL_ICON_RIGHT)
            {
                index -= ID_GSB_SPELL_ICON_LEFT;
            }
            else
            {
                index -= ID_GSB_SPELL_ICON_RIGHT;
            }

            if (iconStartGraphic >= 0x5300 && iconStartGraphic < 0x5500)
            {
                iconStartGraphic += 0x0020;
            }
            else if (BookType == ST_MASTERY && index > 6)
            {
                iconStartGraphic = 0x9b8b - 6; // FIXME: mastery icon fixup
            }
            g_GumpManager.AddGump(new CGumpSpell(
                index + spellIndexOffset,
                g_MouseManager.Position.X - 20,
                g_MouseManager.Position.Y - 20,
                iconStartGraphic + index,
                BookType));
            g_GameWindow.EmulateOnLeftMouseButtonDown();
        }
    }

    if (!Minimized && m_LastSpellPointer != nullptr)
    {
        spellIndexOffset = BookType == ST_MYSTICISM ? 677 : int(BookType) * 100;
        bool wantVisible = false;
        uint16_t graphicBookmark = 0x08ad;
        uint16_t graphicPointer = 0x08af;
        int wantX = m_LastSpellPointer->GetX();
        int wantY = m_LastSpellPointer->GetY();
        if (Page < indexPagesCount)
        {
            //int offs = (spellsOnPage * Page);
            for (int j = 0; j < 2; j++)
            {
                int y = 0;
                for (int i = 0; i < spellsOnPage; i++)
                {
                    const int offs = BookType == ST_MASTERY ? m_SpellIndices8[Page / 2][i] - 1 :
                                                              i + (spellsOnPage * Page);
                    if (offs >= 0 && m_Spells[offs] != 0)
                    {
                        if (offs + spellIndexOffset == g_LastSpellIndex - 1)
                        {
                            wantVisible = true;
                            wantY = 52 + y;
                            if (((Page + j) % 2) == 1)
                            {
                                graphicBookmark = 0x08ae;
                                graphicPointer =
                                    0x08b0 + (BookType == ST_MASTERY ? -1 : 0); // always left side
                                wantX = 203;
                            }
                            else
                            {
                                wantX = 184;
                            }
                            //offs++;
                            break;
                        }
                        y += 15;
                    }
                    //offs++;
                }
            }
        }
        else
        {
            int page = indexPagesCount;
            for (int i = 0; i < maxSpellsCount; i++)
            {
                if (m_Spells[i] == 0)
                {
                    continue;
                }

                if (page == Page || page == (Page + 1))
                {
                    if (i + spellIndexOffset == g_LastSpellIndex - 1)
                    {
                        wantVisible = true;
                        wantY = 40;
                        if ((page % 2) == 1)
                        {
                            graphicBookmark = 0x08ae;
                            graphicPointer = 0x08b0;
                            wantX = 203;
                        }
                        else
                        {
                            wantX = 184;
                        }
                        break;
                    }
                }
                page++;
            }
        }

        if (wantVisible != m_LastSpellPointer->Visible || wantX != m_LastSpellPointer->GetX() ||
            wantY != m_LastSpellPointer->GetY())
        {
            m_LastSpellPointer->Visible = wantVisible;
            m_LastSpellPointer->Graphic = graphicPointer;
            m_LastSpellPointer->SetX(wantX);
            m_LastSpellPointer->SetY(wantY);
            m_LastSpellBookmark->Visible = wantVisible;
            m_LastSpellBookmark->Graphic = graphicBookmark;
            m_LastSpellBookmark->SetX(wantX);
            WantRedraw = true;
        }
    }
}

void CGumpSpellbook::GetTooltipBookInfo(int &indexPagesCount, int &tooltipOffset)
{
    int maxSpellsCount = 0;
    switch (BookType)
    {
        case ST_MAGERY:
        {
            maxSpellsCount = SPELLBOOK_1_SPELLS_COUNT;
            tooltipOffset = 1061290;
            break;
        }
        case ST_NECROMANCY:
        {
            maxSpellsCount = SPELLBOOK_2_SPELLS_COUNT;
            tooltipOffset = 1061390;
            break;
        }
        case ST_CHIVALRY:
        {
            maxSpellsCount = SPELLBOOK_3_SPELLS_COUNT;
            tooltipOffset = 1061490;
            break;
        }
        case ST_BUSHIDO:
        {
            maxSpellsCount = SPELLBOOK_4_SPELLS_COUNT;
            tooltipOffset = 1063263;
            break;
        }
        case ST_NINJITSU:
        {
            maxSpellsCount = SPELLBOOK_5_SPELLS_COUNT;
            tooltipOffset = 1063279;
            break;
        }
        case ST_SPELLWEAVING:
        {
            maxSpellsCount = SPELLBOOK_6_SPELLS_COUNT;
            tooltipOffset = 1072042;
            break;
        }
        case ST_MYSTICISM:
        {
            maxSpellsCount = SPELLBOOK_7_SPELLS_COUNT;
            tooltipOffset = 1095193;
            break;
        }
        case ST_MASTERY:
        {
            maxSpellsCount = SPELLBOOK_8_SPELLS_COUNT;
            tooltipOffset = 0;
            break;
        }
        default:
            break;
    }

    indexPagesCount = (int)ceilf(maxSpellsCount / 8.0f);
    if (indexPagesCount % 2 == 1)
    {
        indexPagesCount++;
    }
}

void CGumpSpellbook::GetSummaryBookInfo(
    int &maxSpellsCount,
    int &indexPagesCount,
    int &spellsOnPage,
    int &spellIndexOffset,
    uint16_t &graphic,
    uint16_t &minimizedGraphic,
    uint16_t &iconStartGraphic)
{
    switch (BookType)
    {
        case ST_MAGERY:
        {
            maxSpellsCount = SPELLBOOK_1_SPELLS_COUNT;
            graphic = 0x08aC;
            minimizedGraphic = 0x08ba;
            iconStartGraphic = 0x08c0;
            spellIndexOffset = 0;
            break;
        }
        case ST_NECROMANCY:
        {
            maxSpellsCount = SPELLBOOK_2_SPELLS_COUNT;
            graphic = 0x2b00;
            minimizedGraphic = 0x2b03;
            iconStartGraphic = 0x5000;
            spellIndexOffset = 64;
            break;
        }
        case ST_CHIVALRY:
        {
            maxSpellsCount = SPELLBOOK_3_SPELLS_COUNT;
            graphic = 0x2b01;
            minimizedGraphic = 0x2b04;
            iconStartGraphic = 0x5100;
            spellIndexOffset = 81;
            break;
        }
        case ST_BUSHIDO:
        {
            maxSpellsCount = SPELLBOOK_4_SPELLS_COUNT;
            graphic = 0x2b07;
            minimizedGraphic = 0x2b09;
            iconStartGraphic = 0x5400;
            spellIndexOffset = 91;
            break;
        }
        case ST_NINJITSU:
        {
            maxSpellsCount = SPELLBOOK_5_SPELLS_COUNT;
            graphic = 0x2B06;
            minimizedGraphic = 0x2b08;
            iconStartGraphic = 0x5300;
            spellIndexOffset = 97;
            break;
        }
        case ST_SPELLWEAVING:
        {
            maxSpellsCount = SPELLBOOK_6_SPELLS_COUNT;
            graphic = 0x2b2f;
            minimizedGraphic = 0x2b2d;
            iconStartGraphic = 0x59d8;
            spellIndexOffset = 105;
            break;
        }
        case ST_MYSTICISM:
        {
            maxSpellsCount = SPELLBOOK_7_SPELLS_COUNT;
            graphic = 0x2b32;
            minimizedGraphic = 0x2b30;
            iconStartGraphic = 0x5dc0;
            spellIndexOffset = 121;
            break;
        }
        case ST_MASTERY:
        {
            maxSpellsCount = SPELLBOOK_8_SPELLS_COUNT;
            graphic = 0x08ac;
            minimizedGraphic = 0x08ba;
            iconStartGraphic = 0x0945;
            spellIndexOffset = 137;
            break;
        }
        default:
            break;
    }

    spellsOnPage = std::min(maxSpellsCount / 2, 8);
    indexPagesCount = (int)ceilf(maxSpellsCount / 8.0f);
    if (BookType == ST_MASTERY)
        indexPagesCount *= 2; // Mastery layout skips a page to show masteries

    if (indexPagesCount % 2 == 1)
    {
        indexPagesCount++;
    }
}

astr_t CGumpSpellbook::GetSpellName(int offset, astr_t &magicWord, astr_t &reagents)
{
    switch (BookType)
    {
        case ST_MAGERY:
            magicWord = m_SpellName1[offset][1];
            reagents = m_SpellReagents1[offset];
            return m_SpellName1[offset][0];
        case ST_NECROMANCY:
            magicWord = m_SpellName2[offset][1];
            reagents = m_SpellReagents2[offset];
            return m_SpellName2[offset][0];
        case ST_CHIVALRY:
            magicWord = m_SpellName3[offset][1];
            return m_SpellName3[offset][0];
        case ST_BUSHIDO:
            return m_SpellName4[offset];
        case ST_NINJITSU:
            return m_SpellName5[offset];
        case ST_SPELLWEAVING:
            magicWord = m_SpellName6[offset][1];
            return m_SpellName6[offset][0];
        case ST_MYSTICISM:
            magicWord = m_SpellName7[offset][1];
            reagents = m_SpellName7[offset][2];
            return m_SpellName7[offset][0];
        case ST_MASTERY:
            magicWord = m_SpellName8[offset][1];
            reagents = m_SpellName8[offset][2];
            return m_SpellName8[offset][0];
        default:
            break;
    }
    return "";
}

astr_t CGumpSpellbook::GetSpellRequries(int offset, int &y)
{
    char buf[100] = { 0 };
    y = 162;

    int manaCost = 0;
    int minSkill = 0;
    switch (BookType)
    {
        case ST_NECROMANCY:
        {
            const SPELL_REQURIES &req = m_SpellRequries2[offset];
            manaCost = req.ManaCost;
            minSkill = req.MinSkill;
            break;
        }
        case ST_CHIVALRY:
        {
            y = 148;
            const PALADIN_SPELL_REQURIES &req = m_SpellRequries3[offset];
            sprintf_s(
                buf,
                "Tithing Cost: %i\nMana Cost: %i\nMin. Skill: %i",
                req.TithingPoints,
                req.ManaCost,
                req.MinSkill);
            return buf;
        }
        case ST_BUSHIDO:
        {
            const SPELL_REQURIES &req = m_SpellRequries4[offset];
            manaCost = req.ManaCost;
            minSkill = req.MinSkill;
            break;
        }
        case ST_NINJITSU:
        {
            const SPELL_REQURIES &req = m_SpellRequries5[offset];
            manaCost = req.ManaCost;
            minSkill = req.MinSkill;
            break;
        }
        case ST_SPELLWEAVING:
        {
            const SPELL_REQURIES &req = m_SpellRequries6[offset];
            manaCost = req.ManaCost;
            minSkill = req.MinSkill;
            break;
        }
        case ST_MYSTICISM:
        {
            const SPELL_REQURIES &req = m_SpellRequries7[offset];
            manaCost = req.ManaCost;
            minSkill = req.MinSkill;
            break;
        }
        case ST_MASTERY:
        {
            const PALADIN_SPELL_REQURIES &req = m_SpellRequries8[offset];
            manaCost = req.ManaCost;
            minSkill = req.MinSkill;
            if (req.TithingPoints > 0)
            {
                y = 148;
                sprintf_s(
                    buf,
                    "Upkeep Cost: %i\nMana Cost: %i\nMin. Skill: %i",
                    req.TithingPoints,
                    req.ManaCost,
                    req.MinSkill);
            }
            else
            {
                sprintf_s(buf, "Mana Cost: %i\nMin. Skill: %i", req.ManaCost, req.MinSkill);
            }
            return buf;
            break;
        }
        default:
            break;
    }

    sprintf_s(buf, "Mana Cost: %i\nMin. Skill: %i", manaCost, minSkill);
    return buf;
}

void CGumpSpellbook::UpdateContent()
{
    m_Body = nullptr;
    m_PrevPage = nullptr;
    m_NextPage = nullptr;
    m_LastSpellPointer = nullptr;
    m_LastSpellBookmark = nullptr;
    m_TithingPointsText = nullptr;

    Clear();

    Add(new CGUIPage(-1));

    int maxSpellsCount = 0;
    int spellsOnPage = 0;
    int indexPagesCount = 0;
    int spellIndexOffset = 0;
    uint16_t graphic = 0;
    uint16_t minimizedGraphic = 0;
    uint16_t iconStartGraphic = 0;

    GetSummaryBookInfo(
        maxSpellsCount,
        indexPagesCount,
        spellsOnPage,
        spellIndexOffset,
        graphic,
        minimizedGraphic,
        iconStartGraphic);

    if (Minimized)
    {
        m_Body = (CGUIGumppic *)Add(new CGUIGumppic(minimizedGraphic, 0, 0));
        return;
    }

    m_Body = (CGUIGumppic *)Add(new CGUIGumppic(graphic, 0, 0));
    Add(new CGUIHitBox(ID_GSB_BUTTON_MINIMIZE, 6, 100, 16, 16, true));

    m_SpellCount = 0;
    memset(&m_Spells[0], 0, sizeof(m_Spells));

    CGameItem *spellbook = g_World->FindWorldItem(Serial);
    if (spellbook == nullptr)
    {
        return;
    }

    QFOR(item, spellbook->m_Items, CGameItem *)
    {
        int currentCount = item->Count;
        if (currentCount > 0 && currentCount <= maxSpellsCount)
        {
            m_Spells[currentCount - 1] = 1;
            m_SpellCount++;
        }
    }

    if (BookType == ST_MAGERY)
    {
        Add(new CGUIButton(ID_GSB_BUTTON_CIRCLE_1_2, 0x08B1, 0x08B1, 0x08B1, 58, 175));
        Add(new CGUIButton(ID_GSB_BUTTON_CIRCLE_1_2, 0x08B2, 0x08B2, 0x08B2, 93, 175));
        Add(new CGUIButton(ID_GSB_BUTTON_CIRCLE_3_4, 0x08B3, 0x08B3, 0x08B3, 130, 175));
        Add(new CGUIButton(ID_GSB_BUTTON_CIRCLE_3_4, 0x08B4, 0x08B4, 0x08B4, 164, 175));
        Add(new CGUIButton(ID_GSB_BUTTON_CIRCLE_5_6, 0x08B5, 0x08B5, 0x08B5, 227, 175));
        Add(new CGUIButton(ID_GSB_BUTTON_CIRCLE_5_6, 0x08B6, 0x08B6, 0x08B6, 260, 175));
        Add(new CGUIButton(ID_GSB_BUTTON_CIRCLE_7_8, 0x08B7, 0x08B7, 0x08B7, 297, 175));
        Add(new CGUIButton(ID_GSB_BUTTON_CIRCLE_7_8, 0x08B8, 0x08B8, 0x08B8, 332, 175));
    }

    PageCount = indexPagesCount + m_SpellCount;
    int page = 0;
    for (; page < indexPagesCount; page++)
    {
        Add(new CGUIPage(page));
        if (page == 0 && BookType == ST_CHIVALRY)
        {
            m_TithingPointsText = (CGUIText *)Add(new CGUIText(0x0288, 62, 162));
            astr_t textData = "Tithing points\nAvailable: " + str_from(g_Player->TithingPoints);
            m_TithingPointsText->CreateTextureA(6, textData);
        }

        int indexX = 106;
        int dataX = 62;
        int y = 0;
        int spellSerial = ID_GSB_SPELL_ICON_LEFT;
        if (page % 2 == 1)
        {
            indexX = 269;
            dataX = 225;
            spellSerial = ID_GSB_SPELL_ICON_RIGHT;
        }

        CGUIText *text = (CGUIText *)Add(new CGUIText(0x0288, indexX, 10));
        text->CreateTextureA(6, "INDEX");
        if (BookType == ST_MASTERY)
        {
            if (page % 2 == 1)
            {
                text = (CGUIText *)Add(new CGUIText(0x0288, dataX, 30));
                text->CreateTextureA(6, "Abilities");
                // TODO: active masteries
                continue;
            }
            else if (page % 4 == 2)
            {
                spellSerial = ID_GSB_SPELL_ICON_RIGHT;
            }
        }

        if (BookType == ST_MAGERY)
        {
            text = (CGUIText *)Add(new CGUIText(0x0288, dataX, 30));
            text->CreateTextureA(6, s_SpellCircleName[page]);
        }
        else if (BookType == ST_MASTERY)
        {
            text = (CGUIText *)Add(new CGUIText(0x0288, dataX, 30));
            text->CreateTextureA(6, page == indexPagesCount ? "Passive" : "Activated");

            for (int i = 0; i < spellsOnPage; i++)
            {
                const int currentSpellIndex = m_SpellIndices8[page / 2][i] - 1;
                if (currentSpellIndex < 0 || m_Spells[currentSpellIndex] == 0)
                    continue;

                astr_t magicWord;
                astr_t reagents;
                auto name = GetSpellName(currentSpellIndex, magicWord, reagents);
                // FIXME: hack to crop the text
                if (currentSpellIndex == 14)
                {
                    name = "Enchanted Summ..";
                }

                const int serial = spellSerial + currentSpellIndex;
                const int tag = currentSpellIndex + 1;
                auto box = (CGUIHitBox *)Add(new CGUIHitBox(serial, dataX, 52 + y, 100, 16, true));
                auto entry = (CGUITextEntry *)Add(
                    new CGUITextEntry(serial, 0x0288, 0, 0, dataX, 52 + y, 0, false, 9));
                entry->m_Entry.SetTextA(name);
                entry->CheckOnSerial = true;
                entry->ReadOnly = true;
                box->MoveOnDrag = true;
                y += 15;
            }
            continue;
        }

        for (int i = 0; i < spellsOnPage; i++)
        {
            const int currentSpellIndex = (page * spellsOnPage) + i;
            if (m_Spells[currentSpellIndex] == 0)
                continue;

            astr_t magicWord;
            astr_t reagents;
            const auto name = GetSpellName(currentSpellIndex, magicWord, reagents);
            const int serial = spellSerial + currentSpellIndex;
            auto box = (CGUIHitBox *)Add(new CGUIHitBox(serial, dataX, 52 + y, 100, 16, true));
            auto entry = (CGUITextEntry *)Add(
                new CGUITextEntry(serial, 0x0288, 0, 0, dataX, 52 + y, 0, false, 9));
            entry->m_Entry.SetTextA(name);
            entry->CheckOnSerial = true;
            entry->ReadOnly = true;
            box->MoveOnDrag = true;
            y += 15;
        }
    }

    const int topTextY = BookType == ST_MAGERY ? 10 : 6;
    const bool haveReagents = BookType <= ST_NECROMANCY || BookType == ST_MYSTICISM;
    const bool hasMagicWords = BookType != ST_BUSHIDO && BookType != ST_NINJITSU;
    for (int spell = 0; spell < maxSpellsCount; spell++)
    {
        if (m_Spells[spell] == 0u)
        {
            continue;
        }

        int iconX = 62;
        int topTextX = 87;
        int iconTextX = 112;
        uint32_t iconSerial = ID_GSB_SPELL_ICON_LEFT + spell;
        if (page % 2 == 1) // page on the right
        {
            iconX = 225;
            topTextX = 244;
            iconTextX = 275;
            iconSerial = ID_GSB_SPELL_ICON_RIGHT + spell;
        }
        Add(new CGUIPage(page));
        page++;

        astr_t magicWord;
        astr_t reagents;
        auto spellName = GetSpellName(spell, magicWord, reagents);
        auto text = (CGUIText *)Add(new CGUIText(0x0288, topTextX, topTextY));
        if (BookType == ST_MAGERY || BookType == ST_MASTERY)
        {
            const auto title = BookType == ST_MAGERY ? s_SpellCircleName[spell / 8] :
                                                       MasterGroupNameById(spell + 1);
            text->CreateTextureA(6, title);
            text = (CGUIText *)Add(new CGUIText(0x0288, iconTextX, 34));
            text->CreateTextureA(6, spellName, 80);
            if (magicWord[0] != 0)
            {
                int magicWordY = (text->m_Texture.Height < 24) ? 31 : 26;
                magicWordY += text->m_Texture.Height;
                text = (CGUIText *)Add(new CGUIText(0x0288, iconTextX, magicWordY));
                const auto font = BookType == ST_MAGERY ? 8 /* runes */ : 6;
                text->CreateTextureA(font, magicWord, 80);
            }
        }
        else
        {
            text->CreateTextureA(6, spellName);
            if (hasMagicWords)
            {
                text = (CGUIText *)Add(new CGUIText(0x0288, iconTextX, 34));
                text->CreateTextureA(6, magicWord, 80);
            }
        }

        if (BookType == ST_MASTERY && iconStartGraphic + spell > 0x094a &&
            iconStartGraphic < 0x9b8b - spell)
        {
            iconStartGraphic = 0x9b8b - spell; // FIXME: icon fixup
        }
        auto icon = (CGUIGumppic *)Add(new CGUIGumppic(iconStartGraphic + spell, iconX, 40));
        icon->Serial = iconSerial;
        if (haveReagents || reagents[0])
        {
            Add(new CGUIGumppicTiled(0x0835, iconX, 88, 120, 0));
            text = (CGUIText *)Add(new CGUIText(0x0288, iconX, 92));
            text->CreateTextureA(6, "Reagents:");
            text = (CGUIText *)Add(new CGUIText(0x0288, iconX, 114));
            text->CreateTextureA(9, reagents);
        }

        if (BookType != ST_MAGERY)
        {
            int requriesY = 0;
            auto requries = GetSpellRequries(spell, requriesY);
            text = (CGUIText *)Add(new CGUIText(0x0288, iconX, requriesY));
            text->CreateTextureA(6, requries);
        }
    }

    Add(new CGUIPage(-1));
    m_PrevPage =
        (CGUIButton *)Add(new CGUIButton(ID_GSB_BUTTON_PREV, 0x08bb, 0x08bb, 0x08bb, 50, 8));
    m_PrevPage->Visible = (Page != 0);
    m_NextPage =
        (CGUIButton *)Add(new CGUIButton(ID_GSB_BUTTON_NEXT, 0x08bc, 0x08bc, 0x08bc, 321, 8));
    m_NextPage->Visible = (Page + 2 < PageCount);
    m_LastSpellBookmark = (CGUIGumppic *)Add(new CGUIGumppic(0x08ad, 184, 2));
    m_LastSpellPointer = (CGUIGumppic *)Add(new CGUIGumppic(0x08af, 184, 52));
}

void CGumpSpellbook::GUMP_BUTTON_EVENT_C
{
    int newPage = -1;
    if (serial == ID_GSB_BUTTON_PREV)
    {
        if (Page > 0)
        {
            newPage = Page - 2;
            if (newPage < 0)
            {
                newPage = 0;
            }
        }
    }
    else if (serial == ID_GSB_BUTTON_NEXT)
    {
        if (Page < PageCount)
        {
            newPage = Page + 2;
            if (newPage >= PageCount)
            {
                newPage = PageCount - 1;
            }
        }
    }
    else if (serial == ID_GSB_BUTTON_CIRCLE_1_2)
    {
        ChangePage(0);
    }
    else if (serial == ID_GSB_BUTTON_CIRCLE_3_4)
    {
        ChangePage(2);
    }
    else if (serial == ID_GSB_BUTTON_CIRCLE_5_6)
    {
        ChangePage(4);
    }
    else if (serial == ID_GSB_BUTTON_CIRCLE_7_8)
    {
        ChangePage(6);
    }
    else if (serial == ID_GSB_BUTTON_MINIMIZE)
    {
        Minimized = true;
        WantUpdateContent = true;
    }
    else if (serial == ID_GSB_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
    else if (serial >= ID_GSB_SPELL_ICON_LEFT)
    {
        int maxSpellsCount = 0;
        int indexPagesCount = 0;
        int spellsOnPage = 0;
        int spellIndexOffset = 0;
        uint16_t graphic = 0;
        uint16_t minimizedGraphic = 0;
        uint16_t iconStartGraphic = 0;

        GetSummaryBookInfo(
            maxSpellsCount,
            indexPagesCount,
            spellsOnPage,
            spellIndexOffset,
            graphic,
            minimizedGraphic,
            iconStartGraphic);

        if (Page < indexPagesCount)
        {
            for (int j = 0; j < 2; j++)
            {
                for (int i = 0; i < spellsOnPage; i++)
                {
                    const int offset = j + (BookType == ST_MASTERY ? Page / 2 : Page);
                    const int spell = BookType == ST_MASTERY ? m_SpellIndices8[Page / 2][i] - 1 :
                                                               i + (spellsOnPage * offset);
                    if (spell >= 0 && m_Spells[spell] == 0)
                    {
                        continue;
                    }

                    if (serial == spell + ID_GSB_SPELL_ICON_LEFT ||
                        serial == spell + ID_GSB_SPELL_ICON_RIGHT)
                    {
                        int c = indexPagesCount;
                        for (int k = 0; k < maxSpellsCount; k++)
                        {
                            if (m_Spells[k] != 0)
                            {
                                if (k == spell)
                                {
                                    break;
                                }
                                c++;
                            }
                        }
                        newPage = c;
                        break;
                    }
                }
            }
        }
    }

    if (newPage > -1 && !g_ClickObject.Enabled)
    {
        if (newPage % 2 == 1)
        {
            newPage--;
        }

        g_ClickObject.Init(g_PressedObject.LeftObject, this);
        g_ClickObject.Timer = g_Ticks + g_MouseManager.DoubleClickDelay;
        g_ClickObject.Page = newPage;
    }
}

void CGumpSpellbook::GUMP_TEXT_ENTRY_EVENT_C
{
    if (serial >= ID_GSB_SPELL_ICON_LEFT)
    {
        OnButton(serial);
    }
}

bool CGumpSpellbook::OnLeftMouseButtonDoubleClick()
{
    bool result = false;

    if (Minimized)
    {
        Minimized = false;
        WantUpdateContent = true;
        result = true;
    }
    else
    {
        if (g_PressedObject.LeftSerial == ID_GSB_BUTTON_PREV)
        {
            ChangePage(0);
            WantRedraw = true;
            result = true;
        }
        else if (g_PressedObject.LeftSerial == ID_GSB_BUTTON_NEXT)
        {
            int newPage = PageCount - 1;
            if (newPage % 2 == 1)
            {
                newPage--;
            }
            ChangePage(newPage);
            WantRedraw = true;
            result = true;
        }
        else
        {
            if (g_PressedObject.LeftSerial >= ID_GSB_SPELL_ICON_LEFT)
            {
                int spellIndex = g_PressedObject.LeftSerial - ID_GSB_SPELL_ICON_RIGHT + 1;
                if (g_PressedObject.LeftSerial < ID_GSB_SPELL_ICON_RIGHT)
                {
                    spellIndex = g_PressedObject.LeftSerial - ID_GSB_SPELL_ICON_LEFT + 1;
                }
                spellIndex += BookType == ST_MYSTICISM ? 677 : int(BookType) * 100;
                if (g_Config.ClientVersion < CV_308Z)
                {
                    g_Game.CastSpellFromBook(spellIndex, Serial);
                }
                else
                {
                    g_Game.CastSpell(spellIndex);
                }
                Minimized = true;
                WantUpdateContent = true;
                result = true;
            }
        }
    }

    return result;
}

void CGumpSpellbook::DelayedClick(CRenderObject *obj)
{
    if (obj != nullptr)
    {
        ChangePage(g_ClickObject.Page);
        WantRedraw = true;
    }
}

void CGumpSpellbook::ChangePage(int newPage)
{
    Page = newPage;
    m_PrevPage->Visible = (Page != 0);
    m_NextPage->Visible = (Page + 2 < PageCount);
    g_Game.PlaySoundEffect(0x0055);
}

astr_t CGumpSpellbook::m_SpellName1[][2] = { { "Clumsy", "U J" },
                                             { "Create Food", "I M Y" },
                                             { "Feeblemind", "R W" },
                                             { "Heal", "I M" },
                                             { "Magic Arrow", "I P Y" },
                                             { "Night Sight", "I L" },
                                             { "Reactive Armor", "F S" },
                                             { "Weaken", "D M" },
                                             { "Agility", "E U" },
                                             { "Cunning", "U W" },
                                             { "Cure", "A N" },
                                             { "Harm", "A M" },
                                             { "Magic Trap", "I J" },
                                             { "Magic Untrap", "A J" },
                                             { "Protection", "U S" },
                                             { "Strength", "U M" },
                                             { "Bless", "R S" },
                                             { "Fireball", "V F" },
                                             { "Magic Lock", "A P" },
                                             { "Poison", "I N" },
                                             { "Telekinesis", "O P Y" },
                                             { "Teleport", "R P" },
                                             { "Unlock", "E P" },
                                             { "Wall of Stone", "I S Y" },
                                             { "Arch Cure", "V A N" },
                                             { "Arch Protection", "V U S" },
                                             { "Curse", "D S" },
                                             { "Fire Field", "I F G" },
                                             { "Greater Heal", "I V M" },
                                             { "Lightning", "P O G" },
                                             { "Mana Drain", "O R" },
                                             { "Recall", "K O P" },
                                             { "Blade Spirits", "I H J Y" },
                                             { "Dispel Field", "A G" },
                                             { "Incognito", "K I E" },
                                             { "Magic Reflection", "I J S" },
                                             { "Mind Blast", "P C W" },
                                             { "Paralyze", "A E P" },
                                             { "Poison Field", "I N G" },
                                             { "Summ. Creature", "K X" },
                                             { "Dispel", "A O" },
                                             { "Energy Bolt", "C P" },
                                             { "Explosion", "V O F" },
                                             { "Invisibility", "A L X" },
                                             { "Mark", "K P Y" },
                                             { "Mass Curse", "V D S" },
                                             { "Paralyze Field", "I E G" },
                                             { "Reveal", "W Q" },
                                             { "Chain Lightning", "V O G" },
                                             { "Energy Field", "I S G" },
                                             { "Flame Strike", "K V F" },
                                             { "Gate Travel", "V R P" },
                                             { "Mana Vampire", "O S" },
                                             { "Mass Dispel", "V A O" },
                                             { "Meteor Swarm", "F K D Y" },
                                             { "Polymorph", "V Y R" },
                                             { "Earthquake", "I V P" },
                                             { "Energy Vortex", "V C P" },
                                             { "Resurrection", "A C" },
                                             { "Air Elemental", "K V X H" },
                                             { "Summon Daemon", "K V X C" },
                                             { "Earth Elemental", "K V X Y" },
                                             { "Fire Elemental", "K V X F" },
                                             { "Water Elemental", "K V X A" } };

astr_t CGumpSpellbook::m_SpellReagents1[SPELLBOOK_1_SPELLS_COUNT] = {
    "Bloodmoss\nNightshade",
    "Garlic\nGinseng\nMandrake root",
    "Ginseng\nNightshade",
    "Garlic\nGinseng\nSpiders silk",
    "Black pearl\nNightshade",
    "Sulfurous ash\nSpiders silk",
    "Garlic\nSulfurous ash\nSpiders silk",
    "Garlic\nNightshade",
    "Bloodmoss\nMandrake root",
    "Mandrake root\nNightshade",
    "Garlic\nGinseng",
    "Nightshade\nSpiders silk",
    "Garlic\nSulfurous ash\nSpiders silk",
    "Bloodmoss\nSulfurous ash",
    "Garlic\nGinseng\nSulfurous ash",
    "Mandrake root\nNightshade",
    "Garlic\nMandrake root",
    "Black pearl\nSulfurous ash",
    "Sulfurous ash\nBloodmoss\nGarlic",
    "Nightshade",
    "Bloodmoss\nMandrake root",
    "Bloodmoss\nMandrake root",
    "Bloodmoss\nSulfurous ash",
    "Bloodmoss\nGarlic",
    "Garlic\nGinseng\nMandrake root",
    "Garlic\nGinseng\nMandrake root\nSulfurous ash",
    "Garlic\nNightshade\nSulfurous ash",
    "Black pearl\nSpiders silk\nSulfurous ash",
    "Garlic\nGinseng\nMandrake root\nSpiders silk",
    "Black pearl\nMandrake root\nSulfurous ash",
    "Black pearl\nMandrake root\nSpiders silk",
    "Black pearl\nBloodmoss\nMandrake root",
    "Black pearl\nMandrake root\nNightshade",
    "Garlic\nBlack pearl\nSpiders silk\nSulfurous ash",
    "Bloodmoss\nGarlic\nNightshade",
    "Garlic\nMandrake root\nSpiders silk",
    "Black pearl\nMandrake root\nNightshade\nSulfurous ash",
    "Garlic\nMandrake root\nSpiders silk",
    "Black pearl\nNightshade\nSpiders silk",
    "Bloodmoss\nMandrake root\nSpiders silk",
    "Garlic\nMandrake root\nSulfurous ash",
    "Black pearl\nNightshade",
    "Black pearl\nMandrake root\nSulfurous ash",
    "Bloodmoss\nNightshade",
    "Black pearl\nBloodmoss\nMandrake root",
    "Garlic\nMandrake root\nNightshade\nSulfurous ash",
    "Black pearl\nGinseng\nSpiders silk",
    "Bloodmoss\nSulfurous ash",
    "Black pearl\nMandrake root\nBloodmoss\nSulfurous ash",
    "Black pearl\nMandrake root\nSpiders silk\nSulfurous ash",
    "Spiders silk\nSulfurous ash",
    "Black pearl\nMandrake root\nSulfurous ash",
    "Black pearl\nBloodmoss\nMandrake root\nSpiders silk",
    "Black pearl\nGarlic\nMandrake root\nSulfurous ash",
    "Bloodmoss\nSpiders silk\nMandrake root\nSulfurous ash",
    "Bloodmoss\nMandrake root\nSpiders silk",
    "Bloodmoss\nGinseng\nMandrake root\nSulfurous ash",
    "Black pearl\nBloodmoss\nMandrake root\nNightshade",
    "Bloodmoss\nGarlic\nGinseng",
    "Bloodmoss\nMandrake root\nSpiders silk",
    "Bloodmoss\nMandrake root\nSpiders silk\nSulfurous ash",
    "Bloodmoss\nMandrake root\nSpiders silk",
    "Bloodmoss\nMandrake root\nSpiders silk\nSulfurous ash",
    "Bloodmoss\nMandrake root\nSpiders silk"
};

const astr_t CGumpSpellbook::m_SpellName2[SPELLBOOK_2_SPELLS_COUNT][2]{
    { "Animate Dead", "Uus Corp" },
    { "Blood Oath", "In Jux Mani Xen" },
    { "Corpse Skin", "In Aglo Corp Ylem" },
    { "Curse Weapon", "An Sanct Gra Char" },
    { "Evil Omen", "Pas Tym An Sanct" },
    { "Horrific Beast", "Rel Xen Vas Bal" },
    { "Lich Form", "Rel Xen Corp Ort" },
    { "Mind Rot", "Wis An Ben" },
    { "Pain Spike", "In Sar" },
    { "Poison Strike", "In Vas Nox" },
    { "Strangle", "In Bal Nox" },
    { "Summon Familiar", "Kal Xen Bal" },
    { "Vampiric Embrace", "Rel Xen An Sanct" },
    { "Vengeful Spirit", "Kal Xen Bal Beh" },
    { "Wither", "Kal Vas An Flam" },
    { "Wraith Form", "Rel Xen Um" },
    { "Exorcism", "Ort Corp Grav" }
};

const astr_t CGumpSpellbook::m_SpellReagents2[SPELLBOOK_2_SPELLS_COUNT] = {
    "Daemon Blood\nGrave Dust",
    "Daemon Blood",
    "Batwing\nGrave Dust",
    "Pig Iron",
    "Batwing\nNox Crystal",
    "Batwing\nDaemon Blood",
    "Nox Crystal\nDaemon Blood\nGrave Dust",
    "Batwing\nDaemon Blood\nPig Iron",
    "Grave Dust\nPig Iron",
    "Nox Crystal",
    "Nox Crystal\nDaemon Blood",
    "Batwing\nGrave Dust\nDaemon Blood",
    "Batwing\nNox Crystal\nPig Iron",
    "Batwing\nGrave Dust\nPig Iron",
    "Nox Crystal\nGrave Dust\nPig Iron",
    "Nox Crystal\nPig Iron",
    "Nox Crystal\nGrave Dust"
};

const astr_t CGumpSpellbook::m_SpellName3[SPELLBOOK_3_SPELLS_COUNT][2]{
    { "Cleanse by Fire", "Expor Flamus" },     { "Close Wounds", "Obsu Vulni" },
    { "Consecrate Weapon", "Consecrus Arma" }, { "Dispel Evil", "Dispiro Malas" },
    { "Divine Fury", "Divinum Furis" },        { "Enemy of One", "Forul Solum" },
    { "Holy Light", "Augus Luminos" },         { "Noble Sacrifice", "Dium Prostra" },
    { "Remove Curse", "Extermo Vomica" },      { "Sacred Journey", "Sanctum Viatas" }
};

const astr_t CGumpSpellbook::m_SpellName4[SPELLBOOK_4_SPELLS_COUNT]{
    "Honorable Execution", "Confidence",       "Evasion",
    "Counter Attack",      "Lightning Strike", "Momentum Strike"
};

const astr_t CGumpSpellbook::m_SpellName5[SPELLBOOK_5_SPELLS_COUNT]{
    "Focus Attack",    "Death Strike", "Animal Form", "Ki Attack",
    "Surprise Attack", "Backstab",     "Shadowjump",  "Mirror Image"
};

const astr_t CGumpSpellbook::m_SpellName6[SPELLBOOK_6_SPELLS_COUNT][2]{
    { "Arcane Circle", "Myrshalee" },     { "Gift of Renewal", "Olorisstra" },
    { "Immolating Weapon", "Thalshara" }, { "Attunement", "Haeldril" },
    { "Thunderstorm", "Erelonia" },       { "Nature's Fury", "Rauvvrae" },
    { "Summon Fey", "Alalithra" },        { "Summon Fiend", "Nylisstra" },
    { "Reaper Form", "Tarisstree" },      { "Wildfire", "Haelyn" },
    { "Essence of Wind", "Anathrae" },    { "Dryad Allure", "Rathril" },
    { "Ethereal Voyage", "Orlavdra" },    { "Word of Death", "Nyraxle" },
    { "Gift of Life", "Illorae" },        { "Arcane Empowerment", "Aslavdra" }
};

const astr_t CGumpSpellbook::m_SpellName7[SPELLBOOK_7_SPELLS_COUNT][3]{
    { "Nether Bolt", "In Corp Ylem", "Black pearl\nSulfurous ash" },
    { "Healing Stone", "Kal In Mani", "Bone\nGarlic\nGinseng\nSpiders silk" },
    { "Purge Magic", "An Ort Sanct", "Fertile dirt\nGarlic\nMandrake root\nSulfurous ash" },
    { "Enchant", "In Ort Ylem", "Spider silk\nMandrake root\nSulfurous ash" },
    { "Sleep", "In Zu", "Nightshade\nSpider silk\nBlack pearl" },
    { "Eagle Strike", "Kal Por Xen", "Bloodmoss\nBone\nMandrake root\nSpiders silk" },
    { "Animated Weapon", "In Jux Por Ylem", "Bone\nBlack pearl\nMandrake root\nNightshade" },
    { "Stone Form", "In Rel Ylem", "Bloodmoss\nFertile dirt\nGarlic" },
    { "Spell Trigger", "In Vas Ort Ex", "Dragon's blood\nGarlic\nMandrake root\nSpiders silk" },
    { "Mass Sleep", "Vas Zu", "Gingeng\nNightshade\nSpiders silk" },
    { "Cleansing Winds", "In Vas Mani Hur", "Dragon's blood\nGarlic\nGinseng\nMandrake root" },
    { "Bombard", "Corp Por Ylem", "Bloodmoss\nDragon's blood\nNightshade\nSulfurous ash" },
    { "Spell Plague", "Vas Rel Jux Ort", "Daemon bone\nDragon's blood\nNightshade\nSulfurous ash" },
    { "Hail Storm", "Kal Des Ylem", "Dragon's blood\nBlack pearl\nBloodmoss\nMandrake root" },
    { "Nether Cyclone", "Grav Hur", "Mandrake root\nNightshade\nSulfurous ash\nBloodmoss" },
    { "Rising Colossus",
      "Kal Vas Xen Corp Ylem",
      "Daemon bone\nDragon's blood\nFertile dirt\nNightshade" },
};

const astr_t CGumpSpellbook::m_SpellName8[SPELLBOOK_8_SPELLS_COUNT][3]{
    { "Inspire", "Uus Por", "" },
    { "Invigorate", "An Zu", "" },
    { "Resilience", "Kal Mani Tym", "" },
    { "Perseverance", "Uus Jux Sanct", "" },
    { "Tribulation", "In Jux Hur Rel", "" },
    { "Despair", "Kal Des Mani Tym", "" },
    { "Death Ray", "In Grav Corp", "Black pearl\nBloodmoss\nSpiders silk" },
    { "Ethereal Burst", "Uus Ort Grav", "Bloodmoss\nGinseng\nMandrake root" },
    { "Nether Blast", "In Vas Xen Por", "Dragon's blood\nDaemon bone" },
    { "Mystic Weapon", "Vas Ylem Wis", "Fertile dirt\nBone" },
    { "Command Undead", "In Corp Xen Por", "Daemon blood\nPig iron\nBatwing" },
    { "Conduit", "Uus Corp Grav", "Nox crystal\nBatwing\nGrave dust" },
    { "Mana Shield", "Faerkulggen", "" },
    { "Summon Reaper", "Lartarisstree", "" },
    { "Enchanted Summoning", "", "" },
    { "Anticipate Hit", "", "" },
    { "Warcry", "", "" },
    { "Intuition", "", "" },
    { "Rejuvenate", "", "" },
    { "Holy Fist", "", "" },
    { "Shadow", "", "" },
    { "White Tiger Form", "", "" },
    { "Flaming Shot", "", "" },
    { "Playing The Odds", "", "" },
    { "Thrust", "", "" },
    { "Pierce", "", "" },
    { "Stagger", "", "" },
    { "Toughness", "", "" },
    { "Onslaught", "", "" },
    { "Focused Eye", "", "" },
    { "Elemental Fury", "", "" },
    { "Called Shot", "", "" },
    { "Warrior's Gifts", "", "" },
    { "Shield Bash", "", "" },
    { "Bodyguard", "", "" },
    { "Highten Senses", "", "" },
    { "Tolerance", "", "" },
    { "Injected Strike", "", "" },
    { "Potency", "", "" },
    { "Rampage", "", "" },
    { "Fists of Fury", "", "" },
    { "Knockout", "", "" },
    { "Whispering", "", "" },
    { "Combat Training", "", "" },
    { "Boarding", "", "" },
};

const SPELL_REQURIES CGumpSpellbook::m_SpellRequries2[SPELLBOOK_2_SPELLS_COUNT] = {
    { 23, 40 }, { 13, 20 }, { 11, 20 }, { 7, 0 },   { 11, 20 }, { 11, 40 },
    { 23, 70 }, { 17, 30 }, { 5, 20 },  { 17, 50 }, { 29, 65 }, { 17, 30 },
    { 23, 99 }, { 41, 80 }, { 23, 60 }, { 17, 20 }, { 40, 80 }
};

const PALADIN_SPELL_REQURIES CGumpSpellbook::m_SpellRequries3[SPELLBOOK_3_SPELLS_COUNT] = {
    { 10, 5, 10 },  { 10, 0, 10 },  { 10, 15, 10 }, { 10, 35, 10 }, { 15, 25, 10 },
    { 20, 45, 10 }, { 10, 55, 10 }, { 20, 65, 30 }, { 20, 5, 10 },  { 10, 15, 15 }
};

const SPELL_REQURIES CGumpSpellbook::m_SpellRequries4[SPELLBOOK_4_SPELLS_COUNT] = {
    { 0, 25 }, { 10, 25 }, { 10, 60 }, { 5, 40 }, { 10, 50 }, { 10, 70 }
};

const SPELL_REQURIES CGumpSpellbook::m_SpellRequries5[SPELLBOOK_5_SPELLS_COUNT] = {
    { 10, 30 }, { 30, 85 }, { 10, 0 }, { 25, 80 }, { 20, 60 }, { 30, 40 }, { 15, 50 }, { 10, 20 }
};

const SPELL_REQURIES CGumpSpellbook::m_SpellRequries6[SPELLBOOK_6_SPELLS_COUNT] = {
    { 20, 0 },  { 24, 0 },  { 32, 10 }, { 24, 0 },  { 32, 10 }, { 24, 0 },  { 10, 38 }, { 10, 38 },
    { 34, 24 }, { 50, 66 }, { 40, 52 }, { 40, 52 }, { 32, 24 }, { 50, 23 }, { 70, 38 }, { 50, 24 }
};

const SPELL_REQURIES CGumpSpellbook::m_SpellRequries7[SPELLBOOK_7_SPELLS_COUNT] = {
    { 4, 0 },   { 4, 0 },   { 6, 8 },   { 6, 8 },   { 8, 20 },  { 9, 20 },  { 11, 33 }, { 11, 33 },
    { 14, 45 }, { 14, 45 }, { 20, 58 }, { 20, 58 }, { 40, 70 }, { 50, 70 }, { 50, 83 }, { 50, 83 }
};

const PALADIN_SPELL_REQURIES CGumpSpellbook::m_SpellRequries8[SPELLBOOK_8_SPELLS_COUNT] = {
    { 16, 90, 4 },  { 22, 90, 5 },  { 16, 90, 4 }, { 18, 90, 5 }, { 24, 90, 10 }, { 26, 90, 12 },
    { 50, 90, 35 }, { 0, 90, 0 },   { 40, 90, 0 }, { 40, 90, 0 }, { 40, 90, 0 },  { 40, 90, 0 },
    { 40, 90, 0 },  { 50, 90, 0 },  { 0, 90, 0 },  { 10, 90, 0 }, { 40, 90, 0 },  { 0, 90, 0 },
    { 10, 90, 35 }, { 50, 90, 35 }, { 10, 90, 4 }, { 10, 90, 0 }, { 30, 90, 0 },  { 25, 90, 0 },
    { 30, 90, 20 }, { 20, 90, 0 },  { 20, 90, 0 }, { 20, 90, 0 }, { 20, 90, 0 },  { 20, 90, 0 },
    { 20, 90, 0 },  { 40, 90, 0 },  { 50, 90, 0 }, { 50, 90, 0 }, { 40, 90, 0 },  { 10, 90, 10 },
    { 20, 90, 0 },  { 30, 90, 0 },  { 0, 90, 0 },  { 20, 90, 0 }, { 20, 90, 0 },  { 0, 90, 0 },
    { 40, 90, 0 },  { 40, 90, 0 },  { 0, 90, 0 },
};

const int CGumpSpellbook::m_SpellIndices8[SPELLBOOK_8_SPELLS_COUNT][8] = {
    { 1, 2, 3, 4, 5, 6, 7, 8 },        { 9, 10, 11, 12, 13, 14, 19, 20 },
    { 17, 21, 22, 25, 34, 35, 36, 0 }, { 27, 28, 29, 32, 37, 38, 40, 41 },
    { 23, 24, 30, 31, 43, 44, 0, 0 },  { 15, 16, 18, 33, 39, 42, 45, 0 },
};