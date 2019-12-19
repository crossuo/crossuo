// GPLv3 License
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
#include "mulstruct.h"
#include "mappedfile.h"

extern std::string g_dumpUopFile;

#if LIBUO == 1
typedef void *SoundInfo;
typedef void *SoundHandle;
#define SOUND_NULL nullptr
#else
struct CSprite;
#endif

typedef void *(*LoadPixelData16Cb)(int width, int height, uint16_t *pixels);

struct UopFileEntry;
struct CUopMappedFile;
using UopSectionHeaderMap = std::unordered_map<uint64_t, const UopFileEntry *>;

struct CTextureAnimationFrame
{
    void *UserData = nullptr;
    int16_t CenterX = 0;
    int16_t CenterY = 0;
};

struct CTextureAnimationDirection
{
    uint8_t FrameCount = 0;
    size_t BaseAddress = 0;
    uint32_t BaseSize = 0;
    size_t PatchedAddress = 0;
    uint32_t PatchedSize = 0;
    int FileIndex = 0;
    size_t Address = 0;
    uint32_t Size = 0;
    uint32_t LastAccessTime = 0;
    bool IsUOP = false;
    bool IsVerdata = false;
    CTextureAnimationFrame *m_Frames = nullptr;
};

struct CTextureAnimationGroup
{
    CTextureAnimationDirection m_Direction[MAX_MOBILE_DIRECTIONS];
    const UopFileEntry *m_UOPAnimData = nullptr;
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

struct CIndexAnimation
{
    uint16_t Graphic = 0;
    uint16_t Color = 0;
    ANIMATION_GROUPS_TYPE Type = AGT_UNKNOWN;
    uint32_t Flags = 0;
    char MountedHeightOffset = 0;
    bool IsUOP = false;
    CTextureAnimationGroup m_Groups[MAX_ANIMATION_GROUPS_COUNT];
};

struct CIndexMusic
{
    std::string FilePath; // FIXME
    bool Loop = false;
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
    CIndexAnimation m_Anim[MAX_ANIMATIONS_DATA_INDEX_COUNT];

    int m_MultiIndexCount = 0;
};

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

    bool Load();
    void Unload();
    void UopReadAnimations();
    bool IsMulFileOpen(int idx) const;

    bool LoadAnimation(const AnimationSelector &anim, LoadPixelData16Cb pLoadFunc);
    void LoadAnimationFrameInfo(
        AnimationFrameInfo &result,
        CTextureAnimationDirection &direction,
        CTextureAnimationGroup &group,
        uint8_t frameIndex,
        bool isCorpse);
    // --

    CFileManager() = default;
    virtual ~CFileManager() = default;

    void WaitTasks() const;

    bool UopLoadFile(CUopMappedFile &file, const char *fileName, bool dumpFile = false);
    bool MulLoadFile(CMappedFile &file, const fs_path &fileName);

private:
    // moved from AnimationManager
    UopAnimationHeader UopReadAnimationHeader();
    UopAnimationFrame UopReadAnimationFrame();
    std::vector<UopAnimationFrame> UopReadAnimationFramesData();
    uint8_t *MulReadAnimationData(const CTextureAnimationDirection &direction) const;
    void LoadAnimationFrame(
        CTextureAnimationFrame &frame, uint16_t *palette, LoadPixelData16Cb pLoadFunc);

    bool LoadWithUop();
    bool LoadWithMul();
    bool LoadCommon();

    void LoadTiledata();
    void LoadIndexFiles();

    void ReadTask();
    void ProcessAnimSequeceData();

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

    // from AnimationManager
    void UopReadAnimationFrameInfo(
        AnimationFrameInfo &result,
        CTextureAnimationDirection &direction,
        const UopFileEntry &block);
    bool UopReadAnimationFrames(
        CTextureAnimationDirection &direction,
        const AnimationSelector &anim,
        LoadPixelData16Cb pLoadFunc);
    void MulReadAnimationFrameInfo(
        AnimationFrameInfo &result,
        CTextureAnimationDirection &direction,
        uint8_t frameIndex,
        bool isCorpse);
    bool MulReadAnimationFrames(CTextureAnimationDirection &direction, LoadPixelData16Cb pLoadFunc);
    // --
};

void uo_data_init(const char *path, uint32_t client_version, bool use_verdata);

extern CFileManager g_FileManager;
