// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

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
    // Art block pixels: the reference decoder (block selection, RLE rows,
    // bounds handling). `applyTileFilters` is false for read-only consumers
    // such as the asset viewer, which must see the stored art rather than the
    // game's stump/cave substitution.
    std::vector<uint16_t> GetArtPixels(
        uint16_t id,
        CIndexObject &io,
        bool run,
        short &width,
        short &height,
        bool applyTileFilters = true);
    CSprite *ReadArt(uint16_t id, CIndexObject &io, bool run);
    CSprite *ReadTexture(CIndexObject &io);
    CSprite *ReadLight(CIndexObject &io);
};

extern UOFileReader g_UOFileReader;
