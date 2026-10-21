// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <common/str.h>
#include "GumpSkill.h"
#include "../CrossUO.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../ToolTip.h"
#include "../Managers/GumpManager.h"
#include "../Managers/MouseManager.h"
#include "../Managers/SkillsManager.h"

CGumpSkill::CGumpSkill(int serial, int x, int y)
    : CGump(GT_SKILL, serial, x, y)
{
    m_Locker.Serial = ID_GS_LOCK_MOVING;
    CGUIResizepic *resizepic = (CGUIResizepic *)Add(new CGUIResizepic(0, 0x24EA, 0, 0, 140, 20));
    resizepic->DrawOnly = true;
    if (Serial < (uint32_t)g_SkillsManager.Count)
    {
        CGUIText *text = (CGUIText *)Add(new CGUIText(0x0481, 10, 5));
        CSkill *skill = g_SkillsManager.Get(Serial);
        if (skill != nullptr)
        {
            text->CreateTextureW(1, wstr_from(skill->Name), 30, 120, TS_CENTER);
        }
        resizepic->Height = 10 + text->m_Texture.Height;
    }
    m_Width = (int)resizepic->Width;
    m_Height = (int)resizepic->Height;
    m_SkillUnlocker = (CGUIButton *)Add(
        new CGUIButton(ID_GS_BUTTON_REMOVE_FROM_GROUP, 0x082C, 0x082C, 0x082C, 122, 2));
    m_SkillUnlocker->BoundingBoxCheck = true;
    m_SkillUnlocker->Visible = false;}

CGumpSkill::~CGumpSkill()
{
    RemoveFromGroup();
}

void CGumpSkill::InitToolTip()
{
    if (g_SelectedObject.Serial == ID_GS_BUTTON_REMOVE_FROM_GROUP)
    {
        g_ToolTip.Set(L"Remove skill from group", 80);
    }
}

void CGumpSkill::PrepareContent()
{
    // the skill value: skill data arrives from the server after the gump can
    // already exist (drag-out preview), so the text is maintained here every
    // frame instead of once in the constructor; right-aligned in a fixed box
    // hugging the right edge (same format and colors as the skills list rows)
    if (Serial < (uint32_t)g_SkillsManager.Count)
    {
        CSkill *skill = g_SkillsManager.Get(Serial);
        if (skill != nullptr && skill->Value != m_LastDrawnValue)
        {
            m_LastDrawnValue = skill->Value;
            if (m_ValueText == nullptr)
            {
                m_ValueText = (CGUIText *)Add(new CGUIText(0x0288, 0, 0));
            }
            char sbf[10] = { 0 };
            snprintf(sbf, sizeof(sbf), "%.1f", skill->Value);
            m_ValueText->CreateTextureA(9, sbf, 60, TS_RIGHT);
            m_ValueText->SetX(m_Width - 60 - 2);
            m_ValueText->SetY((m_Height - (int)m_ValueText->m_Texture.Height) / 2);
        }
    }

    const bool wantUnlocker = g_ShiftPressed && InGroup();
    if (m_SkillUnlocker->Visible != wantUnlocker)
    {
        m_SkillUnlocker->Visible = wantUnlocker;
        WantRedraw = true;
    }
}

CGumpSkill *CGumpSkill::GetTopSkill()
{
    if (!InGroup())
    {
        return nullptr;
    }

    if (m_GroupPrev == nullptr)
    {
        return this;
    }

    CGumpSkill *gump = m_GroupPrev;
    while (gump != nullptr && gump->m_GroupPrev != nullptr)
    {
        gump = gump->m_GroupPrev;
    }
    return gump;
}

// true when no other skill gump already sits at the given top-left (the snap
// target must be empty, mirroring the spell grid behavior)
static bool SkillSpotFree(CGumpSkill *self, int x, int y)
{
    CGump *gump = (CGump *)g_GumpManager.m_Items;
    while (gump != nullptr)
    {
        if (gump != self && gump->GumpType == GT_SKILL && gump->GetX() == x && gump->GetY() == y)
        {
            return false;
        }
        gump = (CGump *)gump->m_Next;
    }
    return true;
}

CGumpSkill *CGumpSkill::GetNearSkill(int &x, int &y)
{
    if (InGroup())
    {
        return nullptr;
    }

    // skill gumps vary in size: the dragged box snaps to the empty spot
    // adjacent to the nearest skill gump's edge, computed from that gump's
    // own dimensions
    const int range = 30;
    CGump *gump = (CGump *)g_GumpManager.m_Items;
    while (gump != nullptr)
    {
        if (gump != this && gump->GumpType == GT_SKILL)
        {
            CGumpSkill *skillGump = (CGumpSkill *)gump;
            const int gx = skillGump->GetX();
            const int gy = skillGump->GetY();
            const int candidates[4][2] = {
                { gx + skillGump->m_Width, gy },   // right of it
                { gx - m_Width, gy },         // left of it
                { gx, gy + skillGump->m_Height },  // below it
                { gx, gy - m_Height },        // above it
            };
            for (const auto &c : candidates)
            {
                if (abs(x - c[0]) < range && abs(y - c[1]) < range &&
                    SkillSpotFree(this, c[0], c[1]))
                {
                    x = c[0];
                    y = c[1];
                    return skillGump;
                }
            }
        }
        gump = (CGump *)gump->m_Next;
    }
    return nullptr;
}

bool CGumpSkill::GetSkillGroupOffset(int &x, int &y)
{
    if (InGroup() && g_MouseManager.LeftButtonPressed && g_PressedObject.LeftGump != nullptr &&
        g_PressedObject.LeftSerial == 0)
    {
        CGumpSkill *gump = GetTopSkill();
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

void CGumpSkill::UpdateGroup(int x, int y)
{
    if (!InGroup())
    {
        return;
    }

    CGumpSkill *gump = GetTopSkill();
    while (gump != nullptr)
    {
        if (gump != this)
        {
            gump->SetX(gump->GetX() + x);
            gump->SetY(gump->GetY() + y);
            g_GumpManager.MoveToBack(gump);
        }
        gump = gump->m_GroupNext;
    }
    g_GumpManager.MoveToBack(this);
}

void CGumpSkill::AddSkill(CGumpSkill *skill)
{
    if (m_GroupNext == nullptr)
    {
        m_GroupNext = skill;
        skill->m_GroupPrev = this;
        skill->m_GroupNext = nullptr;
    }
    else
    {
        CGumpSkill *gump = m_GroupNext;
        while (gump != nullptr && gump->m_GroupNext != nullptr)
        {
            gump = gump->m_GroupNext;
        }
        gump->m_GroupNext = skill;
        skill->m_GroupPrev = gump;
        skill->m_GroupNext = nullptr;
    }

    if (skill->m_SkillUnlocker != nullptr)
    {
        skill->m_SkillUnlocker->Visible = skill->InGroup();
        skill->WantRedraw = true;
    }

    if (m_SkillUnlocker != nullptr)
    {
        m_SkillUnlocker->Visible = InGroup();
        WantRedraw = true;
    }
}

void CGumpSkill::RemoveFromGroup()
{
    if (m_GroupNext != nullptr)
    {
        m_GroupNext->WantRedraw = true;
        m_GroupNext->m_GroupPrev = m_GroupPrev;
        if (m_GroupNext->m_SkillUnlocker != nullptr)
        {
            m_GroupNext->m_SkillUnlocker->Visible = m_GroupNext->InGroup();
        }
    }

    if (m_GroupPrev != nullptr)
    {
        m_GroupPrev->WantRedraw = true;
        m_GroupPrev->m_GroupNext = m_GroupNext;
        if (m_GroupPrev->m_SkillUnlocker != nullptr)
        {
            m_GroupPrev->m_SkillUnlocker->Visible = m_GroupPrev->InGroup();
        }
    }

    m_GroupNext = nullptr;
    m_GroupPrev = nullptr;
    if (m_SkillUnlocker != nullptr)
    {
        m_SkillUnlocker->Visible = InGroup();
        WantRedraw = true;
    }
}

void CGumpSkill::CalculateGumpState()
{
    CGump::CalculateGumpState();
    if ((g_GumpMovingOffset.X != 0) || (g_GumpMovingOffset.Y != 0))
    {
        if (!InGroup())
        {
            int testX = g_MouseManager.Position.X;
            int testY = g_MouseManager.Position.Y;
            if (GetNearSkill(testX, testY) != nullptr)
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
        GetSkillGroupOffset(x, y);
        g_GumpTranslate.X = (float)x;
        g_GumpTranslate.Y = (float)y;
    }
}

void CGumpSkill::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GS_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
    else if (serial == ID_GS_BUTTON_REMOVE_FROM_GROUP)
    {
        CGumpSkill *oldGroup = m_GroupNext;
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

void CGumpSkill::OnLeftMouseButtonUp()
{
    CGump::OnLeftMouseButtonUp();
    if (SkipNextUse)
    {
        SkipNextUse = false;
        return;
    }
    if (g_SelectedObject.Serial != ID_GS_LOCK_MOVING && Serial < (uint32_t)g_SkillsManager.Count)
    {
        CSkill *skill = g_SkillsManager.Get(Serial);
        if (skill != nullptr && skill->Button)
        {
            CPoint2Di offset = g_MouseManager.LeftDroppedOffset();
            if ((offset.X == 0) && (offset.Y == 0))
            {
                g_Game.UseSkill(Serial);
            }
        }
    }
}
