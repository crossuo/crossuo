// MIT License
// Copyright (C) August 2016 Hotride

#include "TextureObject.h"

CTextureAnimationFrame::CTextureAnimationFrame()
    : CenterX(0)
    , CenterY(0)
{
}

CTextureAnimationFrame::~CTextureAnimationFrame()
{
}

CTextureAnimationDirection::CTextureAnimationDirection()
    : FrameCount(0)
    , BaseAddress(0)
    , BaseSize(0)
    , PatchedAddress(0)
    , PatchedSize(0)
    , Address(0)
    , Size(0)
    , LastAccessTime(0)
    , m_Frames(nullptr)
    , IsUOP(false)
    , IsVerdata(false)
{
}

CTextureAnimationDirection::~CTextureAnimationDirection()
{
}

CTextureAnimationGroup::CTextureAnimationGroup()
{
}

CTextureAnimationGroup::~CTextureAnimationGroup()
{
}
