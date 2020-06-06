// MIT License
// Copyright (C) September 2016 Hotride

#pragma once

#include "Gump.h"

struct SPELL_REQURIES
{
    uint8_t ManaCost;
    uint8_t MinSkill;
};

struct PALADIN_SPELL_REQURIES
{
    uint8_t ManaCost;
    uint8_t MinSkill;
    uint8_t TithingPoints;
};

void InitSpells();

class CGumpSpellbook : public CGump
{
    SPELLBOOK_TYPE BookType = ST_MAGERY;

public:
    static const int MAX_SPELLS_COUNT = 64;
    static const int SPELLBOOK_1_SPELLS_COUNT = 64; // Magery
    static const int SPELLBOOK_2_SPELLS_COUNT = 17; // Necromancy
    static const int SPELLBOOK_3_SPELLS_COUNT = 10; // Chivalry
    static const int SPELLBOOK_4_SPELLS_COUNT = 6;  // Bushido
    static const int SPELLBOOK_5_SPELLS_COUNT = 8;  // Ninjitsu
    static const int SPELLBOOK_6_SPELLS_COUNT = 16; // Spellweaving
    static const int SPELLBOOK_7_SPELLS_COUNT = 16; // Mysticism
    static const int SPELLBOOK_8_SPELLS_COUNT = 45; // Mastery

    static astr_t m_SpellName1[SPELLBOOK_1_SPELLS_COUNT][2];
    static const astr_t m_SpellName2[SPELLBOOK_2_SPELLS_COUNT][2];
    static const astr_t m_SpellName3[SPELLBOOK_3_SPELLS_COUNT][2];
    static const astr_t m_SpellName4[SPELLBOOK_4_SPELLS_COUNT];
    static const astr_t m_SpellName5[SPELLBOOK_5_SPELLS_COUNT];
    static const astr_t m_SpellName6[SPELLBOOK_6_SPELLS_COUNT][2];
    static const astr_t m_SpellName7[SPELLBOOK_7_SPELLS_COUNT][3];
    static const astr_t m_SpellName8[SPELLBOOK_8_SPELLS_COUNT][3];

    static astr_t m_SpellReagents1[SPELLBOOK_1_SPELLS_COUNT];

private:
    static const astr_t m_SpellReagents2[SPELLBOOK_2_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries2[SPELLBOOK_2_SPELLS_COUNT];
    static const PALADIN_SPELL_REQURIES m_SpellRequries3[SPELLBOOK_3_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries4[SPELLBOOK_4_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries5[SPELLBOOK_5_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries6[SPELLBOOK_6_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries7[SPELLBOOK_7_SPELLS_COUNT];
    static const PALADIN_SPELL_REQURIES m_SpellRequries8[SPELLBOOK_8_SPELLS_COUNT];
    static const int m_SpellIndices8[SPELLBOOK_8_SPELLS_COUNT][8];

    int m_SpellCount = 0;
    int PageCount = 8;
    uint8_t m_Spells[MAX_SPELLS_COUNT];

    CGUIGumppic *m_Body = nullptr;
    CGUIText *m_TithingPointsText = nullptr;

    CGUIButton *m_PrevPage = nullptr;
    CGUIButton *m_NextPage = nullptr;

    CGUIGumppic *m_LastSpellPointer = nullptr;
    CGUIGumppic *m_LastSpellBookmark = nullptr;

    void GetTooltipBookInfo(int &indexPagesCount, int &tooltipOffset);
    void GetSummaryBookInfo(
        int &maxSpellsCount,
        int &indexPagesCount,
        int &spellsOnPage,
        int &spellIndexOffset,
        uint16_t &graphic,
        uint16_t &minimizedGraphic,
        uint16_t &iconStartGraphic);

    astr_t GetSpellName(int offset, astr_t &magicWord, astr_t &reagents);
    astr_t GetSpellRequries(int offset, int &y);

public:
    CGumpSpellbook(uint32_t serial, int x, int y);
    virtual ~CGumpSpellbook();

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
