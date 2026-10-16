// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "Config.h"

#include "Platform.h"

#include "Globals.h"
#include <xuocore/uodata.h>
#include <xuocore/text_parser.h>
#include <xuocore/vfs.h>
#include <common/logging/logging.h>
#include <common/fs.h>
#include <common/str.h>
#include <sstream>
#include <common/utils.h>
#include <xuocore/enumlist.h>
#include <xuocore/client_info.h>
#include "Crypt/CryptEntry.h"
#include "Managers/PacketManager.h"
#include "Managers/ConnectionManager.h"
#include "ScreenStages/BaseScreen.h"
#include "ScreenStages/MainScreen.h"

#include <external/popts.h>
extern po::parser g_cli;

#define CROSSUO_CONFIG "crossuo.cfg"

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
    MSCC_PROTOCOL_CLIENT_VERSION,
    MSCC_USE_CRYPT,
    MSCC_USE_KEYS,
    MSCC_USE_VERDATA,
    MSCC_CLIENT_TYPE,
    MSCC_LOCALE_OVERRIDE,
    MSCC_REPORT_LANGUAGE,
    MSCC_SEED_OVERRIDE,
    MSCC_SYSTEM_CHECK_OVERRIDE,
    MSCC_SHARD_NAME,
    MSCC_DATA_PRODUCT_URL,
    MSCC_SHARD_WEB,
    MSCC_SHARD_ACCOUNT,
    MSCC_SHARD_HELP,
    MSCC_SHARD_VIDEO,
    MSCC_SHARD_CREDITS,
    MSCC_SHARD_OTHER,
    MSCC_SHARD_COMMUNITY,
    MSCC_IGNORE_RELAY,
    MSCC_SEND_PING,
    MSCC_COUNT,
};

namespace config
{
struct Modified
{
    bool UseVerdata = false;
    bool ClientFlag = false;
    bool ProtocolVersion = false;
    bool ClientVersion = false;
};

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
    { MSCC_PROTOCOL_CLIENT_VERSION, "protocolclientversion" },
    { MSCC_USE_CRYPT, "crypt" },
    { MSCC_USE_KEYS, "keys" },
    { MSCC_USE_VERDATA, "useverdata" },
    { MSCC_CLIENT_TYPE, "clienttype" },
    { MSCC_LOCALE_OVERRIDE, "language" },
    { MSCC_REPORT_LANGUAGE, "reportlanguage" },
    { MSCC_SEED_OVERRIDE, "seedoverride" },
    { MSCC_SYSTEM_CHECK_OVERRIDE, "systemcheckoverride" },
    { MSCC_SHARD_NAME, "shardname" },
    { MSCC_DATA_PRODUCT_URL, "producturl" },
    { MSCC_SHARD_WEB, "shardweb" },
    { MSCC_SHARD_ACCOUNT, "shardaccount" },
    { MSCC_SHARD_HELP, "shardhelp" },
    { MSCC_SHARD_VIDEO, "shardvideo" },
    { MSCC_SHARD_CREDITS, "shardcredits" },
    { MSCC_SHARD_OTHER, "shardother" },
    { MSCC_SHARD_COMMUNITY, "shardcommunity" },
    { MSCC_IGNORE_RELAY, "ignorerelay" },
    { MSCC_SEND_PING, "sendping" },
    { MSCC_COUNT, nullptr },
};
static_assert(countof(s_Keys) == MSCC_COUNT, "missing config entry");

static uint32_t GetConfigKey(const astr_t &key)
{
    auto str = str_lower(key);
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

static config::Modified s_Mark;

static uint32_t ParseVersion(const char *versionStr)
{
    if (!versionStr || !versionStr[0])
    {
        return CV_LATEST;
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

    return VERSION(a, b, c, d);
}

static CLIENT_FLAG GetClientTypeFromString(const astr_t &str)
{
    auto client = str_lower(str);
    if (client == "t2a")
    {
        return CF_T2A;
    }
    if (client == "re")
    {
        return CF_RE;
    }
    if (client == "td")
    {
        return CF_TD;
    }
    if (client == "lbr")
    {
        return CF_LBR;
    }
    if (client == "aos")
    {
        return CF_AOS;
    }
    if (client == "se")
    {
        return CF_SE;
    }
    if (client == "sa")
    {
        return CF_SA;
    }

    return CF_UNDEFINED;
}

static const char *GetClientTypeString(uint16_t clientFlag)
{
    switch (clientFlag)
    {
        case CF_T2A:
            return "t2a";
        case CF_RE:
            return "re";
        case CF_TD:
            return "td";
        case CF_LBR:
            return "lbr";
        case CF_AOS:
            return "aos";
        case CF_SE:
            return "se";
        case CF_SA:
            return "sa";
        default:
            break;
    }
    return "";
}

uint32_t GetClientFlagMask()
{
    if (g_Config.ClientFlag == CF_UNDEFINED || g_Config.ClientFlag > CF_SA)
    {
        return 0;
    }
    return g_Config.ClientFlag ? (uint32_t(g_Config.ClientFlag) << 1) - 1 : 0;
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
        default:
            break;
    }
    return "";
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
        if (!g_Config.ClientKeysSet)
        {
            g_Config.Key1 = 0x2D13A5FC;
            g_Config.Key2 = 0x2D13A5FD;
            g_Config.Key3 = 0xA39D527F;
        }
        g_Config.EncryptionType = ET_203;
        return;
    }

    if (!g_Config.ClientKeysSet)
    {
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
    }

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

static void ClientVersionFixup(bool overrideProtocolVersion)
{
    g_Config.ClientVersion = ParseVersion(g_Config.ClientVersionString.c_str());
    g_Config.ProtocolClientVersion = g_Config.ClientVersion;
    if (overrideProtocolVersion)
    {
        g_Config.ProtocolClientVersion = ParseVersion(g_Config.ProtocolClientVersionString.c_str());
    }
    else
    {
        g_Config.ProtocolClientVersionString = g_Config.ClientVersionString;
    }
    SetClientCrypt(g_Config.ProtocolClientVersion);

    const bool useVerdata = g_Config.ClientVersion < CV_500A;
    const auto clientType = GetClientType(g_Config.ClientVersion);

    if (g_Config.ClientVersion < CV_500A)
    {
        g_MapSize[0].Width = 6144;
        g_MapSize[1].Width = 6144;
    }

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

    if (!s_Mark.ClientFlag)
    {
        g_Config.ClientFlag = clientType;
    }
    if (!s_Mark.UseVerdata)
    {
        g_Config.UseVerdata = useVerdata;
    }
}

// apply the defaults a config file would have set up when there is none:
// the streamed install is always the current uop distribution, so the
// client version defaults to the latest known one and everything derived
// from it (cryptography, client type, packet layout, view range) follows
void ApplyClientVersionDefaults()
{
    char str[32] = {};
    client_version_string(CV_LATEST, str, sizeof(str));
    g_Config.ClientVersionString = str;
    g_Config.ClientVersion = CV_LATEST;
    ClientVersionFixup(false);
}

// command-line override of the version markers (--clientversion /
// --protocolversion / --clienttype): mirrors a config file entry - the
// strings are set and everything derived from them (cryptography, client
// type, packet layout, view range, verdata) is refixed. An empty argument
// leaves its marker untouched; a protocol version set by the config file
// keeps winning unless the command line sets one too.
void ApplyClientVersionOverride(
    const astr_t &client_version, const astr_t &protocol_version, const astr_t &client_type)
{
    if (client_version.empty() && protocol_version.empty() && client_type.empty())
    {
        return;
    }
    if (!client_version.empty())
    {
        g_Config.ClientVersionString = client_version;
        g_Config.ClientVersionModified = true;
    }
    bool protocol_overridden = false;
    if (!protocol_version.empty())
    {
        g_Config.ProtocolClientVersionString = protocol_version;
        protocol_overridden = true;
    }
    const auto flag = GetClientTypeFromString(client_type);
    if (flag != CF_UNDEFINED)
    {
        g_Config.ClientFlag = (uint16_t)flag;
        s_Mark.ClientFlag = true; // the CLI is the last word on the client type
    }
    ClientVersionFixup(protocol_overridden || s_Mark.ProtocolVersion);
}

static fs_path GetDefaultConfigFile()
{
    const auto local_exe = ExeFilePath(CROSSUO_CONFIG);
    if (fs_path_exists(local_exe))
        return local_exe;
    return FilePath(CROSSUO_CONFIG);
}

// priority order looking for a config file:
// 1. command line -c/--config option
// 2. crossuo.cfg on the side with executable
// 3. crossuo.cfg in the crossuo appdata
static fs_path GetConfigFile()
{
    if (!g_cli["config"].was_set())
    {
        return GetDefaultConfigFile();
    }

    return fs_path_from(g_cli["config"].get().string);
}

void ApplyLanguageSettings()
{
    if (g_Config.LocaleOverride.empty())
    {
        Platform::SetLanguageFromSystemLocale();
    }
    else
    {
        g_Language = str_lower(g_Config.LocaleOverride);
    }
    g_ServerLanguageReport = !g_Config.LocaleOverrideReport.empty()
                                 ? str_upper(g_Config.LocaleOverrideReport)
                                 : str_upper(g_Language);
}

size_t ParseByteList(const astr_t &text, uint8_t *out, size_t max)
{
    std::istringstream stream(text);
    size_t count = 0;
    astr_t token;
    while (count < max && stream >> token)
    {
        const auto value = std::stoul(token, nullptr, 16);
        if (value > 0xFF)
        {
            return 0;
        }
        out[count++] = uint8_t(value);
    }
    return count;
}

bool LoadGlobalConfig()
{
    auto cfg = GetConfigFile();
    Info(Config, "loading global config from: '%s'", fs_path_ascii(cfg));
    if (!fs_path_exists(cfg))
    {
        cfg = fs_path_join(fs_path_current(), cfg);
    }

    if (!fs_path_exists(cfg))
    {
        Info(Config, "config not found");
        return false;
    }

    TextFileParser file(cfg, "=,", "#;", "");
    while (!file.IsEOF())
    {
        auto strings = file.ReadTokens(false); // Trim remove spaces from paths
        // section headers are a noop: the grammar is key-based, a file may
        // carry [crossuo] and/or [Shard] sections (a shards.cfg is directly
        // usable as a crossuo.cfg) and the keys are read regardless
        if (!strings.empty() && strings[0][0] == '[')
            continue;
        if (strings.size() == 1)
        {
            // the text parser drops empty values, so `key=` arrives as a
            // single token: assign the empty string as-is instead of keeping
            // the default (launcher profiles intentionally overwrite the
            // white-label defaults with shard-specific values)
            switch (config::GetConfigKey(strings[0]))
            {
                case MSCC_DATA_PRODUCT_URL:
                    g_Config.DataProductUrl.clear();
                    g_Config.DataProductUrlSet = true;
                    break;
                case MSCC_SHARD_WEB:
                    g_Config.ShardWeb.clear();
                    break;
                case MSCC_SHARD_ACCOUNT:
                    g_Config.ShardAccount.clear();
                    break;
                case MSCC_SHARD_HELP:
                    g_Config.ShardHelp.clear();
                    break;
                case MSCC_SHARD_VIDEO:
                    g_Config.ShardVideo.clear();
                    break;
                case MSCC_SHARD_CREDITS:
                    g_Config.ShardCredits.clear();
                    break;
                case MSCC_SHARD_OTHER:
                    g_Config.ShardOther.clear();
                    break;
                case MSCC_SHARD_COMMUNITY:
                    g_Config.ShardCommunity.clear();
                    break;
                default:
                    break;
            }
        }
        if (strings.size() >= 2)
        {
            const auto key = config::GetConfigKey(strings[0]);
            if (key == MSCC_USE_KEYS)
            {
                Info(
                    Config,
                    "\t%s=%s,%s,%s,%s",
                    config::s_Keys[key - 1].key_name,
                    strings[1].c_str(),
                    strings[2].c_str(),
                    strings[3].c_str(),
                    strings[4].c_str());
            }
            else if (key != MSCC_NONE && key != MSCC_ACTPWD)
            {
                Info(Config, "\t%s=%s", config::s_Keys[key - 1].key_name, strings[1].c_str());
            }
            switch (key)
            {
                case MSCC_CLIENT_VERSION:
                {
                    g_Config.ClientVersionString = strings[1];
                    g_Config.ClientVersionModified = true;
                }
                break;
                case MSCC_PROTOCOL_CLIENT_VERSION:
                {
                    g_Config.ProtocolClientVersionString = strings[1];
                    s_Mark.ProtocolVersion = true;
                }
                break;
                case MSCC_CUSTOM_PATH:
                {
                    g_UOPath = fs_path_from(strings[1]);
                    fs_case_insensitive_init(g_UOPath);
                    g_Config.CustomPathSet = true;
                }
                break;
                case MSCC_ACTID:
                {
                    g_Config.Login = strings[1];
                    break;
                }
                case MSCC_ACTPWD:
                {
                    auto password = file.RawLine;
                    size_t pos = password.find_first_of('=');
                    g_Config.Password = password.substr(pos + 1, password.length() - (pos + 1));
                    break;
                }
                case MSCC_REMEMBERPWD:
                {
                    g_Config.SavePassword = str_to_bool(strings[1]);
                    break;
                }
                case MSCC_AUTOLOGIN:
                {
                    g_Config.AutoLogin = str_to_bool(strings[1]);
                    break;
                }
                case MSCC_THE_ABYSS:
                {
                    g_Config.TheAbyss = str_to_bool(strings[1]);
                    break;
                }
                case MSCC_ASMUT:
                {
                    g_Config.Asmut = str_to_bool(strings[1]);
                    break;
                }
                case MSCC_LOGIN_SERVER:
                {
                    g_Config.ServerAddress = strings[1];
                    if (strings.size() > 2)
                    {
                        g_Config.ServerPort = str_to_int(strings[2]);
                    }
                    break;
                }
                case MSCC_USE_CRYPT:
                {
                    g_Config.UseCrypt = str_to_bool(strings[1]);
                    break;
                }
                case MSCC_IGNORE_RELAY:
                {
                    g_Config.IgnoreRelay = str_to_bool(strings[1]);
                    break;
                }
                case MSCC_SEND_PING:
                {
                    g_Config.SendPing = str_to_bool(strings[1]);
                    break;
                }
                case MSCC_USE_KEYS:
                {
                    g_Config.Key1 = str_to_int(strings[1], 16);
                    g_Config.Key2 = str_to_int(strings[2], 16);
                    g_Config.Key3 = str_to_int(strings[3], 16);
                    g_Config.Seed = str_to_int(strings[4], 16);
                    g_Config.UseCrypt = true;
                    g_Config.ClientKeysSet = true;
                    break;
                }
                case MSCC_USE_VERDATA:
                {
                    s_Mark.UseVerdata = true;
                    g_Config.UseVerdata = str_to_bool(strings[1]);
                    break;
                }
                case MSCC_CLIENT_TYPE:
                {
                    auto type = GetClientTypeFromString(strings[1]);
                    if (type != CF_UNDEFINED)
                    {
                        s_Mark.ClientFlag = true;
                        g_Config.ClientFlag = type;
                    }
                    break;
                }
                case MSCC_LOCALE_OVERRIDE:
                {
                    g_Config.LocaleOverride = str_upper(strings[1]);
                }
                break;
                case MSCC_REPORT_LANGUAGE:
                {
                    g_Config.LocaleOverrideReport = str_upper(strings[1]);
                }
                break;
                case MSCC_SEED_OVERRIDE:
                {
                    g_Config.SeedOverride = str_trim(strings[1]);
                }
                break;
                case MSCC_SYSTEM_CHECK_OVERRIDE:
                {
                    g_Config.SystemCheckOverride = str_trim(strings[1]);
                }
                break;
                case MSCC_SHARD_NAME:
                {
                    g_Config.ShardName = strings[1];
                    break;
                }
                case MSCC_DATA_PRODUCT_URL:
                {
                    g_Config.DataProductUrl = strings[1];
                    g_Config.DataProductUrlSet = true;
                    break;
                }
                case MSCC_SHARD_WEB:
                {
                    g_Config.ShardWeb = strings[1];
                    break;
                }
                case MSCC_SHARD_ACCOUNT:
                {
                    g_Config.ShardAccount = strings[1];
                    break;
                }
                case MSCC_SHARD_HELP:
                {
                    g_Config.ShardHelp = strings[1];
                    break;
                }
                case MSCC_SHARD_VIDEO:
                {
                    g_Config.ShardVideo = strings[1];
                    break;
                }
                case MSCC_SHARD_CREDITS:
                {
                    g_Config.ShardCredits = strings[1];
                    break;
                }
                case MSCC_SHARD_OTHER:
                {
                    g_Config.ShardOther = strings[1];
                    break;
                }
                case MSCC_SHARD_COMMUNITY:
                {
                    // launcher-facing presentation link: accepted and stored,
                    // the client has no surface for it yet (see ShardHelp)
                    g_Config.ShardCommunity = strings[1];
                    break;
                }
                default:
                    break;
            }
        }
    }

    ClientVersionFixup(s_Mark.ProtocolVersion);
    ApplyLanguageSettings();
    return true;
}

// the applied version markers and everything derived from them. Called AFTER
// the command-line override so the printed values are the ones in effect
// (the web shell passes the shard profile's version flags; there is no
// launcher report on that path)
void LogClientVersionInfo()
{
    char version[32];
    client_version_string(g_Config.ClientVersion, version, sizeof(version));

    Info(Client, "Client Emulation:");
    Info(Client, "\tClient Version: %s", g_Config.ClientVersionString.c_str());
    Info(Client, "\tEmulation Compatibility Version: %s (0x%08x)", version, g_Config.ClientVersion);
    Info(
        Client,
        "\tProtocol Compatibility Version: %s (0x%08x)",
        g_Config.ProtocolClientVersionString.c_str(),
        g_Config.ProtocolClientVersion);
    Info(
        Client,
        "\tCryptography: %08x %08x %08x %04x (%d)",
        g_Config.Key1,
        g_Config.Key2,
        g_Config.Key3,
        g_Config.Seed,
        g_Config.EncryptionType);
    Info(
        Client,
        "\tClient Type: %s (%d)",
        GetClientTypeName((CLIENT_FLAG)g_Config.ClientFlag),
        g_Config.ClientFlag);
    Info(Client, "\tUse Verdata: %d", g_Config.UseVerdata);
    Info(Client, "\tLanguage: %s (%s)", g_Language.c_str(), g_ServerLanguageReport.c_str());
    Info(Client, "\tUltima Online Path: %s", fs_path_ascii(g_UOPath));
}
void SaveGlobalConfig(bool launcher_apply)
{
    //a config passed via -c/--config (e.g. a launcher profile) is a normal
    //read-write config for session saves, EXCEPT once the config changed
    //live via the embedded launcher: from that point on the session state is
    //launcher-driven and must never be written back into the profile file -
    //it lands on the default crossuo.cfg instead (this includes the shutdown
    //save: without the latch it would re-persist the launcher-picked shard
    //into the profile, overwriting it). A restart without --config picks the
    //default file up
    static bool s_launcher_changed_config = false;
    if (launcher_apply)
    {
        s_launcher_changed_config = true;
    }
    auto cfgPath = (g_cli["config"].was_set() && s_launcher_changed_config)
                       ? GetDefaultConfigFile()
                       : GetConfigFile();

    Info(Config, "saving global config to: %s", fs_path_ascii(cfgPath));
    FILE *cfg = fs_open(cfgPath, FS_WRITE);
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

    if (s_Mark.ClientFlag)
    {
        fprintf(cfg, "ClientType=%s\n", GetClientTypeString(g_Config.ClientFlag));
    }
    if (s_Mark.UseVerdata)
    {
        fprintf(cfg, "UseVerdata=%s\n", (g_Config.UseVerdata ? "yes" : "no"));
    }

    if (!g_Config.LocaleOverride.empty())
    {
        fprintf(cfg, "Language=%s\n", g_Config.LocaleOverride.c_str());
    }

    if (!g_Config.LocaleOverrideReport.empty())
    {
        fprintf(cfg, "ReportLanguage=%s\n", g_Config.LocaleOverrideReport.c_str());
    }

    if (!g_Config.SeedOverride.empty())
    {
        fprintf(cfg, "SeedOverride=%s\n", g_Config.SeedOverride.c_str());
    }

    if (!g_Config.SystemCheckOverride.empty())
    {
        fprintf(cfg, "SystemCheckOverride=%s\n", g_Config.SystemCheckOverride.c_str());
    }

    if (!g_Config.ShardName.empty())
    {
        fprintf(cfg, "ShardName=%s\n", g_Config.ShardName.c_str());
    }

    // shard presentation fields are only written when they differ from the
    // white-label defaults (ShardOther/ShardCommunity when non-empty): baking defaults
    // into every crossuo.cfg would later fight shard switches driven by
    // launcher profiles, as a stale baked value would win over the default
    // of the newly selected shard
    if (g_Config.DataProductUrl != XUO_DEFAULT_DATA_PRODUCT_SERVER XUO_DEFAULT_DATA_PRODUCT_PATH)
    {
        fprintf(cfg, "ProductUrl=%s\n", g_Config.DataProductUrl.c_str());
    }
    if (g_Config.ShardWeb != XUO_DEFAULT_SHARD_WEB)
    {
        fprintf(cfg, "ShardWeb=%s\n", g_Config.ShardWeb.c_str());
    }
    if (g_Config.ShardAccount != XUO_DEFAULT_SHARD_ACCOUNT)
    {
        fprintf(cfg, "ShardAccount=%s\n", g_Config.ShardAccount.c_str());
    }
    if (g_Config.ShardHelp != XUO_DEFAULT_SHARD_HELP)
    {
        fprintf(cfg, "ShardHelp=%s\n", g_Config.ShardHelp.c_str());
    }
    if (g_Config.ShardVideo != XUO_DEFAULT_SHARD_VIDEO)
    {
        fprintf(cfg, "ShardVideo=%s\n", g_Config.ShardVideo.c_str());
    }
    if (g_Config.ShardCredits != XUO_DEFAULT_SHARD_CREDITS)
    {
        fprintf(cfg, "ShardCredits=%s\n", g_Config.ShardCredits.c_str());
    }
    if (!g_Config.ShardOther.empty())
    {
        fprintf(cfg, "ShardOther=%s\n", g_Config.ShardOther.c_str());
    }
    if (!g_Config.ShardCommunity.empty())
    {
        fprintf(cfg, "ShardCommunity=%s\n", g_Config.ShardCommunity.c_str());
    }

    fprintf(cfg, "Crypt=%s\n", (g_Config.UseCrypt ? "yes" : "no"));
    fprintf(cfg, "IgnoreRelay=%s\n", (g_Config.IgnoreRelay ? "yes" : "no"));
    if (g_Config.ClientKeysSet)
    {
        fprintf(
            cfg,
            "Keys=0x%08x,0x%08x,0x%08x,0x%04x\n",
            g_Config.Key1,
            g_Config.Key2,
            g_Config.Key3,
            g_Config.Seed);
    }

    if (!fs_path_equal(g_UOPath, g_ExePath))
    {
        fprintf(cfg, "CustomPath=%s\n", fs_path_ascii(g_UOPath));
    }

    if (!g_Config.ServerAddress.empty())
    {
        fprintf(cfg, "LoginServer=%s,%d\n", g_Config.ServerAddress.c_str(), g_Config.ServerPort);
    }

    if (g_Config.ClientVersionModified)
    {
        fprintf(cfg, "ClientVersion=%s\n", g_Config.ClientVersionString.c_str());
    }

    if (s_Mark.ProtocolVersion)
    {
        fprintf(cfg, "ProtocolClientVersion=%s\n", g_Config.ProtocolClientVersionString.c_str());
    }
    fflush(cfg);
    fs_close(cfg);
}

bool ApplyClientProfile(
    const astr_t &login,
    const astr_t &password,
    bool save_password,
    bool autologin,
    const astr_t &character,
    const astr_t &loginserver,
    const astr_t &custom_path,
    const astr_t &client_version,
    const astr_t &client_type,
    bool crypt,
    const astr_t &shard_name)
{
    const bool connected = g_ConnectionManager.Connected();
    const bool booted = g_CurrentScreen != nullptr;
    const bool can_apply_live = !booted || (g_CurrentScreen == &g_MainScreen && !connected);

    const auto new_path = fs_path_from(custom_path);
    const bool path_changed = !custom_path.empty() && !fs_path_equal(g_UOPath, new_path);

    // 1] mirror the profile into the global configuration state
    if (!custom_path.empty())
    {
        g_UOPath = new_path;
        fs_case_insensitive_init(g_UOPath);
        g_Config.CustomPath = custom_path;
    }
    g_Config.Login = login;
    g_Config.Password = password;
    g_Config.SavePassword = save_password;
    g_Config.AutoLogin = autologin;
    g_Config.UseCrypt = crypt;
    if (!shard_name.empty())
    {
        g_Config.ShardName = shard_name;
    }

    // loginserver uses the config file format and may carry a port: "host,port"
    g_Config.ServerAddress = loginserver;
    g_Config.ServerPort = XUO_DEFAULT_SERVER_PORT;
    const auto comma = loginserver.find(',');
    if (comma != astr_t::npos)
    {
        g_Config.ServerAddress = loginserver.substr(0, comma);
        g_Config.ServerPort = (uint16_t)str_to_int(loginserver.substr(comma + 1));
    }

    if (!client_version.empty())
    {
        g_Config.ClientVersionString = client_version;
        g_Config.ClientVersionModified = true;
        s_Mark.ClientVersion = true;
    }
    const auto flag = GetClientTypeFromString(client_type);
    if (flag != CF_UNDEFINED)
    {
        g_Config.ClientFlag = (uint16_t)flag;
        s_Mark.ClientFlag = true;
    }

    ClientVersionFixup(s_Mark.ProtocolVersion);

    // 2] persist it, keeping the active configuration file in sync - as a
    // launcher apply it must not rewrite a --config profile: it lands on the
    // default crossuo.cfg instead (see SaveGlobalConfig)
    SaveGlobalConfig(true);

    // 3] apply live, when the client state allows it
    if (!can_apply_live)
    {
        return false;
    }

    if (booted)
    {
        g_MainScreen.SetAccounting(login, password);
        g_MainScreen.m_SavePassword->Checked = save_password;
        g_MainScreen.m_AutoLogin->Checked = autologin;
        if (!character.empty())
        {
            g_PacketManager.AutoLoginNames = astr_t("|") + character;
        }
        else
        {
            g_PacketManager.AutoLoginNames.clear();
        }
        // the login screen already loaded art from the old data path
        if (path_changed)
        {
            return false;
        }
    }
    return true;
}

bool ApplyShardProfile(const shard_profile &profile)
{
    // the shard name and the presentation links are applied directly; an
    // empty link resets to the white-label default so shard switches are
    // deterministic (see the contract in Config.h)
    g_Config.ShardName = profile.name;
    g_Config.ShardWeb = !profile.web.empty() ? profile.web : astr_t(XUO_DEFAULT_SHARD_WEB);
    g_Config.ShardAccount =
        !profile.account_page.empty() ? profile.account_page : astr_t(XUO_DEFAULT_SHARD_ACCOUNT);
    g_Config.ShardHelp = !profile.help.empty() ? profile.help : astr_t(XUO_DEFAULT_SHARD_HELP);
    g_Config.ShardVideo = !profile.video.empty() ? profile.video : astr_t(XUO_DEFAULT_SHARD_VIDEO);
    g_Config.ShardCredits =
        !profile.credits.empty() ? profile.credits : astr_t(XUO_DEFAULT_SHARD_CREDITS);
    g_Config.ShardOther = profile.other;
    g_Config.ShardCommunity = profile.community;

    // shard-specific protocol overrides: a shard switch is deterministic, so
    // an empty value clears the override (a shard that needs one always
    // provides it in its profile) - same contract as the link resets above
    g_Config.SeedOverride = str_trim(profile.seed_override);
    g_Config.SystemCheckOverride = str_trim(profile.system_check_override);
    g_Config.LocaleOverride = str_trim(profile.language);
    g_Config.LocaleOverrideReport = str_trim(profile.report_language);

    // an empty product url is not a reset: the launcher always sends a
    // complete profile, and live shard entries without an installer keep
    // whatever product url was in effect
    if (!profile.product_url.empty())
    {
        g_Config.DataProductUrl = profile.product_url;
        g_Config.DataProductUrlSet = true;
    }

    // identity/credentials go through ApplyClientProfile for its live-apply
    // and needs-restart semantics. login/password/save_password/autologin
    // come from the current g_Config (assigning them back is a no-op), and
    // so does the data path: an empty custom_path is skipped there, and a
    // non-empty one is already reflected in g_UOPath, so a shard switch can
    // never report a data-path change (no spurious restart)
    //
    // `character` has no g_Config field: the live client keeps it in
    // g_PacketManager.AutoLoginNames ("|name[|name...]"), which
    // ApplyClientProfile rewrites as "|" + character - passing the current
    // list back (minus the leading separator) reproduces it byte for byte,
    // so shard selection leaves the auto-login character untouched
    auto character = g_PacketManager.AutoLoginNames;
    if (!character.empty() && character[0] == '|')
    {
        character.erase(0, 1);
    }

    return ApplyClientProfile(
        g_Config.Login,
        g_Config.Password,
        g_Config.SavePassword,
        g_Config.AutoLogin,
        character,
        profile.loginserver,
        g_Config.CustomPath,
        profile.clientversion,
        profile.clienttype,
        profile.crypt,
        profile.name);
}
