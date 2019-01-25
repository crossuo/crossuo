// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../IndexObject.h"

class UOFileReader
{
public:
    UOFileReader() {}
    ~UOFileReader() {}

    vector<uint16_t> GetGumpPixels(CIndexObject &io);
    CGLTexture *ReadGump(CIndexObject &io);
    vector<uint16_t>
    GetArtPixels(uint16_t id, CIndexObject &io, bool run, short &width, short &height);
    CGLTexture *ReadArt(uint16_t id, CIndexObject &io, bool run);
    CGLTexture *ReadTexture(CIndexObject &io);
    CGLTexture *ReadLight(CIndexObject &io);
};

extern UOFileReader g_UOFileReader;
