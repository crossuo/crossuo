/***********************************************************************************
**
** GameCharacter.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GAMECHARACTER_H
#define GAMECHARACTER_H

//!Game character class
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
    /*!
	Correct animation index by group
	@param [__in] graphic Body graphic
	@param [__in] group Animation group
	@param [__inout] animation Animation index
	@return 
	*/
    void CorrectAnimationGroup(uint16_t graphic, ANIMATION_GROUPS group, uint8_t &animation);

    /*!
	Bonded pets status dead/alive
	*/
    bool m_Dead = false;

public:
    /*!
	Constructor
	@param [__in] serial Character's serial
	@return
	*/
    CGameCharacter(int serial);

    /*!
	Destructor
	@return
	*/
    virtual ~CGameCharacter();

    //!Damage text container
    CTextContainer m_DamageTextControl{ CTextContainer(10) };

    //!Steps stack
    deque<CWalkData> m_Steps;

    //!Texture for hurrent hitpoints value
    CGLTextTexture m_HitsTexture{ CGLTextTexture() };

    /*!
	Update text coordinates
	@return
	*/
    virtual void UpdateTextCoordinates();

    /*!
	Update hitpoints text texture
	@param [__in] hits Current hitpoints
	@return
	*/
    void UpdateHitsTexture(uint8_t hits);

    /*!
	Process gargoyle animations
	@param [__in] animGroup Animation group
	@return
	*/
    void ProcessGargoyleAnims(int &animGroup);

    /*!
	Сидит ли персонаж
	@return Индекс объекта из таблицы, на котором он восседает
	*/
    /*!
	Sitting/staying character state
	@return Sitting table index+1, 0 if not sitting
	*/
    int IsSitting();

    /*!
	Draw character in the world
	@param [__in] x Place on screen coordinate X
	@param [__in] y Place on screen coordinate Y
	@return
	*/
    virtual void Draw(int x, int y);

    /*!
	Select character in the world
	@param [__in] x Place on screen coordinate X
	@param [__in] y Place on screen coordinate Y
	@return
	*/
    virtual void Select(int x, int y);

    /*!
	Update graphic event
	@param [__in_opt] direction Direction of character
	@return
	*/
    void OnGraphicChange(int direction = 0);

    /*!
	Reset animation group index
	@param [__in] val New animation group index
	@return
	*/
    void ResetAnimationGroup(uint8_t val);

    /*!
	Reset animation group index to random fidget
	@return
	*/
    void SetRandomFidgetAnimation();

    /*!
	Set animation data
	@param [__in] id Animation group
	@param [__in_opt] interval Frame delay
	@param [__in_opt] frameCount Frame count
	@param [__in_opt] repeatCount Repeat animation count
	@param [__in_opt] repeat Is repeated
	@param [__in_opt] frameDirection Frame direction (forwarf/backward)
	@return
	*/
    void SetAnimation(
        uint8_t id,
        uint8_t interval = 0,
        uint8_t frameCount = 0,
        uint8_t repeatCount = 0,
        bool repeat = false,
        bool frameDirection = false);

    /*!
	Get mount animation index
	@return Graphic
	*/
    uint16_t GetMountAnimation();

    /*!
	Get current animation index
	@param [__in_opt] checkGraphic Current graphic
	@return Animation group index
	*/
    uint8_t GetAnimationGroup(uint16_t checkGraphic = 0);

    /*!
	Correct animation index
	@param [__in] group Animation group
	@param [__out] animation Animation index
	@return
	*/
    void GetAnimationGroup(ANIMATION_GROUPS group, uint8_t &animation);

    /*!
	Staying character state
	@return true if character is staying
	*/
    bool Staying() { return AnimationGroup == 0xFF && m_Steps.empty(); }

    /*!
	Check for the possibility of changing the direction of the character when driving in a seated position
	@param [__in] group Animation group
	@return true if direction can be changed
	*/
    bool TestStepNoChangeDirection(uint8_t group);

    /*!
	Character walking state
	@return true if walking
	*/
    virtual bool Walking() { return (LastStepTime > (uint32_t)(g_Ticks - WALKING_DELAY)); }

    /*!
	Check for animation frame changing
	@return true if don't need iterate frames
	*/
    virtual bool NoIterateAnimIndex()
    {
        return ((LastStepTime > (uint32_t)(g_Ticks - WALKING_DELAY)) && m_Steps.empty());
    }

    /*!
	Update character animation state and world position
	@param [__out] dir Direction
	@param [__in_opt] canChange Can change private fields/stacks
	@return
	*/
    void UpdateAnimationInfo(uint8_t &dir, bool canChange = false);

    /*!
	Check on humanoid
	@return true if character is humanoid
	*/
    bool IsHuman()
    {
        return (
            IN_RANGE(Graphic, 0x0190, 0x0193) || IN_RANGE(Graphic, 0x00B7, 0x00BA) ||
            IN_RANGE(Graphic, 0x025D, 0x0260) || IN_RANGE(Graphic, 0x029A, 0x029B) ||
            IN_RANGE(Graphic, 0x02B6, 0x02B7) || (Graphic == 0x03DB) || (Graphic == 0x03DF) ||
            (Graphic == 0x03E2));
    }

    /*!
	Check on dead
	@return true if graphic is ghost
	*/
    bool Dead()
    {
        return (IN_RANGE(Graphic, 0x0192, 0x0193) || IN_RANGE(Graphic, 0x025F, 0x0260) ||
                IN_RANGE(Graphic, 0x02B6, 0x02B7)) ||
               m_Dead;
    }

    /*!
	Get character pointer
	@return Always self
	*/
    virtual CGameCharacter *GameCharacterPtr() { return this; }

    /*!
	Find secure trade box object
	@return Item pointer or nullptr if box is not found
	*/
    virtual CGameItem *FindSecureTradeBox();

    /*!
	Set dead/alive status
	*/
    void SetDead(bool &dead);
};

#endif
