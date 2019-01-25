// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "RenderTextObject.h"

class CRenderWorldObject;

class CTextData : public CRenderTextObject
{
public:
    bool Unicode = false;
    TEXT_TYPE Type = TT_CLIENT;
    uint8_t Font = 0;
    uint32_t Timer = 0;
    uint32_t MoveTimer = 0;
    string Text = "";
    wstring UnicodeText = {};
    uint8_t Alpha = 0xFF;
    CRenderWorldObject *Owner = nullptr;
    CGLTextTexture m_Texture;

    CTextData();
    CTextData(CTextData *obj);
    virtual ~CTextData();

    virtual bool IsText() { return true; }
    bool CanBeDrawedInJournalGump();
    void GenerateTexture(
        int maxWidth,
        uint16_t flags = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint8_t cell = 30,
        int font = -1);
};
