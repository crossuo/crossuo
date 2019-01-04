// GPLv3 License
// Copyright (C) 2019 Danny Angelo Carminati Grein

#include "Config.h"

#include <string>

#include "Globals.h"
#include "FileSystem.h"
#include "OrionApplication.h"
#include "plugin/enumlist.h"
#include "Crypt/CryptEntry.h"
#include "Wisp/WispDefinitions.h"
#include "Wisp/WispLogger.h"
#include "Wisp/WispTextFileParser.h"

#define ORIONUO_CONFIG "OrionUO.cfg"

Config g_Config;

enum
{
    MSCC_NONE,
    MSCC_ACTID,
    MSCC_ACTPWD,
    MSCC_REMEMBERPWD,
    MSCC_AUTOLOGIN,
    MSCC_THE_ABYSS,
    MSCC_ASMUT,
    MSCC_CUSTOM_PATH,
    MSCC_LOGIN_SERVER,
    MSCC_CLIENT_VERSION,
    MSCC_USE_CRYPT,
    MSCC_COUNT,
};

namespace config
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
    { MSCC_THE_ABYSS, "theabyss" },
    { MSCC_ASMUT, "asmut" },
    { MSCC_CUSTOM_PATH, "custompath" },
    { MSCC_LOGIN_SERVER, "loginserver" },
    { MSCC_CLIENT_VERSION, "clientversion" },
    { MSCC_USE_CRYPT, "crypt" },
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

} // namespace config

static void SetClientVersion(const char *versionStr)
{
    DEBUG_TRACE_FUNCTION;

    if (!versionStr || !versionStr[0])
    {
        g_Config.ClientVersion = CV_LATEST;
        return;
    }

    int a = 0, b = 0, c = 0, d = 0;
    char extra[16]{};
    char tok = 0;
    sscanf(versionStr, "%d.%d.%d%s", &a, &b, &c, extra);

    if (strlen(extra))
    {
        tok = extra[0];
        if (tok == '.')
        {
            sscanf(extra, ".%d", &d);
        }
        else if (tok >= 'a' && tok <= 'z')
        {
            d = tok;
        }
    }

    LOG("\tClient Version: %s\n", versionStr);
    g_Config.ClientVersion = VERSION(a, b, c, d);

    /*
    // This was to keep compatibility, but using
    // g_Config.ClientVersion is wrong!
    static CLIENT_VERSION protocolVersion[] = {
        CV_OLD,  CV_200,  CV_200X,  CV_300,   CV_305D,  CV_306E,  CV_308,   CV_308D,
        CV_308J, CV_308Z, CV_400B,  CV_405A,  CV_4011D, CV_500A,  CV_5020,  CV_5090,
        CV_6000, CV_6013, CV_60144, CV_6017,  CV_6040,  CV_6060,  CV_60142, CV_60144,
        CV_7000, CV_7090, CV_70130, CV_70160, CV_70180, CV_70240, CV_70331
    };

    for (int i = countof(protocolVersion) - 1; i > 0; --i)
    {
        if (g_Config.ClientVersion >= (int32_t)protocolVersion[i])
            return protocolVersion[i];
    }
    return CV_OLD;
*/
}

static const char *GetClientTypeName(CLIENT_FLAG clientFlag)
{
    switch (clientFlag)
    {
        case CF_T2A:
            return "The Second Age (T2A)";
        case CF_RE:
            return "Renaissance";
        case CF_TD:
            return "Third Dawn";
        case CF_LBR:
            return "Lord Blackthorn's Revenge";
        case CF_AOS:
            return "Age Of Shadows";
        case CF_SE:
            return "Samurai Empire";
        case CF_SA:
            return "Stygian Abyss";
        case CF_UO3D:
            return "UO3D";
        case CF_RESERVED:
            return "Reserved";
        case CF_3D:
            return "3D Client";
    }
    return "Unknown";
}

static CLIENT_FLAG GetClientType(uint32_t version)
{
    if (version < CV_200)
    {
        return CF_T2A;
    }
    if (version < CV_300)
    {
        return CF_RE;
    }
    if (version < CV_308)
    {
        return CF_TD;
    }
    if (version < CV_308Z)
    {
        return CF_LBR;
    }
    if (version < CV_405A)
    {
        // >=3.0.8z
        return CF_AOS;
    }
    if (version < CV_60144)
    {
        return CF_SE;
    }
    return CF_SA;
}

// Reference: https://github.com/polserver/polserver/blob/5c747bb88123945bb892d3d793b89afcb1dc645a/pol-core/pol/crypt/cryptkey.cpp
static void SetClientCrypt(uint32_t version)
{
    if (version == CV_200X)
    {
        g_Config.Key1 = 0x2D13A5FC;
        g_Config.Key2 = 0x2D13A5FD;
        g_Config.Key3 = 0xA39D527F;
        g_Config.EncryptionType = ET_203;
        return;
    }

    int a = (version >> 24) & 0xff;
    int b = (version >> 16) & 0xff;
    int c = (version >> 8) & 0xff;

    int temp = ((a << 9 | b) << 10 | c) ^ ((c * c) << 5);
    g_Config.Key2 = (temp << 4) ^ (b * b) ^ (b * 0x0B000000) ^ (c * 0x380000) ^ 0x2C13A5FD;
    temp = (((a << 9 | c) << 10 | b) * 8) ^ (c * c * 0x0c00);
    g_Config.Key3 = temp ^ (b * b) ^ (b * 0x6800000) ^ (c * 0x1c0000) ^ 0x0A31D527F;

    // Configurator does this, not sure why
    // lets keep compatibility until we understand more
    g_Config.Key1 = g_Config.Key2 - 1;

    if (version < VERSION(1, 25, 35, 0))
    {
        g_Config.EncryptionType = ET_OLD_BFISH;
    }
    else if (version == VERSION(1, 25, 36, 0))
    {
        g_Config.EncryptionType = ET_1_25_36;
    }
    else if (version < CV_200)
    {
        g_Config.EncryptionType = ET_BFISH;
    }
    else if (version <= VERSION(2, 0, 3, 0))
    {
        g_Config.EncryptionType = ET_203;
    }
    else
    {
        g_Config.EncryptionType = ET_TFISH;
    }
}

static void ClientVersionFixup(const char *versionStr)
{
    DEBUG_TRACE_FUNCTION;

    LOG("Client Emulation:\n");
    SetClientVersion(versionStr);
    SetClientCrypt(g_Config.ClientVersion);

    const bool useVerdata = g_Config.ClientVersion < CV_500A;
    const auto clientType = GetClientType(g_Config.ClientVersion);

    if (g_Config.ClientVersion < CV_500A)
    {
        g_MapSize[0].Width = 6144;
        g_MapSize[1].Width = 6144;
    }

    int a = (g_Config.ClientVersion >> 24) & 0xff;
    int b = (g_Config.ClientVersion >> 16) & 0xff;
    int c = (g_Config.ClientVersion >> 8) & 0xff;
    int d = (g_Config.ClientVersion & 0xff);
    char p1[64], p2[8];
    snprintf(p1, sizeof(p1), "%d.%d.%d", a, b, c);
    if (d >= 'a' && d <= 'z')
    {
        snprintf(p2, sizeof(p2), "%c", d);
    }
    else
    {
        snprintf(p2, sizeof(p2), ".%d", d);
    }

    LOG("\tEmulation Compatibility Version: %s%s (0x%08x)\n", p1, p2, g_Config.ClientVersion);
    LOG("\tCryptography: %08x %08x %08x %04x (%d)\n",
        g_Config.Key1,
        g_Config.Key2,
        g_Config.Key3,
        g_Config.Seed,
        g_Config.EncryptionType);
    LOG("\tClient Type: %s (%d)\n", GetClientTypeName(clientType), clientType);
    LOG("\tUse Verdata: %d\n", useVerdata);

    if (!g_Config.UseCrypt)
    {
        g_Config.EncryptionType = ET_NOCRYPT;
    }

    if (g_Config.ClientVersion >= CV_70331)
    {
        g_MaxViewRange = MAX_VIEW_RANGE_NEW;
    }
    else
    {
        g_MaxViewRange = MAX_VIEW_RANGE_OLD;
    }

    g_PacketManager.ConfigureClientVersion(g_Config.ClientVersion);
    g_Config.ClientFlag = clientType;
    g_Config.UseVerdata = useVerdata;
}

void GetClientVersion(uint32_t *major, uint32_t *minor, uint32_t *rev, uint32_t *proto)
{
    if (major)
    {
        *major = (g_Config.ClientVersion >> 24) & 0xff;
    }

    if (minor)
    {
        *minor = (g_Config.ClientVersion >> 16) & 0xff;
    }

    if (rev)
    {
        *rev = (g_Config.ClientVersion >> 8) & 0xff;
    }

    if (proto)
    {
        *proto = (g_Config.ClientVersion & 0xff);
    }
}

void LoadGlobalConfig()
{
    DEBUG_TRACE_FUNCTION;

    LOG("Loading global config from " ORIONUO_CONFIG "\n");
    Wisp::CTextFileParser file(g_App.ExeFilePath(ORIONUO_CONFIG), "=,", "#;", "");

    while (!file.IsEOF())
    {
        auto strings = file.ReadTokens();
        if (strings.size() >= 2)
        {
            const auto key = config::GetConfigKey(strings[0]);
            switch (key)
            {
                case MSCC_CLIENT_VERSION:
                {
                    g_Config.ClientVersionString = strings[1];
                    ClientVersionFixup(strings[1].c_str());
                }
                break;
                case MSCC_CUSTOM_PATH:
                {
                    g_App.m_UOPath = ToPath(strings[1]);
                    fs_case_insensitive_init(g_App.m_UOPath);
                }
                break;
                case MSCC_ACTID:
                {
                    g_Config.Login = strings[1];
                    break;
                }
                case MSCC_ACTPWD:
                {
                    string password = file.RawLine;
                    size_t pos = password.find_first_of('=');
                    g_Config.Password = password.substr(pos + 1, password.length() - (pos + 1));
                    break;
                }
                case MSCC_REMEMBERPWD:
                {
                    g_Config.SavePassword = ToBool(strings[1]);
                    break;
                }
                case MSCC_AUTOLOGIN:
                {
                    g_Config.AutoLogin = ToBool(strings[1]);
                    break;
                }
                case MSCC_THE_ABYSS:
                {
                    g_Config.TheAbyss = ToBool(strings[1]);
                    break;
                }
                case MSCC_ASMUT:
                {
                    g_Config.Asmut = ToBool(strings[1]);
                    break;
                }
                case MSCC_LOGIN_SERVER:
                {
                    g_Config.ServerAddress = strings[1];
                    g_Config.ServerPort = ToInt(strings[2]);
                    break;
                }
                case MSCC_USE_CRYPT:
                {
                    g_Config.UseCrypt = ToBool(strings[1]);
                    break;
                }
                default:
                    break;
            }
        }
    }
}

void SaveGlobalConfig()
{
    DEBUG_TRACE_FUNCTION;
    LOG("Saving global config to " ORIONUO_CONFIG "\n");
    FILE *cfg = fs_open(g_App.ExeFilePath(ORIONUO_CONFIG), FS_WRITE);
    if (cfg == nullptr)
    {
        return;
    }

    fprintf(cfg, "AcctID=%s\n", g_Config.Login.c_str());
    if (g_Config.SavePassword)
    {
        fprintf(cfg, "AcctPassword=%s\n", g_Config.Password.c_str());
        fprintf(cfg, "RememberAcctPW=yes\n");
    }
    else
    {
        fprintf(cfg, "AcctPassword=\n");
        fprintf(cfg, "RememberAcctPW=no\n");
    }

    fprintf(cfg, "AutoLogin=%s\n", (g_Config.AutoLogin ? "yes" : "no"));
    fprintf(cfg, "TheAbyss=%s\n", (g_Config.TheAbyss ? "yes" : "no"));
    fprintf(cfg, "Asmut=%s\n", (g_Config.Asmut ? "yes" : "no"));

    if (g_App.m_UOPath != g_App.m_ExePath)
    {
        fprintf(cfg, "CustomPath=%s\n", CStringFromPath(g_App.m_UOPath));
    }

    if (!g_Config.ServerAddress.empty())
    {
        fprintf(cfg, "LoginServer=%s,%d\n", g_Config.ServerAddress.c_str(), g_Config.ServerPort);
    }

    fprintf(cfg, "ClientVersion=%s\n", g_Config.ClientVersionString.c_str());
    fprintf(cfg, "Crypt=%s\n", (g_Config.UseCrypt ? "yes" : "no"));

    fflush(cfg);
    fs_close(cfg);
}
