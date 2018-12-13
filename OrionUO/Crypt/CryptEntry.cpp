
#if !USE_ORIONDLL
#include "CryptEntry.h"
#include "../Wisp/WispGlobal.h"
#include "../Wisp/WispDataStream.h"
#include "../EnumList.h"
#include "../plugin/plugininterface.h"
#include "LoginCrypt.h"
#include "GameCrypt.h"

ENCRYPTION_TYPE g_EncryptionType = ET_NOCRYPT;
static size_t s_CryptPluginsCount = 0;
vector<uint8_t> g_RawData;

static void Init(bool is_login, uint8_t seed[4])
{
    if (is_login)
    {
        g_LoginCrypt.Init(seed);
    }
    else
    {
        if (g_EncryptionType != ET_NOCRYPT)
        {
            g_BlowfishCrypt.Init();
        }

        if (g_EncryptionType == ET_203 || g_EncryptionType == ET_TFISH)
        {
            g_TwofishCrypt.Init(seed);
            if (g_EncryptionType == ET_TFISH)
            {
                g_TwofishCrypt.Init_MD5();
            }
        }
    }
}

static void Send(bool is_login, uint8_t *src, uint8_t *dest, int size)
{
    if (g_EncryptionType == ET_NOCRYPT)
    {
        memcpy(dest, src, size);
    }
    else if (is_login)
    {
        if (g_EncryptionType == ET_OLD_BFISH)
        {
            g_LoginCrypt.Encrypt_Old(src, dest, size);
        }
        else if (g_EncryptionType == ET_1_25_36)
        {
            g_LoginCrypt.Encrypt_1_25_36(src, dest, size);
        }
        else if (g_EncryptionType != ET_NOCRYPT)
        {
            g_LoginCrypt.Encrypt(src, dest, size);
        }
    }
    else if (g_EncryptionType == ET_203)
    {
        g_BlowfishCrypt.Encrypt(src, dest, size);
        g_TwofishCrypt.Encrypt(dest, dest, size);
    }
    else if (g_EncryptionType == ET_TFISH)
    {
        g_TwofishCrypt.Encrypt(src, dest, size);
    }
    else
    {
        g_BlowfishCrypt.Encrypt(src, dest, size);
    }
}

static void Decrypt(uint8_t *src, uint8_t *dest, int size)
{
    if (g_EncryptionType == ET_TFISH)
    {
        g_TwofishCrypt.Decrypt(src, dest, size);
    }
    else
    {
        memcpy(dest, src, size);
    }
}

static void LoadPlugins(PLUGIN_INFO *result)
{
    if (static_cast<unsigned int>(!g_RawData.empty()) != 0u)
    {
        Wisp::CDataReader file(&g_RawData[0], (int)g_RawData.size());

        uint8_t ver = file.ReadUInt8();

        file.Move(2);
        int len = file.ReadUInt8();
        file.Move(len + 39);

        if (ver >= 2)
        {
            file.Move(1);

            if (ver >= 3)
            {
                file.Move(1);
            }

            char count = file.ReadInt8();

            for (int i = 0; i < count; i++)
            {
                short len = file.ReadInt16LE();
                string fileName = file.ReadString(len);
                memcpy(&result[i].FileName[0], &fileName[0], fileName.length());

                file.Move(2);
                result[i].Flags = file.ReadUInt32LE();
                file.Move(2);

                len = file.ReadInt16LE();
                string functionName = file.ReadString(len);
                memcpy(&result[i].FunctionName[0], &functionName[0], functionName.length());
            }
        }
    }
}

vector<uint8_t> ApplyInstall(uint8_t *address, size_t size)
{
    vector<uint8_t> result;

    if (size != 0u)
    {
        g_RawData.resize(size);
        memcpy(&g_RawData[0], &address[0], size);

        Wisp::CDataReader file(address, size);
        Wisp::CDataWriter writer;

        uint8_t version = file.ReadInt8();
        writer.WriteUInt8(version);
        writer.WriteUInt8(0xFE); //dll version
        writer.WriteUInt8(0);    //sub version

        g_EncryptionType = (ENCRYPTION_TYPE)file.ReadInt8();
        writer.WriteUInt8(file.ReadInt8()); //ClientVersion

        int len = file.ReadInt8();
        writer.WriteUInt8(len);
        writer.WriteDataLE((uint8_t *)file.ReadString(len).data(), len, 0);

        file.Move(14); //crypt keys & seed
#if defined(_M_IX86)
        writer.WriteUInt32LE((size_t)Init);
        writer.WriteUInt32LE((size_t)Send);
        writer.WriteUInt32LE((size_t)Decrypt);
        writer.WriteUInt32LE((size_t)LoadPlugins);
#else
        writer.WriteUInt64LE((size_t)Init);
        writer.WriteUInt64LE((size_t)Send);
        writer.WriteUInt64LE((size_t)Decrypt);
        writer.WriteUInt64LE((size_t)LoadPlugins);
#endif

        int mapsCount = 6;

        if (version < 4)
        {
            writer.WriteUInt8(file.ReadInt8()); //InverseBuylist
        }
        else
        {
            mapsCount = file.ReadInt8();
            writer.WriteUInt8(mapsCount);
        }

        for (int i = 0; i < mapsCount; i++)
        {
            writer.WriteUInt16LE(file.ReadUInt16LE());
            writer.WriteUInt16LE(file.ReadUInt16LE());
        }

        uint8_t clientFlag = 0;
        uint8_t useVerdata = 0;

        if (version >= 2)
        {
            clientFlag = file.ReadInt8();

            if (version >= 3)
            {
                useVerdata = file.ReadInt8();
            }

            s_CryptPluginsCount = file.ReadInt8();
        }

        writer.WriteUInt8(clientFlag);
        writer.WriteUInt8(useVerdata);

        result = writer.Data();
    }

    //ParseCommandLine();

    return result;
}

size_t GetPluginsCount()
{
    return s_CryptPluginsCount;
}

void CryptInstallNew(uint8_t *address, size_t size, uint8_t *result, size_t &resultSize)
{
    assert(address && result && size && resultSize);
    vector<uint8_t> buf = ApplyInstall(address, size);
    memset(result, 0, resultSize);
    resultSize = buf.size();
    if (resultSize >= buf.size())
    {
        memcpy(result, &buf[0], buf.size());
    }
}
#endif
