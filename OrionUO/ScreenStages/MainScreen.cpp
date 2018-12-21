#include "MainScreen.h"
#include "BaseScreen.h"
#include "Platform.h"
#include "../GUI/GUITextEntry.h"
#include "../Wisp/WispDefinitions.h"
#include "../TextEngine/EntryText.h"

#include "FileSystem.h"

#define ORIONUO_CONFIG "OrionUO.cfg"

CMainScreen g_MainScreen;

CMainScreen::CMainScreen()
    : CBaseScreen(m_MainGump)
    , m_Account(nullptr)
    , m_Password(nullptr)
    , m_SavePassword(nullptr)
    , m_AutoLogin(nullptr)
{
    DEBUG_TRACE_FUNCTION;
    m_Password = new CEntryText(32, 0, 300);
}

CMainScreen::~CMainScreen()
{
    DEBUG_TRACE_FUNCTION;
    delete m_Password;
}

void CMainScreen::Init()
{
    DEBUG_TRACE_FUNCTION;
    g_ConfigLoaded = false;
    g_GlobalScale = 1.0;

    g_OrionWindow.SetSize(Wisp::CSize(640, 480));
    g_OrionWindow.NoResize = true;
    g_OrionWindow.SetTitle("Ultima Online");
    g_GL.UpdateRect();

    if (!m_SavePassword->Checked)
    {
        m_Password->SetTextW({});
        m_MainGump.m_PasswordFake->SetTextW({});
    }

    g_EntryPointer = m_MainGump.m_PasswordFake;

    g_AnimationManager.ClearUnusedTextures(g_Ticks + 100000);

    g_QuestArrow.Enabled = false;

    g_TotalSendSize = 0;
    g_TotalRecvSize = 0;

    g_LightLevel = 0;
    g_PersonalLightLevel = 0;

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
}

void CMainScreen::ProcessSmoothAction(uint8_t action)
{
    DEBUG_TRACE_FUNCTION;
    if (action == 0xFF)
    {
        action = SmoothScreenAction;
    }

    if (action == ID_SMOOTH_MS_CONNECT)
    {
        g_Orion.Connect();
    }
    else if (action == ID_SMOOTH_MS_QUIT)
    {
        g_OrionWindow.Destroy();
    }
}

void CMainScreen::SetAccounting(const string &account, const string &password)
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;

    const auto ch = EvChar(ev);
    if (ch >= 0x0100 || !g_FontManager.IsPrintASCII((uint8_t)ch))
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
    DEBUG_TRACE_FUNCTION;

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

int CMainScreen::GetConfigKeyCode(const string &key)
{
    //DEBUG_TRACE_FUNCTION;
    const int keyCount = MSCC_COUNT - 1;

    static const string m_Keys[keyCount] = { "acctid",    "acctpassword",  "rememberacctpw",
                                             "autologin", "smoothmonitor", "theabyss",
                                             "asmut",     "custompath" };

    string str = ToLowerA(key);
    int result = 0;

    for (int i = 0; i < keyCount && (result == 0); i++)
    {
        if (str == m_Keys[i])
        {
            result = (int)i + 1;
        }
    }

    return result;
}

void CMainScreen::LoadCustomPath()
{
    DEBUG_TRACE_FUNCTION;

    LOG("Loading custom path from " ORIONUO_CONFIG "\n");
    Wisp::CTextFileParser file(g_App.ExeFilePath(ORIONUO_CONFIG), "=", "#;", "");
    while (!file.IsEOF())
    {
        auto strings = file.ReadTokens(false);
        if (strings.size() >= 2)
        {
            const int code = GetConfigKeyCode(strings[0]);
            switch (code)
            {
                case MSCC_CUSTOM_PATH:
                {
                    g_App.m_UOPath = ToPath(strings[1]);
                    fs_case_insensitive_init(g_App.m_UOPath);
                }
            }
        }
    }
}

void CMainScreen::LoadGlobalConfig()
{
    DEBUG_TRACE_FUNCTION;
    m_AutoLogin->Checked = false;
    g_ScreenEffectManager.Enabled = false;

    LOG("Loading global config from " ORIONUO_CONFIG "\n");
    Wisp::CTextFileParser file(g_App.ExeFilePath(ORIONUO_CONFIG), "=", "#;", "");

    while (!file.IsEOF())
    {
        auto strings = file.ReadTokens();
        if (strings.size() >= 2)
        {
            const int code = GetConfigKeyCode(strings[0]);
            switch (code)
            {
                case MSCC_ACTID:
                {
                    m_Account->SetTextA(strings[1]);
                    m_Account->SetPos((int)strings[1].length());
                    break;
                }
                case MSCC_ACTPWD:
                {
                    string password = file.RawLine;
                    size_t pos = password.find_first_of('=');
                    password = password.substr(pos + 1, password.length() - (pos + 1));
                    const auto len = (int)password.length();
                    if (len != 0)
                    {
                        m_Password->SetTextA(password);
                        for (int zv = 0; zv < len; zv++)
                        {
                            m_MainGump.m_PasswordFake->Insert(L'*');
                        }
                        m_Password->SetPos((int)len);
                    }
                    else
                    {
                        m_MainGump.m_PasswordFake->SetTextA("");
                        m_MainGump.m_PasswordFake->SetPos(0);
                        m_Password->SetTextA("");
                        m_Password->SetPos(0);
                    }
                    break;
                }
                case MSCC_REMEMBERPWD:
                {
                    m_SavePassword->Checked = ToBool(strings[1]);
                    if (!m_SavePassword->Checked)
                    {
                        m_MainGump.m_PasswordFake->SetTextA("");
                        m_MainGump.m_PasswordFake->SetPos(0);
                        m_Password->SetTextA("");
                        m_Password->SetPos(0);
                    }
                    break;
                }
                case MSCC_AUTOLOGIN:
                {
                    m_AutoLogin->Checked = ToBool(strings[1]);
                    break;
                }
                case MSCC_SMOOTHMONITOR:
                {
                    g_ScreenEffectManager.Enabled = ToBool(strings[1]);
                    break;
                }
                case MSCC_THE_ABYSS:
                {
                    g_TheAbyss = ToBool(strings[1]);
                    break;
                }
                case MSCC_ASMUT:
                {
                    g_Asmut = ToBool(strings[1]);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void CMainScreen::SaveGlobalConfig()
{
    DEBUG_TRACE_FUNCTION;
    LOG("Saving global config to " ORIONUO_CONFIG "\n");
    FILE *uo_cfg = fs_open(g_App.ExeFilePath(ORIONUO_CONFIG), FS_WRITE);
    if (uo_cfg == nullptr)
    {
        return;
    }

    char buf[128] = { 0 };

    sprintf_s(buf, "AcctID=%s\n", m_Account->c_str());
    fputs(buf, uo_cfg);

    if (m_SavePassword->Checked)
    {
        sprintf_s(buf, "AcctPassword=%s\n", m_Password->c_str());
        fputs(buf, uo_cfg);
        sprintf_s(buf, "RememberAcctPW=yes\n");
        fputs(buf, uo_cfg);
    }
    else
    {
        fputs("AcctPassword=\n", uo_cfg);
        sprintf_s(buf, "RememberAcctPW=no\n");
        fputs(buf, uo_cfg);
    }

    sprintf_s(buf, "AutoLogin=%s\n", (m_AutoLogin->Checked ? "yes" : "no"));
    fputs(buf, uo_cfg);

    sprintf_s(buf, "SmoothMonitor=%s\n", (g_ScreenEffectManager.Enabled ? "yes" : "no"));
    fputs(buf, uo_cfg);

    sprintf_s(buf, "TheAbyss=%s\n", (g_TheAbyss ? "yes" : "no"));
    fputs(buf, uo_cfg);

    sprintf_s(buf, "Asmut=%s\n", (g_Asmut ? "yes" : "no"));
    fputs(buf, uo_cfg);

    if (g_App.m_UOPath != g_App.m_ExePath)
    {
        sprintf_s(buf, "CustomPath=%s\n", CStringFromPath(g_App.m_UOPath));
        fputs(buf, uo_cfg);
    }
    fs_close(uo_cfg);
}
