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
    bool StateMy = false;
    bool StateOpponent = false;
    string Text = "";

private:
    static const int ID_GST_CHECKBOX = 1;

    CTextRenderer m_TextRenderer{ CTextRenderer() };

    CGUIButton *m_MyCheck{ nullptr };
    CGUIGumppic *m_OpponentCheck{ nullptr };

    CGUIDataBox *m_MyDataBox{ nullptr };
    CGUIDataBox *m_OpponentDataBox{ nullptr };

protected:
    virtual void CalculateGumpState();

public:
    CGumpSecureTrading(uint32_t serial, short x, short y, uint32_t id, uint32_t id2);
    virtual ~CGumpSecureTrading();

    void SendTradingResponse(int code);

    CTextRenderer *GetTextRenderer() { return &m_TextRenderer; }

    virtual void PrepareContent();

    virtual void UpdateContent();

    virtual void Draw();
    virtual CRenderObject *Select();

    GUMP_BUTTON_EVENT_H;

    virtual void OnLeftMouseButtonUp();
};
