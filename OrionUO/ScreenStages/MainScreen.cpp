#include "MainScreen.h"
#include "BaseScreen.h"
#include "Platform.h"
#include "../GUI/GUITextEntry.h"
#include "../Wisp/WispDefinitions.h"
#include "../TextEngine/EntryText.h"

#include "FileSystem.h"

#define ORIONUO_CONFIG "OrionUO.cfg"

CMainScreen g_MainScreen;

enum
{
    MSCC_NONE,
    MSCC_ACTID,
    MSCC_ACTPWD,
    MSCC_REMEMBERPWD,
    MSCC_AUTOLOGIN,
    MSCC_SMOOTHMONITOR,
    MSCC_THE_ABYSS,
    MSCC_ASMUT,
    MSCC_CUSTOM_PATH,
    MSCC_LOGIN_SERVER,
    MSCC_COUNT,
};

namespace mscc
{
struct ConfigEntry
{
    uint32_t key;
    const char *key_name;
};

static const ConfigEntry s_Keys[] = {
    { MSCC_ACTID, "acctid" },
    { MSCC_ACTPWD, "acctpassword" },
    { MSCC_REMEMBERPWD, "rememberacctpw" },
    { MSCC_AUTOLOGIN, "autologin" },
    { MSCC_SMOOTHMONITOR, "smoothmonitor" },
    { MSCC_THE_ABYSS, "theabyss" },
    { MSCC_ASMUT, "asmut" },
    { MSCC_CUSTOM_PATH, "custompath" },
    { MSCC_LOGIN_SERVER, "loginserver" },
    { MSCC_COUNT, nullptr },
};

static uint32_t GetConfigKey(const string &key)
{
    auto str = ToLowerA(key);
    for (int i = 0; s_Keys[i].key_name; i++)
    {
        if (str == s_Keys[i].key_name)
        {
            return s_Keys[i].key;
        }
    }
    return MSCC_NONE;
}

} // namespace mscc

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

// FIXME: Merge with LoadGlobalConfig and do it early in the executable load
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
            const auto key = mscc::GetConfigKey(strings[0]);
            switch (key)
            {
                case MSCC_CUSTOM_PATH:
                {
                    g_App.m_UOPath = ToPath(strings[1]);
                    fs_case_insensitive_init(g_App.m_UOPath);
                }
                break;

                default:
                    break;
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
    Wisp::CTextFileParser file(g_App.ExeFilePath(ORIONUO_CONFIG), "=,", "#;", "");

    while (!file.IsEOF())
    {
        auto strings = file.ReadTokens();
        if (strings.size() >= 2)
        {
            const auto key = mscc::GetConfigKey(strings[0]);
            LOG("Key: %d for %s\n", key, strings[0].c_str());
            switch (key)
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
                case MSCC_LOGIN_SERVER:
                {
                    g_App.m_ServerAddress = strings[1];
                    g_App.m_ServerPort = ToInt(strings[2]);
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
    FILE *cfg = fs_open(g_App.ExeFilePath(ORIONUO_CONFIG), FS_WRITE);
    if (cfg == nullptr)
    {
        return;
    }

    fprintf(cfg, "AcctID=%s\n", m_Account->c_str());
    if (m_SavePassword->Checked)
    {
        fprintf(cfg, "AcctPassword=%s\n", m_Password->c_str());
        fprintf(cfg, "RememberAcctPW=yes\n");
    }
    else
    {
        fprintf(cfg, "AcctPassword=\n");
        fprintf(cfg, "RememberAcctPW=no\n");
    }

    fprintf(cfg, "AutoLogin=%s\n", (m_AutoLogin->Checked ? "yes" : "no"));
    fprintf(cfg, "SmoothMonitor=%s\n", (g_ScreenEffectManager.Enabled ? "yes" : "no"));
    fprintf(cfg, "TheAbyss=%s\n", (g_TheAbyss ? "yes" : "no"));
    fprintf(cfg, "Asmut=%s\n", (g_Asmut ? "yes" : "no"));

    if (g_App.m_UOPath != g_App.m_ExePath)
    {
        fprintf(cfg, "CustomPath=%s\n", CStringFromPath(g_App.m_UOPath));
    }

    if (!g_App.m_ServerAddress.empty())
    {
        fprintf(cfg, "LoginServer=%s,%d\n", g_App.m_ServerAddress.c_str(), g_App.m_ServerPort);
    }

    fflush(cfg);
    fs_close(cfg);
}
