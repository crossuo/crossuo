// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "CryptEntry.h"
#include <common/fs.h>
#include <cstdint>
#include <xuocore/enumlist.h>
#include "../Config.h"
#include "LoginCrypt.h"
#include "GameCrypt.h"

namespace Crypt
{
void Init(bool is_login, uint8_t seed[4])
{
    if (is_login)
    {
        g_LoginCrypt.Init(seed);
    }
    else
    {
        if (g_Config.EncryptionType == ET_OLD_BFISH)
        {
            // Old clients derive the stream keys from the connection seed:
            // the game socket re-seeds the same stream used on the login
            // socket with the seed carried by the relay (0x8C) packet.
            g_LoginCrypt.Init(seed);
        }

        if (g_Config.EncryptionType != ET_NOCRYPT)
        {
            g_BlowfishCrypt.Init();
        }

        if (g_Config.EncryptionType == ET_203 || g_Config.EncryptionType == ET_TFISH)
        {
            g_TwofishCrypt.Init(seed);
            if (g_Config.EncryptionType == ET_TFISH)
            {
                g_TwofishCrypt.Init_MD5();
            }
        }
    }
}

void Encrypt(bool is_login, uint8_t *src, uint8_t *dest, int size)
{
    if (g_Config.EncryptionType == ET_NOCRYPT)
    {
        memcpy(dest, src, size);
    }
    else if (is_login)
    {
        if (g_Config.EncryptionType == ET_OLD_BFISH)
        {
            g_LoginCrypt.Encrypt_Old(src, dest, size);
        }
        else if (g_Config.EncryptionType == ET_1_25_36)
        {
            g_LoginCrypt.Encrypt_1_25_36(src, dest, size);
        }
        else if (g_Config.EncryptionType != ET_NOCRYPT)
        {
            g_LoginCrypt.Encrypt(src, dest, size);
        }
    }
    else if (g_Config.EncryptionType == ET_OLD_BFISH)
    {
        // Old clients encrypt every post-seed C->S byte on the game socket
        // with the same seed-keyed stream as the login socket; only the
        // leading 4-byte seed travels in the clear.
        g_LoginCrypt.Encrypt_Old(src, dest, size);
    }
    else if (g_Config.EncryptionType == ET_203)
    {
        g_BlowfishCrypt.Encrypt(src, dest, size);
        g_TwofishCrypt.Encrypt(dest, dest, size);
    }
    else if (g_Config.EncryptionType == ET_TFISH)
    {
        g_TwofishCrypt.Encrypt(src, dest, size);
    }
    else
    {
        g_BlowfishCrypt.Encrypt(src, dest, size);
    }
}

void Decrypt(uint8_t *src, uint8_t *dest, int size)
{
    if (g_Config.EncryptionType == ET_TFISH)
    {
        g_TwofishCrypt.Decrypt(src, dest, size);
    }
    else
    {
        memcpy(dest, src, size);
    }
}

} // namespace Crypt
