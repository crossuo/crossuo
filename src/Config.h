// GPLv3 License
// Copyright (C) 2019 Danny Angelo Carminati Grein

#pragma once

struct Config
{
    string Login;
    string Password;
    string ClientVersionString = "7.0.33.1";
    string CustomPath;
    string ServerAddress;
    uint16_t ServerPort = 2593;
    uint16_t ClientFlag = 0;
    bool SavePassword = false;
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
    uint32_t ClientVersion = CV_LATEST;
    uint32_t EncryptionType = 0;
};

void GetClientVersion(uint32_t *major, uint32_t *minor, uint32_t *rev, uint32_t *proto = nullptr);
void LoadGlobalConfig();
void SaveGlobalConfig();

extern Config g_Config;
