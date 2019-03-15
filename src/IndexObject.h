﻿// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "TextureObject.h"
#include "api/mulstruct.h"

struct UopBlockHeader;

struct CIndexObject
{
    size_t Address = 0;
    int DataSize = 0;
    int Width = 0;
    int Height = 0;
    uint16_t ID = 0;
    uint16_t Color = 0;
    uint32_t LastAccessTime = 0;
    const UopBlockHeader *UopBlock = nullptr;
    CGLTexture *Texture = nullptr;

    virtual void ReadIndexFile(size_t address, IndexBlock *ptr, const uint16_t id);
    CIndexObject() = default;
    virtual ~CIndexObject();
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
    SoundHandle m_Stream = SOUND_NULL;

    CIndexSound() = default;
    virtual ~CIndexSound() = default;
};

struct CIndexMulti : public CIndexObject
{
    uint32_t Count = 0;

    virtual void ReadIndexFile(size_t address, IndexBlock *ptr, const uint16_t id) override;

    CIndexMulti() = default;
    virtual ~CIndexMulti() = default;
};

struct CIndexGump : public CIndexObject
{
    virtual void ReadIndexFile(size_t address, IndexBlock *ptr, const uint16_t id) override;

    CIndexGump() = default;
    virtual ~CIndexGump() = default;
};

struct CIndexLight : public CIndexObject
{
    virtual void ReadIndexFile(size_t address, IndexBlock *ptr, const uint16_t id) override;

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

extern Index g_Index;
