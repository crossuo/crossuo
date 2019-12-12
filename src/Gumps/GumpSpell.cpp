// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpSpell.h"
#include "../CrossUO.h"
#include "../ToolTip.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../Managers/MouseManager.h"
#include "../Managers/GumpManager.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/ClilocManager.h"

CGumpSpell::CGumpSpell(
    uint32_t serial, short x, short y, uint16_t graphic, SPELLBOOK_TYPE spellType)
    : CGump(GT_SPELL, serial, x, y)
    , SpellType(spellType)
{
    Graphic = graphic;
    m_Locker.Serial = ID_GS_LOCK_MOVING;
    BigIcon = false; // (graphic >= 0x5300 && graphic < 0x5500);

    m_Blender = (CGUIAlphaBlending *)Add(new CGUIAlphaBlending(
        g_ConfigManager.TransparentSpellIcons != 0u, g_ConfigManager.GetSpellIconAlpha() / 255.0f));
    Add(new CGUIGumppic(Graphic, 0, 0));

    /*if (BigIcon)
		m_SpellUnlocker = (CGUIButton*)Add(new CGUIButton(ID_GS_BUTTON_REMOVE_FROM_GROUP, 0x082C, 0x082C, 0x082C, 56, 30));
	else*/
    m_SpellUnlocker = (CGUIButton *)Add(
        new CGUIButton(ID_GS_BUTTON_REMOVE_FROM_GROUP, 0x082C, 0x082C, 0x082C, 30, 16));

    m_SpellUnlocker->BoundingBoxCheck = true;
    m_SpellUnlocker->Visible = false;

    Add(new CGUIAlphaBlending(false, 0.0f));
}

CGumpSpell::~CGumpSpell()
{
    RemoveFromGroup();
}

void CGumpSpell::InitToolTip()
{
    if (g_SelectedObject.Serial == ID_GS_BUTTON_REMOVE_FROM_GROUP)
    {
        g_ToolTip.Set(L"Remove spell from group", 80);
    }
    else
    {
        int tooltipOffset = 0;
        int spellIndexOffset = 0;

        GetTooltipSpellInfo(tooltipOffset, spellIndexOffset);

        g_ToolTip.Set(
            g_ClilocManager.Cliloc(g_Language)
                ->GetW(tooltipOffset - spellIndexOffset + Serial, true),
            80);
    }
}

void CGumpSpell::GetTooltipSpellInfo(int &tooltipOffset, int &spellIndexOffset)
{
    switch (SpellType)
    {
        case ST_MAGE:
        {
            tooltipOffset = 3002010;
            spellIndexOffset = 0;

            break;
        }
        case ST_NECRO:
        {
            tooltipOffset = 1060508;
            spellIndexOffset = 64;

            break;
        }
        case ST_PALADIN:
        {
            tooltipOffset = 1060584;
            spellIndexOffset = 81;

            break;
        }
        case ST_BUSHIDO:
        {
            tooltipOffset = 1060594;
            spellIndexOffset = 91;

            break;
        }
        case ST_NINJITSU:
        {
            tooltipOffset = 1060609;
            spellIndexOffset = 97;

            break;
        }
        case ST_SPELL_WEAVING:
        {
            tooltipOffset = 1071025;
            spellIndexOffset = 105;

            break;
        }
        case ST_MYSTICISM: //?
        {
            tooltipOffset = 0;
            spellIndexOffset = 0;

            break;
        }
        default:
            break;
    }
}

void CGumpSpell::PrepareContent()
{
    bool wantBlender =
        ((g_ConfigManager.TransparentSpellIcons != 0u) && g_SelectedObject.Gump != this);

    if (m_Blender->Enabled != wantBlender)
    {
        m_Blender->Enabled = wantBlender;
        WantRedraw = true;
    }

    bool wantUnlocker = (g_ShiftPressed && InGroup());

    if (m_SpellUnlocker->Visible != wantUnlocker)
    {
        m_SpellUnlocker->Visible = wantUnlocker;
        WantRedraw = true;
    }
}

CGumpSpell *CGumpSpell::GetTopSpell()
{
    if (!InGroup())
    {
        return nullptr;
    }

    if (m_GroupPrev == nullptr)
    {
        return this;
    }

    CGumpSpell *gump = m_GroupPrev;

    while (gump != nullptr && gump->m_GroupPrev != nullptr)
    {
        gump = gump->m_GroupPrev;
    }

    return gump;
}

CGumpSpell *CGumpSpell::GetNearSpell(int &x, int &y)
{
    if (InGroup())
    {
        return nullptr;
    }

    int gumpWidth = 44;
    int gumpHeight = 44;

    int rangeX = 22;
    int rangeY = 22;
    int rangeOffsetX = 30;
    int rangeOffsetY = 30;

    if (BigIcon)
    {
        gumpWidth = 70;
        gumpHeight = 70;

        rangeX = 35;
        rangeY = 35;
        rangeOffsetX = 42;
        rangeOffsetY = 42;
    }

    CGump *gump = (CGump *)g_GumpManager.m_Items;

    while (gump != nullptr)
    {
        if (gump != this && gump->GumpType == GT_SPELL && ((CGumpSpell *)gump)->BigIcon == BigIcon)
        {
            int gumpX = gump->GetX();
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

            int gumpY = gump->GetY();

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
                    if (testGump != this && testGump->GumpType == GT_SPELL &&
                        ((CGumpSpell *)testGump)->BigIcon == BigIcon)
                    {
                        if (testGump->GetX() == testX && testGump->GetY() == testY)
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

    return (CGumpSpell *)gump;
}

bool CGumpSpell::GetSpellGroupOffset(int &x, int &y)
{
    if (InGroup() && g_MouseManager.LeftButtonPressed && g_PressedObject.LeftGump != nullptr &&
        (g_PressedObject.LeftSerial == 0u))
    {
        CGumpSpell *gump = GetTopSpell();

        while (gump != nullptr)
        {
            //Если гамп захватили и (может быть) двигают
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

void CGumpSpell::UpdateGroup(int x, int y)
{
    if (!InGroup())
    {
        return;
    }

    CGumpSpell *gump = GetTopSpell();

    while (gump != nullptr)
    {
        if (gump != this)
        {
            gump->SetX(gump->GetX() + x);
            gump->SetY(gump->GetY() + y);

            g_GumpManager.MoveToBack(gump);
            //gump->WantRedraw = true;
        }

        gump = gump->m_GroupNext;
    }

    g_GumpManager.MoveToBack(this);
}

void CGumpSpell::AddSpell(CGumpSpell *spell)
{
    if (m_GroupNext == nullptr)
    {
        m_GroupNext = spell;
        spell->m_GroupPrev = this;
        spell->m_GroupNext = nullptr;
    }
    else
    {
        CGumpSpell *gump = m_GroupNext;

        while (gump != nullptr && gump->m_GroupNext != nullptr)
        {
            gump = gump->m_GroupNext;
        }

        gump->m_GroupNext = spell;
        spell->m_GroupPrev = gump;
        spell->m_GroupNext = nullptr;
    }

    if (spell->m_SpellUnlocker != nullptr)
    {
        spell->m_SpellUnlocker->Visible = spell->InGroup();
        spell->WantRedraw = true;
    }

    if (m_SpellUnlocker != nullptr)
    {
        m_SpellUnlocker->Visible = InGroup();

        WantRedraw = true;
    }
}

void CGumpSpell::RemoveFromGroup()
{
    if (m_GroupNext != nullptr)
    {
        m_GroupNext->WantRedraw = true;
        m_GroupNext->m_GroupPrev = m_GroupPrev;

        if (m_GroupNext->m_SpellUnlocker != nullptr)
        {
            m_GroupNext->m_SpellUnlocker->Visible = m_GroupNext->InGroup();
        }
    }

    if (m_GroupPrev != nullptr)
    {
        m_GroupPrev->WantRedraw = true;
        m_GroupPrev->m_GroupNext = m_GroupNext;

        if (m_GroupPrev->m_SpellUnlocker != nullptr)
        {
            m_GroupPrev->m_SpellUnlocker->Visible = m_GroupPrev->InGroup();
        }
    }

    m_GroupNext = nullptr;
    m_GroupPrev = nullptr;

    if (m_SpellUnlocker != nullptr)
    {
        m_SpellUnlocker->Visible = InGroup();
        WantRedraw = true;
    }
}

void CGumpSpell::CalculateGumpState()
{
    CGump::CalculateGumpState();

    //Если гамп захватили и (может быть) двигают
    if ((g_GumpMovingOffset.X != 0) || (g_GumpMovingOffset.Y != 0))
    {
        if (!InGroup())
        {
            int testX = g_MouseManager.Position.X;
            int testY = g_MouseManager.Position.Y;

            if (GetNearSpell(testX, testY) != nullptr)
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

        GetSpellGroupOffset(x, y);

        g_GumpTranslate.X = (float)x;
        g_GumpTranslate.Y = (float)y;
    }
}

void CGumpSpell::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GS_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
    else if (serial == ID_GS_BUTTON_REMOVE_FROM_GROUP)
    {
        CGumpSpell *oldGroup = m_GroupNext;

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
}

bool CGumpSpell::OnLeftMouseButtonDoubleClick()
{
    int tooltipOffset = 0;
    int spellIndexOffset = 0;

    GetTooltipSpellInfo(tooltipOffset, spellIndexOffset);

    int spellIndex = Serial - spellIndexOffset + ((int)SpellType * 100);

    g_Game.CastSpell(spellIndex);

    return true;
}
