// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUIButton.h"

class CGUIHTMLButton : public CGUIButton
{
private:
    //! Reference to the parent gump
    class CGUIHTMLGump *m_HTMLGump{ nullptr };

public:
    CGUIHTMLButton(
        class CGUIHTMLGump *htmlGump,
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        int x,
        int y);
    virtual ~CGUIHTMLButton() override;

    virtual void SetShaderMode() override;

    //! Scroll handling
    void Scroll(bool up, int delay);

    virtual bool IsControlHTML() override { return true; }
};
