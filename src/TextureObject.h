// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Constants.h"
#include "Backend.h"

struct CUopMappedFile;

struct CTextureAnimationFrame : public CGLTexture
{
    int16_t CenterX = 0;
    int16_t CenterY = 0;
    CTextureAnimationFrame() = default;
    virtual ~CTextureAnimationFrame() = default;
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
    CTextureAnimationFrame *m_Frames;

    CTextureAnimationDirection() = default;
    virtual ~CTextureAnimationDirection() = default;
};

struct CTextureAnimationGroup
{
    CTextureAnimationDirection m_Direction[MAX_MOBILE_DIRECTIONS];
    UopBlockHeader m_UOPAnimData;
};
