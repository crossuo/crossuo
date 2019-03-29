// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CPopupMenuItemInfo
{
public:
    uint32_t Cliloc = 0;
    uint16_t Index = 0;
    uint16_t Flags = 0;
    uint16_t Color = 0;
    uint16_t ReplaceColor = 0;

    CPopupMenuItemInfo() {}
};

class CGumpPopupMenu : public CGump
{
    CGUIColoredPolygone *m_Polygone{ nullptr };

public:
    CGumpPopupMenu(uint32_t serial, short x, short y);
    virtual ~CGumpPopupMenu();

    static const int ID_GPM_MAXIMIZE = 0xDEADBEEF;

    virtual void PrepareContent() override;

    static void Parse(Wisp::CPacketReader &reader);

    GUMP_BUTTON_EVENT_H override;
};

extern CGumpPopupMenu *g_PopupMenu;
