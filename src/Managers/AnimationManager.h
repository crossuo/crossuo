// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <stdint.h>
#include <unordered_map>
#include <deque>
#include <vector>
#include <xuocore/mulstruct.h> // AnimationState
#include "../Globals.h"        // ANIMATION_GROUPS

class CTargetGump;
class CGameCharacter;
struct AnimationDirection;
struct AnimationDirFrames;

static const int MAX_LAYER_DIRECTIONS = 8;
static const float UPPER_BODY_RATIO = 0.35f;
static const float MID_BODY_RATIO = 0.60f;
static const float LOWER_BODY_RATIO = 0.94f;

class CAnimationManager
{
public:
    uint16_t Color = 0;
    AnimationState Anim;

private:
    int m_CharacterFrameHeight = 0;

    static void PrepareTargetAttackGump(
        CTargetGump &gump,
        int drawX,
        int drawY,
        uint16_t targetColor,
        int per,
        CGameCharacter &obj);

    int m_CharacterFrameStartY = 0;
    int m_StartCharacterWaistY = 0;
    int m_StartCharacterKneesY = 0;
    int m_StartCharacterFeetY = 0;
    int m_Sitting = 0;
    bool m_UseBlending = false;

    static const int USED_LAYER_COUNT = 23;
    static const int m_UsedLayers[MAX_LAYER_DIRECTIONS][USED_LAYER_COUNT];

    bool TestPixels(
        class CGameObject *obj,
        int x,
        int y,
        bool mirror,
        uint8_t &frameIndex,
        uint16_t grahic = 0);

    void FixSittingDirection(uint8_t &layerDirection, bool &mirror, int &x, int &y);
    void Draw(
        class CGameObject *obj,
        int x,
        int y,
        bool mirror,
        uint8_t &frameIndex,
        uint16_t graphic = 0,
        bool isShadow = false,
        uint16_t convColor = 0);
    void DrawIntoFrameBuffer(class CGameCharacter *obj, int x, int y);

    bool DrawEquippedLayers(
        bool selection,
        class CGameObject *obj,
        int drawX,
        int drawY,
        bool mirror,
        uint8_t layerDir,
        uint8_t animIndex,
        int lightOffset);

    bool IsCovered(int layer, class CGameObject *owner);
    bool m_Transform = false;

    uint16_t m_CharacterLayerGraphic[25];
    uint16_t m_CharacterLayerAnimID[25];

    uint8_t GetObjectNewAnimationType_0(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_1_2(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_3(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_4(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_5(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_6_14(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_7(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_8(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_9_10(CGameCharacter *obj, uint16_t action, uint8_t mode);
    uint8_t GetObjectNewAnimationType_11(CGameCharacter *obj, uint16_t action, uint8_t mode);

    bool IsReplacedObjectAnimation(uint8_t anim, uint16_t v13) const;

public:
    CAnimationManager();
    ~CAnimationManager();

    void Load(uint32_t *verdata);
    void GetAnimDirection(uint8_t &dir, bool &mirror);
    void GetSittingAnimDirection(uint8_t &dir, bool &mirror, int &x, int &y);
    void DrawCharacter(class CGameCharacter *obj, int x, int y);
    bool CharacterPixelsInXY(class CGameCharacter *obj, int x, int y);
    void DrawCorpse(class CGameItem *obj, int x, int y);
    bool CorpsePixelsInXY(class CGameItem *obj, int x, int y);
    uint16_t ConvertBodyIfNeeded(uint16_t graphic, bool isParent = false);
    uint8_t GetDieGroupIndex(uint16_t graphic, bool running, bool third = false);
    uint8_t GetRandomIdleAnimation(uint16_t graphic) const;
    uint8_t GetStandingGroupForGraphic(uint16_t graphic) const;
    ANIMATION_GROUPS GetGroupIndex(uint16_t graphic) const;
    uint8_t CorrectAnimationGroupServer(
        ANIMATION_GROUPS_TYPE type,
        ANIMATION_GROUPS_TYPE originalType,
        ANIMATION_FLAGS flags,
        uint16_t v13) const;

    AnimationFrameInfo
    GetAnimationDimensions(uint8_t frameIndex, AnimationState anim, bool isCorpse);

    AnimationFrameInfo GetAnimationDimensions(
        uint8_t animIndex,
        AnimationState anim,
        bool isMounted,
        bool isCorpse,
        uint8_t frameIndex = 0xff);

    struct DRAW_FRAME_INFORMATION
    CollectFrameInformation(class CGameObject *gameObject, bool checkLayers = true);

    uint8_t GetReplacedObjectAnimation(CGameCharacter *obj, uint16_t index);

    uint8_t
    GetObjectNewAnimation(CGameCharacter *obj, uint16_t type, uint16_t action, uint8_t mode);

    void GarbageCollect();
    void ClearUnusedAnimations(uint32_t ticks = ~0);
    AnimationDirFrames *ExecuteAnimation(AnimationState anim, uint32_t ticks);
};

extern CAnimationManager g_AnimationManager;
