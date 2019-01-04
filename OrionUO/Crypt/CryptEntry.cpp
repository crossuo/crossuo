
#include "CryptEntry.h"
#include "../Wisp/WispGlobal.h"
#include "../Wisp/WispDataStream.h"
#include "../plugin/enumlist.h"
#include "../plugin/plugininterface.h"
#include "../Config.h"
#include "LoginCrypt.h"
#include "GameCrypt.h"

static size_t s_CryptPluginsCount = 0;

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

size_t GetPluginsCount()
{
    return s_CryptPluginsCount;
}

} // namespace Crypt
