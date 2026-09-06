// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpPopupMenu : public CGump
{
    CGUIColoredPolygone *m_Polygone = nullptr;

public:
    CGumpPopupMenu(uint32_t serial, short x, short y);
    virtual ~CGumpPopupMenu();

    virtual void PrepareContent() override;

    static const int ID_GPM_MAXIMIZE = 0xDEADBEEF;
    static void Parse(Wisp::CPacketReader &reader);

    GUMP_BUTTON_EVENT_H override;
};

extern CGumpPopupMenu *g_PopupMenu;
