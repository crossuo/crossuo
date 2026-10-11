// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "MainScreen.h"
#include "BaseScreen.h"
#include <common/utils.h>
#include <cstdlib>
#include "revision.h"
#include "../Config.h"
#include "../Point.h"
#include "../CrossUO.h"
#include "../QuestArrow.h"
#include "../GameWindow.h"
#include "../Managers/FontsManager.h"
#include "../Managers/ScreenEffectManager.h"
#include "../Managers/AnimationManager.h"
#include "../Managers/GumpManager.h"
#include "../GUI/GUITextEntry.h"
#include "../TextEngine/EntryText.h"

CMainScreen g_MainScreen;

bool DebugRenderScreen()
{
    static const bool debug = getenv("XUO_DEBUG_SCREEN") != nullptr;
    return debug;
}

#if defined(USE_GL3) || defined(USE_GLES)
static void DebugRenderScreenInternal()
{
    // big red untextured quad
    RenderAdd_DrawUntexturedQuad(
        g_renderCmdList, DrawUntexturedQuadCmd{ 40, 200, 150, 150, { 1.f, 0.f, 0.f, 1.f } });

    // big green circle (uses the current color state)
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 0.f, 1.f, 0.f, 1.f } });
    RenderAdd_DrawCircle(g_renderCmdList, DrawCircleCmd{ 320, 275, 80.f, 0 });
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });

    // big blue line
    RenderAdd_DrawLine(
        g_renderCmdList, DrawLineCmd{ 450, 350, 600, 200, { 0.f, 0.f, 1.f, 1.f } });
}
#endif

CMainScreen::CMainScreen()
    : CBaseScreen(m_MainGump)
    , m_Account(nullptr)
    , m_Password(nullptr)
    , m_SavePassword(nullptr)
    , m_AutoLogin(nullptr)
{
    m_Password = new CEntryText(32, 0, 300);
}

CMainScreen::~CMainScreen()
{
    delete m_Password;
}

void CMainScreen::Init()
{
    CBaseScreen::Init();
    // The 1998 (1.25.32) login screens keep both layouts in one gump;
    // re-entering the main screen always goes back to the title screen.
    if (g_Config.ClientVersion < CV_200)
    {
        m_MainGump.Page = 1;
    }
    g_ConfigLoaded = false;
    g_GlobalScale = 1.0;
    Load();
    Reset();
    g_GameWindow.SetTitle(CLIENT_TITLE);
    g_GumpManager.RedrawAll();

    g_EntryPointer = m_MainGump.m_PasswordFake;
    g_AnimationManager.ClearUnusedAnimations();
    g_QuestArrow.Enabled = false;
    g_TotalSendSize = 0;
    g_TotalRecvSize = 0;
    g_LightLevel = 0;
    g_PersonalLightLevel = 0;
    if (!DebugRenderScreen())
    {
        g_ScreenEffectManager.UseSunrise();
    }
    SmoothScreenAction = 0;
    m_Gump.PrepareTextures();

    if (TestAutoLogin())
    {
        CreateSmoothAction(ID_SMOOTH_MS_CONNECT);
    }
}

void CMainScreen::Reset() const
{
    g_GameWindow.RestoreWindow();
    g_GameWindow.SetSize(CSize(640, 480));
    g_GameWindow.SetWindowResizable(false);
}

void CMainScreen::ProcessSmoothAction(uint8_t action)
{
    if (action == 0xFF)
    {
        action = SmoothScreenAction;
    }

    if (action == ID_SMOOTH_MS_CONNECT)
    {
        g_Game.Connect();
    }
    else if (action == ID_SMOOTH_MS_QUIT)
    {
#if defined(XUO_EMSC)
        // quitting makes no sense in a browser; ignore
#else
        g_GameWindow.Destroy();
#endif
    }
}

void CMainScreen::SetAccounting(const astr_t &account, const astr_t &password)
{
    if (DebugRenderScreen())
    {
        return;
    }

    m_Account->SetTextA(account);
    m_Password->SetTextA(password);

    const auto len = (int)password.length();
    m_MainGump.m_PasswordFake->Clear();

    for (int i = 0; i < len; i++)
    {
        m_MainGump.m_PasswordFake->Insert(L'*');
    }
}

void CMainScreen::Paste()
{
    if (DebugRenderScreen())
    {
        return;
    }

    if (g_EntryPointer == m_MainGump.m_PasswordFake)
    {
        m_Password->Paste();

        const auto len = (int)m_Password->Length();
        g_EntryPointer->Clear();

        for (int i = 0; i < len; i++)
        {
            g_EntryPointer->Insert(L'*');
        }
    }
    else
    {
        g_EntryPointer->Paste();
    }
}

void CMainScreen::OnTextInput(const TextEvent &ev)
{
    if (DebugRenderScreen())
    {
        return;
    }

    const auto ch = EvChar(ev);
    if (!IsPrintable(ch) || !g_FontManager.IsPrintASCII((uint8_t)ch))
    {
        return;
    }
    if (g_EntryPointer == nullptr)
    {
        g_EntryPointer = m_MainGump.m_PasswordFake;
    }

    if (g_EntryPointer->Length() < 16) //add char to text field
    {
        if (g_EntryPointer == m_MainGump.m_PasswordFake)
        {
            if (g_EntryPointer->Insert(L'*'))
            {
                m_Password->Insert(ch);
            }
        }
        else
        {
            g_EntryPointer->Insert(ch);
        }
    }

    m_Gump.WantRedraw = true;
}

void CMainScreen::OnKeyDown(const KeyEvent &ev)
{
    if (DebugRenderScreen())
    {
        return;
    }

    if (g_EntryPointer == nullptr)
    {
        g_EntryPointer = m_MainGump.m_PasswordFake;
    }

    const auto key = EvKey(ev);
    switch (key)
    {
        case KEY_TAB:
        {
            if (g_EntryPointer == m_Account)
            {
                g_EntryPointer = m_MainGump.m_PasswordFake;
            }
            else
            {
                g_EntryPointer = m_Account;
            }
            break;
        }
        case KEY_RETURN:
        case KEY_RETURN2:
        {
            CreateSmoothAction(ID_SMOOTH_MS_CONNECT);
            break;
        }
        default:
        {
            if (g_EntryPointer == m_MainGump.m_PasswordFake)
            {
                m_Password->OnKey(nullptr, key);
            }

            g_EntryPointer->OnKey(nullptr, key);
            break;
        }
    }
    m_Gump.WantRedraw = true;
}

void CMainScreen::DebugScreen()
{
    if (!DebugRenderScreen())
    {
        return;
    }

#if defined(USE_GL3) || defined(USE_GLES)
    DebugRenderScreenInternal();
#endif
}

void CMainScreen::Load()
{
    if (DebugRenderScreen())
    {
        return;
    }

    m_AutoLogin->Checked = g_Config.AutoLogin;

    m_Account->SetTextA(g_Config.Login);
    m_Account->SetPos(checked_cast<int>(g_Config.Login.length()));

    m_MainGump.m_PasswordFake->SetTextA("");
    m_MainGump.m_PasswordFake->SetPos(0);

    const size_t len = g_Config.Password.length();
    if (len != 0)
    {
        m_Password->SetTextA(g_Config.Password);
        for (size_t zv = 0; zv < len; zv++)
        {
            m_MainGump.m_PasswordFake->Insert(L'*');
        }
        m_Password->SetPos(checked_cast<int>(len));
    }
    else
    {
        m_Password->SetTextA("");
        m_Password->SetPos(0);
    }

    m_SavePassword->Checked = g_Config.SavePassword;
    if (!m_SavePassword->Checked)
    {
        m_Password->SetTextW({});
        m_MainGump.m_PasswordFake->SetTextW({});
    }
}

void CMainScreen::Save()
{
    if (DebugRenderScreen())
    {
        return;
    }

    g_Config.AutoLogin = m_AutoLogin->Checked;
    g_Config.SavePassword = m_SavePassword->Checked;
    g_Config.Password = m_Password->GetTextA();
    g_Config.Login = m_Account->GetTextA();
}
