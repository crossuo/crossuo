// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "TextureObject.h"
#include "api/mulstruct.h"

struct UopBlockHeader;

struct CIndexObject
{
    const UopBlockHeader *UopBlock = nullptr;
    CGLTexture *Texture = nullptr;
    size_t Address = 0;
    uint32_t DataSize = 0;
    int32_t Width = 0;
    int32_t Height = 0;
    uint32_t LastAccessTime = 0;
    uint16_t Color = 0;
    void ReadIndexFile(size_t address, IndexBlock *ptr);
};

struct CIndexObjectLand : public CIndexObject
{
    bool AllBlack = false;
};

struct CIndexObjectStatic : public CIndexObject
{
    uint16_t Index = 0;
    char Offset = 0;
    char AnimIndex = 0;
    uint32_t ChangeTime = 0;
    uint16_t LightColor = 0;
    bool IsFiled = false;
};

struct CIndexSound : public CIndexObject
{
    uint32_t Delay = 0;
    uint8_t *m_WaveFile = nullptr;
    SoundHandle m_Stream = SOUND_NULL;
};

struct CIndexMulti : public CIndexObject
{
    uint32_t Count = 0;
    void ReadIndexFile(size_t address, IndexBlock *ptr);
};

struct CIndexGump : public CIndexObject
{
    void ReadIndexFile(size_t address, IndexBlock *ptr);
};

struct CIndexLight : public CIndexObject
{
    void ReadIndexFile(size_t address, IndexBlock *ptr);
};

struct CIndexAnimation
{
    uint16_t Graphic = 0;
    uint16_t Color = 0;
    ANIMATION_GROUPS_TYPE Type = AGT_UNKNOWN;
    uint32_t Flags = 0;
    char MountedHeightOffset = 0;
    bool IsUOP = false;
    CTextureAnimationGroup m_Groups[ANIMATION_GROUPS_COUNT];
};

struct CIndexMusic
{
    string FilePath;
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

    int m_MultiIndexCount = 0;
};


template <typename T, size_t SIZE>
void ValidateTextureIsDeleted(T (&arr)[SIZE])
{
    DEBUG_TRACE_FUNCTION;
    for (int i = 0; i < SIZE; ++i)
    {
        CIndexObject &obj = arr[i];
        assert(obj.Texture == nullptr);
    }
}

extern Index g_Index;
