// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "RenderTextObject.h"
#include "../ImageBounds.h"

class CRenderWorldObject;

class CTextRenderer : public CRenderTextObject
{
private:
    deque<CTextImageBounds> m_TextRect;
    bool ProcessTextRemoveBlending(CTextData &text);

public:
    CTextRenderer();
    virtual ~CTextRenderer();

    CRenderTextObject *m_TextItems{ nullptr };
    CRenderTextObject *m_DrawPointer{ nullptr };
    CRenderTextObject *AddText(CRenderTextObject *obj);

    void ToTop(CRenderTextObject *obj);
    void AddRect(const CTextImageBounds &rect) { m_TextRect.push_back(rect); }
    void ClearRect() { m_TextRect.clear(); }
    bool InRect(CTextData *text, CRenderWorldObject *rwo);
    bool CalculatePositions(bool noCalculate);
    void Draw();
    void Select(class CGump *gump);
    bool CalculateWorldPositions(bool noCalculate);
    void WorldDraw();
};

extern CTextRenderer g_WorldTextRenderer;
