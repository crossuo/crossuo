// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/finditem.h): strings are astr_t,
// astr_t lists are std::vector<astr_t>, uint16_t lists are std::vector<uint16_t>.

#pragma once

#include "core/global.h"

namespace xuoassist {

enum FIND_TOKEN_STATE
{
    FTS_NORMAL = 0x00,
    FTS_NOT_EQUAL = 0x01,
    FTS_LOWER = 0x02,
    FTS_HIGHTER = 0x04
};

class CFindItem
{
public:
    astr_t Graphic = "";
    astr_t Color = "";
    astr_t Comment = "";

    // setters keep firing the OnChange* list reparsers on every mutation
    void SetGraphic(const astr_t &val)
    {
        OnChangeGraphic(val);
        Graphic = val;
    }
    void SetColor(const astr_t &val)
    {
        OnChangeColor(val);
        Color = val;
    }

protected:
    vector<uint16_t> m_ListGraphic;
    vector<uint16_t> m_ListGraphicFlags;
    vector<uint16_t> m_IgnoreGraphics;

    vector<uint16_t> m_ListColor;
    vector<uint16_t> m_ListColorFlags;
    vector<uint16_t> m_IgnoreColors;

    uint32_t ParseToken(astr_t str, uint32_t &value, const bool &isGraphic) const;

    bool TestValues(const uint32_t &original, const uint32_t &test, const uint32_t &flags) const;

public:
    CFindItem();
    CFindItem(const astr_t &graphic, const astr_t &color);
    virtual ~CFindItem();

    void OnChangeGraphic(const astr_t &val);
    void OnChangeColor(const astr_t &val);

    bool Found(const uint16_t &graphic, const uint16_t &color) const;
};

} // namespace xuoassist
