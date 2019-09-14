// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"
#include "../TextEngine/TextContainer.h"
#include "../TextEngine/EntryText.h"
#include "../TextEngine/TextRenderer.h"

class CGumpSecureTrading : public CGump
{
public:
    uint32_t ID2 = 0;
    uint32_t Gold = 0;
    uint32_t Platinum = 0;
    bool StateMine = false;
    bool StateOpponent = false;
    std::string Text;

private:
    uint8_t mdbx = 45;
    uint8_t mdby = 70;
    uint8_t odbx = 192;
    uint8_t odby = 70;
    CTextRenderer m_TextRenderer = CTextRenderer();
    CGUIButton *m_MyCheck = nullptr;
    CGUIGumppic *m_OpponentCheck = nullptr;
    CGUIDataBox *m_MyDataBox = nullptr;
    CGUIDataBox *m_OpponentDataBox = nullptr;

protected:
    virtual void CalculateGumpState() override;

public:
    CGumpSecureTrading(uint32_t serial, int x, int y, uint32_t id, uint32_t id2);
    virtual ~CGumpSecureTrading() = default;

    void SendTradingResponse(int code);
    CTextRenderer *GetTextRenderer() override { return &m_TextRenderer; }

    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void Draw() override;
    virtual CRenderObject *Select() override;

    GUMP_BUTTON_EVENT_H override;
    virtual void OnLeftMouseButtonUp() override;
};
