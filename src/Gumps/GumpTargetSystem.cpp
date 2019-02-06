// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpTargetSystem.h"
#include "../CrossUO.h"
#include "../Target.h"
#include "../TargetGump.h"
#include "../PressedObject.h"
#include "../Managers/MouseManager.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/ClilocManager.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/GamePlayer.h"

CGumpTargetSystem::CGumpTargetSystem(uint32_t serial, short x, short y)
    : CGump(GT_TARGET_SYSTEM, serial, x, y)
{
    m_Locker.Serial = ID_GSB_LOCK_MOVING;
}

CGumpTargetSystem::~CGumpTargetSystem()
{
    g_NewTargetSystem.GumpX = m_X;
    g_NewTargetSystem.GumpY = m_Y;
}

bool CGumpTargetSystem::CanBeDisplayed()
{
    return !(g_ConfigManager.DisableNewTargetSystem || (g_NewTargetSystem.Serial == 0u));
}

void CGumpTargetSystem::PrepareContent()
{
    //Если гамп захватили и (может быть) двигают
    /*if (g_Target.IsTargeting())
	{
		if (g_GumpMovingOffsetX &&)
			g_GeneratedMouseDown = true;
		else if (g_GumpMovingOffsetY && Target.IsTargeting())
			g_GeneratedMouseDown = true;
	}*/
}

void CGumpTargetSystem::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    if (g_ConfigManager.DisableNewTargetSystem || (g_NewTargetSystem.Serial == 0u))
    {
        return;
    }

    if (m_Items == nullptr)
    {
        m_Body = (CGUIGumppic *)Add(new CGUIGumppic(0x0804, 0, 0));
        m_Body->SelectOnly = true;
        m_DataBox = (CGUIDataBox *)Add(new CGUIDataBox());
    }

    CGameObject *obj = g_World->FindWorldObject(g_NewTargetSystem.Serial);

    if (obj != nullptr)
    {
        //Вычисляем цвет статусбара
        uint16_t color = 0;
        CGameCharacter *character = nullptr;

        if (obj->NPC)
        {
            character = obj->GameCharacterPtr();
            NOTORIETY_TYPE noto = (NOTORIETY_TYPE)character->Notoriety;

            color = g_ConfigManager.GetColorByNotoriety(noto);

            if (noto == NT_CRIMINAL || noto == NT_SOMEONE_GRAY)
            {
                color = 0;
            }
        }

        if (color != 0u)
        {
            m_DataBox->Add(new CGUIShader(&g_ColorizerShader, true));
        }

        //Гамп статус бара
        CGUIGumppic *gumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x0804, 0, 0));
        gumppic->Color = color;

        if (color != 0u)
        {
            m_DataBox->Add(new CGUIShader(&g_ColorizerShader, false));
        }

        OldName = obj->GetName();

        if (!obj->NPC && (OldName.length() == 0u))
        {
            STATIC_TILES *st = obj->StaticGroupObjectPtr()->GetStaticData();

            OldName =
                g_ClilocManager.Cliloc(g_Language)->GetA(1020000 + obj->Graphic, true, st->Name);
        }

        CGUIText *text = (CGUIText *)m_DataBox->Add(new CGUIText(0x0386, 16, 14));
        text->CreateTextureA(1, OldName, 150, TS_LEFT, UOFONT_FIXED);

        //Hits
        m_DataBox->Add(new CGUIGumppic(0x0805, 34, 38));

        if (character != nullptr)
        {
            int per = CalculatePercents(character->MaxHits, character->Hits, 109);

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

                m_DataBox->Add(new CGUIGumppicTiled(gumpid, 34, 38, per, 0));
            }
        }
    }
    else //Серенький статус
    {
        m_DataBox->Add(new CGUIShader(&g_ColorizerShader, true));

        //Гамп статус бара
        CGUIGumppic *gumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x0804, 0, 0));
        gumppic->Color = 0x0386;

        //Hits
        gumppic = (CGUIGumppic *)m_DataBox->Add(new CGUIGumppic(0x0805, 34, 38));
        gumppic->Color = 0x0386;

        m_DataBox->Add(new CGUIShader(&g_ColorizerShader, false));

        CGUIText *text = (CGUIText *)m_DataBox->Add(new CGUIText(0x0386, 16, 14));
        text->CreateTextureA(1, OldName, 150, TS_LEFT, UOFONT_FIXED);
    }
}

void CGumpTargetSystem::OnLeftMouseDown()
{
    DEBUG_TRACE_FUNCTION;
    if (g_GeneratedMouseDown)
    {
        return;
    }

    if (g_PressedObject.LeftSerial == 0u)
    {
        //Проверим, может быть есть таргет, который нужно повесить на данного чара
        if (g_Target.IsTargeting())
        {
            g_Target.SendTargetObject(g_NewTargetSystem.Serial);
            g_MouseManager.CancelDoubleClick = true;
        }
    }
}

void CGumpTargetSystem::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (g_GeneratedMouseDown)
    {
        return;
    }

    if (serial == ID_GSB_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
        g_MouseManager.CancelDoubleClick = true;
    }
}

bool CGumpTargetSystem::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    if (g_GeneratedMouseDown)
    {
        return false;
    }

    uint32_t serial = g_NewTargetSystem.Serial;

    if (serial != g_PlayerSerial)
    {
        if (g_Player->Warmode)
        {
            if (serial < 0x40000000)
            {
                g_Game.Attack(serial); //Если в вармоде - атакуем
            }
        }
        else
        {
            g_Game.DoubleClick(serial); //Или используем предмет
        }

        return true;
    }

    return false;
}
