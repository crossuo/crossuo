// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "../GUI/GUIText.h"
#include "../TextEngine/TextContainer.h"
#include "../TextEngine/EntryText.h"
#include "../TextEngine/TextRenderer.h"

class CGumpSecureTrading : public CGump
{
public:
    uint32_t ID2 = 0;
    uint32_t Gold = 0;
    uint32_t Platinum = 0;
    // 0x6F gold-ledger sub-commands carry a single value pair; sub-command 3
    // is the opponent's ledger, sub-command 4 is ours (see SecureTrading)
    uint32_t HisGold = 0;
    uint32_t HisPlatinum = 0;
    bool StateMine = false;
    bool StateOpponent = false;
    astr_t Text;

private:
    // Gold ledger labels for the new trade window (client >= 7.0.45.65),
    // indexed as 0 = my gold, 1 = my platinum, 2 = opponent gold,
    // 3 = opponent platinum; null on the old trade gump
    enum
    {
        COIN_MY_GOLD,
        COIN_MY_PLATINUM,
        COIN_HIS_GOLD,
        COIN_HIS_PLATINUM,
        COIN_COUNT
    };
    CGUIText *m_CoinLabels[COIN_COUNT] = { nullptr };
    uint32_t m_CoinValues[COIN_COUNT] = { 0 };
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
