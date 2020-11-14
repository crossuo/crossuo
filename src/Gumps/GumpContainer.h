// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

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
    virtual void CalculateGumpState() override;

public:
    CGumpContainer(uint32_t serial, uint32_t id, short x, short y);
    virtual ~CGumpContainer();

    static const uint32_t ID_GC_LOCK_MOVING;
    static const uint32_t ID_GC_MINIMIZE;

    CGUIGumppic *m_BodyGump{ nullptr };

    void UpdateItemCoordinates(class CGameObject *item);

    CTextRenderer *GetTextRenderer() override { return &m_TextRenderer; }

    virtual void PrepareTextures() override;
    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void InitToolTip() override;
    virtual void Draw() override;
    virtual CRenderObject *Select() override;

    GUMP_BUTTON_EVENT_H override;

    virtual void OnLeftMouseButtonUp() override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
};
