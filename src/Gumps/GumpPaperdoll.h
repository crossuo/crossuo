// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "../TextEngine/TextRenderer.h"
#include "../TextEngine/TextContainer.h"

class CGumpPaperdoll : public CGump
{
protected:
    static const int m_LayerCount = 23;
    static int UsedLayers[m_LayerCount];

    CTextRenderer m_TextRenderer;
    CGUIDataBox *m_DataBox = nullptr;
    CGUIText *m_Description = nullptr;
    bool m_WantTransparentContent = false;

    virtual void CalculateGumpState() override;

public:
    static const int ID_GP_ITEMS = 20;

    CGumpPaperdoll(uint32_t serial, short x, short y, bool minimized);
    virtual ~CGumpPaperdoll();

    bool CanLift = false;
    CGUIButton *m_ButtonWarmode = nullptr;
    CTextContainer m_TextContainer{ CTextContainer(10) };

    virtual CTextRenderer *GetTextRenderer() override { return &m_TextRenderer; }
    void UpdateDescription(const astr_t &text);
    virtual void DelayedClick(CRenderObject *obj) override;
    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void InitToolTip() override;
    virtual void Draw() override;
    virtual CRenderObject *Select() override;

    GUMP_BUTTON_EVENT_H override;
    virtual void OnLeftMouseButtonUp() override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
};
