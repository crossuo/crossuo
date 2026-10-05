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
    // no white-label default: shards carry discord via `urlother = Discord+<url>`
    astr_t ShardDiscord;
    uint16_t ServerPort = XUO_DEFAULT_SERVER_PORT;
    uint16_t ClientFlag = 0;
    bool SavePassword = true;
    bool AutoLogin = false;
    bool TheAbyss = false;
    bool Asmut = false;
    bool UseVerdata = false;
    bool UseCrypt = false;

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
};

bool LoadGlobalConfig();
void SaveGlobalConfig();
void ApplyClientVersionDefaults();

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

extern Config g_Config;
