// MIT License
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
