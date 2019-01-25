// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpSkill.h"
#include "../CrossUO.h"
#include "../SelectedObject.h"
#include "../Managers/MouseManager.h"
#include "../Managers/SkillsManager.h"

CGumpSkill::CGumpSkill(int serial, int x, int y)
    : CGump(GT_SKILL, serial, x, y)
{
    DEBUG_TRACE_FUNCTION;
    m_Locker.Serial = ID_GS_LOCK_MOVING;

    CGUIResizepic *resizepic = (CGUIResizepic *)Add(new CGUIResizepic(0, 0x24EA, 0, 0, 140, 20));
    resizepic->DrawOnly = true;

    if (Serial < (uint32_t)g_SkillsManager.Count)
    {
        CGUIText *text = (CGUIText *)Add(new CGUIText(0x0481, 10, 5));

        CSkill *skill = g_SkillsManager.Get(Serial);

        if (skill != nullptr)
        {
            text->CreateTextureW(1, ToWString(skill->Name), 30, 120, TS_CENTER);
        }

        resizepic->Height = 20 + text->m_Texture.Height;
    }
}

CGumpSkill::~CGumpSkill()
{
}

void CGumpSkill::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (serial == ID_GS_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
}

void CGumpSkill::OnLeftMouseButtonUp()
{
    DEBUG_TRACE_FUNCTION;
    CGump::OnLeftMouseButtonUp();

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
