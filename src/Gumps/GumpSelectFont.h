// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpSelectFont : public CGump
{
private:
    SELECT_FONT_GUMP_STATE m_State = SFGS_OPT_POPUP;

public:
    CGumpSelectFont(uint32_t serial, short x, short y, SELECT_FONT_GUMP_STATE state);
    virtual ~CGumpSelectFont();
    virtual void UpdateContent() override;
    GUMP_RADIO_EVENT_H override;
};
