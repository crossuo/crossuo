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

CGumpSpellbook::CGumpSpellbook(uint32_t serial, int x, int y)
    : CGump(GT_SPELLBOOK, serial, x, y)
{
    Draw2Page = 1;
    memset(&m_Spells[0], 0, sizeof(m_Spells));
}

CGumpSpellbook::~CGumpSpellbook()
{
}

void CGumpSpellbook::InitStaticData()
{
    // TODO ???
    return;

#if 0
    s_SpellCircleName[0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028384, false, "First Circle");
    s_SpellCircleName[1] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028385, false, "Second Circle");
    s_SpellCircleName[2] = g_ClilocManager.Cliloc(g_Language)->GetA(1028386, false, "Third Circle");
    s_SpellCircleName[3] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028387, false, "Fourth Circle");
    s_SpellCircleName[4] = g_ClilocManager.Cliloc(g_Language)->GetA(1028388, false, "Fifth Circle");
    s_SpellCircleName[5] = g_ClilocManager.Cliloc(g_Language)->GetA(1028389, false, "Sixth Circle");
    s_SpellCircleName[6] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028390, false, "Seventh Circle");
    s_SpellCircleName[7] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028391, false, "Eighth Circle");

    m_SpellName1[0][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028320, false, "Clumsy");
    m_SpellName1[1][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028321, false, "Create Food");
    m_SpellName1[2][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028322, false, "Feeblemind");
    m_SpellName1[3][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028323, false, "Heal");
    m_SpellName1[4][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028324, false, "Magic Arrow");
    m_SpellName1[5][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028325, false, "Night Sight");
    m_SpellName1[6][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028326, false, "Reactive Armor");
    m_SpellName1[7][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028327, false, "Weaken");
    m_SpellName1[8][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028328, false, "Agility");
    m_SpellName1[9][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028329, false, "Cunning");
    m_SpellName1[10][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028330, false, "Cure");
    m_SpellName1[11][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028331, false, "Harm");
    m_SpellName1[12][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028332, false, "Magic Trap");
    m_SpellName1[13][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028333, false, "Magic Untrap");
    m_SpellName1[14][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028334, false, "Protection");
    m_SpellName1[15][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028335, false, "Strength");
    m_SpellName1[16][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028336, false, "Bless");
    m_SpellName1[17][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028337, false, "Fireball");
    m_SpellName1[18][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028338, false, "Magic Lock");
    m_SpellName1[19][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028339, false, "Poison");
    m_SpellName1[20][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028340, false, "Telekinesis");
    m_SpellName1[21][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028341, false, "Teleport");
    m_SpellName1[22][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028342, false, "Unlock");
    m_SpellName1[23][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028343, false, "Wall of Stone");
    m_SpellName1[24][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028344, false, "Arch Cure");
    m_SpellName1[25][0] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028345, false, "Arch Protection");
    m_SpellName1[26][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028346, false, "Curse");
    m_SpellName1[27][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028347, false, "Fire Field");
    m_SpellName1[28][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028348, false, "Greater Heal");
    m_SpellName1[29][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028349, false, "Lightning");
    m_SpellName1[30][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028350, false, "Mana Drain");
    m_SpellName1[31][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028351, false, "Recall");
    m_SpellName1[32][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028352, false, "Blade Spirits");
    m_SpellName1[33][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028353, false, "Dispel Field");
    m_SpellName1[34][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028354, false, "Incognito");
    m_SpellName1[35][0] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028355, false, "Magic Reflection");
    m_SpellName1[36][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028356, false, "Mind Blast");
    m_SpellName1[37][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028357, false, "Paralyze");
    m_SpellName1[38][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028358, false, "Poison Field");
    m_SpellName1[39][0] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028359, false, "Summ. Creature");
    m_SpellName1[40][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028360, false, "Dispel");
    m_SpellName1[41][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028361, false, "Energy Bolt");
    m_SpellName1[42][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028362, false, "Explosion");
    m_SpellName1[43][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028363, false, "Invisibility");
    m_SpellName1[44][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028364, false, "Mark");
    m_SpellName1[45][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028365, false, "Mass Curse");
    m_SpellName1[46][0] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028366, false, "Paralyze Field");
    m_SpellName1[47][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028367, false, "Reveal");
    m_SpellName1[48][0] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028368, false, "Chain Lightning");
    m_SpellName1[49][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028369, false, "Energy Field");
    m_SpellName1[50][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028370, false, "Flame Strike");
    m_SpellName1[51][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028371, false, "Gate Travel");
    m_SpellName1[52][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028372, false, "Mana Vampire");
    m_SpellName1[53][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028373, false, "Mass Dispel");
    m_SpellName1[54][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028374, false, "Meteor Swarm");
    m_SpellName1[55][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028375, false, "Polymorph");
    m_SpellName1[56][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028376, false, "Earthquake");
    m_SpellName1[57][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028377, false, "Energy Vortex");
    m_SpellName1[58][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028378, false, "Resurrection");
    m_SpellName1[59][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028379, false, "Air Elemental");
    m_SpellName1[60][0] = g_ClilocManager.Cliloc(g_Language)->GetA(1028380, false, "Summon Daemon");
    m_SpellName1[61][0] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028381, false, "Earth Elemental");
    m_SpellName1[62][0] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028382, false, "Fire Elemental");
    m_SpellName1[63][0] =
        g_ClilocManager.Cliloc(g_Language)->GetA(1028383, false, "Water Elemental");
#endif // 0
}

void CGumpSpellbook::UpdateGraphic(uint16_t parentGraphic)
{
    SPELLBOOK_TYPE bookType = BookType;

    switch (parentGraphic)
    {
        case 0x0efa:
        {
            BookType = ST_MAGE;
            break;
        }
        case 0x2253:
        {
            BookType = ST_NECRO;
            break;
        }
        case 0x2252:
        {
            BookType = ST_PALADIN;
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
            BookType = ST_SPELL_WEAVING;
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
            BookType = ST_MAGE;
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

    int dictionaryPagesCount = 0;
    int tooltipOffset = 0;
    GetTooltipBookInfo(dictionaryPagesCount, tooltipOffset);
    if (Page >= dictionaryPagesCount)
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
            g_ToolTip.Set(
                g_ClilocManager.Cliloc(g_Language)->GetW(tooltipOffset + serial, true), 150);
        }
    }
}

void CGumpSpellbook::PrepareContent()
{
    int maxSpellsCount = 0;
    int spellsOnPage = 0;
    int dictionaryPagesCount = 0;
    int spellIndexOffset = 0;
    uint16_t graphic = 0;
    uint16_t minimizedGraphic = 0;
    uint16_t iconStartGraphic = 0;

    GetSummaryBookInfo(
        maxSpellsCount,
        dictionaryPagesCount,
        spellsOnPage,
        spellIndexOffset,
        graphic,
        minimizedGraphic,
        iconStartGraphic);

    if (g_PressedObject.LeftGump == this && Page >= dictionaryPagesCount &&
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
            g_GumpManager.AddGump(new CGumpSpell(
                index + spellIndexOffset + 1,
                g_MouseManager.Position.X - 20,
                g_MouseManager.Position.Y - 20,
                iconStartGraphic + index,
                BookType));
            g_GameWindow.EmulateOnLeftMouseButtonDown();
        }
    }

    if (!Minimized && m_LastSpellPointer != nullptr)
    {
        spellIndexOffset = (int)BookType * 100;
        bool wantVisible = false;
        uint16_t graphicBookmark = 0x08ad;
        uint16_t graphicPointer = 0x08af;
        int wantX = m_LastSpellPointer->GetX();
        int wantY = m_LastSpellPointer->GetY();
        if (Page < dictionaryPagesCount)
        {
            int offs = (spellsOnPage * Page);
            for (int j = 0; j < 2; j++)
            {
                int y = 0;
                for (int i = 0; i < spellsOnPage; i++)
                {
                    if (m_Spells[offs] != 0u)
                    {
                        if (offs + spellIndexOffset == g_LastSpellIndex - 1)
                        {
                            wantVisible = true;
                            wantY = 52 + y;
                            if (((Page + j) % 2) != 0)
                            {
                                graphicBookmark = 0x08AE;
                                graphicPointer = 0x08B0;
                                wantX = 203;
                            }
                            else
                            {
                                wantX = 184;
                            }
                            offs++;
                            break;
                        }
                        y += 15;
                    }
                    offs++;
                }
            }
        }
        else
        {
            int page = dictionaryPagesCount;
            for (int i = 0; i < maxSpellsCount; i++)
            {
                if (m_Spells[i] == 0u)
                {
                    continue;
                }

                if (page == Page || page == (Page + 1))
                {
                    if (i + spellIndexOffset == g_LastSpellIndex - 1)
                    {
                        wantVisible = true;
                        wantY = 40;
                        if ((page % 2) != 0)
                        {
                            graphicBookmark = 0x08AE;
                            graphicPointer = 0x08B0;
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

void CGumpSpellbook::GetTooltipBookInfo(int &dictionaryPagesCount, int &tooltipOffset)
{
    int maxSpellsCount = 0;
    switch (BookType)
    {
        case ST_MAGE:
        {
            maxSpellsCount = SPELLBOOK_1_SPELLS_COUNT;
            tooltipOffset = 1061290;
            break;
        }
        case ST_NECRO:
        {
            maxSpellsCount = SPELLBOOK_2_SPELLS_COUNT;
            tooltipOffset = 1061390;
            break;
        }
        case ST_PALADIN:
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
        case ST_SPELL_WEAVING:
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

    dictionaryPagesCount = (int)ceilf(maxSpellsCount / 8.0f);
    if ((dictionaryPagesCount % 2) != 0)
    {
        dictionaryPagesCount++;
    }
}

void CGumpSpellbook::GetSummaryBookInfo(
    int &maxSpellsCount,
    int &dictionaryPagesCount,
    int &spellsOnPage,
    int &spellIndexOffset,
    uint16_t &graphic,
    uint16_t &minimizedGraphic,
    uint16_t &iconStartGraphic)
{
    switch (BookType)
    {
        case ST_MAGE:
        {
            maxSpellsCount = SPELLBOOK_1_SPELLS_COUNT;
            graphic = 0x08aC;
            minimizedGraphic = 0x08ba;
            iconStartGraphic = 0x08c0;
            spellIndexOffset = 0;
            break;
        }
        case ST_NECRO:
        {
            maxSpellsCount = SPELLBOOK_2_SPELLS_COUNT;
            graphic = 0x2b00;
            minimizedGraphic = 0x2b03;
            iconStartGraphic = 0x5000;
            spellIndexOffset = 64;
            break;
        }
        case ST_PALADIN:
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
        case ST_SPELL_WEAVING:
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
    dictionaryPagesCount = (int)ceilf(maxSpellsCount / 8.0f);
    if ((dictionaryPagesCount % 2) != 0)
    {
        dictionaryPagesCount++;
    }
}

astr_t CGumpSpellbook::GetSpellName(int offset, astr_t &abbreviature, astr_t &reagents)
{
    switch (BookType)
    {
        case ST_MAGE:
            abbreviature = m_SpellName1[offset][1];
            reagents = m_SpellReagents1[offset];
            return m_SpellName1[offset][0];
        case ST_NECRO:
            abbreviature = m_SpellName2[offset][1];
            reagents = m_SpellReagents2[offset];
            return m_SpellName2[offset][0];
        case ST_PALADIN:
            abbreviature = m_SpellName3[offset][1];
            return m_SpellName3[offset][0];
        case ST_BUSHIDO:
            return m_SpellName4[offset];
        case ST_NINJITSU:
            return m_SpellName5[offset];
        case ST_SPELL_WEAVING:
            abbreviature = m_SpellName6[offset][1];
            return m_SpellName6[offset][0];
        case ST_MYSTICISM:
            abbreviature = m_SpellName7[offset][1];
            reagents = m_SpellName7[offset][2];
            return m_SpellName7[offset][0];
        case ST_MASTERY:
            abbreviature = m_SpellName8[offset][1];
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
        case ST_NECRO:
        {
            const SPELL_REQURIES &req = m_SpellRequries2[offset];
            manaCost = req.ManaCost;
            minSkill = req.MinSkill;
            break;
        }
        case ST_PALADIN:
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
        case ST_SPELL_WEAVING:
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
    int dictionaryPagesCount = 0;
    int spellIndexOffset = 0;
    uint16_t graphic = 0;
    uint16_t minimizedGraphic = 0;
    uint16_t iconStartGraphic = 0;

    GetSummaryBookInfo(
        maxSpellsCount,
        dictionaryPagesCount,
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

    PageCount = dictionaryPagesCount + m_SpellCount;
    //if (m_SpellCount % 2)
    //	PageCount--;
    int offs = 0;
    if (BookType == ST_MAGE)
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

    for (int page = 0; page < dictionaryPagesCount; page++)
    {
        Add(new CGUIPage((int)page));
        if (page == 0 && BookType == ST_PALADIN)
        {
            m_TithingPointsText = (CGUIText *)Add(new CGUIText(0x0288, 62, 162));
            astr_t textData = "Tithing points\nAvailable: " + str_from(g_Player->TithingPoints);
            m_TithingPointsText->CreateTextureA(6, textData);
        }

        int indexX = 106;
        int dataX = 62;
        int y = 0;
        uint32_t spellSerial = ID_GSB_SPELL_ICON_LEFT;
        if ((page % 2) != 0)
        {
            indexX = 269;
            dataX = 225;
            spellSerial = ID_GSB_SPELL_ICON_RIGHT;
        }

        CGUIText *text = (CGUIText *)Add(new CGUIText(0x0288, indexX, 10));
        text->CreateTextureA(6, "INDEX");

        if (BookType == ST_MAGE)
        {
            text = (CGUIText *)Add(new CGUIText(0x0288, dataX, 30));
            text->CreateTextureA(6, s_SpellCircleName[page]);
        }
        else if (BookType == ST_MASTERY)
        {
            if ((page % 2) == 1)
            {
                // text = (CGUIText *)Add(new CGUIText(0x0288, dataX, 30));
                // text->CreateTextureA(6, "Abilities" );
            }
            text = (CGUIText *)Add(new CGUIText(0x0288, dataX, 30));
            text->CreateTextureA(6, (page % 2) == 0 ? "Passive" : "Activated");
        }

        for (int i = 0; i < spellsOnPage; i++)
        {
            offs = BookType == ST_MASTERY ? m_SpellIndices8[page][i] : offs;
            if (m_Spells[offs] != 0u)
            {
                CGUIHitBox *box = (CGUIHitBox *)Add(
                    new CGUIHitBox(spellSerial + offs, dataX, 52 + y, 100, 16, true));
                box->MoveOnDrag = true;

                CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
                    spellSerial + offs, 0x0288, 0, 0, dataX, 52 + y, 0, false, 9));
                astr_t abbreviature;
                astr_t reagents;
                entry->m_Entry.SetTextA(GetSpellName(offs, abbreviature, reagents));
                entry->CheckOnSerial = true;
                entry->ReadOnly = true;
                y += 15;
            }
            offs++;
        }
    }

    int page = dictionaryPagesCount;
    const int topTextY = BookType == ST_MAGE ? 10 : 6;
    const bool haveReagents = BookType <= ST_NECRO || BookType == ST_MYSTICISM;
    const bool haveAbbreviature = BookType != ST_BUSHIDO && BookType != ST_NINJITSU;
    for (int i = 0; i < maxSpellsCount; i++)
    {
        if (m_Spells[i] == 0u)
        {
            continue;
        }

        int iconX = 62;
        int topTextX = 87;
        int iconTextX = 112;
        uint32_t iconSerial = ID_GSB_SPELL_ICON_LEFT + (uint32_t)i;
        if ((page % 2) != 0)
        {
            iconX = 225;
            topTextX = 244;
            iconTextX = 275;
            iconSerial = ID_GSB_SPELL_ICON_RIGHT + (int)i;
        }

        Add(new CGUIPage(page));
        page++;

        astr_t spellAbbreviature;
        astr_t reagents;
        auto spellName = GetSpellName(i, spellAbbreviature, reagents);
        CGUIText *text = (CGUIText *)Add(new CGUIText(0x0288, topTextX, topTextY));
        if (BookType == ST_MASTERY)
        {
            text->CreateTextureA(6, s_SpellCircleName[i / 8]);
            text = (CGUIText *)Add(new CGUIText(0x0288, iconTextX, 34));
            text->CreateTextureA(6, spellName, 80);

            int abbreviatureY = 26;
            if (text->m_Texture.Height < 24)
            {
                abbreviatureY = 31;
            }

            abbreviatureY += text->m_Texture.Height;
            text = (CGUIText *)Add(new CGUIText(0x0288, iconTextX, abbreviatureY));
            text->CreateTextureA(8, spellAbbreviature);
        }
        else
        {
            text->CreateTextureA(6, spellName);
            if (haveAbbreviature)
            {
                text = (CGUIText *)Add(new CGUIText(0x0288, iconTextX, 34));
                text->CreateTextureA(6, spellAbbreviature, 80);
            }
        }

        if (BookType == ST_MASTERY && iconStartGraphic + i > 0x094a)
        {
            iconStartGraphic = 0x9b8a; // FIXME: icon fixup
        }
        auto icon = (CGUIGumppic *)Add(new CGUIGumppic(iconStartGraphic + i, iconX, 40));
        icon->Serial = iconSerial;
        if (haveReagents || reagents.size())
        {
            Add(new CGUIGumppicTiled(0x0835, iconX, 88, 120, 0));
            text = (CGUIText *)Add(new CGUIText(0x0288, iconX, 92));
            text->CreateTextureA(6, "Reagents:");
            text = (CGUIText *)Add(new CGUIText(0x0288, iconX, 114));
            text->CreateTextureA(9, reagents);
        }

        if (BookType != ST_MASTERY)
        {
            int requriesY = 0;
            auto requries = GetSpellRequries((int)i, requriesY);
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
        int dictionaryPagesCount = 0;
        int spellsOnPage = 0;
        int spellIndexOffset = 0;
        uint16_t graphic = 0;
        uint16_t minimizedGraphic = 0;
        uint16_t iconStartGraphic = 0;

        GetSummaryBookInfo(
            maxSpellsCount,
            dictionaryPagesCount,
            spellsOnPage,
            spellIndexOffset,
            graphic,
            minimizedGraphic,
            iconStartGraphic);

        if (Page < dictionaryPagesCount) //List of spells
        {
            for (int j = 0; j < 2; j++)
            {
                for (int i = 0; i < spellsOnPage; i++)
                {
                    intptr_t offs = i + (spellsOnPage * (Page + j));
                    if (m_Spells[offs] == 0u)
                    {
                        continue;
                    }

                    if (serial == offs + ID_GSB_SPELL_ICON_LEFT ||
                        serial == offs + ID_GSB_SPELL_ICON_RIGHT)
                    {
                        int c = dictionaryPagesCount;
                        for (int k = 0; k < maxSpellsCount; k++)
                        {
                            if (m_Spells[k] != 0u)
                            {
                                if (k == offs)
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
        if ((newPage % 2) != 0)
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
            if ((newPage % 2) != 0)
            {
                newPage--;
            }
            ChangePage(newPage);
            WantRedraw = true;
            result = true;
        }
        else
        {
            if (g_PressedObject.LeftSerial >= (uint32_t)ID_GSB_SPELL_ICON_LEFT)
            {
                int spellIndex = g_PressedObject.LeftSerial - ID_GSB_SPELL_ICON_RIGHT + 1;
                if (g_PressedObject.LeftSerial < (uint32_t)ID_GSB_SPELL_ICON_RIGHT)
                {
                    spellIndex = g_PressedObject.LeftSerial - ID_GSB_SPELL_ICON_LEFT + 1;
                }
                spellIndex += ((int)BookType * 100);
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

// Name, Magic Words, Reagents
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