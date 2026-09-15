// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gumpitem.cpp), verbatim.

#include "gumpitem.h"

namespace xuoassist {

CGumpItem::CGumpItem()
{
}

CGumpItem::CGumpItem(const GUMP_ITEM_TYPE &type, const string &command)
    : Type(type)
    , Command(command)
{
}

CGumpItem::~CGumpItem()
{
}

void CGumpItem::RestoreCommand(const vector<string> &list)
{
    Command = "";

    if (!list.empty())
    {
        for (const string &str : list)
        {
            if (Command.length() != 0)
            {
                Command += " ";
            }
            Command += str;
        }
    }
}

} // namespace xuoassist
