#pragma once

#include "Platform.h"

class CMainScreen : public CBaseScreen
{
private:
    CGumpScreenMain m_MainGump;

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

    void SetAccounting(const string &account, const string &password);
    void Paste();
    void ProcessSmoothAction(uint8_t action = 0xFF);
    void LoadGlobalConfig();
    void LoadCustomPath();
    void SaveGlobalConfig();
    void Init();

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

extern CMainScreen g_MainScreen;
