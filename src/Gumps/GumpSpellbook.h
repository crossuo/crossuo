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

class CGumpSpellbook : public CGump
{
    SPELLBOOK_TYPE BookType = ST_MAGE;

public:
    static const int MAX_SPELLS_COUNT = 64;
    static const int SPELLBOOK_1_SPELLS_COUNT = 64;
    static const int SPELLBOOK_2_SPELLS_COUNT = 17;
    static const int SPELLBOOK_3_SPELLS_COUNT = 10;
    static const int SPELLBOOK_4_SPELLS_COUNT = 6;
    static const int SPELLBOOK_5_SPELLS_COUNT = 8;
    static const int SPELLBOOK_6_SPELLS_COUNT = 16;
    static const int SPELLBOOK_7_SPELLS_COUNT = 30;

    static astr_t m_SpellName1[SPELLBOOK_1_SPELLS_COUNT][2];
    static const astr_t m_SpellName2[SPELLBOOK_2_SPELLS_COUNT][2];
    static const astr_t m_SpellName3[SPELLBOOK_3_SPELLS_COUNT][2];
    static const astr_t m_SpellName4[SPELLBOOK_4_SPELLS_COUNT];
    static const astr_t m_SpellName5[SPELLBOOK_5_SPELLS_COUNT];
    static const astr_t m_SpellName6[SPELLBOOK_6_SPELLS_COUNT][2];
    static const astr_t m_SpellName7[SPELLBOOK_7_SPELLS_COUNT][2];

    static astr_t m_SpellReagents1[SPELLBOOK_1_SPELLS_COUNT];

private:
    static const astr_t m_SpellReagents2[SPELLBOOK_2_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries2[SPELLBOOK_2_SPELLS_COUNT];
    static const PALADIN_SPELL_REQURIES m_SpellRequries3[SPELLBOOK_3_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries4[SPELLBOOK_4_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries5[SPELLBOOK_5_SPELLS_COUNT];
    static const SPELL_REQURIES m_SpellRequries6[SPELLBOOK_6_SPELLS_COUNT];
    //static const SPELL_REQURIES m_SpellRequries7[SPELLBOOK_7_SPELLS_COUNT];

    int m_SpellCount{ 0 };
    uint8_t m_Spells[MAX_SPELLS_COUNT];
    int PageCount{ 8 };

    CGUIGumppic *m_Body{ nullptr };
    CGUIText *m_TithingPointsText{ nullptr };

    CGUIButton *m_PrevPage{ nullptr };
    CGUIButton *m_NextPage{ nullptr };

    CGUIGumppic *m_LastSpellPointer{ nullptr };
    CGUIGumppic *m_LastSpellBookmark{ nullptr };

    void GetTooltipBookInfo(int &dictionaryPagesCount, int &tooltipOffset);
    void GetSummaryBookInfo(
        int &maxSpellsCount,
        int &dictionaryPagesCount,
        int &spellsOnPage,
        int &spellIndexOffset,
        uint16_t &graphic,
        uint16_t &minimizedGraphic,
        uint16_t &iconStartGraphic);

    astr_t GetSpellName(int offset, astr_t &abbreviature, astr_t &reagents);
    astr_t GetSpellRequries(int offset, int &y);

public:
    CGumpSpellbook(uint32_t serial, int x, int y);
    virtual ~CGumpSpellbook();

    static void InitStaticData();
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
