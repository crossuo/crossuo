// MIT License
// Copyright (C) November 2017 Hotride

#pragma once

#include "Gump.h"

class CGumpProperty : public CGump
{
    uint32_t Timer = 0;

private:
    CGLTextTexture m_Texture;

public:
    CGumpProperty(const wstring &text);
    virtual ~CGumpProperty();

    virtual void PrepareContent();

    virtual class CRenderObject *Select() { return nullptr; }
};
