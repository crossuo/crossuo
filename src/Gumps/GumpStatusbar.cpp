// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpStatusbar.h"
#include <SDL_rect.h>
#include "../Config.h"
#include "../CrossUO.h"
#include "../Application.h"
#include "../Target.h"
#include "../ToolTip.h"
#include "../TargetGump.h"
#include "../Party.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../TextEngine/GameConsole.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/ConnectionManager.h"
#include "../Managers/FontsManager.h"
#include "../Managers/GumpManager.h"
#include "../Managers/MouseManager.h"
#include "../Managers/FileManager.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/GamePlayer.h"
#include "../Network/Packets.h"

int CGumpStatusbar::m_StatusbarDefaultWidth = 154;
int CGumpStatusbar::m_StatusbarDefaultHeight = 59;

enum
{
    ID_GSB_NONE,

    ID_GSB_MINIMIZE,
    ID_GSB_TEXT_FIELD,
    ID_GSB_BUTTON_HEAL_1,
    ID_GSB_BUTTON_HEAL_2,
    ID_GSB_BUTTON_REMOVE_FROM_GROUP,
    ID_GSB_BUFF_GUMP,
    ID_GSB_BUFF_LOCKER_STR,
    ID_GSB_BUFF_LOCKER_DEX,
    ID_GSB_BUFF_LOCKER_INT,
    ID_GSB_TEXT_STR,
    ID_GSB_TEXT_DEX,
    ID_GSB_TEXT_INT,
    ID_GSB_TEXT_SEX,
    ID_GSB_TEXT_ARMOR,
    ID_GSB_TEXT_HITS,
    ID_GSB_TEXT_STAM,
    ID_GSB_TEXT_MANA,
    ID_GSB_TEXT_MAX_STATS,
    ID_GSB_TEXT_LUCK,
    ID_GSB_TEXT_WEIGHT,
    ID_GSB_TEXT_DAMAGE,
    ID_GSB_TEXT_GOLD,
    ID_GSB_TEXT_FOLLOWERS,
    ID_GSB_TEXT_RESISTANCE_PHYSICAL,
    ID_GSB_TEXT_RESISTANCE_FIRE,
    ID_GSB_TEXT_RESISTANCE_COLD,
    ID_GSB_TEXT_RESISTANCE_POISON,
    ID_GSB_TEXT_RESISTANCE_ENERGY,
    ID_GSB_TEXT_TITHING_POINTS,
    ID_GSB_TEXT_HIT_CHANCE,
    ID_GSB_TEXT_DEFENCE_CHANCE,
    ID_GSB_TEXT_LOWER_MANA,
    ID_GSB_TEXT_SWING_SPEED,
    ID_GSB_TEXT_WEAPON_DMG,
    ID_GSB_TEXT_LOWER_REG_COST,
    ID_GSB_TEXT_SPELL_DMG,
    ID_GSB_TEXT_FASTER_CASTING,
    ID_GSB_TEXT_CAST_RECOVERY,

    ID_GSB_COUNT,
};

static const wstring tooltip[] = {
    L"(none)",                      //ID_GSB_NONE
    L"Minimize the statusbar gump", //ID_GSB_MINIMIZE
    L"",                            //ID_GSB_TEXT_FIELD
    L"",                            //ID_GSB_BUTTON_HEAL_1
    L"",                            //ID_GSB_BUTTON_HEAL_2
    L"Remove bar from group",       //ID_GSB_BUTTON_REMOVE_FROM_GROUP
    L"Open buff window",            //ID_GSB_BUFF_GUMP
    L"Change strength state",       //ID_GSB_BUFF_LOCKER_STR
    L"Change dexterity state",      //ID_GSB_BUFF_LOCKER_DEX
    L"Change intelligence state",   //ID_GSB_BUFF_LOCKER_INT
    L"",                            //
    L"Strength",                    //ID_GSB_TEXT_STR
    L"Dexterity",                   //ID_GSB_TEXT_DEX
    L"Intelligence",                //ID_GSB_TEXT_INT
    L"Sex",                         //ID_GSB_TEXT_SEX
    L"Armor",                       //ID_GSB_TEXT_ARMOR
    L"Hit Points",                  //ID_GSB_TEXT_HITS
    L"Stamina",                     //ID_GSB_TEXT_STAM
    L"Mana",                        //ID_GSB_TEXT_MANA
    L"Maximum stats",               //ID_GSB_TEXT_MAX_STATS
    L"Luck",                        //ID_GSB_TEXT_LUCK
    L"Weight",                      //ID_GSB_TEXT_WEIGHT
    L"Damage",                      //ID_GSB_TEXT_DAMAGE
    L"Gold",                        //ID_GSB_TEXT_GOLD
    L"Followers",                   //ID_GSB_TEXT_FOLLOWERS
    L"Physical Resistance",         //ID_GSB_TEXT_RESISTANCE_PHYSICAL
    L"Fire Resistance",             //ID_GSB_TEXT_RESISTANCE_FIRE
    L"Cold Resistance",             //ID_GSB_TEXT_RESISTANCE_COLD
    L"Poison Resistance",           //ID_GSB_TEXT_RESISTANCE_POISON
    L"Energy Resistance",           //ID_GSB_TEXT_RESISTANCE_ENERGY
    L"Tithing Points",              //ID_GSB_TEXT_TITHING_POINTS
    L"Hit Chance Increase",         //ID_GSB_TEXT_HIT_CHANCE
    L"Defense Chance Increase",     //ID_GSB_TEXT_DEFENCE_CHANCE
    L"Lower Mana Cost",             //ID_GSB_TEXT_LOWER_MANA
    L"Swing Speed Increase",        //ID_GSB_TEXT_SWING_SPEED
    L"Weapon Damage Increase",      //ID_GSB_TEXT_WEAPON_DMG
    L"Lower Reagent Cost",          //ID_GSB_TEXT_LOWER_REG_COST
    L"Spell Damage Increase",       //ID_GSB_TEXT_SPELL_DMG
    L"Faster Casting",              //ID_GSB_TEXT_FASTER_CASTING
    L"Faster Cast Recovery"         //ID_GSB_TEXT_CAST_RECOVERY
};

static_assert(countof(tooltip) - 1 == ID_GSB_COUNT, "Wrong amount of entries in tooltip table");

CGumpStatusbar::CGumpStatusbar(uint32_t serial, short x, short y, bool minimized)
    : CGump(GT_STATUSBAR, serial, x, y)
{
    DEBUG_TRACE_FUNCTION;
    if (minimized)
    {
        Minimized = true;
        MinimizedX = x;
        MinimizedY = y;
    }

    if (!g_ConfigManager.DisableNewTargetSystem && Serial == g_NewTargetSystem.Serial)
    {
        g_GumpManager.UpdateGump(Serial, 0, GT_TARGET_SYSTEM);
    }

    m_Locker.Serial = ID_GSB_LOCK_MOVING;
}

CGumpStatusbar::~CGumpStatusbar()
{
    DEBUG_TRACE_FUNCTION;
    if (g_ConnectionManager.Connected() && g_Config.ClientVersion >= CV_200 && g_World != nullptr &&
        g_World->FindWorldObject(Serial) != nullptr)
    {
        CPacketCloseStatusbarGump(Serial).Send();
    }

    RemoveFromGroup();
}

void CGumpStatusbar::InitToolTip()
{
    DEBUG_TRACE_FUNCTION;
    const uint32_t id = g_SelectedObject.Serial;
    if (id == ID_GSB_NONE || id >= ID_GSB_COUNT)
    {
        return;
    }

    //if (Minimized && Serial == g_PlayerSerial)
    //    g_ToolTip.Set(L"Double click to maximize the statusbar gump");
    const wstring &text = tooltip[id];
    if (!text.empty())
    {
        g_ToolTip.Set(text, 80);
    }
}

CGumpStatusbar *CGumpStatusbar::GetTopStatusbar()
{
    DEBUG_TRACE_FUNCTION;
    if (!InGroup())
    {
        return nullptr;
    }

    if (m_GroupPrev == nullptr)
    {
        return this;
    }

    CGumpStatusbar *gump = m_GroupPrev;

    while (gump != nullptr && gump->m_GroupPrev != nullptr)
    {
        gump = gump->m_GroupPrev;
    }

    return gump;
}

CGumpStatusbar *CGumpStatusbar::GetNearStatusbar(int &x, int &y)
{
    DEBUG_TRACE_FUNCTION;
    if (InGroup() || !Minimized)
    {
        return nullptr;
    }

    //154x59 mini-gump

    /*
    static const int gumpWidth = 154;
    static const int gumpHeight = 59;

    const int rangeX = 77;
    const int rangeY = 29;
    */

    int gumpWidth = m_StatusbarDefaultWidth;
    int gumpHeight = m_StatusbarDefaultHeight;

    int rangeX = gumpWidth / 2;
    int rangeY = gumpHeight / 2;

    const int rangeOffsetX = 60;
    const int rangeOffsetY = 24;

    CGump *gump = (CGump *)g_GumpManager.m_Items;

    while (gump != nullptr)
    {
        if (gump != this && gump->GumpType == GT_STATUSBAR && gump->Minimized)
        {
            int gumpX = gump->MinimizedX;
            int offsetX = abs(x - gumpX);
            int passed = 0;

            if (x >= gumpX && x <= (gumpX + gumpWidth))
            {
                passed = 2;
            }
            else if (offsetX < rangeOffsetX)
            { //left part of gump
                passed = 1;
            }
            else
            {
                offsetX = abs(x - (gumpX + gumpWidth));

                if (offsetX < rangeOffsetX)
                { //right part of gump
                    passed = -1;
                }
                else if (x >= (gumpX - rangeX) && x <= (gumpX + gumpWidth + rangeX))
                {
                    passed = 2;
                }
            }

            int gumpY = gump->MinimizedY;

            if (abs(passed) == 1)
            {
                if (y < (gumpY - rangeY) || y > (gumpY + gumpHeight + rangeY))
                {
                    passed = 0;
                }
            }
            else if (passed == 2) //in gump range X
            {
                int offsetY = abs(y - gumpY);

                if (offsetY < rangeOffsetY)
                { //top part of gump
                    passed = 2;
                }
                else
                {
                    offsetY = abs(y - (gumpY + gumpHeight));

                    if (offsetY < rangeOffsetY)
                    { //bottom part of gump
                        passed = -2;
                    }
                    else
                    {
                        passed = 0;
                    }
                }
            }

            if (passed != 0)
            {
                int testX = gumpX;
                int testY = gumpY;

                switch (passed)
                {
                    case -2: //gump bottom
                    {
                        testY += gumpHeight;
                        break;
                    }
                    case -1: //gump right
                    {
                        testX += gumpWidth;
                        break;
                    }
                    case 1: //gump left
                    {
                        testX -= gumpWidth;
                        break;
                    }
                    case 2: //gump top
                    {
                        testY -= gumpHeight;
                        break;
                    }
                    default:
                        break;
                }

                CGump *testGump = (CGump *)g_GumpManager.m_Items;

                while (testGump != nullptr)
                {
                    if (testGump != this && testGump->GumpType == GT_STATUSBAR &&
                        testGump->Minimized)
                    {
                        if (testGump->MinimizedX == testX && testGump->MinimizedY == testY)
                        {
                            break;
                        }
                    }

                    testGump = (CGump *)testGump->m_Next;
                }

                if (testGump == nullptr)
                {
                    x = testX;
                    y = testY;

                    break;
                }
            }
        }

        gump = (CGump *)gump->m_Next;
    }

    return (CGumpStatusbar *)gump;
}

bool CGumpStatusbar::GetStatusbarGroupOffset(int &x, int &y)
{
    DEBUG_TRACE_FUNCTION;
    if (InGroup() && Minimized && g_MouseManager.LeftButtonPressed &&
        g_PressedObject.LeftGump != nullptr &&
        (g_PressedObject.LeftObject == nullptr ||
         (g_PressedObject.LeftObject->IsGUI() &&
          ((CBaseGUI *)g_PressedObject.LeftObject)->MoveOnDrag)))
    {
        CGumpStatusbar *gump = GetTopStatusbar();

        while (gump != nullptr)
        {
            if (gump != this && g_PressedObject.LeftGump == gump && gump->CanBeMoved())
            {
                CPoint2Di offset = g_MouseManager.LeftDroppedOffset();

                x += offset.X;
                y += offset.Y;

                return true;
            }

            gump = gump->m_GroupNext;
        }
    }

    return false;
}

void CGumpStatusbar::UpdateGroup(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    if (!InGroup())
    {
        return;
    }

    CGumpStatusbar *gump = GetTopStatusbar();

    while (gump != nullptr)
    {
        if (gump != this)
        {
            gump->MinimizedX += x;
            gump->MinimizedY += y;

            g_GumpManager.MoveToBack(gump);
            //gump->WantRedraw = true;
        }

        gump = gump->m_GroupNext;
    }

    g_GumpManager.MoveToBack(this);
}

void CGumpStatusbar::AddStatusbar(CGumpStatusbar *bar)
{
    DEBUG_TRACE_FUNCTION;
    if (m_GroupNext == nullptr)
    {
        m_GroupNext = bar;
        bar->m_GroupPrev = this;
        bar->m_GroupNext = nullptr;
    }
    else
    {
        CGumpStatusbar *gump = m_GroupNext;

        while (gump != nullptr && gump->m_GroupNext != nullptr)
        {
            gump = gump->m_GroupNext;
        }

        gump->m_GroupNext = bar;
        bar->m_GroupPrev = gump;
        bar->m_GroupNext = nullptr;
    }

    if (bar->m_StatusbarUnlocker != nullptr)
    {
        bar->m_StatusbarUnlocker->Visible = bar->InGroup();
        bar->WantRedraw = true;
    }

    if (m_StatusbarUnlocker != nullptr)
    {
        m_StatusbarUnlocker->Visible = InGroup();

        WantRedraw = true;
    }
}

void CGumpStatusbar::RemoveFromGroup()
{
    DEBUG_TRACE_FUNCTION;
    if (m_GroupNext != nullptr)
    {
        m_GroupNext->WantRedraw = true;
        m_GroupNext->m_GroupPrev = m_GroupPrev;

        if (m_GroupNext->m_StatusbarUnlocker != nullptr)
        {
            m_GroupNext->m_StatusbarUnlocker->Visible = m_GroupNext->InGroup();
        }
    }

    if (m_GroupPrev != nullptr)
    {
        m_GroupPrev->WantRedraw = true;
        m_GroupPrev->m_GroupNext = m_GroupNext;

        if (m_GroupPrev->m_StatusbarUnlocker != nullptr)
        {
            m_GroupPrev->m_StatusbarUnlocker->Visible = m_GroupPrev->InGroup();
        }
    }

    m_GroupNext = nullptr;
    m_GroupPrev = nullptr;

    if (m_StatusbarUnlocker != nullptr)
    {
        m_StatusbarUnlocker->Visible = InGroup();
        WantRedraw = true;
    }
}

void CGumpStatusbar::CalculateGumpState()
{
    DEBUG_TRACE_FUNCTION;

    CGump::CalculateGumpState();
    if ((g_GumpMovingOffset.X != 0) || (g_GumpMovingOffset.Y != 0))
    {
        if (g_Target.IsTargeting())
        {
            g_GeneratedMouseDown = true;
        }

        if (!InGroup())
        {
            int testX = g_MouseManager.Position.X;
            int testY = g_MouseManager.Position.Y;

            if (GetNearStatusbar(testX, testY) != nullptr)
            {
                g_GumpTranslate.X = (float)testX;
                g_GumpTranslate.Y = (float)testY;
            }
        }
    }
    else if (InGroup())
    {
        int x = (int)g_GumpTranslate.X;
        int y = (int)g_GumpTranslate.Y;

        GetStatusbarGroupOffset(x, y);

        g_GumpTranslate.X = (float)x;
        g_GumpTranslate.Y = (float)y;
    }
}

void CGumpStatusbar::PrepareContent()
{
    DEBUG_TRACE_FUNCTION;
    if (m_HitsBody != nullptr)
    {
        CGameCharacter *obj = g_World->FindWorldCharacter(Serial);

        if (obj == nullptr && m_HitsBody->Color != 0x0386)
        {
            m_Body->Color = 0x0386;
            m_HitsBody->Color = 0x0386;
            m_Entry->Color = 0x0386;
            m_Entry->ReadOnly = true;
        }
        else if (obj != nullptr && m_HitsBody->Color == 0x0386)
        {
            m_Body->Color = g_ConfigManager.GetColorByNotoriety(obj->Notoriety);
            m_HitsBody->Color = 0;

            if (obj->CanChangeName)
            {
                m_Entry->ReadOnly = false;
                m_Entry->Color = 0x000E;
            }
            else
            {
                m_Entry->ReadOnly = true;
                m_Entry->Color = 0x0386;
            }
        }
    }
}

void CGumpStatusbar::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
    m_StatusbarUnlocker = nullptr;
    m_Body = nullptr;
    m_HitsBody = nullptr;
    m_Entry = nullptr;
    bool useUOPGumps = g_FileManager.UseUOPGumps;
    CGUIText *text = nullptr;

    if (Serial == g_PlayerSerial)
    {
        if (!Minimized)
        {
            SDL_Point p = { 0, 0 };

            uint16_t gumpId = 0x2A6C;
            if (g_Config.ClientVersion < CV_308D || g_ConfigManager.GetOldStyleStatusbar())
            {
                p.x = 244;
                p.y = 112;
                gumpId = 0x0802;
            }
            Add(new CGUIGumppic(gumpId, 0, 0));
            auto &io = g_Game.m_GumpDataIndex[gumpId];
            int xOffset = 0;

            if (io.DataSize == 169884 && io.Height == 171 && io.Width == 409)
            {
                static const int ROW_0_Y = 26;
                static const int ROW_1_Y = 56;
                static const int ROW_2_Y = 86;
                static const int ROW_3_Y = 116;
                static const int ROW_HEIGHT = 24;
                static const int ROW_PADDING = 2;

                static const int LOCKER_COLUMN_X = 10;
                static const int LOCKER_COLUMN_WIDTH = 10;

                static const int COLUMN_1_X = 20;
                static const int COLUMN_1_WIDTH = 80;
                static const int COLUMN_1_ICON_WIDTH = 35;

                static const int COLUMN_2_X = 100;
                static const int COLUMN_2_WIDTH = 60;
                static const int COLUMN_2_ICON_WIDTH = 20;

                static const int COLUMN_3_X = 160;
                static const int COLUMN_3_WIDTH = 60;
                static const int COLUMN_3_ICON_WIDTH = 30;

                static const int COLUMN_4_X = 220;
                static const int COLUMN_4_WIDTH = 80;
                static const int COLUMN_4_ICON_WIDTH = 35;

                static const int COLUMN_5_X = 300;
                static const int COLUMN_5_WIDTH = 80;
                static const int COLUMN_5_ICON_WIDTH = 55;

                Add(new CGUIGumppic(0x0805, 34, 12));
                int percent = CalculatePercents(g_Player->MaxHits, g_Player->Hits, 109);
                if (percent > 0)
                {
                    short gumpid = 0x0806;
                    if (g_Player->Poisoned())
                    {
                        gumpid = 0x0808;
                    }
                    else if (g_Player->YellowHits())
                    {
                        gumpid = 0x0809;
                    }

                    Add(new CGUIGumppicTiled(gumpid, 34, 12, percent, 0));
                }

                Add(new CGUIGumppic(0x0805, 34, 25));
                percent = CalculatePercents(g_Player->MaxMana, g_Player->Mana, 109);
                if (percent > 0)
                {
                    Add(new CGUIGumppicTiled(0x0806, 34, 25, percent, 0));
                }

                Add(new CGUIGumppic(0x0805, 34, 38));
                percent = CalculatePercents(g_Player->MaxStam, g_Player->Stam, 109);
                if (percent > 0)
                {
                    Add(new CGUIGumppicTiled(0x0806, 34, 38, percent, 0));
                }

                if (g_Player->GetName().length())
                {
                    text = (CGUIText *)Add(new CGUIText(0x0386, 100, 10));
                    text->CreateTextureA(1, g_Player->GetName(), 320, TS_CENTER);
                }

                Add(new CGUIButton(ID_GSB_BUFF_GUMP, 0x7538, 0x7538, 0x7538, 40, 50));

                char status = g_Player->LockStr;
                short gumpID = 0x0984;
                if (status == 1)
                {
                    gumpID = 0x0986;
                }
                else if (status == 2)
                {
                    gumpID = 0x082C;
                }
                Add(new CGUIButton(
                    ID_GSB_BUFF_LOCKER_STR,
                    gumpID,
                    gumpID,
                    gumpID,
                    LOCKER_COLUMN_X,
                    ROW_1_Y + ROW_HEIGHT - ROW_PADDING));
                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_1_X + COLUMN_1_ICON_WIDTH,
                    ROW_1_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->Str));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_STR, COLUMN_1_X, ROW_1_Y, COLUMN_1_WIDTH, ROW_HEIGHT));

                status = g_Player->LockDex;
                gumpID = 0x0984;
                if (status == 1)
                {
                    gumpID = 0x0986;
                }
                else if (status == 2)
                {
                    gumpID = 0x082C;
                }
                Add(new CGUIButton(
                    ID_GSB_BUFF_LOCKER_DEX,
                    gumpID,
                    gumpID,
                    gumpID,
                    LOCKER_COLUMN_X,
                    ROW_2_Y + ROW_HEIGHT - ROW_PADDING));
                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_1_X + COLUMN_1_ICON_WIDTH,
                    ROW_2_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->Dex));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_DEX, COLUMN_1_X, ROW_2_Y, COLUMN_1_WIDTH, ROW_HEIGHT));

                status = g_Player->LockInt;
                gumpID = 0x0984;
                if (status == 1)
                {
                    gumpID = 0x0986;
                }
                else if (status == 2)
                {
                    gumpID = 0x082C;
                }
                Add(new CGUIButton(
                    ID_GSB_BUFF_LOCKER_INT,
                    gumpID,
                    gumpID,
                    gumpID,
                    LOCKER_COLUMN_X,
                    ROW_3_Y + ROW_HEIGHT - ROW_PADDING));
                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_1_X + COLUMN_1_ICON_WIDTH,
                    ROW_3_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->Int));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_INT, COLUMN_1_X, ROW_3_Y, COLUMN_1_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH,
                    ROW_1_Y + (ROW_HEIGHT / 2) - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->Hits), 40, TS_CENTER);
                Add(new CGUILine(
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH,
                    ROW_1_Y + 23,
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH + 30,
                    ROW_1_Y + 23,
                    0xFF383838));
                text = (CGUIText *)Add(new CGUIText(
                    0x0386, COLUMN_2_X + COLUMN_2_ICON_WIDTH, ROW_1_Y + ROW_HEIGHT - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->MaxHits), 40, TS_CENTER);
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_HITS, COLUMN_2_X, ROW_1_Y, COLUMN_2_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH,
                    ROW_2_Y + (ROW_HEIGHT / 2) - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->Stam), 40, TS_CENTER);
                Add(new CGUILine(
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH,
                    ROW_2_Y + 23,
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH + 30,
                    ROW_2_Y + 23,
                    0xFF383838));
                text = (CGUIText *)Add(new CGUIText(
                    0x0386, COLUMN_2_X + COLUMN_2_ICON_WIDTH, ROW_2_Y + ROW_HEIGHT - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->MaxStam), 40, TS_CENTER);
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_STAM, COLUMN_2_X, ROW_2_Y, COLUMN_2_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH,
                    ROW_3_Y + (ROW_HEIGHT / 2) - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->Mana), 40, TS_CENTER);
                Add(new CGUILine(
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH,
                    ROW_3_Y + 23,
                    COLUMN_2_X + COLUMN_2_ICON_WIDTH + 30,
                    ROW_3_Y + 23,
                    0xFF383838));
                text = (CGUIText *)Add(new CGUIText(
                    0x0386, COLUMN_2_X + COLUMN_2_ICON_WIDTH, ROW_3_Y + ROW_HEIGHT - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->MaxMana), 40, TS_CENTER);
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_MANA, COLUMN_2_X, ROW_3_Y, COLUMN_2_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_3_X + COLUMN_3_ICON_WIDTH,
                    ROW_1_Y + (ROW_HEIGHT / 2) - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->Followers), 40, TS_CENTER);
                Add(new CGUILine(
                    COLUMN_3_X + COLUMN_3_ICON_WIDTH,
                    ROW_1_Y + 23,
                    COLUMN_3_X + COLUMN_3_ICON_WIDTH + 25,
                    ROW_1_Y + 23,
                    0xFF383838));
                text = (CGUIText *)Add(new CGUIText(
                    0x0386, COLUMN_3_X + COLUMN_3_ICON_WIDTH, ROW_1_Y + ROW_HEIGHT - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->MaxFollowers), 40, TS_CENTER);
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_FOLLOWERS, COLUMN_3_X, ROW_1_Y, COLUMN_3_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_3_X + COLUMN_3_ICON_WIDTH,
                    ROW_2_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->Armor));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_ARMOR, COLUMN_3_X, ROW_2_Y, COLUMN_3_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_3_X + COLUMN_3_ICON_WIDTH,
                    ROW_3_Y + (ROW_HEIGHT / 2) - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->Weight), 40, TS_CENTER);
                Add(new CGUILine(
                    COLUMN_3_X + COLUMN_3_ICON_WIDTH,
                    ROW_3_Y + 23,
                    COLUMN_3_X + COLUMN_3_ICON_WIDTH + 25,
                    ROW_3_Y + 23,
                    0xFF383838));
                text = (CGUIText *)Add(new CGUIText(
                    0x0386, COLUMN_3_X + COLUMN_3_ICON_WIDTH, ROW_3_Y + ROW_HEIGHT - ROW_PADDING));
                text->CreateTextureA(1, std::to_string(g_Player->MaxWeight), 40, TS_CENTER);
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_WEIGHT, COLUMN_3_X, ROW_3_Y, COLUMN_3_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_4_X + COLUMN_4_ICON_WIDTH + 20,
                    ROW_0_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->Luck));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_RESISTANCE_FIRE, COLUMN_4_X, ROW_0_Y, COLUMN_4_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_4_X + COLUMN_4_ICON_WIDTH,
                    ROW_1_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->StatsCap));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_DAMAGE, COLUMN_4_X, ROW_1_Y, COLUMN_4_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_4_X + COLUMN_4_ICON_WIDTH,
                    ROW_2_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(
                    1,
                    std::to_string(g_Player->MinDamage) + "-" +
                        std::to_string(g_Player->MaxDamage));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_DAMAGE, COLUMN_4_X, ROW_2_Y, COLUMN_4_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_4_X + COLUMN_4_ICON_WIDTH,
                    ROW_3_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->Gold));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_GOLD, COLUMN_4_X, ROW_3_Y, COLUMN_4_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_5_X + COLUMN_5_ICON_WIDTH,
                    ROW_0_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->ColdResistance));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_LUCK, COLUMN_5_X, ROW_0_Y, COLUMN_5_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_5_X + COLUMN_5_ICON_WIDTH,
                    ROW_1_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->FireResistance));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_LUCK, COLUMN_5_X, ROW_1_Y, COLUMN_5_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_5_X + COLUMN_5_ICON_WIDTH,
                    ROW_2_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->PoisonResistance));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_LUCK, COLUMN_5_X, ROW_2_Y, COLUMN_5_WIDTH, ROW_HEIGHT));

                text = (CGUIText *)Add(new CGUIText(
                    0x0386,
                    COLUMN_5_X + COLUMN_5_ICON_WIDTH,
                    ROW_3_Y + ROW_HEIGHT - (3 * ROW_PADDING)));
                text->CreateTextureA(1, std::to_string(g_Player->EnergyResistance));
                Add(new CGUIHitBox(
                    ID_GSB_TEXT_LUCK, COLUMN_5_X, ROW_3_Y, COLUMN_5_WIDTH, ROW_HEIGHT));

                Add(new CGUIHitBox(
                    ID_GSB_MINIMIZE,
                    COLUMN_5_X + COLUMN_5_WIDTH + 5,
                    ROW_3_Y + ROW_HEIGHT + 5,
                    20,
                    20,
                    true));
            }
            else if (g_Config.ClientVersion >= CV_308Z && !g_ConfigManager.GetOldStyleStatusbar())
            {
                p.x = 389;
                p.y = 152;

                // Player name
                if (g_Player->GetName().length() != 0u)
                {
                    text = (CGUIText *)Add(new CGUIText(0x0386, useUOPGumps ? 90 : 58, 50));
                    text->CreateTextureA(1, g_Player->GetName(), 320, TS_CENTER);
                }

                if (g_Config.ClientVersion >= CV_5020)
                {
                    Add(new CGUIButton(ID_GSB_BUFF_GUMP, 0x7538, 0x7538, 0x7538, 40, 50));
                }

                if (g_DrawStatLockers)
                {
                    //Str
                    uint8_t status = g_Player->LockStr; // Status (down / up / lock)
                    xOffset = useUOPGumps ? 28 : 40;
                    uint16_t gumpID = 0x0984; //Up
                    if (status == 1)
                    {
                        gumpID = 0x0986; //Down
                    }
                    else if (status == 2)
                    {
                        gumpID = 0x082C; //Lock
                    }

                    Add(new CGUIButton(
                        ID_GSB_BUFF_LOCKER_STR, gumpID, gumpID, gumpID, xOffset, 76));

                    //Dex
                    status = g_Player->LockDex; // Status (down / up / lock)

                    gumpID = 0x0984; //Up
                    if (status == 1)
                    {
                        gumpID = 0x0986; //Down
                    }
                    else if (status == 2)
                    {
                        gumpID = 0x082C; //Lock
                    }

                    Add(new CGUIButton(
                        ID_GSB_BUFF_LOCKER_DEX, gumpID, gumpID, gumpID, xOffset, 102));

                    //Int
                    status = g_Player->LockInt; // Status (down / up / lock)

                    gumpID = 0x0984; //Up
                    if (status == 1)
                    {
                        gumpID = 0x0986; //Down
                    }
                    else if (status == 2)
                    {
                        gumpID = 0x082C; //Lock
                    }

                    Add(new CGUIButton(
                        ID_GSB_BUFF_LOCKER_INT, gumpID, gumpID, gumpID, xOffset, 132));
                }

                if (useUOPGumps)
                {
                    xOffset = 80;
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 161));
                    text->CreateTextureA(1, std::to_string(g_Player->AttackChance));
                    Add(new CGUIHitBox(ID_GSB_TEXT_HIT_CHANCE, 58, 154, 59, 24));
                }
                else
                {
                    xOffset = 88;
                }

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 77));
                text->CreateTextureA(1, std::to_string(g_Player->Str));

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 105));
                text->CreateTextureA(1, std::to_string(g_Player->Dex));

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 133));
                text->CreateTextureA(1, std::to_string(g_Player->Int));

                Add(new CGUIHitBox(ID_GSB_TEXT_STR, 58, 70, 59, 24));
                Add(new CGUIHitBox(ID_GSB_TEXT_DEX, 58, 98, 59, 24));
                Add(new CGUIHitBox(ID_GSB_TEXT_INT, 58, 126, 59, 24));

                int textWidth = 40;
                if (useUOPGumps)
                {
                    xOffset = 150;
                    //Defence chance increase
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 161));
                    text->CreateTextureA(
                        1,
                        std::to_string(g_Player->DefenceChance) + "/" +
                            std::to_string(g_Player->MaxDefenceChance));
                    Add(new CGUIHitBox(ID_GSB_TEXT_DEFENCE_CHANCE, 124, 154, 59, 24));
                }
                else
                {
                    xOffset = 146;
                }
                //Hits
                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 70));
                text->CreateTextureA(1, std::to_string(g_Player->Hits), textWidth, TS_CENTER);

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 83));
                text->CreateTextureA(1, std::to_string(g_Player->MaxHits), textWidth, TS_CENTER);

                //Stam
                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 98));
                text->CreateTextureA(1, std::to_string(g_Player->Stam), textWidth, TS_CENTER);

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 111));
                text->CreateTextureA(1, std::to_string(g_Player->MaxStam), textWidth, TS_CENTER);

                //Mana
                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 126));
                text->CreateTextureA(1, std::to_string(g_Player->Mana), textWidth, TS_CENTER);

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 139));
                text->CreateTextureA(1, std::to_string(g_Player->MaxMana), textWidth, TS_CENTER);

                Add(new CGUILine(xOffset, 138, 185, 138, 0xFF383838));
                Add(new CGUILine(xOffset, 110, 185, 110, 0xFF383838));
                Add(new CGUILine(xOffset, 82, 185, 82, 0xFF383838));

                Add(new CGUIHitBox(ID_GSB_TEXT_HITS, 124, 70, 59, 24));
                Add(new CGUIHitBox(ID_GSB_TEXT_STAM, 124, 98, 59, 24));
                Add(new CGUIHitBox(ID_GSB_TEXT_MANA, 124, 126, 59, 24));

                if (useUOPGumps)
                {
                    xOffset = 240;
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 161));
                    text->CreateTextureA(1, std::to_string(g_Player->LowerManaCost));
                    Add(new CGUIHitBox(ID_GSB_TEXT_LOWER_MANA, 205, 154, 65, 24));
                }
                else
                {
                    xOffset = 220;
                }
                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 77));
                text->CreateTextureA(1, std::to_string(g_Player->StatsCap));

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 105));
                text->CreateTextureA(1, std::to_string(g_Player->Luck));

                //Weights
                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 126));
                text->CreateTextureA(1, std::to_string(g_Player->Weight), textWidth, TS_CENTER);

                Add(new CGUILine(
                    useUOPGumps ? 236 : 216, 138, useUOPGumps ? 270 : 250, 138, 0xFF383838));

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 139));
                text->CreateTextureA(1, std::to_string(g_Player->MaxWeight), textWidth, TS_CENTER);

                xOffset = useUOPGumps ? 205 : 188;
                Add(new CGUIHitBox(ID_GSB_TEXT_TITHING_POINTS, xOffset, 70, 65, 24));
                Add(new CGUIHitBox(ID_GSB_TEXT_LUCK, xOffset, 98, 65, 24));
                Add(new CGUIHitBox(ID_GSB_TEXT_WEIGHT, xOffset, 126, 65, 24));

                if (useUOPGumps)
                {
                    xOffset = 320;
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 105));
                    text->CreateTextureA(1, std::to_string(g_Player->WeaponDamage));
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 161));
                    text->CreateTextureA(1, std::to_string(g_Player->WeaponSpeed));

                    Add(new CGUIHitBox(ID_GSB_TEXT_WEAPON_DMG, 285, 98, 69, 24));
                    Add(new CGUIHitBox(ID_GSB_TEXT_SWING_SPEED, 285, 154, 69, 24));
                }
                else
                {
                    xOffset = 280;
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 105));
                    text->CreateTextureA(1, std::to_string(g_Player->Gold));
                    Add(new CGUIHitBox(ID_GSB_TEXT_GOLD, 260, 98, 69, 24));
                }

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 77));
                text->CreateTextureA(
                    1,
                    std::to_string(g_Player->MinDamage) + "-" +
                        std::to_string(g_Player->MaxDamage));

                text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 133));
                text->CreateTextureA(
                    1,
                    std::to_string(g_Player->Followers) + "/" +
                        std::to_string(g_Player->MaxFollowers));

                xOffset = useUOPGumps ? 285 : 260;
                Add(new CGUIHitBox(ID_GSB_TEXT_DAMAGE, xOffset, 70, 69, 24));
                Add(new CGUIHitBox(ID_GSB_TEXT_FOLLOWERS, xOffset, 126, 69, 24));

                if (useUOPGumps)
                {
                    xOffset = 400;
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 77));
                    text->CreateTextureA(1, std::to_string(g_Player->LowerRegCost));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 105));
                    text->CreateTextureA(1, std::to_string(g_Player->SpellDamage));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 133));
                    text->CreateTextureA(1, std::to_string(g_Player->CastSpeed));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 161));
                    text->CreateTextureA(1, std::to_string(g_Player->CastRecovery));

                    xOffset = 365;
                    Add(new CGUIHitBox(ID_GSB_TEXT_LOWER_REG_COST, xOffset, 70, 55, 24));
                    Add(new CGUIHitBox(ID_GSB_TEXT_SPELL_DMG, xOffset, 98, 55, 24));
                    Add(new CGUIHitBox(ID_GSB_TEXT_FASTER_CASTING, xOffset, 126, 55, 24));
                    Add(new CGUIHitBox(ID_GSB_TEXT_CAST_RECOVERY, xOffset, 154, 55, 24));

                    xOffset = 480;
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 161));
                    text->CreateTextureA(1, std::to_string(g_Player->Gold));
                    Add(new CGUIHitBox(ID_GSB_TEXT_GOLD, 445, 154, 55, 24));

                    xOffset = 475;
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 74));
                    text->CreateTextureA(
                        1,
                        std::to_string(g_Player->Armor) + "/" +
                            std::to_string(g_Player->MaxPhysicalResistance));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 92));
                    text->CreateTextureA(
                        1,
                        std::to_string(g_Player->FireResistance) + "/" +
                            std::to_string(g_Player->MaxFireResistance));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 106));
                    text->CreateTextureA(
                        1,
                        std::to_string(g_Player->ColdResistance) + "/" +
                            std::to_string(g_Player->MaxColdResistance));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 120));
                    text->CreateTextureA(
                        1,
                        std::to_string(g_Player->PoisonResistance) + "/" +
                            std::to_string(g_Player->MaxPoisonResistance));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 134));
                    text->CreateTextureA(
                        1,
                        std::to_string(g_Player->EnergyResistance) + "/" +
                            std::to_string(g_Player->MaxEnergyResistance));
                }
                else
                {
                    xOffset = 354;
                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 76));
                    text->CreateTextureA(1, std::to_string(g_Player->Armor));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 92));
                    text->CreateTextureA(1, std::to_string(g_Player->FireResistance));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 106));
                    text->CreateTextureA(1, std::to_string(g_Player->ColdResistance));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 120));
                    text->CreateTextureA(1, std::to_string(g_Player->PoisonResistance));

                    text = (CGUIText *)Add(new CGUIText(0x0386, xOffset, 134));
                    text->CreateTextureA(1, std::to_string(g_Player->EnergyResistance));
                }

                xOffset = useUOPGumps ? 445 : 334;
                Add(new CGUIHitBox(ID_GSB_TEXT_RESISTANCE_PHYSICAL, xOffset, 76, 40, 14));
                Add(new CGUIHitBox(ID_GSB_TEXT_RESISTANCE_FIRE, xOffset, 92, 40, 14));
                Add(new CGUIHitBox(ID_GSB_TEXT_RESISTANCE_COLD, xOffset, 106, 40, 14));
                Add(new CGUIHitBox(ID_GSB_TEXT_RESISTANCE_POISON, xOffset, 120, 40, 14));
                Add(new CGUIHitBox(ID_GSB_TEXT_RESISTANCE_ENERGY, xOffset, 134, 40, 14));
            }
            else
            {
                if (p.x == 0)
                {
                    p.x = 243;
                    p.y = 150;
                }

                if (g_Player->GetName().length() != 0u)
                {
                    text = (CGUIText *)Add(new CGUIText(0x0386, 86, 42));
                    text->CreateTextureA(1, g_Player->GetName());
                }

                text = (CGUIText *)Add(new CGUIText(0x0386, 86, 61));
                text->CreateTextureA(1, std::to_string(g_Player->Str));

                text = (CGUIText *)Add(new CGUIText(0x0386, 86, 73));
                text->CreateTextureA(1, std::to_string(g_Player->Dex));

                text = (CGUIText *)Add(new CGUIText(0x0386, 86, 85));
                text->CreateTextureA(1, std::to_string(g_Player->Int));

                text = (CGUIText *)Add(new CGUIText(0x0386, 86, 97));
                text->CreateTextureA(1, (g_Player->Gender == GENDER_FEMALE ? "F" : "M"));

                text = (CGUIText *)Add(new CGUIText(0x0386, 86, 109));
                text->CreateTextureA(1, std::to_string(g_Player->Armor));

                text = (CGUIText *)Add(new CGUIText(0x0386, 171, 61));
                text->CreateTextureA(
                    1, std::to_string(g_Player->Hits) + "/" + std::to_string(g_Player->MaxHits));

                text = (CGUIText *)Add(new CGUIText(0x0386, 171, 73));
                text->CreateTextureA(
                    1, std::to_string(g_Player->Mana) + "/" + std::to_string(g_Player->MaxMana));

                text = (CGUIText *)Add(new CGUIText(0x0386, 171, 85));
                text->CreateTextureA(
                    1, std::to_string(g_Player->Stam) + "/" + std::to_string(g_Player->MaxStam));

                text = (CGUIText *)Add(new CGUIText(0x0386, 171, 97));
                text->CreateTextureA(1, std::to_string(g_Player->Gold));

                text = (CGUIText *)Add(new CGUIText(0x0386, 171, 109));
                text->CreateTextureA(1, std::to_string(g_Player->Weight));

                Add(new CGUIHitBox(ID_GSB_TEXT_STR, 86, 61, 34, 12));
                Add(new CGUIHitBox(ID_GSB_TEXT_DEX, 86, 73, 34, 12));
                Add(new CGUIHitBox(ID_GSB_TEXT_INT, 86, 85, 34, 12));
                Add(new CGUIHitBox(ID_GSB_TEXT_SEX, 86, 97, 34, 12));
                Add(new CGUIHitBox(ID_GSB_TEXT_ARMOR, 86, 109, 34, 12));

                Add(new CGUIHitBox(ID_GSB_TEXT_HITS, 171, 61, 66, 12));
                Add(new CGUIHitBox(ID_GSB_TEXT_MANA, 171, 73, 66, 12));
                Add(new CGUIHitBox(ID_GSB_TEXT_STAM, 171, 85, 66, 12));
                Add(new CGUIHitBox(ID_GSB_TEXT_GOLD, 171, 97, 66, 12));
                Add(new CGUIHitBox(ID_GSB_TEXT_WEIGHT, 171, 109, 66, 12));

                if (!g_ConfigManager.GetOldStyleStatusbar())
                {
                    if (g_Config.ClientVersion == CV_308D)
                    {
                        text = (CGUIText *)Add(new CGUIText(0x0386, 171, 124));
                        text->CreateTextureA(1, std::to_string(g_Player->StatsCap));

                        Add(new CGUIHitBox(ID_GSB_TEXT_MAX_STATS, 171, 124, 34, 12));
                    }
                    else if (g_Config.ClientVersion == CV_308J)
                    {
                        text = (CGUIText *)Add(new CGUIText(0x0386, 180, 131));
                        text->CreateTextureA(1, std::to_string(g_Player->StatsCap));

                        text = (CGUIText *)Add(new CGUIText(0x0386, 180, 144));
                        text->CreateTextureA(
                            1,
                            std::to_string(g_Player->Followers) + "/" +
                                std::to_string(g_Player->MaxFollowers));

                        Add(new CGUIHitBox(ID_GSB_TEXT_MAX_STATS, 180, 131, 34, 12));
                        Add(new CGUIHitBox(ID_GSB_TEXT_FOLLOWERS, 171, 144, 34, 12));
                    }
                }
            }

            if (!useUOPGumps)
            {
                Add(new CGUIHitBox(ID_GSB_MINIMIZE, p.x, p.y, 16, 16, true));
            }
            else
            {
                p.x = 540;
                p.y = 180;
                Add(new CGUIHitBox(ID_GSB_MINIMIZE, p.x, p.y, 16, 16, true));
            }
        }
        else // Reduced status bar version
        {
            if (g_Party.Leader != 0 && !g_ConfigManager.GetOriginalPartyStatusbar()) //inParty
            {
                CGUIGumppic *bodyGump = (CGUIGumppic *)Add(new CGUIGumppic(0x0803, 0, 0));
                bodyGump->SelectOnly = true;

                text = (CGUIText *)Add(new CGUIText(0x0386, 16, -2));
                text->CreateTextureA(3, "[* SELF *]");

                Add(new CGUIButton(ID_GSB_BUTTON_HEAL_1, 0x0938, 0x093A, 0x093A, 16, 20));
                Add(new CGUIButton(ID_GSB_BUTTON_HEAL_2, 0x0939, 0x093A, 0x093A, 16, 33));

                int color = 0;
                if (g_Player->Poisoned())
                {
                    color = 63; //Character status line (green)
                }
                else if (g_Player->YellowHits())
                {
                    color = 353; //Character status line (yellow)
                }

                //Hits
                Add(new CGUIGumppic(0x0028, 34, 20));

                int per = CalculatePercents(g_Player->MaxHits, g_Player->Hits, 96);
                Add(new CGUIShader(&g_ColorizerShader, true));
                if (per > 0)
                {
                    CGUIGumppic *gumppic =
                        (CGUIGumppic *)Add(new CGUIGumppicTiled(0x0029, 34, 20, per, 0));
                    gumppic->Color = color;
                }

                //Mana
                Add(new CGUIGumppic(0x0028, 34, 33));

                per = CalculatePercents(g_Player->MaxMana, g_Player->Mana, 96);

                if (per > 0)
                {
                    CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppicTiled(
                        0x0029, 34, 33, per, 0)); //0x0170 green //0x0035 yellow
                    gumppic->Color = 0x0482;
                }

                //Stam
                Add(new CGUIGumppic(0x0028, 34, 45));

                per = CalculatePercents(g_Player->MaxStam, g_Player->Stam, 96);

                if (per > 0)
                {
                    CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppicTiled(
                        0x0029, 34, 45, per, 0)); //0x0170 green //0x0035 yellow
                    gumppic->Color = 0x0075;
                }

                Add(new CGUIShader(&g_ColorizerShader, false));
            }
            else
            {
                if (g_Player->Warmode)
                {
                    Add(new CGUIGumppic(0x0807, 0, 0)); // warmode enabled
                }
                else
                {
                    Add(new CGUIGumppic(0x0803, 0, 0));
                }

                //Hits
                Add(new CGUIGumppic(0x0805, 34, 12));

                int per = CalculatePercents(g_Player->MaxHits, g_Player->Hits, 109);

                if (per > 0)
                {
                    uint16_t gumpid = 0x0806; //Character status line (blue)
                    if (g_Player->Poisoned())
                    {
                        gumpid = 0x0808; //Character status line (green)
                    }
                    else if (g_Player->YellowHits())
                    {
                        gumpid = 0x0809; //Character status line (yellow)
                    }

                    Add(new CGUIGumppicTiled(gumpid, 34, 12, per, 0));
                }

                //Mana
                Add(new CGUIGumppic(0x0805, 34, 25));

                per = CalculatePercents(g_Player->MaxMana, g_Player->Mana, 109);

                if (per > 0)
                {
                    Add(new CGUIGumppicTiled(0x0806, 34, 25, per, 0));
                }

                //Stam
                Add(new CGUIGumppic(0x0805, 34, 38));

                per = CalculatePercents(g_Player->MaxStam, g_Player->Stam, 109);

                if (per > 0)
                {
                    Add(new CGUIGumppicTiled(0x0806, 34, 38, per, 0));
                }
            }

            m_StatusbarUnlocker = (CGUIButton *)Add(
                new CGUIButton(ID_GSB_BUTTON_REMOVE_FROM_GROUP, 0x082C, 0x082C, 0x082C, 136, 24));
            m_StatusbarUnlocker->CheckPolygone = true;
            m_StatusbarUnlocker->Visible = InGroup();
        }
    }
    else
    {
        if (g_Party.Contains(Serial) && !g_ConfigManager.GetOriginalPartyStatusbar())
        {
            for (int i = 0; i < 10; i++)
            {
                if (g_Party.Member[i].Serial == Serial)
                {
                    CPartyObject &member = g_Party.Member[i];
                    if (member.Character == nullptr)
                    {
                        member.Character = g_World->FindWorldCharacter(member.Serial);

                        if (member.Character == nullptr)
                        {
                            break;
                        }
                    }

                    CGUIGumppic *bodyGump = (CGUIGumppic *)Add(new CGUIGumppic(0x0803, 0, 0));
                    bodyGump->SelectOnly = true;

                    string memberName = member.GetName((int)i);
                    bool outofRange = false;
                    if (member.Character->RemovedFromRender())
                    {
                        outofRange = true;
                    }
                    uint16_t textColor =
                        outofRange ?
                            912 :
                            g_ConfigManager.GetColorByNotoriety(member.Character->Notoriety);

                    Add(new CGUIHitBox(ID_GSB_TEXT_FIELD, 16, -2, 109, 16));
                    m_Entry = (CGUITextEntry *)Add(new CGUITextEntry(
                        ID_GSB_TEXT_FIELD,
                        textColor,
                        textColor,
                        textColor,
                        16,
                        -2,
                        125,
                        false,
                        1,
                        TS_LEFT,
                        UOFONT_FIXED));
                    m_Entry->m_Entry.SetTextA(memberName);
                    m_Entry->CheckOnSerial = true;

                    if (member.Character->CanChangeName)
                    {
                        m_Entry->ReadOnly = false;
                    }
                    else
                    {
                        m_Entry->ReadOnly = true;
                    }

                    if (!outofRange)
                    {
                        Add(new CGUIButton(ID_GSB_BUTTON_HEAL_1, 0x0938, 0x093A, 0x093A, 16, 20));
                        Add(new CGUIButton(ID_GSB_BUTTON_HEAL_2, 0x0939, 0x093A, 0x093A, 16, 33));
                    }
                    int color = 0;
                    if (member.Character->Poisoned())
                    {
                        color = 63; //Character status line (green)
                    }
                    else if (member.Character->YellowHits())
                    {
                        color = 353; //Character status line (yellow)
                    }

                    if (outofRange)
                    {
                        color = 912;
                    }
                    Add(new CGUIShader(&g_ColorizerShader, true));
                    //Hits
                    CGUIGumppic *g = new CGUIGumppic(0x0028, 34, 20);
                    g->Color = outofRange ? color : 0;
                    Add(g);

                    int per =
                        CalculatePercents(member.Character->MaxHits, member.Character->Hits, 96);

                    if (per > 0)
                    {
                        CGUIGumppic *gumppic =
                            (CGUIGumppic *)Add(new CGUIGumppicTiled(0x0029, 34, 20, per, 0));
                        gumppic->Color = color;
                    }

                    //Mana
                    g = new CGUIGumppic(0x0028, 34, 33);
                    g->Color = outofRange ? color : 0;
                    Add(g);

                    per = CalculatePercents(member.Character->MaxMana, member.Character->Mana, 96);

                    if (per > 0)
                    {
                        CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppicTiled(
                            0x0029, 34, 33, per, 0)); //0x0170 green //0x0035 yellow
                        gumppic->Color = outofRange ? color : 0x0482;
                    }

                    //Stam
                    g = new CGUIGumppic(0x0028, 34, 45);
                    g->Color = outofRange ? color : 0;
                    Add(g);

                    per = CalculatePercents(member.Character->MaxStam, member.Character->Stam, 96);

                    if (per > 0)
                    {
                        CGUIGumppic *gumppic = (CGUIGumppic *)Add(new CGUIGumppicTiled(
                            0x0029, 34, 45, per, 0)); //0x0170 green //0x0035 yellow
                        gumppic->Color = outofRange ? color : 0x0075;
                    }

                    Add(new CGUIShader(&g_ColorizerShader, false));

                    break;
                }
            }
        }
        else
        {
            Add(new CGUIShader(&g_ColorizerShader, true));

            uint16_t color = 0;
            uint16_t hitsColor = 0x0386;
            uint16_t textColor = 0x0386;
            CGameCharacter *obj = g_World->FindWorldCharacter(Serial);
            string objName = m_Name;
            bool canChangeName = false;

            if (obj != nullptr)
            {
                hitsColor = 0;
                color = g_ConfigManager.GetColorByNotoriety(obj->Notoriety);

                if (obj->Notoriety == NT_CRIMINAL || obj->Notoriety == NT_SOMEONE_GRAY)
                {
                    color = 0;
                }

                objName = obj->GetName();
                m_Name = objName;

                if (obj->CanChangeName)
                {
                    textColor = 0x000E;
                    canChangeName = true;
                }
            }

            m_Body = (CGUIGumppic *)Add(new CGUIGumppic(0x0804, 0, 0));
            m_Body->Color = color;

            m_HitsBody = (CGUIGumppic *)Add(new CGUIGumppic(0x0805, 34, 38));
            m_HitsBody->Color = hitsColor;

            Add(new CGUIShader(&g_ColorizerShader, false));

            if (obj != nullptr && obj->MaxHits > 0)
            {
                int per = CalculatePercents(obj->MaxHits, obj->Hits, 109);

                if (per > 0)
                {
                    uint16_t gumpid = 0x0806; //Character status line (blue)
                    if (obj->Poisoned())
                    {
                        gumpid = 0x0808; //Character status line (green)
                    }
                    else if (obj->YellowHits())
                    {
                        gumpid = 0x0809; //Character status line (yellow)
                    }

                    Add(new CGUIGumppicTiled(gumpid, 34, 38, per, 0));
                }
            }

            Add(new CGUIHitBox(ID_GSB_TEXT_FIELD, 16, 14, 109, 16));
            m_Entry = (CGUITextEntry *)Add(new CGUITextEntry(
                ID_GSB_TEXT_FIELD,
                textColor,
                textColor,
                textColor,
                16,
                14,
                125,
                false,
                1,
                TS_LEFT,
                UOFONT_FIXED));
            m_Entry->m_Entry.SetTextA(objName);
            m_Entry->CheckOnSerial = true;

            if (canChangeName)
            {
                m_Entry->ReadOnly = false;
            }
            else
            {
                m_Entry->ReadOnly = true;
            }
        }

        m_StatusbarUnlocker = (CGUIButton *)Add(
            new CGUIButton(ID_GSB_BUTTON_REMOVE_FROM_GROUP, 0x082C, 0x082C, 0x082C, 136, 24));
        m_StatusbarUnlocker->CheckPolygone = true;
        m_StatusbarUnlocker->Visible = InGroup();
    }

    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Serial != ID_GSB_MINIMIZE)
        {
            item->MoveOnDrag = true;
        }
    }
}

void CGumpStatusbar::OnLeftMouseButtonDown()
{
    DEBUG_TRACE_FUNCTION;
    if (g_GeneratedMouseDown)
    {
        return;
    }

    if (g_Target.IsTargeting())
    {
        g_Target.SendTargetObject(Serial);
        g_MouseManager.CancelDoubleClick = true;
    }
    else
    {
        CGump::OnLeftMouseButtonDown();
    }
}

void CGumpStatusbar::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (g_GeneratedMouseDown)
    {
        return;
    }

    if (serial == ID_GSB_MINIMIZE && Serial == g_PlayerSerial)
    {
        Minimized = true;
        WantUpdateContent = true;
    }
    else if (serial == ID_GSB_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
    else if (serial == ID_GSB_BUTTON_HEAL_1)
    {
        g_Game.CastSpell(29);
        g_PartyHelperTimer = g_Ticks + 500;
        g_PartyHelperTarget = Serial;
    }
    else if (serial == ID_GSB_BUTTON_HEAL_2)
    {
        g_Game.CastSpell(11);
        g_PartyHelperTimer = g_Ticks + 500;
        g_PartyHelperTarget = Serial;
    }
    else if (serial == ID_GSB_BUTTON_REMOVE_FROM_GROUP)
    {
        CGumpStatusbar *oldGroup = m_GroupNext;

        if (oldGroup == nullptr)
        {
            oldGroup = m_GroupPrev;
        }

        RemoveFromGroup();

        if (oldGroup != nullptr)
        {
            oldGroup->UpdateGroup(0, 0);
            oldGroup->WantRedraw = true;
        }
    }
    else if (serial == ID_GSB_BUFF_GUMP)
    {
        g_ConfigManager.ToggleBufficonWindow = true;
    }
    else if (serial == ID_GSB_BUFF_LOCKER_STR)
    {
        g_Player->LockStr = (g_Player->LockStr + 1) % 3;
        WantUpdateContent = true;

        CPacketChangeStatLockStateRequest(0, g_Player->LockStr).Send();
    }
    else if (serial == ID_GSB_BUFF_LOCKER_DEX)
    {
        g_Player->LockDex = (g_Player->LockDex + 1) % 3;
        WantUpdateContent = true;

        CPacketChangeStatLockStateRequest(1, g_Player->LockDex).Send();
    }
    else if (serial == ID_GSB_BUFF_LOCKER_INT)
    {
        g_Player->LockInt = (g_Player->LockInt + 1) % 3;
        WantUpdateContent = true;

        CPacketChangeStatLockStateRequest(2, g_Player->LockInt).Send();
    }
}

bool CGumpStatusbar::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    if (g_GeneratedMouseDown)
    {
        return false;
    }

    if ((g_PressedObject.LeftSerial == 0u) && Serial == g_PlayerSerial && Minimized)
    {
        Minimized = false;

        if (InGroup())
        {
            CGumpStatusbar *oldGroup = m_GroupNext;

            if (oldGroup == nullptr)
            {
                oldGroup = m_GroupPrev;
            }

            RemoveFromGroup();

            if (oldGroup != nullptr)
            {
                oldGroup->UpdateGroup(0, 0);
                oldGroup->WantRedraw = true;
            }
        }

        WantUpdateContent = true;

        return true;
    }
    if (Serial != g_PlayerSerial)
    {
        if (g_Player->Warmode)
        {
            g_Game.Attack(Serial);
        }
        else
        {
            g_Game.DoubleClick(Serial);
        }
        return true;
    }
    if (!Minimized)
    {
        g_Game.PaperdollReq(Serial);
        return true;
    }

    return false;
}

void CGumpStatusbar::OnTextInput(const TextEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    if (Serial != g_PlayerSerial)
    {
        string str = g_EntryPointer->c_str();
        if (g_EntryPointer->Pos() > 0)
        {
            str.resize(g_EntryPointer->Pos());
        }
        else
        {
            str = "";
        }

        const auto ch = EvChar(ev);
        if ((g_EntryPointer->Length() <= 15) && g_FontManager.GetWidthA(1, str) <= 100 &&
            ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')))
        {
            g_EntryPointer->Insert(ch);
            WantRedraw = true;
        }
    }
}

void CGumpStatusbar::OnKeyDown(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    const auto key = EvKey(ev);
    switch (key)
    {
        case KEY_RETURN:
        case KEY_RETURN2:
        {
            if (g_EntryPointer->Length() != 0u)
            {
                SendRenameRequest();
            }
            else
            {
                CGameObject *obj = g_World->FindWorldObject(Serial);
                if (obj != nullptr)
                {
                    g_EntryPointer->SetTextA(obj->GetName());
                }
            }

            if (g_ConfigManager.GetConsoleNeedEnter())
            {
                g_EntryPointer = nullptr;
            }
            else
            {
                g_EntryPointer = &g_GameConsole;
            }

            WantRedraw = true;
            break;
        }
        case KEY_HOME:
        case KEY_LEFT:
        case KEY_RIGHT:
        case KEY_BACK:
        case KEY_DELETE:
        case KEY_END:
        {
            g_EntryPointer->OnKey(this, key);
            WantRedraw = true;
            break;
        }
        case KEY_ESCAPE:
        {
            CGameObject *obj = g_World->FindWorldObject(Serial);
            if (obj != nullptr)
            {
                g_EntryPointer->SetTextA(obj->GetName());
            }

            if (g_ConfigManager.GetConsoleNeedEnter())
            {
                g_EntryPointer = nullptr;
            }
            else
            {
                g_EntryPointer = &g_GameConsole;
            }

            WantRedraw = true;
            break;
        }
        default:
            break;
    }
}

void CGumpStatusbar::SendRenameRequest()
{
    DEBUG_TRACE_FUNCTION;
    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Type != GOT_TEXTENTRY)
        {
            continue;
        }

        CEntryText *entry = &((CGUITextEntry *)item)->m_Entry;
        if (entry->Length() != 0u)
        {
            CPacketRenameRequest(Serial, entry->c_str()).Send();
        }
    }
}
