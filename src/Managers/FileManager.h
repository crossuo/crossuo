// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../FileSystem.h"
#include "../Wisp/WispMappedFile.h"
#include "../Utility/AutoResetEvent.h"

struct UOPAnimationData;
class CTextureAnimationDirection;

class CUopBlockHeader
{
public:
    uint64_t Offset = 0;
    //uint32_t HeaderSize = 0;
    uint32_t CompressedSize = 0;
    uint32_t DecompressedSize = 0;
    //uint64_t Hash = 0;
    //uint32_t Unknown = 0;
    //uint16_t Flags = 0;

    CUopBlockHeader() {}
    CUopBlockHeader(
        uint64_t offset,
        int headerSize,
        int compresseSize,
        int decompressedSize,
        uint64_t Hash,
        int unknown,
        uint16_t flags)
        : Offset(offset)
        , CompressedSize(compresseSize)
        , DecompressedSize(decompressedSize)
    {
    }
    ~CUopBlockHeader() {}
};

class CUopMappedFile : public Wisp::CMappedFile
{
public:
    unordered_map<uint64_t, CUopBlockHeader> m_Map;

public:
    CUopMappedFile();
    virtual ~CUopMappedFile();

    void Add(uint64_t hash, const CUopBlockHeader &item);

    CUopBlockHeader *GetBlock(uint64_t hash);

    vector<uint8_t> GetData(const CUopBlockHeader &block);
};

class CFileManager : public Wisp::CDataReader
{
public:
    bool UseUOPGumps = false;
    int UnicodeFontsCount = 0;

    CFileManager();
    virtual ~CFileManager();

    AutoResetEvent m_AutoResetEvent;

    //!Адреса файлов в памяти
    Wisp::CMappedFile m_AnimIdx[6];
    Wisp::CMappedFile m_ArtIdx;
    Wisp::CMappedFile m_GumpIdx;
    Wisp::CMappedFile m_LightIdx;
    Wisp::CMappedFile m_MultiIdx;
    Wisp::CMappedFile m_SkillsIdx;
    Wisp::CMappedFile m_SoundIdx;
    Wisp::CMappedFile m_StaticIdx[6];
    Wisp::CMappedFile m_TextureIdx;

    std::fstream m_AnimMul[6];
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

    //UOP
    CUopMappedFile m_ArtLegacyMUL;
    CUopMappedFile m_GumpartLegacyMUL;
    CUopMappedFile m_SoundLegacyMUL;
    CUopMappedFile m_Tileart;
    CUopMappedFile m_MainMisc;
    CUopMappedFile m_MapUOP[6];
    CUopMappedFile m_MapXUOP[6];
    CUopMappedFile m_AnimationSequence;
    CUopMappedFile m_MultiCollection;

    //Map patches
    Wisp::CMappedFile m_MapDifl[6];
    Wisp::CMappedFile m_MapDif[6];

    Wisp::CMappedFile m_StaDifl[6];
    Wisp::CMappedFile m_StaDifi[6];
    Wisp::CMappedFile m_StaDif[6];

    bool Load();
    bool LoadWithUOP();
    void Unload();
    void TryReadUOPAnimations();

    //Чтение сжатых данных с кадрами из УОП аним файла.
    static char *ReadUOPDataFromFileStream(UOPAnimationData &animData);

    void
    ReadAnimMulDataFromFileStream(vector<char> &animData, CTextureAnimationDirection &direction);

    //Разжатие данных с кадрами из УОП аним файла.
    static bool
    DecompressUOPFileData(UOPAnimationData &animData, vector<uint8_t> &decLayoutData, char *buf);

    void SendFilesInfo();

    bool IsMulFileOpen(int idx) const;

private:
    void ReadTask();
    static bool FileExists(const os_path &filename);

    static bool TryOpenFileStream(std::fstream &fileStream, const os_path &filePath);

    bool LoadUOPFile(CUopMappedFile &file, const char *fileName);
};

extern CFileManager g_FileManager;
