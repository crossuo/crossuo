// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpMenubar : public CGump
{
    bool m_Opened = true;
#if defined(XUO_EMSC)
    // canvas width the current layout was built for
    int m_BuiltCanvasWidth = -1;
#endif

    void RebuildLayout();

public:
    CGumpMenubar(short x, short y);
    virtual ~CGumpMenubar() override;

    bool GetOpened() const { return m_Opened; };
    void SetOpened(bool val);

    virtual void UpdateContent() override;

    virtual bool CanBeDisplayed() override { return false; }
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;

    virtual void OnLeftMouseButtonUp() override;
};
