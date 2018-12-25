// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CIndexObject
{
public:
    size_t Address = 0;
    int DataSize = 0;
    int Width = 0;
    int Height = 0;
    uint16_t ID = 0;
    uint16_t Color = 0;
    uint32_t LastAccessTime = 0;
    CUopBlockHeader *UopBlock = nullptr;
    CIndexObject();
    virtual ~CIndexObject();
    virtual void ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id);
    CGLTexture *Texture{ nullptr };
};

class CIndexObjectLand : public CIndexObject
{
public:
    bool AllBlack = false;

    CIndexObjectLand();
    virtual ~CIndexObjectLand();
};

class CIndexObjectStatic : public CIndexObject
{
public:
    uint16_t Index = 0;
    char Offset = 0;
    char AnimIndex = 0;
    uint32_t ChangeTime = 0;
    uint16_t LightColor = 0;
    bool IsFiled = false;

    CIndexObjectStatic();
    virtual ~CIndexObjectStatic();
};

class CIndexSound : public CIndexObject
{
public:
    uint32_t Delay = 0;

    CIndexSound();
    virtual ~CIndexSound();

    uint8_t *m_WaveFile = nullptr;
    SoundHandle m_Stream = SOUND_NULL;
};

class CIndexMulti : public CIndexObject
{
public:
    uint32_t Count = 0;

    CIndexMulti();
    virtual ~CIndexMulti();
    virtual void ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id) override;
};

class CIndexGump : public CIndexObject
{
public:
    CIndexGump();
    virtual ~CIndexGump();
    virtual void ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id) override;
};

class CIndexLight : public CIndexObject
{
public:
    CIndexLight();
    virtual ~CIndexLight();
    virtual void ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id) override;
};

class CIndexAnimation
{
public:
    uint16_t Graphic = 0;
    uint16_t Color = 0;
    ANIMATION_GROUPS_TYPE Type = AGT_UNKNOWN;
    uint32_t Flags = 0;
    char MountedHeightOffset = 0;
    bool IsUOP = false;

    CIndexAnimation();
    virtual ~CIndexAnimation();

    CTextureAnimationGroup m_Groups[ANIMATION_GROUPS_COUNT];
};

class CIndexMusic
{
public:
    string FilePath;
    bool Loop = false;

    CIndexMusic();
    virtual ~CIndexMusic();
};
