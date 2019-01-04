// MIT License
// Copyright (C) December 2016 Hotride

#include "GumpCombatBook.h"
#include "../Config.h"

CGumpCombatBook::CGumpCombatBook(int x, int y)
    : CGump(GT_COMBAT_BOOK, 0, x, y)
{
    DEBUG_TRACE_FUNCTION;
    Draw2Page = 1;

    if (g_Config.ClientVersion < CV_7000)
    {
        if (g_Config.ClientVersion >= CV_500A)
        {
            AbilityCount = 29;
        }
        else
        {
            AbilityCount = 13;
            DictionaryPagesCount = 2;
        }
    }

    PagesCount = DictionaryPagesCount + (AbilityCount * 2);
}

CGumpCombatBook::~CGumpCombatBook()
{
}

vector<uint16_t> CGumpCombatBook::GetItemsList(uint8_t index)
{
    DEBUG_TRACE_FUNCTION;
    vector<uint16_t> list;

    switch (index)
    {
        case 0:
        {
            list.push_back(3908);
            list.push_back(5048);
            list.push_back(3935);
            list.push_back(5119);
            list.push_back(9927);
            list.push_back(5181);
            list.push_back(5040);
            list.push_back(5121);
            list.push_back(3939);
            list.push_back(9932);
            list.push_back(11554);
            list.push_back(16497);
            list.push_back(16502);
            list.push_back(16494);
            list.push_back(16491);
            break;
        }
        case 1:
        {
            list.push_back(3779);
            list.push_back(5115);
            list.push_back(3912);
            list.push_back(3910);
            list.push_back(5185);
            list.push_back(9924);
            list.push_back(5127);
            list.push_back(5040);
            list.push_back(3720);
            list.push_back(5125);
            list.push_back(11552);
            list.push_back(16499);
            list.push_back(16498);
            break;
        }
        case 2:
        {
            list.push_back(5048);
            list.push_back(3912);
            list.push_back(5183);
            list.push_back(5179);
            list.push_back(3933);
            list.push_back(5113);
            list.push_back(3722);
            list.push_back(9930);
            list.push_back(3920);
            list.push_back(11556);
            list.push_back(16487);
            list.push_back(16500);
            break;
        }
        case 3:
        {
            list.push_back(5050);
            list.push_back(3914);
            list.push_back(3935);
            list.push_back(3714);
            list.push_back(5092);
            list.push_back(5179);
            list.push_back(5127);
            list.push_back(5177);
            list.push_back(9926);
            list.push_back(4021);
            list.push_back(10146);
            list.push_back(11556);
            list.push_back(11560);
            list.push_back(5109);
            list.push_back(16500);
            list.push_back(16495);
            break;
        }
        case 4:
        {
            list.push_back(5111);
            list.push_back(3718);
            list.push_back(3781);
            list.push_back(3908);
            list.push_back(3573);
            list.push_back(3714);
            list.push_back(3933);
            list.push_back(5125);
            list.push_back(11558);
            list.push_back(11560);
            list.push_back(5109);
            list.push_back(9934);
            list.push_back(16493);
            list.push_back(16494);
            break;
        }
        case 5:
        {
            list.push_back(3918);
            list.push_back(3914);
            list.push_back(9927);
            list.push_back(3573);
            list.push_back(5044);
            list.push_back(3720);
            list.push_back(9930);
            list.push_back(5117);
            list.push_back(16501);
            list.push_back(16495);
            break;
        }
        case 6:
        {
            list.push_back(3718);
            list.push_back(5187);
            list.push_back(3916);
            list.push_back(5046);
            list.push_back(5119);
            list.push_back(9931);
            list.push_back(3722);
            list.push_back(9929);
            list.push_back(9933);
            list.push_back(10148);
            list.push_back(10153);
            list.push_back(16488);
            list.push_back(16493);
            list.push_back(16496);
            break;
        }
        case 7:
        {
            list.push_back(5111);
            list.push_back(3779);
            list.push_back(3922);
            list.push_back(9928);
            list.push_back(5121);
            list.push_back(9929);
            list.push_back(11553);
            list.push_back(16490);
            list.push_back(16488);
            break;
        }
        case 8:
        {
            list.push_back(3910);
            list.push_back(9925);
            list.push_back(9931);
            list.push_back(5181);
            list.push_back(9926);
            list.push_back(5123);
            list.push_back(3920);
            list.push_back(5042);
            list.push_back(16499);
            list.push_back(16502);
            list.push_back(16496);
            list.push_back(16491);
            break;
        }
        case 9:
        {
            list.push_back(5117);
            list.push_back(9932);
            list.push_back(9933);
            list.push_back(16492);
            break;
        }
        case 10:
        {
            list.push_back(5050);
            list.push_back(3918);
            list.push_back(5046);
            list.push_back(9924);
            list.push_back(9925);
            list.push_back(5113);
            list.push_back(3569);
            list.push_back(9928);
            list.push_back(3939);
            list.push_back(5042);
            list.push_back(16497);
            list.push_back(16498);
            break;
        }
        case 11:
        {
            list.push_back(3781);
            list.push_back(5187);
            list.push_back(5185);
            list.push_back(5092);
            list.push_back(5044);
            list.push_back(3922);
            list.push_back(5123);
            list.push_back(4021);
            list.push_back(11553);
            list.push_back(16490);
            break;
        }
        case 12:
        {
            list.push_back(5115);
            list.push_back(5183);
            list.push_back(3916);
            list.push_back(5177);
            list.push_back(3569);
            list.push_back(10157);
            list.push_back(11559);
            list.push_back(9934);
            list.push_back(16501);
            break;
        }
        case 13:
        {
            list.push_back(10146);
            break;
        }
        case 14:
        {
            list.push_back(10148);
            list.push_back(10150);
            list.push_back(10151);
            break;
        }
        case 15:
        {
            list.push_back(10147);
            list.push_back(10158);
            list.push_back(10159);
            list.push_back(11557);
            break;
        }
        case 16:
        {
            list.push_back(10151);
            list.push_back(10157);
            list.push_back(11561);
            break;
        }
        case 17:
        {
            list.push_back(10152);
            break;
        }
        case 18:
        case 20:
        {
            list.push_back(10155);
            break;
        }
        case 19:
        {
            list.push_back(10152);
            list.push_back(10153);
            list.push_back(10158);
            list.push_back(11554);
            break;
        }
        case 21:
        {
            list.push_back(10149);
            break;
        }
        case 22:
        {
            list.push_back(10149);
            list.push_back(10159);
            break;
        }
        case 23:
        {
            list.push_back(11555);
            list.push_back(11558);
            list.push_back(11559);
            list.push_back(11561);
            break;
        }
        case 24:
        case 27:
        {
            list.push_back(11550);
            break;
        }
        case 25:
        {
            list.push_back(11551);
            break;
        }
        case 26:
        {
            list.push_back(11551);
            list.push_back(11552);
            break;
        }
        case 28:
        {
            list.push_back(11557);
            break;
        }
        case 29:
        {
            list.push_back(16492);
            break;
        }
        case 30:
        {
            list.push_back(16487);
            break;
        }
        default:
            break;
    }

    return list;
}

void CGumpCombatBook::InitToolTip()
{
    DEBUG_TRACE_FUNCTION;
    if (Minimized)
    {
        g_ToolTip.Set(L"Double click to maximize book gump");
        return;
    }

    uint32_t serial = g_SelectedObject.Serial;

    if (Page >= DictionaryPagesCount)
    {
        if (serial >= (uint32_t)ID_GCB_ICON)
        {
            g_ToolTip.Set(
                g_ClilocManager.Cliloc(g_Language)->GetW(1061693 + (serial - ID_GCB_ICON), true),
                150);
        }
    }
    else
    {
        if (serial == ID_GCB_ICON_FIRST)
        {
            g_ToolTip.Set(
                g_ClilocManager.Cliloc(g_Language)->GetW(1028838 + (g_Ability[0] & 0x7F) - 1, true),
                80);
        }
        else if (serial == ID_GCB_ICON_SECOND)
        {
            g_ToolTip.Set(
                g_ClilocManager.Cliloc(g_Language)->GetW(1028838 + (g_Ability[1] & 0x7F) - 1, true),
                80);
        }
    }
}

void CGumpCombatBook::PrepareContent()
{
    DEBUG_TRACE_FUNCTION;
    if (g_PressedObject.LeftGump == this && Page < DictionaryPagesCount &&
        (g_PressedObject.LeftSerial == ID_GCB_ICON_FIRST ||
         g_PressedObject.LeftSerial == ID_GCB_ICON_SECOND))
    {
        Wisp::CPoint2Di offset = g_MouseManager.LeftDroppedOffset();

        if ((abs(offset.X) >= DRAG_PIXEL_RANGE || abs(offset.Y) >= DRAG_PIXEL_RANGE) ||
            (g_MouseManager.LastLeftButtonClickTimer + g_MouseManager.DoubleClickDelay < g_Ticks))
        {
            g_GumpManager.AddGump(new CGumpAbility(
                static_cast<int>(g_PressedObject.LeftSerial == ID_GCB_ICON_SECOND),
                g_MouseManager.Position.X - 20,
                g_MouseManager.Position.Y - 20));

            g_OrionWindow.EmulateOnLeftMouseButtonDown();
        }
    }
}

void CGumpCombatBook::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    m_PrevPage = nullptr;
    m_NextPage = nullptr;

    Clear();

    Add(new CGUIPage(-1));

    if (Minimized)
    {
        Add(new CGUIGumppic(0x2B05, 0, 0));

        return;
    }

    Add(new CGUIGumppic(0x2B02, 0, 0));

    Add(new CGUIHitBox(ID_GCB_BUTTON_MINIMIZE, 6, 100, 16, 16, true));

    int offs = 0;

    for (int page = 0; page < DictionaryPagesCount; page++)
    {
        Add(new CGUIPage((int)page));

        int indexX = 96;
        int dataX = 52;
        int y = 0;
        int spellsOnPage = 9;

        if ((page % 2) != 0)
        {
            indexX = 259;
            dataX = 215;
            spellsOnPage = 4;
        }

        CGUIText *text = (CGUIText *)Add(new CGUIText(0x0288, indexX, 6));
        text->CreateTextureA(6, "INDEX");

        for (int i = 0; i < spellsOnPage; i++)
        {
            if (offs >= AbilityCount)
            {
                break;
            }

            CGUIHitBox *box =
                (CGUIHitBox *)Add(new CGUIHitBox(ID_GCB_ICON + offs, dataX, 42 + y, 100, 16, true));
            box->MoveOnDrag = true;

            CGUITextEntry *entry = (CGUITextEntry *)Add(
                new CGUITextEntry(ID_GCB_ICON + offs, 0x0288, 0, 0, dataX, 42 + y, 0, false, 9));
            entry->m_Entry.SetTextA(m_AbilityName[offs]);
            entry->CheckOnSerial = true;
            entry->ReadOnly = true;

            y += 15;

            offs++;
        }

        if (spellsOnPage == 4)
        {
            CGUIGumppic *icon =
                (CGUIGumppic *)Add(new CGUIGumppic(0x5200 + (g_Ability[0] & 0x7F) - 1, 215, 105));
            icon->Serial = ID_GCB_ICON_FIRST;

            text = (CGUIText *)Add(new CGUIText(0x0288, 265, 105));
            text->CreateTextureA(6, "Primary Ability Icon", 80);

            icon =
                (CGUIGumppic *)Add(new CGUIGumppic(0x5200 + (g_Ability[1] & 0x7F) - 1, 215, 150));
            icon->Serial = ID_GCB_ICON_SECOND;

            text = (CGUIText *)Add(new CGUIText(0x0288, 265, 150));
            text->CreateTextureA(6, "Secondary Ability Icon", 80);
        }
    }

    int page = DictionaryPagesCount;

    for (int i = 0; i < AbilityCount; i++)
    {
        Add(new CGUIPage(page));
        page += 2;

        CGUIGumppic *icon = (CGUIGumppic *)Add(new CGUIGumppic(0x5200 + (int)i, 62, 40));
        icon->Serial = ID_GCB_ICON + (int)i;
        icon->MoveOnDrag = true;

        Add(new CGUIGumppicTiled(0x0835, 62, 88, 128, 0));

        vector<uint16_t> list = GetItemsList((uint8_t)i);

        int size = (int)list.size();
        size_t maxStaticCount = g_Orion.m_StaticData.size();

        int textX = 62;
        int textY = 98;

        for (int j = 0; j < size; j++)
        {
            if (j == 6)
            {
                textX = 215;
                textY = 34;
            }

            uint16_t &id = list[j];

            if (id >= maxStaticCount)
            {
                continue;
            }

            CGUIText *text = (CGUIText *)Add(new CGUIText(0x0288, textX, textY));
            text->CreateTextureA(9, ToCamelCase(g_Orion.m_StaticData[id].Name));

            textY += 16;
        }
    }

    Add(new CGUIPage(-1));

    m_PrevPage =
        (CGUIButton *)Add(new CGUIButton(ID_GCB_BUTTON_PREV, 0x08BB, 0x08BB, 0x08BB, 50, 8));
    m_PrevPage->Visible = (Page != 0);
    m_NextPage =
        (CGUIButton *)Add(new CGUIButton(ID_GCB_BUTTON_NEXT, 0x08BC, 0x08BC, 0x08BC, 321, 8));
    m_NextPage->Visible = (Page + 2 < PagesCount);
}

void CGumpCombatBook::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    int newPage = -1;

    if (serial == ID_GCB_BUTTON_PREV)
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
    else if (serial == ID_GCB_BUTTON_NEXT)
    {
        if (Page < PagesCount)
        {
            newPage = Page + 2;

            if (newPage >= PagesCount)
            {
                newPage = PagesCount - 1;
            }
        }
    }
    else if (serial == ID_GCB_BUTTON_MINIMIZE)
    {
        Minimized = true;
        WantUpdateContent = true;
    }
    else if (serial == ID_GCB_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
    else if (serial >= ID_GCB_ICON)
    {
        if (Page < DictionaryPagesCount)
        { //List of spells
            newPage = DictionaryPagesCount + ((serial - ID_GCB_ICON) * 2);
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

bool CGumpCombatBook::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    if (Minimized)
    {
        Minimized = false;
        WantUpdateContent = true;

        result = true;
    }
    else
    {
        if (g_PressedObject.LeftSerial == ID_GCB_BUTTON_PREV)
        {
            ChangePage(0);

            WantRedraw = true;

            result = true;
        }
        else if (g_PressedObject.LeftSerial == ID_GCB_BUTTON_NEXT)
        {
            int newPage = PagesCount - 1;

            if ((newPage % 2) != 0)
            {
                newPage--;
            }

            ChangePage(newPage);

            WantRedraw = true;

            result = true;
        }
        else if (g_PressedObject.LeftSerial == ID_GCB_ICON_FIRST)
        {
            CGumpAbility::OnAbilityUse(0);

            WantUpdateContent = true;

            result = true;
        }
        else if (g_PressedObject.LeftSerial == ID_GCB_ICON_SECOND)
        {
            CGumpAbility::OnAbilityUse(1);

            WantUpdateContent = true;

            result = true;
        }
    }

    return result;
}

void CGumpCombatBook::DelayedClick(CRenderObject *obj)
{
    DEBUG_TRACE_FUNCTION;
    if (obj != nullptr)
    {
        ChangePage(g_ClickObject.Page);
        WantRedraw = true;
    }
}

void CGumpCombatBook::ChangePage(int newPage)
{
    DEBUG_TRACE_FUNCTION;
    Page = newPage;

    m_PrevPage->Visible = (Page != 0);
    m_NextPage->Visible = (Page + 2 < PagesCount);

    g_Orion.PlaySoundEffect(0x0055);
}

const string CGumpCombatBook::m_AbilityName[MAX_ABILITIES_COUNT]{
    "Armor Ignore",       "Bleed Attack",    "Concussion Blow",
    "Crushing Blow",      "Disarm",          "Dismount",
    "Double Strike",      "Infecting",       "Mortal Strike",
    "Moving Shot",        "Paralyzing Blow", "Shadow Strike",
    "Whirlwind Attack",
    "Riding Swipe", //CV_500a
    "Frenzied Whirlwind", "Block",           "Defense Mastery",
    "Nerve Strike",       "Talon Strike",    "Feint",
    "Dual Wield",         "Double Shot",     "Armor Pierce",
    "Bladeweave",         "Force Arrow",     "Lightning Arrow",
    "Psychic Attack",     "Serpent Arrow",   "Force of Nature",
    "Infused Throw", //CV_7000
    "Mystic Arc",         "Disrobe"
};
