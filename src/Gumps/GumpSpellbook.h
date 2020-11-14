// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "../Spells.h"

class CGumpSpellbook : public CGump
{
private:
    SPELLBOOK_TYPE BookType = ST_MAGERY;
    int SpellCount = 0;
    int PageCount = 8;
    uint8_t Spells[MAX_SPELLS_COUNT];
    uint8_t SpellPage[MAX_SPELLS_COUNT];

    CGUIGumppic *m_Body = nullptr;
    CGUIText *m_TithingPointsText = nullptr;

    CGUIButton *m_PrevPage = nullptr;
    CGUIButton *m_NextPage = nullptr;

    CGUIGumppic *m_LastSpellPointer = nullptr;
    CGUIGumppic *m_LastSpellBookmark = nullptr;

    astr_t GetSpellRequries(int offset, int &y) const;

public:
    CGumpSpellbook(uint32_t serial, int x, int y);
    virtual ~CGumpSpellbook() = default;

    void UpdateGraphic(uint16_t parentGraphic);
    void ChangePage(int newPage);

    virtual void InitToolTip() override;
    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void DelayedClick(CRenderObject *obj) override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;

    virtual bool OnLeftMouseButtonDoubleClick() override;
};
