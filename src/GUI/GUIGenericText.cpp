// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIGenericText.h"
#include "../Config.h"
#include "../Globals.h" // UOFONT_*

void CGUIGenericText::Create(const wstr_t &str)
{
    uint16_t flags = UOFONT_BLACK_BORDER;

    if (MaxWidth != 0)
    {
        flags |= UOFONT_CROPPED;
    }

    CreateTextureW((uint8_t)(g_Config.ClientVersion >= CV_305D), str, 30, MaxWidth, TS_LEFT, flags);
}
