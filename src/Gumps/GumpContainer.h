// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"
#include "../TextEngine/TextRenderer.h"

class CGumpContainer : public CGump
{
    bool IsGameBoard = false;

private:
    uint32_t m_CorpseEyesTicks{ 0 };
    uint8_t m_CorpseEyesOffset{ 0 };

    CTextRenderer m_TextRenderer{ CTextRenderer() };

    CGUIGumppic *m_CorpseEyes{ nullptr };
    CGUIDataBox *m_DataBox{ nullptr };

protected:
    virtual void CalculateGumpState();

public:
    CGumpContainer(uint32_t serial, uint32_t id, short x, short y);
    virtual ~CGumpContainer();

    static const uint32_t ID_GC_LOCK_MOVING;
    static const uint32_t ID_GC_MINIMIZE;

    CGUIGumppic *m_BodyGump{ nullptr };

    void UpdateItemCoordinates(class CGameObject *item);

    CTextRenderer *GetTextRenderer() { return &m_TextRenderer; }

    virtual void PrepareTextures();
    virtual void PrepareContent();
    virtual void UpdateContent();
    virtual void InitToolTip();
    virtual void Draw();
    virtual CRenderObject *Select();

    GUMP_BUTTON_EVENT_H;

    virtual void OnLeftMouseButtonUp();
    virtual bool OnLeftMouseButtonDoubleClick();
};
