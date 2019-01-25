// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../RenderObject.h"

class CRenderTextObject : public CRenderObject
{
public:
    bool Transparent = false;

    CRenderTextObject();
    virtual ~CRenderTextObject();

    CRenderTextObject *m_NextDraw{ nullptr };
    CRenderTextObject *m_PrevDraw{ nullptr };

    void UnlinkDraw();
    void ToTop();
};
