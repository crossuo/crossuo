// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/finditem.h): strings are std::string,
// string lists are std::vector<std::string>, ushort lists are std::vector<ushort>.

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
    string Graphic = "";
    string Color = "";
    string Comment = "";

    // setters keep firing the OnChange* list reparsers on every mutation
    void SetGraphic(const string &val)
    {
        OnChangeGraphic(val);
        Graphic = val;
    }
    void SetColor(const string &val)
    {
        OnChangeColor(val);
        Color = val;
    }

protected:
    vector<ushort> m_ListGraphic;
    vector<ushort> m_ListGraphicFlags;
    vector<ushort> m_IgnoreGraphics;

    vector<ushort> m_ListColor;
    vector<ushort> m_ListColorFlags;
    vector<ushort> m_IgnoreColors;

    uint ParseToken(string str, uint &value, const bool &isGraphic) const;

    bool TestValues(const uint &original, const uint &test, const uint &flags) const;

public:
    CFindItem();
    CFindItem(const string &graphic, const string &color);
    virtual ~CFindItem();

    void OnChangeGraphic(const string &val);
    void OnChangeColor(const string &val);

    bool Found(const ushort &graphic, const ushort &color) const;
};

} // namespace xuoassist

