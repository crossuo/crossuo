// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../FileSystem.h"
#include "../Wisp/WispMappedFile.h"
#include "../Utility/AutoResetEvent.h"

struct CTextureAnimationDirection;
using UopBlockHeaderMap = std::unordered_map<uint64_t, const UopBlockHeader *>;

struct CUopMappedFile : public Wisp::CMappedFile
{
    UopHeader *Header = nullptr;
    UopBlockHeaderMap m_Map;

    bool HasAsset(uint64_t hash) const;
    void Add(uint64_t hash, const UopBlockHeader *item);
    const UopBlockHeader *GetBlock(uint64_t hash);
    vector<uint8_t> GetData(const UopBlockHeader *block);

    CUopMappedFile() = default;
    virtual ~CUopMappedFile() = default;
};

struct CFileManager : public Wisp::CDataReader
{
    bool UseUOPGumps = false;
    int UnicodeFontsCount = 0; // never read

    AutoResetEvent m_AutoResetEvent;

    // Idx
    Wisp::CMappedFile m_AnimIdx[6];
    Wisp::CMappedFile m_ArtIdx;
    Wisp::CMappedFile m_GumpIdx;
    Wisp::CMappedFile m_LightIdx;
    Wisp::CMappedFile m_MultiIdx;
    Wisp::CMappedFile m_SkillsIdx;
    Wisp::CMappedFile m_SoundIdx;
    Wisp::CMappedFile m_StaticIdx[6];
    Wisp::CMappedFile m_TextureIdx;

    // Mul
    Wisp::CMappedFile m_AnimMul[6];
    Wisp::CMappedFile m_AnimdataMul;
    Wisp::CMappedFile m_ArtMul;
    Wisp::CMappedFile m_HuesMul;
    Wisp::CMappedFile m_GumpMul;
    Wisp::CMappedFile m_LightMul;
    Wisp::CMappedFile m_MapMul[6];
    Wisp::CMappedFile m_MultiMul;
    Wisp::CMappedFile m_RadarcolMul;
    Wisp::CMappedFile m_SkillsMul;
    Wisp::CMappedFile m_SoundMul;
    Wisp::CMappedFile m_StaticMul[6];
    Wisp::CMappedFile m_TextureMul;
    Wisp::CMappedFile m_TiledataMul;
    Wisp::CMappedFile m_UnifontMul[20];
    Wisp::CMappedFile m_VerdataMul;
    Wisp::CMappedFile m_FacetMul[6];

    Wisp::CMappedFile m_MultiMap;
    Wisp::CMappedFile m_SpeechMul;
    Wisp::CMappedFile m_LangcodeIff;

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
    Wisp::CMappedFile m_MapDifl[6];
    Wisp::CMappedFile m_MapDif[6];

    Wisp::CMappedFile m_StaDifl[6];
    Wisp::CMappedFile m_StaDifi[6];
    Wisp::CMappedFile m_StaDif[6];

    bool Load();
    bool LoadWithUop();
    void Unload();
    void UopReadAnimations();
    uint8_t *MulReadAnimationData(const CTextureAnimationDirection &direction) const;
    bool IsMulFileOpen(int idx) const;
    static bool UopDecompressBlock(const UopBlockHeader &block, uint8_t *dst, int fileId);

    CFileManager() = default;
    virtual ~CFileManager() = default;

private:
    void ReadTask();
    bool UopLoadFile(CUopMappedFile &file, const char *fileName);
    bool MulLoadFile(Wisp::CMappedFile &file, const os_path &fileName);
    void ProcessAnimSequeceData();
};

extern CFileManager g_FileManager;
