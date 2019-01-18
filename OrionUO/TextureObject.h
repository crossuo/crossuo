// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Constants.h"
#include "Backend.h"

struct UOPAnimationData
{
    os_path path;
    unsigned int offset;
    unsigned int compressedLength;
    unsigned int decompressedLength;
    std::fstream *fileStream;
};

class CTextureAnimationFrame : public CGLTexture
{
public:
    short CenterX = 0;
    short CenterY = 0;

    CTextureAnimationFrame();
    virtual ~CTextureAnimationFrame();
};

class CTextureAnimationDirection
{
public:
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

    CTextureAnimationDirection();
    virtual ~CTextureAnimationDirection();

    CTextureAnimationFrame *m_Frames;
};

class CTextureAnimationGroup
{
public:
    CTextureAnimationGroup();
    virtual ~CTextureAnimationGroup();

    CTextureAnimationDirection m_Direction[MAX_MOBILE_DIRECTIONS];
    UOPAnimationData m_UOPAnimData;
};
