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
    UopBlockHeader *UopBlock = nullptr;
    CGLTexture *Texture = nullptr;

    CIndexObject() = default;
    virtual ~CIndexObject();
    virtual void ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id);
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

    CIndexMulti() = default;
    virtual ~CIndexMulti() = default;
    virtual void ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id) override;
};

struct CIndexGump : public CIndexObject
{
    CIndexGump() = default;
    virtual ~CIndexGump() = default;
    virtual void ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id) override;
};

struct CIndexLight : public CIndexObject
{
    CIndexLight() = default;
    virtual ~CIndexLight() = default;
    virtual void ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id) override;
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

    CIndexAnimation() = default;
    virtual ~CIndexAnimation() = default;
};

struct CIndexMusic
{
    string FilePath;
    bool Loop = false;

    CIndexMusic() = default;
    virtual ~CIndexMusic() = default;
};
