// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include "BaseScreen.h"
#include "../Platform.h"
#include "../Gumps/GumpScreenMain.h"

class CEntryText;
class CGUICheckbox;

class CMainScreen : public CBaseScreen
{
private:
    CGumpScreenMain m_MainGump;

    void Load();

public:
    enum
    {
        ID_SMOOTH_MS_QUIT = 1,
        ID_SMOOTH_MS_CONNECT = 2,
    };

    CEntryText *m_Account = nullptr;
    CEntryText *m_Password = nullptr;
    CGUICheckbox *m_SavePassword = nullptr;
    CGUICheckbox *m_AutoLogin = nullptr;

public:
    CMainScreen();
    virtual ~CMainScreen();

    void SetAccounting(const astr_t &account, const astr_t &password);
    void Paste();
    virtual void ProcessSmoothAction(uint8_t action = 0xFF) override;
    virtual void Init() override;

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;

    void Save();
    void Reset() const;
};

extern CMainScreen g_MainScreen;
