// MIT License
// Copyright (C) August 2016 Hotride

#include "BaseGUI.h"

CBaseGUI::CBaseGUI(
    GUMP_OBJECT_TYPE type, int serial, uint16_t graphic, uint16_t color, int x, int y)
    : CRenderObject(serial, graphic, color, x, y)
    , Type(type)
{
}

CBaseGUI::~CBaseGUI()
{
}
