// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

// FIXME: LIBUO ifdefs
// FIXME: remove dependency on DataReader and MappedFile
// FIXME: bring UOFileReader here
// FIXME: Sprite should be application side, filemanager should give back
// recipe on how to acquire the sprite data, eg:
// (address, size, compressed flag) + function to access compressed data

#pragma once

#include <stdint.h>
#include <functional>
#include <unordered_map>
#include <vector>
#include "enumlist.h"
#include "uop.h"
#include "mulstruct.h" // FIXME: MulLandTile2, MulStaticTile2 -> g_Data
#include "mappedfile.h"

struct MapSize
{
    int Width = 0;
    int Height = 0;
};

struct IndexBlock;
struct AnimationFrameInfo;
struct AnimationState;

extern astr_t g_dumpUopFile;
extern MapSize g_MapSize[MAX_MAPS_COUNT];
extern MapSize g_MapBlockSize[MAX_MAPS_COUNT];

#if LIBUO == 1
typedef void *SoundInfo;
typedef void *SoundHandle;
#define SOUND_NULL nullptr
#else
struct CSprite;
#endif

typedef void *(*LoadPixelData16Cb)(int width, int height, uint16_t *pixels);
typedef void (*DeleteUserData)(void *UserData);
typedef uint32_t AnimationId;
typedef uint32_t AnimationGroupId;

struct UopFileEntry;
struct CUopMappedFile;
using UopSectionHeaderMap = std::unordered_map<uint64_t, const UopFileEntry *>;
using UopFileMap = std::unordered_map<AnimationGroupId, const UopFileEntry *>;

struct AnimationFrame // AnimationFrameTexture
{
    void *UserData = nullptr;
    int16_t CenterX = 0;
    int16_t CenterY = 0;
};

struct AnimationDirFrames
{
    AnimationFrame *Frames = nullptr;
    uint8_t FrameCount = 0;
};

struct AnimationDirection
{
    size_t Address = 0;
    size_t BaseAddress = 0;
    size_t PatchedAddress = 0; // BodyConvGroups
    uint32_t BaseSize = 0;
    uint32_t PatchedSize = 0;
    uint32_t Size = 0;
    uint8_t FileIndex = 0;
    uint8_t FrameCount = 0;
    bool IsUOP = false;
    bool IsVerdata = false;
};

struct AnimationGroup // AnimationGroup
{
    AnimationDirection Direction[MAX_MOBILE_DIRECTIONS];
};

struct IndexAnimation
{
    AnimationGroup Groups[MAX_ANIMATION_GROUPS_COUNT];
    ANIMATION_FLAGS Flags = AF_NONE;
    uint16_t Graphic = 0;
    uint16_t Color = 0;
    uint16_t CorpseGraphic = 0;
    uint16_t CorpseColor = 0;
    uint16_t GraphicConversion = 0;
    ANIMATION_GROUPS_TYPE Type = AGT_UNKNOWN;
    uint8_t FileIndex = 0;
    char MountedHeightOffset = 0;
    bool IsUOP = false;
    bool IsValidMUL = false;
};

struct SkillData
{
    astr_t Name;
    bool Iteractive = false;
};

struct CIndexObject
{
    void *UserData = nullptr;
    const UopFileEntry *UopBlock = nullptr;
    size_t Address = 0;
    uint32_t DataSize = 0;
    int32_t Width = 0;
    int32_t Height = 0;
    uint32_t LastAccessTime = 0;
    uint16_t Color = 0;

    virtual void ReadIndexFile(size_t address, IndexBlock *ptr);

    CIndexObject() = default;
    virtual ~CIndexObject() = default;
};

struct CIndexObjectLand : public CIndexObject
{
    bool AllBlack = false;

    CIndexObjectLand() = default;
    virtual ~CIndexObjectLand() = default;
};

struct CIndexObjectStatic : public CIndexObject
{
    uint16_t Index = 0;
    char Offset = 0;
    char AnimIndex = 0;
    uint32_t ChangeTime = 0;
    uint16_t LightColor = 0;
    bool IsFiled = false;

    CIndexObjectStatic() = default;
    virtual ~CIndexObjectStatic() = default;
};

struct CIndexSound : public CIndexObject
{
    uint32_t Delay = 0;
    uint8_t *m_WaveFile = nullptr;

    CIndexSound() = default;
    virtual ~CIndexSound() = default;
};

struct CIndexMulti : public CIndexObject
{
    uint32_t Count = 0;

    virtual void ReadIndexFile(size_t address, IndexBlock *ptr) override;

    CIndexMulti() = default;
    virtual ~CIndexMulti() = default;
};

struct CIndexGump : public CIndexObject
{
    virtual void ReadIndexFile(size_t address, IndexBlock *ptr) override;

    CIndexGump() = default;
    virtual ~CIndexGump() = default;
};

struct CIndexLight : public CIndexObject
{
    virtual void ReadIndexFile(size_t address, IndexBlock *ptr) override;

    CIndexLight() = default;
    virtual ~CIndexLight() = default;
};

struct CIndexMusic
{
    astr_t FilePath;
    bool Loop = false;
};

struct CEquipConvData
{
    uint16_t Graphic = 0;
    uint16_t Gump = 0;
    uint16_t Color = 0;
};

struct CIndexAnimationSequence
{
    // FIXME
};

struct CIndexMap
{
    size_t OriginalMapAddress = 0;
    size_t OriginalStaticAddress = 0;
    uint32_t OriginalStaticCount = 0;
    size_t MapAddress = 0;
    size_t StaticAddress = 0;
    uint32_t StaticCount = 0;
};

struct Index
{
    CIndexObjectLand m_Land[MAX_LAND_DATA_INDEX_COUNT];
    CIndexObjectStatic m_Static[MAX_STATIC_DATA_INDEX_COUNT];
    CIndexGump m_Gump[MAX_GUMP_DATA_INDEX_COUNT];
    CIndexObject m_Texture[MAX_LAND_TEXTURES_DATA_INDEX_COUNT];
    CIndexSound m_Sound[MAX_SOUND_DATA_INDEX_COUNT];
    CIndexMusic m_MP3[MAX_MUSIC_DATA_INDEX_COUNT];
    CIndexMulti m_Multi[MAX_MULTI_DATA_INDEX_COUNT];
    CIndexLight m_Light[MAX_LIGHTS_DATA_INDEX_COUNT];
    IndexAnimation m_Anim[MAX_ANIMATIONS_DATA_INDEX_COUNT];
    std::unordered_map<AnimationId, AnimationDirFrames *> m_Animations;
    std::unordered_map<AnimationGroupId, const UopFileEntry *> m_AnimationGroupFile;
    int m_MultiIndexCount = 0;
};

// FIXME: make private
struct UOData
{
    std::vector<MulLandTile2> m_Land;
    std::vector<MulStaticTile2> m_Static;
    std::vector<uint16_t> m_StumpTiles;
    std::vector<uint16_t> m_CaveTiles;
    std::vector<uint8_t> m_Anim;
};

extern Index g_Index;
extern UOData g_Data;

inline AnimationId AnimId(AnimationState anim)
{
    return (AnimationId)((anim.Graphic << 16) | (anim.Group << 8) | anim.Direction);
}

inline AnimationId AnimId(uint16_t graphic, uint8_t group, uint8_t dir = 0)
{
    return (AnimationId)((graphic << 16) | (group << 8) | dir);
}

inline AnimationDirFrames *uo_animation_get(AnimationId animId)
{
    const auto it = g_Index.m_Animations.find(animId);
    if (it == g_Index.m_Animations.end())
        return nullptr;
    return it->second;
}

inline AnimationDirFrames *uo_animation_get(AnimationState anim)
{
    return uo_animation_get(AnimId(anim));
}

inline AnimationDirFrames *uo_animation_get(uint16_t graphic, uint8_t group, uint8_t dir = 0)
{
    return uo_animation_get(AnimId({ graphic, group, dir }));
}

inline AnimationDirFrames *uo_animation_create(AnimationId animId)
{
    assert(uo_animation_get(animId) == nullptr);
    auto animation = new AnimationDirFrames();
    assert(animation != nullptr);
    const auto graphic = uint16_t((animId >> 16) & 0xffff);
    const auto group = uint8_t((animId >> 8) & 0xff);
    const auto dir = uint8_t(animId & 0xff);
    assert(group < MAX_ANIMATION_GROUPS_COUNT);
    const auto &animInfo = g_Index.m_Anim[graphic].Groups[group].Direction[dir];
    const auto frameCount = animInfo.FrameCount;
    animation->Frames = new AnimationFrame[frameCount];
    assert(animation->Frames);
    animation->FrameCount = frameCount;
    g_Index.m_Animations.emplace(animId, animation);
    return animation;
}

inline AnimationDirFrames *uo_animation_create(AnimationState anim)
{
    return uo_animation_create(AnimId(anim));
}

inline void uo_animation_destroy(AnimationId animId, DeleteUserData pDeleter)
{
    auto it = g_Index.m_Animations.find(animId);
    if (it == g_Index.m_Animations.end())
        return;

    auto dir = it->second;
    g_Index.m_Animations.erase(it);

    if (dir->Frames)
    {
        if (pDeleter)
        {
            for (int i = 0; i < dir->FrameCount; i++)
            {
                if (dir->Frames[i].UserData)
                {
                    pDeleter(dir->Frames[i].UserData);
                }
            }
        }
        delete[] dir->Frames;
        dir->Frames = nullptr;
    }

    delete dir;
}

inline AnimationGroupId GroupId(AnimationId animId)
{
    return (AnimationGroupId)(animId & 0xffffff00);
}

inline AnimationGroupId GroupId(uint16_t graphic, uint8_t group, uint8_t dir = 0)
{
    return (AnimationGroupId)((graphic << 16) | (group << 8));
}

inline const UopFileEntry *uo_animation_group_get(AnimationGroupId groupId)
{
    const auto it = g_Index.m_AnimationGroupFile.find(groupId);
    if (it == g_Index.m_AnimationGroupFile.end())
        return nullptr;
    return it->second;
}

inline const UopFileEntry *uo_animation_group_get(AnimationState anim)
{
    const auto groupId = GroupId(AnimId(anim));
    return uo_animation_group_get(groupId);
}

inline static bool uo_has_body_conversion(const IndexAnimation &data)
{
    const auto flag = data.GraphicConversion;
    const bool hasBodyConv = (flag & APF_BODYCONV) == APF_BODYCONV;
    const bool hasGroupPatch = (flag & APF_GROUP) == APF_GROUP;
    return !hasBodyConv && hasGroupPatch;
}

inline static uint16_t uo_get_graphic_replacement(
    uint16_t graphic, ANIMATION_GROUPS_TYPE &outType, ANIMATION_GROUPS_TYPE &outOriginalType)
{
    const auto &animData = g_Index.m_Anim[graphic];
    if (!uo_has_body_conversion(animData))
    {
        const auto newGraphic = animData.Graphic;
        if (graphic != newGraphic)
        {
            graphic = newGraphic;
            const auto newType = g_Index.m_Anim[newGraphic].Type;
            if (newType != outType)
            {
                outOriginalType = outType;
                outType = newType;
            }
            return newGraphic;
        }
    }
    return graphic;
}

inline static bool uo_find_body_replacement(uint16_t &outGraphic, bool isParent = false)
{
    assert(outGraphic < MAX_ANIMATIONS_DATA_INDEX_COUNT);
    const auto &anim = g_Index.m_Anim[outGraphic];
    const bool uop = anim.IsUOP && (isParent || !anim.IsValidMUL);
    if (uop)
    {
        return false;
    }
    uint16_t newGraphic = anim.Graphic;
    do
    {
        const auto conv = uo_has_body_conversion(anim);
        if (conv)
        {
            break;
        }
        const auto &newAnim = g_Index.m_Anim[newGraphic];
        if (outGraphic != newGraphic)
        {
            outGraphic = newGraphic;
            newGraphic = newAnim.Graphic;
        }
    } while (outGraphic != newGraphic);
    return true;
}
/*
inline static bool uo_find_corpse_replacement(uint16_t &outGraphic, bool isParent = false)
{
    assert(outGraphic < MAX_ANIMATIONS_DATA_INDEX_COUNT);
    const auto &anim = g_Index.m_Anim[outGraphic];
    const bool uop = anim.IsUOP && (isParent || !anim.IsValidMUL);
    if (uop)
    {
        return false;
    }
    uint16_t newGraphic = anim.CorpseGraphic;
    do
    {
        const auto conv = uo_has_body_conversion(anim);
        if (conv)
        {
            break;
        }
        const auto &newAnim = g_Index.m_Anim[newGraphic];
        if (outGraphic != newGraphic)
        {
            outGraphic = newGraphic;
            newGraphic = newAnim.CorpseGraphic;
        }
    } while (outGraphic != newGraphic);
    return true;
}
*/
struct CUopMappedFile : public CMappedFile // FIXME: not needed
{
    UopHeader *Header = nullptr;
    std::vector<uint64_t> m_NameHashes;
    std::vector<uint64_t> m_FileOffsets;
    UopSectionHeaderMap m_MapByHash;
    UopSectionHeaderMap m_MapByOffset;

    bool HasAsset(uint64_t hash) const;
    void AddAsset(const UopFileEntry *item);
    const UopFileEntry *GetAsset(const char *filename) const;
    const UopFileEntry *GetAsset(uint64_t hash) const;
    std::vector<uint8_t> GetData(const UopFileEntry *block);
    std::vector<uint8_t> GetMeta(const UopFileEntry *block);
    std::vector<uint8_t> GetRaw(const UopFileEntry *block);
    size_t FileCount() const;

    CUopMappedFile() = default;
    virtual ~CUopMappedFile() = default;
};

struct CFileManager : public CDataReader // FIXME: not needed
{
    bool UseUOPGumps = false;
    int UnicodeFontsCount = 0; // never read
    int TexturesDataCount = 0;

    // Idx
    CMappedFile m_AnimIdx[6];
    CMappedFile m_ArtIdx;
    CMappedFile m_GumpIdx;
    CMappedFile m_LightIdx;
    CMappedFile m_MultiIdx;
    CMappedFile m_SkillsIdx;
    CMappedFile m_SoundIdx;
    CMappedFile m_StaticIdx[6];
    CMappedFile m_TextureIdx;

    // Mul
    CMappedFile m_AnimMul[6];
    CMappedFile m_AnimdataMul;
    CMappedFile m_ArtMul;
    CMappedFile m_HuesMul;
    CMappedFile m_GumpMul;
    CMappedFile m_LightMul;
    CMappedFile m_MapMul[6];
    CMappedFile m_MultiMul;
    CMappedFile m_RadarcolMul;
    CMappedFile m_SkillsMul;
    CMappedFile m_SoundMul;
    CMappedFile m_StaticMul[6];
    CMappedFile m_TextureMul;
    CMappedFile m_TiledataMul;
    CMappedFile m_UnifontMul[20];
    CMappedFile m_VerdataMul;
    CMappedFile m_FacetMul[6];

    CMappedFile m_MultiMap;
    CMappedFile m_SpeechMul;
    CMappedFile m_LangcodeIff;

    // UOP
    CUopMappedFile m_StringDictionary;
    CUopMappedFile m_ArtLegacyMUL;
    CUopMappedFile m_GumpartLegacyMUL;
    CUopMappedFile m_SoundLegacyMUL;
    CUopMappedFile m_Tileart;
    CUopMappedFile m_MainMisc;
    CUopMappedFile m_MapUOP[6];
    CUopMappedFile m_MapXUOP[6];
    CUopMappedFile m_AnimationSequence;
    CUopMappedFile m_AnimationFrame[4];
    CUopMappedFile m_MultiCollection;

    // Map patches
    CMappedFile m_MapDifl[6];
    CMappedFile m_MapDif[6];

    CMappedFile m_StaDifl[6];
    CMappedFile m_StaDifi[6];
    CMappedFile m_StaDif[6];

    // Animation Frames
    size_t m_AddressIdx[6];
    size_t m_SizeIdx[6];
    std::vector<std::pair<uint16_t, uint8_t>> m_GroupReplaces[2]; // FIXME: map

    bool Load();
    void Unload();

    std::vector<SkillData> m_Skills;
    void LoadSkills();

    std::vector<HUES_GROUP> m_Hues;
    void LoadHues();

    typedef std::vector<CIndexMap> MAP_INDEX_LIST;
    MAP_INDEX_LIST m_BlockData[MAX_MAPS_COUNT];
    void CreateBlocksTable();
    void CreateBlockTable(int map, int width, int height);

    bool IsMulFileOpen(int idx) const;

    void LoadStringDictionary();
    bool LoadAnimation(const AnimationState &anim, LoadPixelData16Cb pLoadFunc);
    void LoadAnimationFrameInfo(
        AnimationFrameInfo &result, const AnimationState &anim, uint8_t frameIndex, bool isCorpse);
    // --

    CFileManager() = default;
    virtual ~CFileManager() = default;

    bool UopLoadFile(CUopMappedFile &file, const char *fileName, bool dumpFile = false);

private:
    UopAnimationHeader UopReadAnimationHeader();
    UopAnimationFrame UopReadAnimationFrame();
    std::vector<UopAnimationFrame> UopReadAnimationFramesData();
    uint8_t *MulReadAnimationData(const AnimationDirection &direction) const;
    void LoadAnimationFrame(AnimationFrame &frame, uint16_t *palette, LoadPixelData16Cb pLoadFunc);

    bool LoadWithUop();
    bool LoadWithMul();
    bool LoadCommon();

    void LoadTiledata();
    void LoadIndexFiles();
    void LoadAnimations();

    void UopReadAnimations();
    void ProcessAnimSequeceData();

    void PatchFiles();
    void IndexReplaces();

    void MulReadIndexFile(
        size_t indexMaxCount,
        const std::function<CIndexObject *(int index)> &getIdxObj,
        size_t address,
        IndexBlock *ptr,
        const std::function<IndexBlock *()> &getNewPtrValue);
    void UopReadIndexFile(
        size_t indexMaxCount,
        const std::function<CIndexObject *(int index)> &getIdxObj,
        const char *uopFileName,
        int padding,
        const char *extesion,
        CUopMappedFile &uopFile,
        int startIndex = 0);

    void UopReadAnimationFrameInfo(
        AnimationFrameInfo &result,
        AnimationDirection &direction,
        const UopFileEntry *block,
        bool isCorpse);
    bool UopReadAnimationFrames(const AnimationState &anim, LoadPixelData16Cb pLoadFunc);
    void MulReadAnimationFrameInfo(
        AnimationFrameInfo &result,
        AnimationDirection &direction,
        uint8_t frameIndex,
        bool isCorpse);
    bool MulReadAnimationFrames(const AnimationState &anim, LoadPixelData16Cb pLoadFunc);
    // --
};

void uo_data_init(const char *path, uint32_t client_version, bool use_verdata);
ANIMATION_GROUPS_TYPE uo_type_by_graphic(uint16_t graphic); // CalculateTypeByGraphic
uint64_t uo_get_anim_offset(
    uint16_t graphic,
    uint32_t flags,
    ANIMATION_GROUPS_TYPE type,
    int &groupCount); // CalculateOffset
uint32_t uo_get_group_offset(
    ANIMATION_GROUPS group,
    uint16_t
        graphic); // CalculateLowGroupOffset, CalculateHighGroupOffset, CalculatePeopleGroupOffset
void uo_update_animation_tables(uint32_t lockedFlags);
bool uo_animation_exists(uint16_t graphic, uint8_t group);
void uo_animation_dump(const char *file);

typedef std::unordered_map<uint16_t, CEquipConvData> EQUIP_CONV_DATA_MAP;
typedef std::unordered_map<uint16_t, EQUIP_CONV_DATA_MAP> EQUIP_CONV_BODY_MAP;
const CEquipConvData *
uo_get_equipconv(const EQUIP_CONV_BODY_MAP::iterator bodyMapIter, uint16_t graphic);
const CEquipConvData *uo_get_equipconv(uint16_t bodyGraphic, uint16_t graphic);
const EQUIP_CONV_BODY_MAP::iterator uo_get_equip_body_conv(uint16_t bodyGraphic);

extern CFileManager g_FileManager;
