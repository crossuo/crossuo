// AGPLv3 License
// Copyright (c) 2020 Danny Angelo Carminati Grein

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

constexpr uint16_t nameColor = 0x0288;
constexpr uint16_t powerColor = 0x0386;

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
    ID_GSB_SPELL_SERIAL = 100,
};

// clang-format off
static int s_MasteryIndexOrder[][8] = { {  0,  1,  2,  3,  4,  5,  6,  7 },
                                        {  8,  9, 10, 11, 12, 13, 18, 19 },
                                        { 16, 20, 21, 24, 25, 33, 34, 35 },
                                        { 26, 27, 28, 31, 36, 37, 39, 40 },
                                        { 22, 23, 29, 30, 42, 43, -1, -1 },
                                        { 14, 15, 17, 32, 38, 41, 44, -1 } };
// clang-format on

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

void CGumpSpellbook::UpdateGraphic(uint16_t parentGraphic)
{
    SPELLBOOK_TYPE oldType = BookType;
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
            if (g_Config.ClientVersion >= CV_704565)
            {
                BookType = ST_MASTERY;
            }
            break;
        }
        default:
        {
            BookType = ST_MAGERY;
            break;
        }
    }

    if (oldType != BookType)
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

    const auto book = GetSpellbook(BookType);
    if (Page >= book.IndexPagesCount)
    {
        uint32_t serial = g_SelectedObject.Serial;
        if (serial >= ID_GSB_SPELL_SERIAL)
        {
            const int spellIndex = serial - ID_GSB_SPELL_SERIAL;
            assert(spellIndex < book.SpellCount);
            const auto spell = book.Spells[spellIndex];
            g_ToolTip.Set(
                g_ClilocManager.Cliloc(g_Language)->GetW(spell->DescriptionCliloc, true), 150);
        }
    }
}

void CGumpSpellbook::PrepareContent()
{
    const auto book = GetSpellbook(BookType);
    if (g_PressedObject.LeftGump == this && Page >= book.IndexPagesCount &&
        g_PressedObject.LeftSerial >= ID_GSB_SPELL_SERIAL)
    {
        CPoint2Di offset = g_MouseManager.LeftDroppedOffset();
        if ((abs(offset.X) >= DRAG_PIXEL_RANGE || abs(offset.Y) >= DRAG_PIXEL_RANGE) ||
            (g_MouseManager.LastLeftButtonClickTimer + g_MouseManager.DoubleClickDelay < g_Ticks))
        {
            const int serial = g_PressedObject.LeftSerial;
            const int spellIndex = serial - ID_GSB_SPELL_SERIAL;
            assert(spellIndex < book.SpellCount);
            const auto spell = book.Spells[spellIndex];
            g_GumpManager.AddGump(new CGumpSpell(
                serial,
                spell->Type,
                spell->SpellIconGraphic,
                g_MouseManager.Position.X - 20,
                g_MouseManager.Position.Y - 20));
            g_GameWindow.EmulateOnLeftMouseButtonDown();
        }
    }

    if (!Minimized && m_LastSpellPointer != nullptr)
    {
        bool wantVisible = false;
        uint16_t graphicBookmark = 0x08ad;
        uint16_t graphicPointer = 0x08af;
        int wantX = m_LastSpellPointer->GetX();
        int wantY = m_LastSpellPointer->GetY();
        if (Page < book.IndexPagesCount)
        {
            for (int j = 0; j < 2; j++)
            {
                int y = 0;
                const int currentPage = Page + j;
                const int indexPage = BookType == ST_MASTERY ? currentPage / 2 : currentPage;
                for (int i = 0; i < book.SpellsPerPage; i++)
                {
                    const int spellIndex = i + (book.SpellsPerPage * indexPage);
                    assert(spellIndex >= 0 && spellIndex < MAX_SPELLS_COUNT);
                    if (m_Spells[spellIndex] == 0)
                    {
                        continue;
                    }

                    assert(book.Spells[spellIndex]);
                    const auto spell = book.Spells[spellIndex];
                    if (spell->Id == g_LastSpellIndex)
                    {
                        wantVisible = true;
                        wantY = 52 + y;
                        if (indexPage % 2 == 1)
                        {
                            graphicBookmark = 0x08ae;
                            graphicPointer =
                                0x08b0 + (book.Type == ST_MASTERY ? -1 : 0); // always left side
                            wantX = 203;
                        }
                        else
                        {
                            wantX = 184;
                        }
                        break;
                    }
                    y += 15;
                }
            }
        }
        else
        {
            int page = book.IndexPagesCount;
            for (int spellIndex = 0; spellIndex < book.SpellCount; spellIndex++)
            {
                if (m_Spells[spellIndex] == 0)
                {
                    continue;
                }

                assert(book.Spells[spellIndex]);
                const auto spell = book.Spells[spellIndex];
                if (page == Page || page == (Page + 1))
                {
                    if (spell->Id == g_LastSpellIndex)
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

astr_t CGumpSpellbook::GetSpellRequries(int spellIndex, int &y) const
{
    char buf[100];
    const auto book = GetSpellbook(BookType);
    const auto spell = book.Spells[spellIndex];
    if (spell->TithingCost)
    {
        y = 148;
        const char *upkeep = book.Type == ST_MASTERY ? "Upkeep" : "Tithing";
        snprintf(
            buf,
            sizeof(buf),
            "%s Cost: %i\nMana Cost: %i\nMin. Skill: %i",
            upkeep,
            spell->TithingCost,
            spell->ManaCost,
            spell->MinSkill);
        return buf;
    }
    y = 162;
    snprintf(buf, sizeof(buf), "Mana Cost: %i\nMin. Skill: %i", spell->ManaCost, spell->MinSkill);
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

    const auto book = GetSpellbook(BookType);
    Clear();
    Add(new CGUIPage(-1));
    if (Minimized)
    {
        m_Body = (CGUIGumppic *)Add(new CGUIGumppic(book.MinimizedGraphic, 0, 0));
        return;
    }

    m_Body = (CGUIGumppic *)Add(new CGUIGumppic(book.Graphic, 0, 0));
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
        const int currentCount = item->Count;
        if (currentCount > 0 && currentCount <= book.SpellCount)
        {
            m_Spells[currentCount - 1] = 1;
            m_SpellCount++;
        }
    }

    if (book.Type == ST_MAGERY)
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

    PageCount = book.IndexPagesCount + m_SpellCount;
    int page = 0;
    for (; page < book.IndexPagesCount; page++)
    {
        Add(new CGUIPage(page));
        if (page == 0 && book.Type == ST_CHIVALRY)
        {
            m_TithingPointsText = (CGUIText *)Add(new CGUIText(nameColor, 62, 162));
            astr_t textData = "Tithing points\nAvailable: " + str_from(g_Player->TithingPoints);
            m_TithingPointsText->CreateTextureA(6, textData);
        }

        int indexX = 106;
        int dataX = 56;
        int y = 0;
        if (page % 2 == 1)
        {
            indexX = 269;
            dataX = 221;
        }

        CGUIText *text = (CGUIText *)Add(new CGUIText(nameColor, indexX, 10));
        text->CreateTextureA(6, "INDEX");
        if (book.Type == ST_MASTERY && page % 2 == 1)
        {
            text = (CGUIText *)Add(new CGUIText(nameColor, dataX, 30));
            text->CreateTextureA(6, "Abilities");
            // TODO: active masteries
            continue;
        }

        int indexPage = page;
        if (book.Type == ST_MAGERY)
        {
            text = (CGUIText *)Add(new CGUIText(nameColor, dataX, 30));
            text->CreateTextureA(6, s_SpellCircleName[page]);
        }
        else if (book.Type == ST_MASTERY)
        {
            text = (CGUIText *)Add(new CGUIText(nameColor, dataX, 30));
            text->CreateTextureA(6, page == book.IndexPagesCount ? "Passive" : "Activated");
            if (page % 2 == 1)
                continue;
            indexPage = page / 2;
        }

        for (int i = 0; i < book.SpellsPerPage; i++)
        {
            const int spellIndex = book.Type == ST_MASTERY ? s_MasteryIndexOrder[indexPage][i] :
                                                             i + (indexPage * book.SpellsPerPage);
            assert(spellIndex < MAX_SPELLS_COUNT);
            if (spellIndex < 0 || m_Spells[spellIndex] == 0)
                continue;

            assert(spellIndex < book.SpellCount && book.Spells[spellIndex]);
            const auto spell = book.Spells[spellIndex];
            astr_t name = spell->Name;
            if (name == "Arcane Empowerment")
                name = "Arcane Empowe...";
            else if (name == "Enchanted Summoning")
                name = "Enchanted Summo..";
            const int serial = ID_GSB_SPELL_SERIAL + spellIndex;
            auto box = (CGUIHitBox *)Add(new CGUIHitBox(serial, dataX, 52 + y, 100, 16, true));
            auto entry = (CGUITextEntry *)Add(
                new CGUITextEntry(serial, nameColor, 0, 0, dataX, 52 + y, 0, false, 9));
            entry->m_Entry.SetTextA(name);
            entry->CheckOnSerial = true;
            entry->ReadOnly = true;
            box->MoveOnDrag = true;
            y += 15;
        }
    }

    const int topTextY = book.Type == ST_MAGERY ? 10 : 6;
    for (int spellIndex = 0; spellIndex < book.SpellCount; spellIndex++)
    {
        if (m_Spells[spellIndex] == 0)
        {
            continue;
        }

        assert(book.Spells[spellIndex]);
        const auto baseSpell = book.Spells[spellIndex];

        int iconX = 56;
        int topNameX = 87;
        int topTextX = 87;
        int iconTextX = 112;
        const uint32_t iconSerial = ID_GSB_SPELL_SERIAL + spellIndex;
        if (page % 2 == 1)
        {
            iconX = 224;
            topNameX = 224;
            topTextX = 242;
            iconTextX = 275;
        }
        Add(new CGUIPage(page));
        page++;

        auto spellName = baseSpell->Name;
        //g_ClilocManager.Cliloc(g_Language)->GetA(baseSpell->NameCliloc, true);
        CGUIText *text = nullptr;
        if (book.Type == ST_MAGERY || book.Type == ST_MASTERY)
        {
            text = (CGUIText *)Add(new CGUIText(nameColor, topTextX, topTextY));
            const auto title = book.Type == ST_MAGERY ? s_SpellCircleName[spellIndex / 8] :
                                                        MasterGroupNameById(spellIndex + 1);
            text->CreateTextureA(6, title);
            text = (CGUIText *)Add(new CGUIText(nameColor, iconTextX, 34));
            text->CreateTextureA(6, spellName, 80);
            if (baseSpell->PowerWords != nullptr)
            {
                int magicWordY = (text->m_Texture.Height < 24) ? 31 : 26;
                magicWordY += text->m_Texture.Height;
                const auto color = book.Type == ST_MAGERY ? nameColor : powerColor;
                text = (CGUIText *)Add(new CGUIText(color, iconTextX, magicWordY));
                const auto font = book.Type == ST_MAGERY ? 8 /* runes */ : 6;
                text->CreateTextureA(font, baseSpell->PowerWords, 80);
            }
        }
        else
        {
            text = (CGUIText *)Add(new CGUIText(nameColor, topNameX, topTextY));
            text->CreateTextureA(6, spellName, 100);
            if (baseSpell->PowerWords != nullptr)
            {
                text = (CGUIText *)Add(new CGUIText(nameColor, iconTextX, 34));
                text->CreateTextureA(6, baseSpell->PowerWords, 80);
            }
        }

        auto icon = (CGUIGumppic *)Add(new CGUIGumppic(baseSpell->BookIconGraphic, iconX, 40));
        icon->Serial = iconSerial;
        if (baseSpell->Reagents != nullptr)
        {
            if (book.HasSeparator)
            {
                // line separator
                Add(new CGUIGumppicTiled(0x0835, iconX, 88, 120, 0));
            }
            text = (CGUIText *)Add(new CGUIText(nameColor, iconX, 92));
            text->CreateTextureA(6, "Reagents:");
            text = (CGUIText *)Add(new CGUIText(nameColor, iconX, 114));
            text->CreateTextureA(9, baseSpell->Reagents);
        }

        if (book.Type != ST_MAGERY)
        {
            int requriesY = 0;
            auto requries = GetSpellRequries(spellIndex, requriesY);
            text = (CGUIText *)Add(new CGUIText(nameColor, iconX, requriesY));
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
    else if (serial >= ID_GSB_SPELL_SERIAL)
    {
        const auto book = GetSpellbook(BookType);
        if (Page < book.IndexPagesCount)
        {
            const int spellIndex = serial - ID_GSB_SPELL_SERIAL;
            if (m_Spells[spellIndex] != 0)
            {
                newPage = book.IndexPagesCount + spellIndex;
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
    if (serial >= ID_GSB_SPELL_SERIAL)
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
        else if (g_PressedObject.LeftSerial >= ID_GSB_SPELL_SERIAL)
        {
            const int spellIndex = g_PressedObject.LeftSerial - ID_GSB_SPELL_SERIAL;
            const auto book = GetSpellbook(BookType);
            const auto spell = book.Spells[spellIndex];
            if (g_Config.ClientVersion < CV_308Z)
            {
                g_Game.CastSpellFromBook(spell->Id, Serial);
            }
            else
            {
                g_Game.CastSpell(spell->Id);
            }
            Minimized = true;
            WantUpdateContent = true;
            result = true;
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
