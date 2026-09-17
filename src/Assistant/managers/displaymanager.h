// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/displaymanager.h). Textual-only port:
// the colored/graphical title modes and the icon cache are gone (an OS
// window caption carries no colors/icons/fonts).

#pragma once

#include "core/global.h"
#include <vector>

namespace xuoassist {

enum DISPLAY_ITEM_TYPE
{
    DIT_NONE = 0,
    DIT_NAME,
    DIT_SERVER,
    DIT_HITS,
    DIT_MAX_HITS,
    DIT_HITS_MAX_HITS,
    DIT_MANA,
    DIT_MAX_MANA,
    DIT_MANA_MAX_MANA,
    DIT_STAM,
    DIT_MAX_STAM,
    DIT_STAM_MAX_STAM,
    DIT_STR,
    DIT_INT,
    DIT_DEX,
    DIT_WEIGHT,
    DIT_MAX_WEIGHT,
    DIT_WEIGHT_MAX_WEIGHT,
    DIT_ARMOR,
    DIT_GOLD,
    DIT_X,
    DIT_Y,
    DIT_Z
};

// The display item model (DISPLAY_ITEM_TYPE + DisplayInfo) lives here rather
// than in a model/ header: the manager, the config storage and the tab UI all
// share the type.
//
// One title-data tag: its catalog identity (Name/Command/Type) plus the
// values it renders with (Text/MinValue/MidValue). The manager's catalog rows
// double as the per-type defaults; custom items carry their own Text.
// Default-initialized: DisplayInfo info; is fully usable (a DIT_NONE tag
// renders plain Text).
struct DisplayInfo
{
    astr_t Name{ "" };
    astr_t Command{ "" };
    DISPLAY_ITEM_TYPE Type = DIT_NONE;
    astr_t Text{ "" };
    int MinValue = 0;
    int MidValue = 0;
};

enum DISPLAY_KEY_IDENTIFIER
{
    DKI_NONE = 0,
    DKI_TEXT,
    DKI_MIN_VAULE,
    DKI_MID_VALUE,
    DKI_COUNT
};

struct DISPLAY_KEY_ITEM
{
    const char *Name;
    DISPLAY_KEY_IDENTIFIER Identifier;
};

class CDisplayManager
{
private:
    vector<DisplayInfo> m_InfoCommands;
    vector<DisplayInfo> m_Commands;
    astr_t m_TitleText;

    static const DISPLAY_KEY_ITEM m_KeysList[DKI_COUNT];

    void ApplyAttributeToDisplayItem(DisplayInfo &obj, const astr_t &name, const astr_t &value);
    void CompileData(const astr_t &text);
    void GetValues(const DisplayInfo &item, astr_t &value, astr_t &maxValue);

public:
    CDisplayManager();
    ~CDisplayManager() = default;

    CDisplayManager(const CDisplayManager &) = delete;
    CDisplayManager &operator=(const CDisplayManager &) = delete;

    const vector<DisplayInfo> &GetCommands() const { return m_InfoCommands; }

    DisplayInfo GetDisplayItemByName(const astr_t &name) const;

    // Title-bar caption: composes the plain text of the enabled tags into
    // m_TitleText. Apply it with CWindow::SetTitle - ui/assistantui.cpp does
    // that on change (throttled).
    void Redraw(bool enabled);

    // The last composed caption (Redraw output)
    const astr_t &GetTitleText() const { return m_TitleText; }

    void RecompileData(const astr_t &text);
};

extern CDisplayManager g_DisplayManager;

} // namespace xuoassist
