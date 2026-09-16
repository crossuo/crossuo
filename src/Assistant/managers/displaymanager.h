// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/displaymanager.h), data-only port
// (master plan §4.1: the win32 HBITMAP icon cache and the GDI title-bar
// rendering are Phase 2/4 - this round keeps the bytes/parsing/state and the
// title-data compiler).

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

// Raw icon payload (was a win32 HBITMAP / Pixmap wrapper in the original app).
// Rendering is Phase 2/4; the pixels are kept in the struct so the drawing
// round can take the bytes as-is.
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
    std::map<uint, CDisplayIcon *> m_Icons;
    astr_t m_TitleText;

    static const CDisplayItem m_DefaultItems[26];
    static const DISPLAY_KEY_ITEM m_KeysList[DKI_COUNT];

    CDisplayIcon *LoadDisplayIcon(ushort graphic, uint16_t color);
    void ApplyAttributeToDisplayItem(CDisplayItem &obj, const astr_t &name, const astr_t &value);
    void CompilleData(const astr_t &text, uint textColor);
    void GetValues(
        const CDisplayItem &item,
        astr_t &value,
        astr_t &maxValue,
        uint textColor,
        uint backgroundColor,
        bool useNotoriety);

public:
    CDisplayManager();
    ~CDisplayManager();

    // The static-art icon cache (was the win32 HBITMAP cache of the original
    // displaymanager): keyed by (graphic << 16) | hue, decoded through the
    // client's art reader. nullptr when the item has no graphic. The
    // Display items editor previews these as ImGui user textures.
    CDisplayIcon *GetIcon(const CDisplayItem &item);

    void ClearIcons();

    const vector<CDisplayInfo> &GetCommands() const { return m_InfoCommands; }

    CDisplayItem GetDisplayItemByName(const astr_t &name);

    // Title-bar rendering (was the win32 GDI body): composes the caption text
    // into m_TitleText. The original textual path (Redraw RDM_TEXTUAL) is followed
    // exactly; the two colored modes fall back to the same plain string (an
    // OS caption carries no colors/icons). Apply it with CWindow::SetTitle -
    // ui/assistantui.cpp does that on change (throttled).
    void Redraw(
        REDRAW_DISPLAY_MODE mode,
        bool useNotoriety,
        bool hightFont,
        bool highlightReagents,
        bool displayAppIcon,
        uint backgroundColor);

    // The last composed caption (Redraw output; the original's local title string)
    const astr_t &GetTitleText() const { return m_TitleText; }

    void RecompilleData(const astr_t &text, uint textColor);

    bool CanRedraw(const ushort &graphic, const ushort &color);
};

extern CDisplayManager g_DisplayManager;

} // namespace xuoassist
