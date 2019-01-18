// MIT License
// Copyright (C) August 2016 Hotride

#include "GameEffect.h"
#include "GameEffectMoving.h"
#include "../OrionUO.h"
#include "../Managers/EffectManager.h"
#include "../Managers/FileManager.h"

CGameEffect::CGameEffect()
    : CRenderWorldObject(ROT_EFFECT, 0, 0, 0, 0, 0, 0)
{
}

CGameEffect::~CGameEffect()
{
}

void CGameEffect::Draw(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
#if UO_DEBUG_INFO != 0
    g_RenderedObjectsCountInGameWindow++;
#endif

    uint16_t objGraphic = GetCurrentGraphic();

    ApplyRenderMode();

    if (EffectType == EF_MOVING)
    {
        CGameEffectMoving *moving = (CGameEffectMoving *)this;

        int drawEffectX = x + moving->OffsetX;
        int drawEffectY = y + moving->OffsetY + moving->OffsetZ;

        if (moving->FixedDirection)
        {
            g_Orion.DrawStaticArt(objGraphic, Color, drawEffectX, drawEffectY);
        }
        else
        {
            g_Orion.DrawStaticArtRotated(
                objGraphic, Color, drawEffectX, drawEffectY, moving->Angle);
        }
    }
    else if (EffectType == EF_DRAG)
    {
        CGameEffectDrag *dragEffect = (CGameEffectDrag *)this;

        g_Orion.DrawStaticArt(Graphic, Color, x - dragEffect->OffsetX, y - dragEffect->OffsetY);
    }
    else
    {
        g_Orion.DrawStaticArt(objGraphic, Color, x, y);
    }

    RemoveRenderMode();
}

void CGameEffect::Update(CGameObject *parent)
{
    DEBUG_TRACE_FUNCTION;
    if (EffectType != EF_MOVING)
    {
        if (Duration < g_Ticks)
        {
            if (parent != nullptr)
            {
                parent->RemoveEffect(this);
            }
            else
            {
                g_EffectManager.RemoveEffect(this);
            }
        }
        else if (LastChangeFrameTime < g_Ticks)
        {
            LastChangeFrameTime = g_Ticks + Speed;

            if (EffectType == EF_LIGHTING)
            {
                AnimIndex++;

                if (AnimIndex >= 10)
                {
                    if (parent != nullptr)
                    {
                        parent->RemoveEffect(this);
                    }
                    else
                    {
                        g_EffectManager.RemoveEffect(this);
                    }
                }
            }
            else
            {
                CalculateCurrentGraphic();
            }
        }
    }
    else if (LastChangeFrameTime < g_Ticks)
    {
        LastChangeFrameTime = g_Ticks + Speed;

        CalculateCurrentGraphic();
    }
}

uint16_t CGameEffect::CalculateCurrentGraphic()
{
    DEBUG_TRACE_FUNCTION;
    uintptr_t addressAnimData = (uintptr_t)g_FileManager.m_AnimdataMul.Start;

    if (addressAnimData != 0u)
    {
        uint32_t addr = (Graphic * 68) + 4 * ((Graphic / 8) + 1);
        ANIM_DATA *pad = (ANIM_DATA *)(addressAnimData + addr);

        if (AnimIndex < (int)pad->FrameCount)
        {
            Increment = pad->FrameData[AnimIndex];
            AnimIndex++;
        }

        if (AnimIndex >= (int)pad->FrameCount)
        {
            AnimIndex = 0;
        }
    }

    return Graphic + Increment;
}

uint16_t CGameEffect::GetCurrentGraphic()
{
    DEBUG_TRACE_FUNCTION;
    return Graphic + Increment;
}

void CGameEffect::ApplyRenderMode()
{
    DEBUG_TRACE_FUNCTION;
    switch (RenderMode)
    {
        case 1: //ok
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);
            break;
        }
        case 2: //ok
        case 3: //ok
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            break;
        }
        case 4: //?
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
            break;
        }
        case 5: //?
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
            break;
        }
        case 6: //ok
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            break;
        }
        default:
            break;
    }
}

void CGameEffect::RemoveRenderMode()
{
    DEBUG_TRACE_FUNCTION;
    switch (RenderMode)
    {
        case 1: //ok
        case 2: //ok
        case 3: //ok
        {
            glDisable(GL_BLEND);
            break;
        }
        case 4:
        {
            glDisable(GL_BLEND);
            break;
        }
        case 5:
        {
            glDisable(GL_BLEND);
            break;
        }
        case 6: //ok
        {
            glDisable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            break;
        }
        default:
            break;
    }
}
