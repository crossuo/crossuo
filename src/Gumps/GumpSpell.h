// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"

struct Spell;

class CGumpSpell : public CGump
{
private:
    static constexpr int ID_GS_LOCK_MOVING = 1;
    static constexpr int ID_GS_BUTTON_REMOVE_FROM_GROUP = 2;
    const Spell *BaseSpell;

protected:
    virtual void CalculateGumpState() override;
    CGUIButton *m_SpellUnlocker = nullptr;

public:
    CGumpSpell(uint32_t serial, SPELLBOOK_TYPE type, uint16_t graphic, short x, short y);
    virtual ~CGumpSpell();

    SPELLBOOK_TYPE SpellType = ST_MAGERY;
    CGUIAlphaBlending *m_Blender = nullptr;
    CGumpSpell *m_GroupNext = nullptr;
    CGumpSpell *m_GroupPrev = nullptr;

    CGumpSpell *GetTopSpell();
    CGumpSpell *GetNearSpell(int &x, int &y);
    bool GetSpellGroupOffset(int &x, int &y);
    void UpdateGroup(int x, int y);
    void AddSpell(CGumpSpell *spell);
    void RemoveFromGroup();
    bool InGroup() { return (m_GroupNext != nullptr || m_GroupPrev != nullptr); }

    virtual void InitToolTip() override;
    virtual void PrepareContent() override;
    GUMP_BUTTON_EVENT_H override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
};
