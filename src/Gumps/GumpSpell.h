// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpSpell : public CGump
{
public:
    bool BigIcon = false;
    SPELLBOOK_TYPE SpellType = ST_MAGE;

private:
    static constexpr int ID_GS_LOCK_MOVING = 1;
    static constexpr int ID_GS_BUTTON_REMOVE_FROM_GROUP = 2;

    void GetTooltipSpellInfo(int &tooltipOffset, int &spellIndexOffset);

protected:
    virtual void CalculateGumpState();

    CGUIButton *m_SpellUnlocker{ nullptr };

public:
    CGumpSpell(uint32_t serial, short x, short y, uint16_t graphic, SPELLBOOK_TYPE spellType);
    virtual ~CGumpSpell();

    CGUIAlphaBlending *m_Blender{ nullptr };

    CGumpSpell *m_GroupNext{ nullptr };
    CGumpSpell *m_GroupPrev{ nullptr };

    CGumpSpell *GetTopSpell();
    CGumpSpell *GetNearSpell(int &x, int &y);
    bool GetSpellGroupOffset(int &x, int &y);
    void UpdateGroup(int x, int y);
    void AddSpell(CGumpSpell *spell);
    void RemoveFromGroup();

    bool InGroup() { return (m_GroupNext != nullptr || m_GroupPrev != nullptr); }

    virtual void InitToolTip();

    virtual void PrepareContent();

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};
