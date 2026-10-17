// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <common/fs.h>
#include <xuocore/enumlist.h>
#include "../white_label.h"

struct Config
{
    astr_t Login;
    astr_t Password;
    astr_t ClientVersionString = XUO_DEFAULT_CLIENT_VERSION_STR;
    astr_t ProtocolClientVersionString = XUO_DEFAULT_PROTOCOL_VERSION_STR;
    astr_t CustomPath;
    astr_t ServerAddress = XUO_DEFAULT_SERVER_ADDRESS;
    astr_t LocaleOverride;
    // ReportLanguage=: the language code reported to the server on all
    // communications (g_ServerLanguageReport); overrides the one derived
    // from the client language when set
    astr_t LocaleOverrideReport;
    // SeedOverride=<4 hex bytes>: replaces the IP-derived login seed on the
    // login connection (e.g. "SeedOverride= 0x00 0x01 0x48 0xaa")
    astr_t SeedOverride;
    // SystemCheckOverride=<hex bytes>: payload of the client->server 0x03
    // system-check reply (PlayUO-detection ack); the client prepends the
    // 0x03 header and the big-endian size itself, so the field holds only
    // the bytes after those
    astr_t SystemCheckOverride;
    // shard name: comes from the launcher profile, the config file, the
    // command line (--shard) or the white-label default, in that order of
    // recency; names the per-shard streaming install directory
    astr_t ShardName = XUO_DEFAULT_SHARD_NAME;
    // shard presentation + data-product fields: like ShardName they come
    // from the shard profile (launcher pick / crossuo.cfg keys below) and
    // override the white-label defaults at runtime
    astr_t DataProductUrl = XUO_DEFAULT_DATA_PRODUCT_SERVER XUO_DEFAULT_DATA_PRODUCT_PATH;
    astr_t ShardWeb = XUO_DEFAULT_SHARD_WEB;
    astr_t ShardAccount = XUO_DEFAULT_SHARD_ACCOUNT;
    astr_t ShardHelp = XUO_DEFAULT_SHARD_HELP;
    astr_t ShardVideo = XUO_DEFAULT_SHARD_VIDEO;
    astr_t ShardCredits = XUO_DEFAULT_SHARD_CREDITS;
    // no white-label default: shards carry community links as plain urls
    astr_t ShardOther;
    astr_t ShardCommunity;
    uint16_t ServerPort = XUO_DEFAULT_SERVER_PORT;
    uint16_t ClientFlag = 0;
    bool SavePassword = true;
    bool AutoLogin = false;
    bool TheAbyss = false;
    bool Asmut = false;
    bool UseVerdata = false;
    bool UseCrypt = false;
    // IgnoreRelay=yes: bypass the relay (0x8C) redirect and do the second
    // login on the same address/port as the first login
    bool IgnoreRelay = false;
    // SendPing=yes: periodically send the 0x73 keepalive ping in-game
    // (the classic client does this every second; off by default)
    bool SendPing = false;
    // Assistant=yes: allow the in-client X:UO Assist host to initialize.
    // Assistant=no keeps the assistant from ever being launched: it hides the
    // Assistant entry from the macro editor and the menubar button. Defaults
    // to yes when the key is missing from crossuo.cfg
    bool AssistantEnabled = true;

    // Calculated stuff that are not saved back
    uint16_t Seed = 0x1357;
    uint32_t Key1 = 0;
    uint32_t Key2 = 0;
    uint32_t Key3 = 0;
    uint32_t ClientVersion = XUO_DEFAULT_CLIENT;
    uint32_t ProtocolClientVersion = XUO_DEFAULT_CLIENT;
    uint32_t EncryptionType = 0;

    bool ClientVersionModified = false;
    bool ClientKeysSet = false;

    // true when the config file explicitly set the data path (CustomPath);
    // not saved back
    bool CustomPathSet = false;

    // true when the config file explicitly set the data product url
    // (ProductUrl=); not saved back
    bool DataProductUrlSet = false;

    // command-line account credentials (--login/--password). Kept here
    // because the web boot order builds the main screen (whose account and
    // password entries CPacketFirstLogin reads) BEFORE ProcessCommandLine
    // runs; the screen-init path consumes these directly
    astr_t CliLogin;
    astr_t CliPassword;
    bool CliLoginSet = false;
};

bool LoadGlobalConfig();
// launcher_apply: the change came from the embedded launcher's live apply -
// a profile file passed via -c/--config is never overwritten by launcher
// changes: the snapshot lands on the default crossuo.cfg instead (which a
// restart without --config picks up), and the latch is sticky: every later
// save in that session (shutdown included) keeps targeting the default file.
// Plain session saves (shutdown, etc.) write the active config file,
// --config included, as long as no launcher apply happened
void SaveGlobalConfig(bool launcher_apply = false);
// parses "<0xaa> <0xbb> ..." style byte lists used by SeedOverride and
// SystemCheckOverride; returns the number of bytes written (<= max)
size_t ParseByteList(const astr_t &text, uint8_t *out, size_t max);
void ApplyClientVersionDefaults();
void ApplyLanguageSettings();
uint32_t GetClientFlagMask();
void ApplyClientVersionOverride(
    const astr_t &client_version, const astr_t &protocol_version, const astr_t &client_type);
// prints the APPLIED version markers and everything derived from them (after
// the command-line override)
void LogClientVersionInfo();

// Applies a launcher profile to the running client (embedded launcher):
// persists the new state with SaveGlobalConfig() and mirrors it into the
// live client state. Returns true when fully applied; false when the client
// must be restarted for the changes to take effect.
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
    const astr_t &shard_name);

// a complete shard description as handed over by every shard-selection path
// (launcher pick, embedded live-apply, later --config / web flows)
struct shard_profile
{
    astr_t name;        // shard name; names the per-shard streaming install dir
    astr_t loginserver; // "host,port" (config-file format)
    astr_t clientversion;
    astr_t clienttype;
    bool crypt = false;
    astr_t product_url; // manifest root (trailing /) or full manifest url
    astr_t web, account_page, help, video, credits; // login-screen links
    astr_t other;     // plain community url ("" = none)
    astr_t community; // forum/community page (accepted, launcher-facing)
    astr_t seed_override;           // "0xaa 0xbb 0xcc 0xdd" login seed ("" = none)
    astr_t system_check_override;   // 0x03 payload bytes ("" = none)
    // assistant constraint: "" = keep the client's own Assistant= value,
    // "no" = forbid launching the assistant on this shard ("yes" is never
    // sent: it is the default everywhere)
    astr_t assistant;
    astr_t language;                // client locale (.loc), "" = keep default
    astr_t report_language;         // report locale, "" = keep default
};

// The single entry point all shard-selection paths go through. The caller
// provides a COMPLETE shard description: an empty link field resets that
// link to its white-label default, so switching shards is deterministic
// (defaults are never empty for web/account/help/video/credits; other may
// be genuinely empty). Sets the shard name, the links, the streaming product
// url (only when non-empty: shards without an installer keep whatever url
// was in effect), then delegates identity/credentials to ApplyClientProfile
// passing the CURRENT credentials from g_Config - shard selection never
// touches them - and the current data path (preserved, never triggers a
// restart). Returns ApplyClientProfile's result: false when the client must
// be restarted for the shard to take effect.
bool ApplyShardProfile(const shard_profile &profile);

extern Config g_Config;
