﻿// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIGenericText.h"
#include "../Config.h"

void CGUIGenericText::Create(const wstring &str)
{
    DEBUG_TRACE_FUNCTION;
    uint16_t flags = UOFONT_BLACK_BORDER;

    if (MaxWidth != 0)
    {
        flags |= UOFONT_CROPPED;
    }

    CreateTextureW((uint8_t)(g_Config.ClientVersion >= CV_305D), str, 30, MaxWidth, TS_LEFT, flags);
}
