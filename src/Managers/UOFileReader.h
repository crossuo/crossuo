// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Sprite.h"
struct CIndexObject;

// FIXME: C
struct UOFileReader
{
    UOFileReader() = default;
    ~UOFileReader() = default;

    std::vector<uint16_t> GetGumpPixels(CIndexObject &io);
    CSprite *ReadGump(CIndexObject &io);
    std::vector<uint16_t>
    GetArtPixels(uint16_t id, CIndexObject &io, bool run, short &width, short &height);
    CSprite *ReadArt(uint16_t id, CIndexObject &io, bool run);
    CSprite *ReadTexture(CIndexObject &io);
    CSprite *ReadLight(CIndexObject &io);
};

extern UOFileReader g_UOFileReader;
