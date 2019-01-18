// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GameObject.h"
#include "../Walker/WalkData.h"
#include "../TextEngine/TextContainer.h"

class CGameCharacter : public CGameObject
{
public:
    //!Hitpoints
    short Hits = 0;

    //!Max hitpoints
    short MaxHits = 0;

    //!Mana
    short Mana = 0;

    //!Max mana
    short MaxMana = 0;

    //!Stamina
    short Stam = 0;

    //!Max stamina
    short MaxStam = 0;

    //!Is female
    bool Female = false;

    //!Character's race
    RACE_TYPE Race = RT_HUMAN;

    //!Current direction
    uint8_t Direction = 0;

    //!Character's notoriety
    uint8_t Notoriety = 0;

    //!Player can change the name for this character
    bool CanChangeName = false;

    //!Interval between animation playback
    uint8_t AnimationInterval = 0;

    //!Current animation frame count
    uint8_t AnimationFrameCount = 0;

    //!Current animation repeat mode
    uint8_t AnimationRepeatMode = 1;

    //!Current animation group
    uint8_t AnimationGroup = 0xFF;

    //!Current animation is repeat
    bool AnimationRepeat = false;

    //!Current animation direction (front or back)
    bool AnimationDirection = false;

    //!Current animation called from the server
    bool AnimationFromServer = false;

    //!Last step sound time stamp
    uint32_t LastStepSoundTime = 0;

    //!Time stamp to fidget animation
    uint32_t TimeToRandomFidget = 0;

    //!Offset to step sound
    uint8_t StepSoundOffset = 0;

    //!Sprite offset by X coordinate on the tile
    int OffsetX = 0;

    //!Sprite offset by Y coordinate on the tile
    int OffsetY = 0;

    //!Sprite offset by Z coordinate on the tile
    int OffsetZ = 0;

    //!Last step time stamp
    uint32_t LastStepTime = 0;

    //!Character's title
    string Title = "";

    //!Percent of hits
    uint8_t HitsPercent = 0;

protected:
    void CorrectAnimationGroup(uint16_t graphic, ANIMATION_GROUPS group, uint8_t &animation);
    bool m_Dead = false;

public:
    CGameCharacter(int serial);
    virtual ~CGameCharacter();

    CTextContainer m_DamageTextControl{ CTextContainer(10) };
    deque<CWalkData> m_Steps;
    CGLTextTexture m_HitsTexture{ CGLTextTexture() };

    virtual void UpdateTextCoordinates();
    void UpdateHitsTexture(uint8_t hits);
    void ProcessGargoyleAnims(int &animGroup);
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

    uint16_t GetMountAnimation();
    uint8_t GetAnimationGroup(uint16_t checkGraphic = 0);
    void GetAnimationGroup(ANIMATION_GROUPS group, uint8_t &animation);
    bool Staying() { return AnimationGroup == 0xFF && m_Steps.empty(); }
    bool TestStepNoChangeDirection(uint8_t group);
    virtual bool Walking() { return (LastStepTime > (uint32_t)(g_Ticks - WALKING_DELAY)); }
    virtual bool NoIterateAnimIndex()
    {
        return ((LastStepTime > (uint32_t)(g_Ticks - WALKING_DELAY)) && m_Steps.empty());
    }

    void UpdateAnimationInfo(uint8_t &dir, bool canChange = false);

    bool IsHuman()
    {
        return (
            IN_RANGE(Graphic, 0x0190, 0x0193) || IN_RANGE(Graphic, 0x00B7, 0x00BA) ||
            IN_RANGE(Graphic, 0x025D, 0x0260) || IN_RANGE(Graphic, 0x029A, 0x029B) ||
            IN_RANGE(Graphic, 0x02B6, 0x02B7) || (Graphic == 0x03DB) || (Graphic == 0x03DF) ||
            (Graphic == 0x03E2));
    }

    bool Dead()
    {
        return (IN_RANGE(Graphic, 0x0192, 0x0193) || IN_RANGE(Graphic, 0x025F, 0x0260) ||
                IN_RANGE(Graphic, 0x02B6, 0x02B7)) ||
               m_Dead;
    }

    virtual CGameCharacter *GameCharacterPtr() { return this; }
    virtual CGameItem *FindSecureTradeBox();
    void SetDead(bool &dead);
};
