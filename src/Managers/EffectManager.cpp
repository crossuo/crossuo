// MIT License
// Copyright (C) August 2016 Hotride

#include "EffectManager.h"
#include "MapManager.h"
#include "../CrossUO.h"
#include "../GameObjects/GameWorld.h"
#include "../GameObjects/GameEffectMoving.h"

CEffectManager g_EffectManager;

CEffectManager::CEffectManager()
{
}

void CEffectManager::AddEffect(CGameEffect *effect)
{
    DEBUG_TRACE_FUNCTION;
    switch (effect->EffectType)
    {
        case EF_MOVING:
        case EF_STAY_AT_POS:
        case EF_DRAG:
        {
            if (effect->EffectType == EF_MOVING)
            {
                CGameObject *obj = g_World->FindWorldObject(effect->DestSerial);
                if (obj != nullptr)
                {
                    obj = obj->GetTopObject();

                    if (obj != nullptr)
                    {
                        effect->DestX = obj->GetX();
                        effect->DestY = obj->GetY();
                        effect->DestZ = obj->GetZ();
                    }
                }

                CGameEffectMoving *moving = (CGameEffectMoving *)effect;

                if (moving->GetX() == moving->DestX && moving->GetY() == moving->DestY &&
                    moving->GetZ() == moving->DestZ)
                {
                    if (moving->Explode)
                    {
                        EFFECT_TYPE type = EF_STAY_AT_POS;

                        if (obj != nullptr)
                        {
                            type = EF_STAY_AT_SOURCE;
                        }

                        CreateExplodeEffect(moving, type);
                    }

                    delete effect;

                    return;
                }
            }

            if (effect->EffectType != EF_STAY_AT_POS)
            {
                CGameEffectDrag *effectDrag = (CGameEffectDrag *)effect;
                effectDrag->OffsetX = 0;
                effectDrag->OffsetY = 0;
            }

            Add(effect);

            g_MapManager.AddRender(effect);

            effect->Update(nullptr);

            break;
        }
        case EF_LIGHTING:
        case EF_STAY_AT_SOURCE:
        {
            CGameObject *obj = g_World->FindWorldObject(effect->Serial);
            if (obj != nullptr)
            {
                if (effect->EffectType == EF_LIGHTING)
                {
                    g_Game.ExecuteGumpPart(0x4E20, 10);

                    effect->Duration = g_Ticks + 400;
                    effect->Speed = 50;
                }

                obj->AddEffect(effect);
            }
            else
            {
                delete effect;
            }

            break;
        }
        default:
            break;
    }
}

void CEffectManager::RemoveEffect(CGameEffect *effect)
{
    DEBUG_TRACE_FUNCTION;
    Unlink(effect);

    effect->m_Next = nullptr;
    effect->m_Prev = nullptr;
    delete effect;
}

void CEffectManager::CreateExplodeEffect(CGameEffect *effect, const EFFECT_TYPE &type)
{
    DEBUG_TRACE_FUNCTION;
    CGameEffect *newEffect = new CGameEffect();

    newEffect->EffectType = type;
    newEffect->Serial = effect->DestSerial;
    newEffect->SetX(effect->DestX);
    newEffect->SetY(effect->DestY);
    newEffect->SetZ(effect->DestZ);
    newEffect->Graphic = 0x36CB;
    newEffect->Speed = 50;
    newEffect->Duration = g_Ticks + 400;
    newEffect->FixedDirection = effect->FixedDirection;

    newEffect->Color = effect->Color;
    newEffect->RenderMode = effect->RenderMode;

    AddEffect(newEffect);
}

void CEffectManager::UpdateEffects()
{
    DEBUG_TRACE_FUNCTION;
    for (CGameEffect *effect = (CGameEffect *)m_Items; effect != nullptr;)
    {
        CGameEffect *next = (CGameEffect *)effect->m_Next;

        effect->Update(nullptr);

        effect = next;
    }
}

void CEffectManager::RemoveRangedEffects()
{
    DEBUG_TRACE_FUNCTION;
}
