// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gumpitem.h): strings are std::string,
// string lists are std::vector<std::string>, lists are std::vector.

#pragma once

#include "core/global.h"

namespace xuoassist {

enum GUMP_ITEM_TYPE
{
    GIT_INVALID_TYPE = 0,
    GIT_NODISPOSE,
    GIT_NOMOVE,
    GIT_NOCLOSE,
    GIT_PAGE,
    GIT_GROUP,
    GIT_RESIZEPIC,
    GIT_CHECKERTRANS,
    GIT_BUTTON,
    GIT_BUTTONTILEART,
    GIT_CHECKBOX,
    GIT_RADIO,
    GIT_TEXT,
    GIT_CROPPEDTEXT,
    GIT_TEXTENTRY,
    GIT_TEXTENTRYLIMITED,
    GIT_TILEPIC,
    GIT_TILEPICHUE,
    GIT_GUMPPIC,
    GIT_GUMPPICTILED,
    GIT_HTMLGUMP,
    GIT_XMFHTMLGUMP,
    GIT_XMLHTMLGUMPCOLOR,
    GIT_XMLHTMLTOK,
    GIT_TOOLTIP,
    GIT_MASTERGUMP
};

class CGumpItem
{
public:
    GUMP_ITEM_TYPE Type = GIT_INVALID_TYPE;
    string Command = "";

    CGumpItem();
    CGumpItem(const GUMP_ITEM_TYPE &type, const string &command);
    virtual ~CGumpItem();

    void RestoreCommand(const vector<string> &list);
};

typedef vector<CGumpItem> GUMP_LIST;

} // namespace xuoassist

