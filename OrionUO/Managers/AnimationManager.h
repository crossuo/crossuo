// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

static const float UPPER_BODY_RATIO = 0.35f;
static const float MID_BODY_RATIO = 0.60f;
static const float LOWER_BODY_RATIO = 0.94f;

struct FRAME_OUTPUT_INFO
{
    int StartX = 0;
    int StartY = 0;
    int EndX = 0;
    int EndY = 0;
};

struct ANIMATION_DIMENSIONS
{
    int Width = 0;
    int Height = 0;
    int CenterX = 0;
    int CenterY = 0;
};
struct UOPFrameData
{
    uint8_t *dataStart = nullptr;
    short frameId = 0;
    uint32_t pixelDataOffset = 0;
};

class CEquipConvData
{
public:
    uint16_t Graphic = 0;
    uint16_t Gump = 0;
    uint16_t Color = 0;

    CEquipConvData(uint16_t graphic, uint16_t gump, uint16_t color)
        : Graphic(graphic)
        , Gump(gump)
        , Color(color)
    {
    }
    ~CEquipConvData() {}
};

typedef unordered_map<uint16_t, CEquipConvData> EQUIP_CONV_DATA_MAP;
typedef unordered_map<uint16_t, EQUIP_CONV_DATA_MAP> EQUIP_CONV_BODY_MAP;

class CAnimationManager : public Wisp::CDataReader
{
public:
    uint16_t Color = 0;
    uint8_t AnimGroup = 0;
    uint8_t Direction = 0;
    uint16_t AnimID = 0;
    uint8_t AnimGroupCount = PAG_ANIMATION_COUNT;

private:
    size_t m_AddressIdx[6];
    size_t m_SizeIdx[6];

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

    EQUIP_CONV_BODY_MAP m_EquipConv;

    CEquipConvData *m_EquipConvItem{ nullptr };

    static const int USED_LAYER_COUNT = 23;
    static const int m_UsedLayers[8][USED_LAYER_COUNT];

    vector<std::pair<uint16_t, uint8_t>> m_GroupReplaces[2];

    deque<CTextureAnimationDirection *> m_UsedAnimList;

    bool TestPixels(
        class CGameObject *obj,
        int x,
        int y,
        bool mirror,
        uint8_t &frameIndex,
        uint16_t id = 0x0000);

    void FixSittingDirection(uint8_t &layerDirection, bool &mirror, int &x, int &y);
    void Draw(class CGameObject *obj, int x, int y, bool mirror, uint8_t &frameIndex, int id = 0);
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

    bool TryReadUOPAnimDimins(CTextureAnimationDirection &direction);
    vector<UOPFrameData> ReadUOPFrameDataOffsets();

    void ReadUOPFrameData(
        short &imageCenterX,
        short &imageCenterY,
        short &imageWidth,
        short &imageHeight,
        uint16_t *&palette,
        UOPFrameData &frameData);

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

    void ReadFrameDimensionData(ANIMATION_DIMENSIONS &result, uint8_t frameIndex, bool isCorpse);
    void ReadFramesPixelData(CTextureAnimationDirection &direction);

public:
    CAnimationManager();
    ~CAnimationManager();

    CIndexAnimation m_DataIndex[MAX_ANIMATIONS_DATA_INDEX_COUNT];

    void Init(int graphic, size_t addressIdx, size_t sizeIdx)
    {
        m_AddressIdx[graphic] = addressIdx;
        m_SizeIdx[graphic] = sizeIdx;
    }

    EQUIP_CONV_BODY_MAP &GetEquipConv() { return m_EquipConv; }
    void InitIndexReplaces(uint32_t *verdata);
    void UpdateAnimationAddressTable();
    void Load(uint32_t *verdata);
    void ClearUnusedTextures(uint32_t ticks);
    bool LoadDirectionGroup(CTextureAnimationDirection &direction);
    void GetAnimDirection(uint8_t &dir, bool &mirror);
    void GetSittingAnimDirection(uint8_t &dir, bool &mirror, int &x, int &y);
    void DrawCharacter(class CGameCharacter *obj, int x, int y);
    bool CharacterPixelsInXY(class CGameCharacter *obj, int x, int y);
    void DrawCorpse(class CGameItem *obj, int x, int y);
    bool CorpsePixelsInXY(class CGameItem *obj, int x, int y);
    uint8_t GetDieGroupIndex(uint16_t id, bool second);
    ANIMATION_GROUPS GetGroupIndex(uint16_t id);
    bool AnimationExists(uint16_t graphic, uint8_t group);

    ANIMATION_DIMENSIONS
    GetAnimationDimensions(
        uint8_t frameIndex, uint16_t id, uint8_t dir, uint8_t animGroup, bool isCorpse);

    ANIMATION_DIMENSIONS GetAnimationDimensions(
        class CGameObject *obj,
        uint8_t frameIndex = 0xFF,
        uint8_t defaultDirection = 0,
        uint8_t defaultGroup = 0);

    void CalculateFrameInformation(
        FRAME_OUTPUT_INFO &info, class CGameObject *obj, bool mirror, uint8_t animIndex);

    struct DRAW_FRAME_INFORMATION
    CollectFrameInformation(class CGameObject *gameObject, bool checkLayers = true);

    uint8_t GetReplacedObjectAnimation(CGameCharacter *obj, uint16_t index);

    uint8_t
    GetObjectNewAnimation(CGameCharacter *obj, uint16_t type, uint16_t action, uint8_t mode);
};

extern CAnimationManager g_AnimationManager;
