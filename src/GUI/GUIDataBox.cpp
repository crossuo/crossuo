// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIDataBox.h"
#include "GUI/GUIPolygonal.h"

CGUIDataBox::CGUIDataBox()
    : CGUIPolygonal(GOT_DATABOX, 0, 0, 0, 0)
{
}

void CGUIDataBox::PrepareTextures()
{
    for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next) { item->PrepareTextures(); }
}

bool CGUIDataBox::EntryPointerHere()
{
    for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Visible && item->EntryPointerHere())
        {
            return true;
        }
    }
    return false;
}
