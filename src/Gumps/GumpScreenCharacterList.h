// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"

struct CGumpScreenCharacterList : public CGump
{
    CGumpScreenCharacterList();
    virtual ~CGumpScreenCharacterList() = default;

    virtual void UpdateContent() override;
    virtual void InitToolTip() override;
    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
};
