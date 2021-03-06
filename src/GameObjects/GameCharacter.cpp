// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <SDL_timer.h>
#include <xuocore/uodata.h>
#include <common/utils.h>
#include "GameItem.h"
#include "../CrossUO.h"
#include "../TargetGump.h"
#include "../Party.h"
#include "../SelectedObject.h"
#include "../Managers/AnimationManager.h"
#include "../Managers/GumpManager.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MapManager.h"
#include "../Network/Packets.h"
#include "../Gumps/GumpTargetSystem.h"
#include "../Walker/Walker.h"
#include "../Walker/PathFinder.h"
#include "../TextEngine/TextData.h"
#include "../GameObjects/GamePlayer.h"
#include "../Utility/PerfMarker.h"

#define USE_NEW_ANIM_CODE 1

#if USE_NEW_ANIM_CODE

static uint16_t HANDS_BASE_ANIMID[] = { 0x0263, 0x0264, 0x0265, 0x0266, 0x0267, 0x0268, 0x0269,
                                        0x026D, 0x0270, 0x0272, 0x0274, 0x027A, 0x027C, 0x027F,
                                        0x0281, 0x0286, 0x0288, 0x0289, 0x028B, 0 };

static uint16_t HAND2_BASE_ANIMID[] = { 0x0240, 0x0241, 0x0242, 0x0243, 0x0244,
                                        0x0245, 0x0246, 0x03E0, 0x03E1, 0 };

#endif // #if USE_NEW_ANIM_CODE

CGameCharacter::CGameCharacter(int serial)
    : CGameObject(serial)
    , Hits(0)
    , MaxHits(0)
    , LastStepSoundTime(SDL_GetTicks())
    , TimeToRandomFidget(SDL_GetTicks() + RANDOM_FIDGET_ANIMATION_DELAY)
{
    NPC = true;
    NoDrawTile = false;

    bool wantStatusRequest =
        (g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR) != nullptr) ||
        (g_GumpManager.UpdateContent(serial, 0, GT_TARGET_SYSTEM) != nullptr) ||
        (g_ConfigManager.GetDrawStatusState() != 0u) || (serial == g_LastTargetObject) ||
        (serial == g_LastAttackObject);

    if (!g_ConfigManager.DisableNewTargetSystem && g_NewTargetSystem.Serial == serial &&
        g_GumpManager.UpdateContent(serial, 0, GT_TARGET_SYSTEM) == nullptr)
    {
        wantStatusRequest = true;
        g_GumpManager.AddGump(
            new CGumpTargetSystem(Serial, g_NewTargetSystem.GumpX, g_NewTargetSystem.GumpY));
    }

    if (wantStatusRequest)
    {
        CPacketStatusRequest(Serial).Send();
    }
}

CGameCharacter::~CGameCharacter()
{
    //!Чистим память
    m_Steps.clear();

    m_HitsTexture.Clear();

    uint32_t serial = Serial & 0x3FFFFFFF;

    if (g_ConfigManager.RemoveStatusbarsWithoutObjects)
    {
        g_GumpManager.CloseGump(serial, 0, GT_STATUSBAR);
    }
    else
    {
        //!Если стянут статусбар - обновим его
        g_GumpManager.UpdateContent(serial, 0, GT_STATUSBAR);
    }

    //!Если стянут статусбар таргет системы - обновим его
    g_GumpManager.UpdateContent(serial, 0, GT_TARGET_SYSTEM);

    if (!IsPlayer())
    {
        g_GumpManager.CloseGump(serial, 0, GT_PAPERDOLL);
    }
    //Чистим если находился в пати.
    if (g_Party.Contains(serial))
    {
        for (int i = 0; i < 10; i++)
        {
            CPartyObject &member = g_Party.Member[i];

            if (member.Serial != serial)
            {
                continue;
            }

            member.Serial = 0;
            member.Character = nullptr;
        }
    }
}

void CGameCharacter::UpdateTextCoordinates()
{
    CTextData *text = (CTextData *)m_TextControl->Last();

    if (text == nullptr)
    {
        return;
    }

    const AnimationState anim = { this->GetGraphicForAnimation(), 0, 0 };
    const auto dims = g_AnimationManager.GetAnimationDimensions(
        this->AnimIndex, anim, this->IsMounted(), this->IsCorpse());
    int offset = 0;

    int x = DrawX + OffsetX;
    int y = DrawY + (OffsetY - OffsetZ) - ((dims.Height + dims.CenterY) + 8);

    if (g_ConfigManager.GetDrawStatusState() == DCSS_ABOVE ||
        g_ConfigManager.AlwaysDisplayHumanoidsName == true)
    {
        y -= 14;
    }

    for (; text != nullptr; text = (CTextData *)text->m_Prev)
    {
        offset += text->m_TextSprite.Height;

        text->RealDrawX = x - (text->m_TextSprite.Width / 2);
        text->RealDrawY = y - offset;
    }
}

void CGameCharacter::UpdateHitsTexture(uint8_t hits)
{
    if (HitsPercent != hits || m_HitsTexture.Empty())
    {
        HitsPercent = hits;

        char hitsText[10] = { 0 };
        sprintf_s(hitsText, "[%i%%]", hits);

        uint16_t color = 0x0044;

        if (hits < 30)
        {
            color = 0x0021;
        }
        else if (hits < 50)
        {
            color = 0x0030;
        }
        else if (hits < 80)
        {
            color = 0x0058;
        }

        g_FontManager.GenerateA(3, m_HitsTexture, hitsText, color);
    }
}

int CGameCharacter::IsSitting()
{
    int result = 0;
    if (IsHuman() && !IsMounted() && !IsFlying() &&
        !TestStepNoChangeDirection(GetGroupForAnimation(0, true)))
    {
        CRenderWorldObject *obj = this;
        while (obj != nullptr && obj->m_PrevXY != nullptr)
        {
            obj = obj->m_PrevXY;
        }

        while (obj != nullptr && result == 0)
        {
            if (obj->IsStaticGroupObject() && abs(m_Z - obj->GetZ()) <= 1) //m_Z == obj->GetZ()
            {
                uint16_t graphic = obj->Graphic;
                /*
                if (obj->IsGameObject())
                {
                    if (((CGameObject *)obj)->NPC || ((CGameItem *)obj)->MultiBody)
                    {
                        graphic = 0;
                    }
                }
                */
                switch (graphic)
                {
                    case 0x0459:
                    case 0x045A:
                    case 0x045B:
                    case 0x045C:
                    case 0x0A2A:
                    case 0x0A2B:
                    case 0x0B2C:
                    case 0x0B2D:
                    case 0x0B2E:
                    case 0x0B2F:
                    case 0x0B30:
                    case 0x0B31:
                    case 0x0B32:
                    case 0x0B33:
                    case 0x0B4E:
                    case 0x0B4F:
                    case 0x0B50:
                    case 0x0B51:
                    case 0x0B52:
                    case 0x0B53:
                    case 0x0B54:
                    case 0x0B55:
                    case 0x0B56:
                    case 0x0B57:
                    case 0x0B58:
                    case 0x0B59:
                    case 0x0B5A:
                    case 0x0B5B:
                    case 0x0B5C:
                    case 0x0B5D:
                    case 0x0B5E:
                    case 0x0B5F:
                    case 0x0B60:
                    case 0x0B61:
                    case 0x0B62:
                    case 0x0B63:
                    case 0x0B64:
                    case 0x0B65:
                    case 0x0B66:
                    case 0x0B67:
                    case 0x0B68:
                    case 0x0B69:
                    case 0x0B6A:
                    case 0x0B91:
                    case 0x0B92:
                    case 0x0B93:
                    case 0x0B94:
                    case 0x0CF3:
                    case 0x0CF4:
                    case 0x0CF6:
                    case 0x0CF7:
                    case 0x11FC:
                    case 0x1218:
                    case 0x1219:
                    case 0x121A:
                    case 0x121B:
                    case 0x1527:
                    case 0x1771:
                    case 0x1776:
                    case 0x1779:
                    case 0x1DC7:
                    case 0x1DC8:
                    case 0x1DC9:
                    case 0x1DCA:
                    case 0x1DCB:
                    case 0x1DCC:
                    case 0x1DCD:
                    case 0x1DCE:
                    case 0x1DCF:
                    case 0x1DD0:
                    case 0x1DD1:
                    case 0x1DD2:
                    case 0x2A58:
                    case 0x2A59:
                    case 0x2A5A:
                    case 0x2A5B:
                    case 0x2A7F:
                    case 0x2A80:
                    case 0x2DDF:
                    case 0x2DE0:
                    case 0x2DE3:
                    case 0x2DE4:
                    case 0x2DE5:
                    case 0x2DE6:
                    case 0x2DEB:
                    case 0x2DEC:
                    case 0x2DED:
                    case 0x2DEE:
                    case 0x2DF5:
                    case 0x2DF6:
                    case 0x3088:
                    case 0x3089:
                    case 0x308A:
                    case 0x308B:
                    case 0x35ED:
                    case 0x35EE:
                    case 0x3DFF:
                    case 0x3E00:
                    case 0x4C8D:
                    case 0x4C8E:
                    case 0x4C8F:
                    case 0x4C1E:
                    case 0xA05F:
                    case 0xA05E:
                    case 0xA05D:
                    case 0xA05C:
                    case 0x9EA2:
                    case 0x9EA1:
                    case 0x9E9F:
                    case 0x9EA0:
                    case 0x9E91:
                    case 0x9E90:
                    case 0x9E8F:
                    case 0x9E8E:
                    case 0x9C62:
                    case 0x9C61:
                    case 0x9C60:
                    case 0x9C5F:
                    case 0x9C5E:
                    case 0x9C5D:
                    case 0x9C5A:
                    case 0x9C59:
                    case 0x9C58:
                    case 0x9C57:
                    case 0x402A:
                    case 0x4029:
                    case 0x4028:
                    case 0x4027:
                    case 0x4023:
                    case 0x4024:
                    case 0x4C1B:
                    case 0x7132:
                    case 0x71C2:
                    case 0x9977:
                    case 0x996C:
                        //case 0x4C1F:
                        {
                            for (int i = 0; i < countof(SITTING_INFO); i++)
                            {
                                if (SITTING_INFO[i].Graphic == graphic)
                                {
                                    result = (int)i + 1;
                                    break;
                                }
                            }
                            break;
                        }
                    default:
                        break;
                }
            }
            obj = obj->m_NextXY;
        }
    }
    return result;
}

void CGameCharacter::Draw(int x, int y)
{
    ScopedPerfMarker(__FUNCTION__);

    if (TimeToRandomFidget < g_Ticks)
    {
        SetRandomFidgetAnimation();
    }

    g_RenderedObjectsCountInGameWindow++;

    uint32_t lastSBsel = g_StatusbarUnderMouse;

    if (!IsPlayer() && g_Player->Warmode && g_SelectedObject.Object == this)
    {
        g_StatusbarUnderMouse = Serial;
    }

    g_AnimationManager.DrawCharacter(this, x, y); //Draw character

    g_StatusbarUnderMouse = lastSBsel;

    DrawEffects(x, y);
}

void CGameCharacter::Select(int x, int y)
{
    if (g_AnimationManager.CharacterPixelsInXY(this, x, y))
    {
        g_SelectedObject.Init(this);
    }
}

void CGameCharacter::OnGraphicChange(int direction)
{
    switch (Graphic)
    {
        case 0x0190:
        case 0x0192:
        {
            Gender = GENDER_MALE;
            Race = RT_HUMAN;
            break;
        }
        case 0x0191:
        case 0x0193:
        {
            Gender = GENDER_FEMALE;
            Race = RT_HUMAN;
            break;
        }
        case 0x025D:
        {
            Gender = GENDER_MALE;
            Race = RT_ELF;
            break;
        }
        case 0x025E:
        {
            Gender = GENDER_FEMALE;
            Race = RT_ELF;
            break;
        }
        case 0x029A:
        {
            Gender = GENDER_MALE;
            Race = RT_GARGOYLE;
            break;
        }
        case 0x029B:
        {
            Gender = GENDER_FEMALE;
            Race = RT_GARGOYLE;
            break;
        }
        default:
            break;
    }

    if (direction == 1000)
    {
        g_GumpManager.UpdateContent(Serial, 0, GT_PAPERDOLL);

        if ((g_GumpManager.UpdateContent(Serial, 0, GT_STATUSBAR) != nullptr ||
             g_GumpManager.UpdateContent(Serial, 0, GT_TARGET_SYSTEM) != nullptr) &&
            MaxHits == 0)
        {
            CPacketStatusRequest(Serial).Send();
        }
    }
}

void CGameCharacter::SetAnimation(
    uint8_t id,
    uint8_t interval,
    uint8_t frameCount,
    uint8_t repeatCount,
    bool repeat,
    bool frameDirection)
{
    AnimationGroup = id;
    AnimIndex = 0;
    AnimationInterval = interval;
    AnimationFrameCount = frameCount;
    AnimationRepeatMode = repeatCount;
    AnimationRepeat = repeat;
    AnimationDirection = frameDirection;
    AnimationFromServer = false;

    LastAnimationChangeTime = g_Ticks;
    TimeToRandomFidget = g_Ticks + RANDOM_FIDGET_ANIMATION_DELAY;
}

void CGameCharacter::ResetAnimationGroup(uint8_t val)
{
    AnimationFrameCount = 0;
    AnimationInterval = 0;
    AnimationRepeat = false;
    AnimationRepeatMode = 0;
    AnimationDirection = false;
    AnimationFromServer = false;

    AnimationGroup = val;
}

void CGameCharacter::SetRandomFidgetAnimation()
{
    TimeToRandomFidget = g_Ticks + RANDOM_FIDGET_ANIMATION_DELAY;

    if (!IsMounted())
    {
        AnimIndex = 0;
        AnimationFrameCount = 0;
        AnimationInterval = 1;
        AnimationRepeatMode = 1;
        AnimationDirection = true;
        AnimationRepeat = false;
        AnimationFromServer = true;
        AnimationGroup = g_AnimationManager.GetRandomIdleAnimation(GetGraphicForAnimation());
    }
}

static const uint8_t s_animAssociateTable[PAG_ANIMATION_COUNT][3] = {
    { LAG_WALK, HAG_WALK, PAG_WALK_UNARMED },
    { LAG_WALK, HAG_WALK, PAG_WALK_ARMED },
    { LAG_RUN, HAG_FLY, PAG_RUN_UNARMED },
    { LAG_RUN, HAG_FLY, PAG_RUN_ARMED },
    { LAG_STAND, HAG_STAND, PAG_STAND },
    { LAG_FIDGET_1, HAG_FIDGET_1, PAG_FIDGET_1 },
    { LAG_FIDGET_2, HAG_FIDGET_2, PAG_FIDGET_2 },
    { LAG_STAND, HAG_STAND, PAG_STAND_ONEHANDED_ATTACK },
    { LAG_STAND, HAG_STAND, PAG_STAND_TWOHANDED_ATTACK },
    { LAG_EAT, HAG_ATTACK_3, PAG_ATTACK_ONEHANDED },
    { LAG_EAT, HAG_ATTACK_1, PAG_ATTACK_UNARMED_1 },
    { LAG_EAT, HAG_ATTACK_2, PAG_ATTACK_UNARMED_2 },
    { LAG_EAT, HAG_ATTACK_3, PAG_ATTACK_TWOHANDED_DOWN },
    { LAG_EAT, HAG_ATTACK_1, PAG_ATTACK_TWOHANDED_WIDE },
    { LAG_EAT, HAG_ATTACK_2, PAG_ATTACK_TWOHANDED_JAB },
    { LAG_WALK, HAG_WALK, PAG_WALK_WARMODE },
    { LAG_EAT, HAG_ATTACK_2, PAG_CAST_DIRECTED },
    { LAG_EAT, HAG_ATTACK_3, PAG_CAST_AREA },
    { LAG_EAT, HAG_ATTACK_1, PAG_ATTACK_BOW },
    { LAG_EAT, HAG_ATTACK_2, PAG_ATTACK_CROSSBOW },
    { LAG_EAT, HAG_GET_HIT_1, PAG_GET_HIT },
    { LAG_DIE_1, HAG_DIE_1, PAG_DIE_1 },
    { LAG_DIE_2, HAG_DIE_2, PAG_DIE_2 },
    { LAG_WALK, HAG_WALK, PAG_ONMOUNT_RIDE_SLOW },
    { LAG_RUN, HAG_FLY, PAG_ONMOUNT_RIDE_FAST },
    { LAG_STAND, HAG_STAND, PAG_ONMOUNT_STAND },
    { LAG_EAT, HAG_ATTACK_1, PAG_ONMOUNT_ATTACK },
    { LAG_EAT, HAG_ATTACK_2, PAG_ONMOUNT_ATTACK_BOW },
    { LAG_EAT, HAG_ATTACK_1, PAG_ONMOUNT_ATTACK_CROSSBOW },
    { LAG_EAT, HAG_ATTACK_2, PAG_ONMOUNT_SLAP_HORSE },
    { LAG_EAT, HAG_STAND, PAG_TURN },
    { LAG_WALK, HAG_WALK, PAG_ATTACK_UNARMED_AND_WALK },
    { LAG_EAT, HAG_STAND, PAG_EMOTE_BOW },
    { LAG_EAT, HAG_STAND, PAG_EMOTE_SALUTE },
    { LAG_FIDGET_1, HAG_FIDGET_1, PAG_FIDGET_3 }
};

void CGameCharacter::GetAnimationGroup(ANIMATION_GROUPS group, uint8_t &animation)
{
    if (group != 0 && animation < PAG_ANIMATION_COUNT)
    {
        animation = s_animAssociateTable[animation][group - 1];
    }
}

void CGameCharacter::CorrectAnimationGroup(
    uint16_t graphic, ANIMATION_GROUPS group, uint8_t &animation)
{
    if (group == AG_LOW)
    {
        switch (animation)
        {
            case LAG_DIE_2:
                animation = LAG_DIE_1;
                break;
            case LAG_FIDGET_2:
                animation = LAG_FIDGET_1;
                break;
            case LAG_ATTACK_3:
            case LAG_ATTACK_2:
                animation = LAG_ATTACK_1;
                break;
            default:
                break;
        }

        if (!uo_animation_exists(graphic, animation))
        {
            animation = LAG_STAND;
        }
    }
    else if (group == AG_HIGH)
    {
        switch (animation)
        {
            case HAG_DIE_2:
                animation = HAG_DIE_1;
                break;
            case HAG_FIDGET_2:
                animation = HAG_FIDGET_1;
                break;
            case HAG_ATTACK_3:
            case HAG_ATTACK_2:
                animation = HAG_ATTACK_1;
                break;
            case HAG_GET_HIT_3:
            case HAG_GET_HIT_2:
                animation = HAG_GET_HIT_1;
                break;
            case HAG_MISC_4:
            case HAG_MISC_3:
            case HAG_MISC_2:
                animation = HAG_MISC_1;
                break;
            case HAG_FLY:
                animation = HAG_WALK;
                break;
            default:
                break;
        }

        if (!uo_animation_exists(graphic, animation))
        {
            animation = HAG_STAND;
        }
    }
}

bool CGameCharacter::TestStepNoChangeDirection(uint8_t group)
{
    bool result = false;

    switch (group)
    {
        case PAG_ONMOUNT_RIDE_FAST:
        case PAG_RUN_ARMED:
        case PAG_RUN_UNARMED:
        case PAG_ONMOUNT_RIDE_SLOW:
        case PAG_WALK_WARMODE:
        case PAG_WALK_ARMED:
        case PAG_WALK_UNARMED:
        {
            if (!m_Steps.empty())
            {
                if (m_Steps.front().X != m_X || m_Steps.front().Y != m_Y)
                {
                    result = true;
                }
            }

            break;
        }
        default:
            break;
    }

    return result;
}

#if USE_NEW_ANIM_CODE

static void CalculateHigh(
    uint16_t graphic,
    CGameCharacter *mobile,
    ANIMATION_FLAGS flags,
    bool isrun,
    bool iswalking,
    uint8_t &result)
{
    if (flags & AF_CALCULATE_OFFSET_BY_PEOPLE_GROUP)
    {
        if (result == AG_INVALID)
        {
            result = 0;
        }
    }
    else if (flags & AF_CALCULATE_OFFSET_BY_LOW_GROUP)
    {
        if (!iswalking)
        {
            if (result == AG_INVALID)
            {
                result = 2;
            }
        }
        else if (isrun)
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
    }
    else
    {
        if (mobile->IsFlying())
        {
            result = 19;
        }
        else if (!iswalking)
        {
            if (result == AG_INVALID)
            {
                if ((flags & AF_IDLE_AT_8_FRAME) && uo_animation_exists(graphic, 8))
                {
                    result = 8;
                }
                else
                {
                    if ((flags & AF_USE_UOP_ANIMATION) && !mobile->InWarMode())
                    {
                        result = 25;
                    }
                    else
                    {
                        result = 1;
                    }
                }
            }
        }
        else if (isrun)
        {
            if ((flags & AF_CAN_FLYING) && uo_animation_exists(graphic, 19))
            {
                result = 19;
            }
            else
            {
                result = (flags & AF_USE_UOP_ANIMATION) ? 24 : 0;
            }
        }
        else
        {
            if ((flags & AF_USE_UOP_ANIMATION) && !mobile->InWarMode())
            {
                result = 22;
            }
            else
            {
                result = 0;
            }
        }
    }
}

#endif // #if USE_NEW_ANIM_CODE

uint8_t CGameCharacter::GetGroupForAnimation(uint16_t graphic, bool isParent)
{
    assert(graphic < MAX_ANIMATIONS_DATA_INDEX_COUNT);
    if (graphic == 0)
    {
        graphic = GetGraphicForAnimation();
    }

#if USE_NEW_ANIM_CODE
    const auto &animData = g_Index.m_Anim[graphic];
    ANIMATION_GROUPS_TYPE originalType = AGT_UNKNOWN;
    ANIMATION_GROUPS_TYPE type = animData.Type;
    const bool uop = animData.IsUOP && (isParent || !animData.IsValidMUL);
    if (!uop)
    {
        graphic = uo_get_graphic_replacement(graphic, type, originalType);
    }

    const ANIMATION_FLAGS flags = animData.Flags;
    if (AnimationFromServer && AnimationGroup != AG_INVALID)
    {
        // FIXME: debug - combat animations seems broken
        /*return g_AnimationManager.CorrectAnimationGroupServer(
            type, originalType, flags, AnimationGroup);*/
    }

    uint8_t result = AnimationGroup;
    bool isWalking = Walking();
    bool isRun = (Direction & 0x80) != 0;
    if (!m_Steps.empty())
    {
        isWalking = true;
        isRun = m_Steps.front().Run();
    }

    switch (type)
    {
        case AGT_ANIMAL:
        {
            if (flags & AF_CALCULATE_OFFSET_LOW_GROUP_EXTENDED)
            {
                CalculateHigh(graphic, this, flags, isRun, isWalking, result);
            }
            else
            {
                if (!isWalking)
                {
                    if (result == AG_INVALID)
                    {
                        if (flags & AF_USE_UOP_ANIMATION)
                        {
                            if (InWarMode() && uo_animation_exists(graphic, 1))
                            {
                                result = 1;
                            }
                            else
                            {
                                result = 25;
                            }
                        }
                        else
                        {
                            result = 2;
                        }
                    }
                }
                else if (isRun)
                {
                    if (flags & AF_USE_UOP_ANIMATION)
                    {
                        result = 24;
                    }
                    else
                    {
                        result = uo_animation_exists(graphic, 1) ? 1 : 2;
                    }
                }
                else if (
                    (flags & AF_USE_UOP_ANIMATION) != 0 &&
                    (!InWarMode() || !uo_animation_exists(graphic, 0)))
                {
                    result = 22;
                }
                else
                {
                    result = 0;
                }
            }
            break;
        }
        case AGT_MONSTER:
        {
            CalculateHigh(graphic, this, flags, isRun, isWalking, result);
            break;
        }
        case AGT_SEA_MONSTER:
        {
            if (!isWalking)
            {
                if (result == AG_INVALID)
                {
                    result = 2;
                }
            }
            else if (isRun)
            {
                result = 1;
            }
            else
            {
                result = 0;
            }
            break;
        }
        default:
        {
            auto hand2 = FindLayer(OL_2_HAND);
            if (!isWalking)
            {
                if (result == AG_INVALID)
                {
                    bool haveLightAtHand2 =
                        hand2 != nullptr && hand2->IsLightSource() && hand2->AnimID == graphic;
                    if (IsMounted())
                    {
                        result = haveLightAtHand2 ? 28 : 25;
                    }
                    else if (IsGargoyle() && IsFlying()) // TODO: what's up when it is dead?
                    {
                        result = InWarMode() ? 65 : 64;
                    }
                    else if (!InWarMode() || IsDead())
                    {
                        if (haveLightAtHand2)
                        {
                            // TODO: UOP EQUIPMENT ?
                            result = 0;
                        }
                        else
                        {
                            if (uop && type == AGT_EQUIPMENT && !uo_animation_exists(graphic, 4))
                            {
                                result = 37;
                            }
                            else
                            {
                                result = 4;
                            }
                        }
                    }
                    else if (haveLightAtHand2)
                    {
                        // TODO: UOP EQUIPMENT ?
                        result = 2;
                    }
                    else
                    {
                        uint16_t handAnimIDs[2];
                        const auto hand1 = FindLayer(OL_1_HAND);
                        if (hand1 != nullptr)
                        {
                            handAnimIDs[0] = hand1->AnimID;
                        }
                        if (hand2 != nullptr)
                        {
                            handAnimIDs[1] = hand2->AnimID;
                        }
                        if (hand1 == nullptr)
                        {
                            if (hand2 != nullptr)
                            {
                                if (uop && type == AGT_EQUIPMENT &&
                                    !uo_animation_exists(graphic, 7))
                                {
                                    result = 8;
                                }
                                else
                                {
                                    result = 7;
                                }

                                for (int i = 0; i < 2; i++)
                                {
                                    if (handAnimIDs[i] >= 0x0263 && handAnimIDs[i] <= 0x028B)
                                    {
                                        for (int k = 0; k < countof(HANDS_BASE_ANIMID); k++)
                                        {
                                            if (handAnimIDs[i] == HANDS_BASE_ANIMID[k])
                                            {
                                                result = 8;
                                                i = 2;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            else if (IsGargoyle() && IsFlying())
                            {
                                result = 64;
                            }
                            else
                            {
                                result = 7;
                            }
                        }
                        else
                        {
                            result = 7;
                        }
                    }
                }
            }
            else if (IsMounted())
            {
                result = isRun ? 24 : 23;
            }
            //else if (EquippedGraphic0x3E96)
            //{

            //}
            else if (isRun || !InWarMode() || IsDead())
            {
                if (flags & AF_USE_UOP_ANIMATION)
                {
                    // i'm not sure here if it's necessary the isgargoyle
                    if (IsGargoyle() && IsFlying())
                    {
                        result = isRun ? 63 : 62;
                    }
                    else
                    {
                        if (isRun && uo_animation_exists(graphic, 24))
                        {
                            result = 24;
                        }
                        else
                        {
                            if (isRun)
                            {
                                if (uop && type == AGT_EQUIPMENT &&
                                    !uo_animation_exists(graphic, 2))
                                {
                                    result = 3;
                                }
                                else
                                {
                                    result = 2;
                                    if (IsGargoyle())
                                    {
                                        hand2 = FindLayer(OL_1_HAND);
                                    }
                                }
                            }
                            else
                            {
                                if (uop && type == AGT_EQUIPMENT &&
                                    !uo_animation_exists(graphic, 0))
                                {
                                    result = 1;
                                }
                                else
                                {
                                    result = 0;
                                }
                            }
                        }
                    }
                }
                else
                {
                    result = isRun ? 2 : 0;
                }

                if (hand2 != nullptr)
                {
                    uint16_t hand2Graphic = hand2->AnimID;
                    if (hand2Graphic < 0x0240 || hand2Graphic > 0x03E1)
                    {
                        if (IsGargoyle() && IsFlying())
                        {
                            result = isRun ? 63 : 62;
                        }
                        else
                        {
                            result = isRun ? 3 : 1;
                        }
                        break;
                    }
                    else
                    {
                        for (int i = 0; i < countof(HAND2_BASE_ANIMID); i++)
                        {
                            if (HAND2_BASE_ANIMID[i] == hand2Graphic)
                            {
                                if (IsGargoyle() && IsFlying())
                                {
                                    result = isRun ? 63 : 62;
                                }
                                else
                                {
                                    result = isRun ? 3 : 1;
                                }
                                break;
                            }
                        }
                    }
                }
            }
            else if (IsGargoyle() && IsFlying())
            {
                result = 62;
            }
            else
            {
                result = 15;
            }
        }
    }
#else
    const ANIMATION_GROUPS groupIndex = g_AnimationManager.GetGroupIndex(graphic);
    uint8_t result = AnimationGroup;

    if (result != AG_INVALID && ((Serial & 0x80000000) != 0) &&
        (!AnimationFromServer || graphic != 0))
    {
        GetAnimationGroup(groupIndex, result);
        if (!uo_animation_exists(graphic, result))
        {
            CorrectAnimationGroup(graphic, groupIndex, result);
        }
    }

    bool isWalking = Walking();
    bool isRun = (Direction & 0x80) != 0;
    if (!m_Steps.empty())
    {
        isWalking = true;
        isRun = m_Steps.front().Run();
    }

    if (groupIndex == AG_LOW)
    {
        if (isWalking)
        {
            if (isRun)
            {
                result = (uint8_t)LAG_RUN;
            }
            else
            {
                result = (uint8_t)LAG_WALK;
            }
        }
        else if (AnimationGroup == AG_INVALID)
        {
            result = (uint8_t)LAG_STAND;
            AnimIndex = 0;
        }
    }
    else if (groupIndex == AG_HIGH)
    {
        if (isWalking)
        {
            result = (uint8_t)HAG_WALK;

            if (isRun)
            {
                if (uo_animation_exists(graphic, HAG_FLY))
                {
                    result = (uint8_t)HAG_FLY;
                }
            }
        }
        else if (AnimationGroup == AG_INVALID)
        {
            result = (uint8_t)HAG_STAND;
            AnimIndex = 0;
        }

        //!Глюченный дельфин на всех клиентах
        if (graphic == 151)
        {
            result++;
        }
    }
    else if (groupIndex == AG_PEOPLE)
    {
        bool InWar = InWarMode();

        if (IsPlayer())
        {
            InWar = g_Player->Warmode;
        }

        if (isWalking)
        {
            if (isRun)
            {
                if (IsMounted())
                {
                    result = (uint8_t)PAG_ONMOUNT_RIDE_FAST;
                }
                else if (FindLayer(OL_1_HAND) != nullptr || FindLayer(OL_2_HAND) != nullptr)
                {
                    result = (uint8_t)PAG_RUN_ARMED;
                }
                else
                {
                    result = (uint8_t)PAG_RUN_UNARMED;
                }

                if (!IsHuman() && !uo_animation_exists(graphic, result))
                {
                    goto test_walk;
                }
            }
            else
            {
            test_walk:
                if (IsMounted())
                {
                    result = (uint8_t)PAG_ONMOUNT_RIDE_SLOW;
                }
                else if (
                    (FindLayer(OL_1_HAND) != nullptr || FindLayer(OL_2_HAND) != nullptr) &&
                    !IsDead())
                {
                    if (InWar)
                    {
                        result = (uint8_t)PAG_WALK_WARMODE;
                    }
                    else
                    {
                        result = (uint8_t)PAG_WALK_ARMED;
                    }
                }
                else if (InWar && !IsDead())
                {
                    result = (uint8_t)PAG_WALK_WARMODE;
                }
                else
                {
                    result = (uint8_t)PAG_WALK_UNARMED;
                }
            }
        }
        else if (AnimationGroup == AG_INVALID)
        {
            if (IsMounted() && !IsDrivingBoat())
            {
                result = (uint8_t)PAG_ONMOUNT_STAND;
            }
            else if (InWar && !IsDead())
            {
                if (FindLayer(OL_1_HAND) != nullptr)
                {
                    result = (uint8_t)PAG_STAND_ONEHANDED_ATTACK;
                }
                else if (FindLayer(OL_2_HAND) != nullptr)
                {
                    result = (uint8_t)PAG_STAND_TWOHANDED_ATTACK;
                }
                else
                {
                    result = (uint8_t)PAG_STAND_ONEHANDED_ATTACK;
                }
            }
            else
            {
                result = (uint8_t)PAG_STAND;
            }

            AnimIndex = 0;
        }
        //62 gg flying walk
        //63 gg flying run
        //64 gg flying idle
        //65 gg flying warmode on
        //66 gg flying taking1 dmg?
        //67 same as 60/61
        //68 gg flying taking dmg

        if (Race == RT_GARGOYLE && IsFlying())
        {
            if (result == 0 || result == 1)
            {
                result = 62;
            }
            else if (result == 2 || result == 3)
            {
                result = 63;
            }
            else if (result == 4)
            {
                result = 64;
            }
            else if (result == 6)
            {
                result = 66;
            }
            else if (result == 7 || result == 8)
            {
                result = 65;
            }
            else if (result >= 9 && result <= 11)
            {
                result = 71;
            }
            else if (result >= 12 && result <= 14)
            {
                result = 72;
            }
            else if (result == 15)
            {
                result = 62;
            }
            else if (result == 20)
            {
                result = 77;
            }
            else if (result == 31)
            {
                result = 71;
            }
            else if (result == 34)
            {
                result = 78;
            }
            else if (
                result >=
                200) // && result <= 259) // FIXME: CHECK client.exe - result is a byte, what it was meant to be values >=256 ???
            {
                result = 75;
            }
            /*else if (result >= 260 && result <= 270)
            {
                result = 75;
            }
            */
        }
    }
#endif
    return result;
}

void CGameCharacter::ProcessGargoyleAnims(uint8_t &group)
{
    if (group == 64 || group == 65)
    {
        group = InWarMode() ? 65 : 64;
        AnimationGroup = group;
    }
}

// 0x03CA dead shroud
uint16_t CGameCharacter::GetGraphicForAnimation()
{
    uint16_t graphic = Graphic;
    switch (graphic)
    {
        case 0x0192: // male ghost
        case 0x0193: // female ghost
        {
            graphic -= 2;
            break;
        }
        case 0x02B6: // dead gargoyle
        {
            graphic = 0x029B;
            break;
        }
        case 0x02B7: // dead gargoyle
        {
            graphic = 0x029A;
            break;
        }
        default:
            break;
    }

    assert(graphic < MAX_ANIMATIONS_DATA_INDEX_COUNT);
    return graphic;
}

void CGameCharacter::UpdateAnimationInfo_ProcessSteps(uint8_t &dir, bool canChange)
{
    dir = Direction & 7;

    if (!m_Steps.empty())
    {
        CWalkData &wd = m_Steps.front();

        dir = wd.Direction;
        int run = 0;

        if ((dir & 0x80) != 0)
        {
            dir &= 7;
            run = 1;
        }

        if (canChange)
        {
            if (AnimationFromServer)
            {
                SetAnimation(AG_INVALID);
            }

            int maxDelay = g_PathFinder.GetWalkSpeed(run != 0, IsMounted()) - 15;

            int delay = (int)g_Ticks - (int)LastStepTime;
            bool removeStep = (delay >= maxDelay);
            bool directionChange = false;

            if (m_X != wd.X || m_Y != wd.Y)
            {
                bool badStep = false;

                if ((OffsetX == 0) && (OffsetY == 0))
                {
                    int absX = abs(m_X - wd.X);
                    int absY = abs(m_Y - wd.Y);

                    badStep = (absX > 1 || absY > 1 || ((absX + absY) == 0));

                    if (!badStep)
                    {
                        absX = m_X;
                        absY = m_Y;

                        g_PathFinder.GetNewXY(wd.Direction & 7, absX, absY);

                        badStep = (absX != wd.X || absY != wd.Y);
                    }
                }

                if (badStep)
                {
                    removeStep = true;
                }
                else
                {
                    float steps = maxDelay / g_AnimCharactersDelayValue;

                    float x = delay / g_AnimCharactersDelayValue;
                    float y = x;
                    OffsetZ = (char)(((wd.Z - m_Z) * x) * (4.0f / steps));

                    wd.GetOffset(x, y, steps);

                    OffsetX = (char)x;
                    OffsetY = (char)y;
                }
            }
            else
            {
                directionChange = true;

                removeStep = true; //direction change
            }

            if (removeStep)
            {
                if (IsPlayer())
                {
                    if (m_X != wd.X || m_Y != wd.Y || m_Z != wd.Z)
                    {
                        UOI_PLAYER_XYZ_DATA xyzData = { wd.X, wd.Y, wd.Z };
                        PLUGIN_EVENT(UOMSG_UPDATE_PLAYER_XYZ, &xyzData);
                    }

                    if (Direction != wd.Direction)
                    {
                        PLUGIN_EVENT(UOMSG_UPDATE_PLAYER_DIR, wd.Direction);
                    }

                    if (m_Z - wd.Z >= 22)
                    {
                        g_Game.CreateTextMessage(TT_OBJECT, g_PlayerSerial, 3, 0, "Ouch!");
                        //play sound (5) ?
                    }

                    if (g_Walker.m_Step[g_Walker.CurrentWalkSequence].Accepted)
                    {
                        int sequencePtr = g_Walker.CurrentWalkSequence + 1;

                        if (sequencePtr < g_Walker.StepsCount)
                        {
                            int count = g_Walker.StepsCount - sequencePtr;

                            for (int i = 0; i < count; i++)
                            {
                                g_Walker.m_Step[sequencePtr - 1] = g_Walker.m_Step[sequencePtr];
                                sequencePtr++;
                            }
                        }

                        g_Walker.StepsCount--;
                    }
                    else
                    {
                        g_Walker.CurrentWalkSequence++;
                    }
                }

                m_X = wd.X;
                m_Y = wd.Y;
                m_Z = wd.Z;

                if (IsPlayer())
                {
                    g_GumpManager.RemoveRangedGumps();
                }

                UpdateRealDrawCoordinates();

                Direction = wd.Direction;

                OffsetX = 0;
                OffsetY = 0;
                OffsetZ = 0;

                m_Steps.pop_front();

                TimeToRandomFidget = g_Ticks + RANDOM_FIDGET_ANIMATION_DELAY;

                if (directionChange)
                {
                    UpdateAnimationInfo_ProcessSteps(dir, canChange);
                    return;
                }

                if (!RemovedFromRender())
                {
                    g_MapManager.AddRender(this);
                }

                LastStepTime = g_Ticks;
            }
        }
    }
    else
    {
        OffsetX = 0;
        OffsetY = 0;
        OffsetZ = 0;
    }
}

CGameItem *CGameCharacter::FindSecureTradeBox()
{
    QFOR(obj, m_Items, CGameItem *)
    {
        if (obj->Graphic == 0x1E5E && (obj->Layer == 0u))
        {
            return obj;
        }
    }

    return nullptr;
}

void CGameCharacter::SetDead(bool &dead)
{
    m_Dead = dead;
}
