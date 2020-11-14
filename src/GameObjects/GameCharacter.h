// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GameObject.h"
#include "Globals.h"
#include "GameItem.h"
#include "../Sprite.h"
#include "../Walker/WalkData.h"
#include "../TextEngine/TextContainer.h"

class CGameCharacter : public CGameObject
{
public:
    uint16_t Hits = 0;
    uint16_t MaxHits = 0;
    uint16_t Mana = 0;
    uint16_t MaxMana = 0;
    uint16_t Stam = 0;
    uint16_t MaxStam = 0;
    GENDER Gender = GENDER_MALE;
    RACE_TYPE Race = RT_HUMAN;
    uint8_t Direction = 0;
    uint8_t Notoriety = 0;
    bool CanChangeName = false;
    uint8_t AnimationInterval = 0;
    uint8_t AnimationFrameCount = 0;
    uint8_t AnimationRepeatMode = 1;
    uint8_t AnimationGroup = AG_INVALID;
    bool AnimationRepeat = false;
    bool AnimationDirection = false;
    bool AnimationFromServer = false;
    uint32_t LastStepSoundTime = 0;
    uint32_t TimeToRandomFidget = 0;
    uint8_t StepSoundOffset = 0;
    int OffsetX = 0; // Sprite Offset
    int OffsetY = 0;
    int OffsetZ = 0;
    uint32_t LastStepTime = 0;
    astr_t Title;
    uint8_t HitsPercent = 0;

protected:
    void CorrectAnimationGroup(uint16_t graphic, ANIMATION_GROUPS group, uint8_t &animation);
    bool m_Dead = false;

public:
    CGameCharacter(int serial);
    virtual ~CGameCharacter();

    CTextContainer m_DamageTextControl{ CTextContainer(10) };
    std::deque<CWalkData> m_Steps;
    CTextSprite m_HitsTexture;

    virtual void UpdateTextCoordinates();
    void UpdateHitsTexture(uint8_t hits);
    void ProcessGargoyleAnims(uint8_t &group);
    int IsSitting();
    virtual void Draw(int x, int y);
    virtual void Select(int x, int y);
    void OnGraphicChange(int direction = 0);
    void ResetAnimationGroup(uint8_t val);
    void SetRandomFidgetAnimation();
    void SetAnimation(
        uint8_t id,
        uint8_t interval = 0,
        uint8_t frameCount = 0,
        uint8_t repeatCount = 0,
        bool repeat = false,
        bool frameDirection = false);

    uint16_t GetGraphicForAnimation();
    uint8_t GetGroupForAnimation(uint16_t graphic, bool isParent);
    void GetAnimationGroup(ANIMATION_GROUPS group, uint8_t &animation);
    bool Staying() { return AnimationGroup == AG_INVALID && m_Steps.empty(); }
    bool TestStepNoChangeDirection(uint8_t group);
    virtual bool Walking() { return (LastStepTime > (uint32_t)(g_Ticks - WALKING_DELAY)); }
    virtual bool NoIterateAnimIndex()
    {
        return ((LastStepTime > (uint32_t)(g_Ticks - WALKING_DELAY)) && m_Steps.empty());
    }

    void UpdateAnimationInfo_ProcessSteps(uint8_t &dir, bool canChange = false);

    bool IsHuman() const
    {
        return (
            IN_RANGE(Graphic, 0x0190, 0x0193) || /*h_male,h_female*/
            IN_RANGE(Graphic, 0x00B7, 0x00BA) || /*Savage_Male,Savage_Female*/
            IN_RANGE(Graphic, 0x025D, 0x0260) || /*Elf h_male1,Elf h_female1, dead..*/
            IN_RANGE(Graphic, 0x029A, 0x029B) || /*gargoyle_male, gargoyle_female*/
            IN_RANGE(Graphic, 0x02B6, 0x02B7) || /*dead gargoyle_male, gargoyle_female*/
            (Graphic == 0x03DB) ||               /**/
            (Graphic == 0x03DF) ||               /*character_blackthorn*/
            (Graphic == 0x03E2) ||               /*character_dupre*/
            (Graphic == 0x02E8) ||               /*h_male3*/
            (Graphic == 0x02E9) ||               /*h_female3*/
            (Graphic == 0x04E5) ||               /*blackthorn*/
            0);
    }

    inline bool IsGargoyle() const
    {
        return g_Config.ClientVersion >= CV_7000 && IN_RANGE(Graphic, 0x029A, 0x029B);
    }

    inline bool IsDead() const
    {
        return (IN_RANGE(Graphic, 0x0192, 0x0193) || /*dead h_male, h_female*/
                IN_RANGE(Graphic, 0x025F, 0x0260) || /*dead elf h_male1, h_female1*/
                IN_RANGE(Graphic, 0x02B6, 0x02B7) || /*dead gargoyle_male, gargoyle_female*/
                0) ||
               m_Dead;
    }

    virtual CGameCharacter *GameCharacterPtr() { return this; }
    virtual CGameItem *FindSecureTradeBox();
    void SetDead(bool &dead);
};
