// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include <map>

struct LoginTheme;

class CGumpScreenMain : public CGump
{
    CGUIButton *m_Arrow{ nullptr };

    // data-driven login screen state (see BuildThemed)
    bool m_Themed = false;
    bool m_ThemedArrowBlinks = false;
    int m_ThemedArrowArt = 0;
    std::map<int, astr_t> m_Tooltips;

    void BuildThemed(const LoginTheme &theme);
    const astr_t *ThemedTooltip(int serial) const;

public:
    CGumpScreenMain();
    virtual ~CGumpScreenMain() override;

    CEntryText *m_PasswordFake{ nullptr };

protected:
    //1.25.32 login: the entry box art is swapped per state (filler text /
    //empty / focused) - see PrepareContent
    CGUIGumppic *m_1998AccountArt{ nullptr };
    CGUIGumppic *m_1998PasswordArt{ nullptr };
    CGUITextEntry *m_1998AccountEntry{ nullptr };

    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;
};
