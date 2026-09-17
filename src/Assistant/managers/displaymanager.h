// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/displaymanager.h), data-only port
// (the icon cache and the title-bar

#pragma once

#include "core/global.h"
#include "model/commonitems/displayitem.h"
#include <map>

namespace xuoassist {

enum REDRAW_DISPLAY_MODE
{
    RDM_DISABLED = 0,
    RDM_TEXTUAL,
    RDM_COLORED_TEXTUAL,
    RDM_COLORED_GRAPHICAL
};

enum DISPLAY_KEY_IDENTIFIER
{
    DKI_NONE = 0,
    DKI_TEXT,
    DKI_ID,
    DKI_HUE,
    DKI_COLOR_MIN,
    DKI_COLOR_MID,
    DKI_COLOR,
    DKI_BACKGROUND,
    DKI_BG_COLOR_MIN,
    DKI_BG_COLOR_MID,
    DKI_BG_COLOR,
    DKI_MIN_VAULE,
    DKI_MID_VALUE,
    DKI_PERCENTS,
    DKI_COUNT
};

struct DISPLAY_KEY_ITEM
{
    const char *Name;
    DISPLAY_KEY_IDENTIFIER Identifier;
};

// Raw icon payload: the decoded pixels are kept in the struct so the
// renderer can take the bytes as-is.
struct CDisplayIcon
{
    vector<uint32_t> Pixels;
    int Width{ 0 };
    int Height{ 0 };
};

class CDisplayManager
{
private:
    vector<CDisplayInfo> m_InfoCommands;
    vector<CDisplayItem> m_Commands;
    std::map<uint32_t, CDisplayIcon *> m_Icons;
    astr_t m_TitleText;

    static const CDisplayItem m_DefaultItems[26];
    static const DISPLAY_KEY_ITEM m_KeysList[DKI_COUNT];

    CDisplayIcon *LoadDisplayIcon(uint16_t graphic, uint16_t color);
    void ApplyAttributeToDisplayItem(CDisplayItem &obj, const astr_t &name, const astr_t &value);
    void CompileData(const astr_t &text, uint32_t textColor);
    void GetValues(
        const CDisplayItem &item,
        astr_t &value,
        astr_t &maxValue,
        uint32_t textColor,
        uint32_t backgroundColor,
        bool useNotoriety);

public:
    CDisplayManager();
    ~CDisplayManager() = default;

    CDisplayManager(const CDisplayManager &) = delete;
    CDisplayManager &operator=(const CDisplayManager &) = delete;

    // The static-art icon cache: keyed by (graphic << 16) | hue, decoded through the
    // client's art reader. nullptr when the item has no graphic. The
    // Display items editor previews these as ImGui user textures.
    CDisplayIcon *GetIcon(const CDisplayItem &item);

    void ClearIcons();

    const vector<CDisplayInfo> &GetCommands() const { return m_InfoCommands; }

    CDisplayItem GetDisplayItemByName(const astr_t &name) const;

    // Title-bar rendering: composes the caption text into m_TitleText (the
    // textual mode; the two colored modes fall back to the same plain string -
    // an OS caption carries no colors/icons). Apply it with CWindow::SetTitle -
    // ui/assistantui.cpp does that on change (throttled).
    void Redraw(
        REDRAW_DISPLAY_MODE mode,
        bool useNotoriety,
        bool hightFont,
        bool highlightReagents,
        bool displayAppIcon,
        uint32_t backgroundColor);

    // The last composed caption (Redraw output)
    const astr_t &GetTitleText() const { return m_TitleText; }

    void RecompileData(const astr_t &text, uint32_t textColor);

    bool CanRedraw(const uint16_t &graphic, const uint16_t &color) const;
};

extern CDisplayManager g_DisplayManager;

} // namespace xuoassist
