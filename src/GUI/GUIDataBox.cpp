// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIDataBox.h"

CGUIDataBox::CGUIDataBox()
    : CGUIPolygonal(GOT_DATABOX, 0, 0, 0, 0)
{
}

void CGUIDataBox::PrepareTextures()
{
    QFOR(item, m_Items, CBaseGUI *) { item->PrepareTextures(); }
}

bool CGUIDataBox::EntryPointerHere()
{
    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Visible && item->EntryPointerHere())
        {
            return true;
        }
    }
    return false;
}
