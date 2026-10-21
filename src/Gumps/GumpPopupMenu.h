// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "../Managers/PacketManager.h" // CPacketManager

class CGumpPopupMenu : public CGump
{
    CGUIColoredPolygone *m_Polygone = nullptr;

public:
    CGumpPopupMenu(uint32_t serial, short x, short y);
    virtual ~CGumpPopupMenu() override;

    virtual void PrepareContent() override;

    static const int ID_GPM_MAXIMIZE = 0xDEADBEEF;
    static void Parse(CPacketManager &reader);

    GUMP_BUTTON_EVENT_H override;
};

extern CGumpPopupMenu *g_PopupMenu;
