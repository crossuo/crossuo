// MIT License

#pragma once

#include "md5.h"

#define CRYPT_AUTO_VALUE 0x80

#define CRYPT_GAMEKEY_LENGTH 6
#define CRYPT_GAMEKEY_COUNT 25

#define CRYPT_GAMESEED_LENGTH 8
#define CRYPT_GAMESEED_COUNT 25

#define CRYPT_GAMETABLE_START 1
#define CRYPT_GAMETABLE_STEP 3
#define CRYPT_GAMETABLE_MODULO 11
#define CRYPT_GAMETABLE_TRIGGER 21036

#define DIR_ENCRYPT 0 /* Are we encrpyting? */
#define DIR_DECRYPT 1 /* Are we decrpyting? */
#define MODE_ECB 1    /* Are we ciphering in ECB mode? */

/* The structure for key information */
struct keyInstance
{
    unsigned char direction;
    int keyLen;
    int numRounds;
    char keyMaterial[68];
    unsigned int keySig;
    unsigned int key32[8];
    unsigned int sboxKeys[4];
    unsigned int subKeys[40];
};

/* The structure for cipher information */
struct cipherInstance
{
    unsigned char mode;
    unsigned char IV[16];
    unsigned int cipherSig;
    unsigned int iv32[4];
};

class CBlowfishCrypt
{
    unsigned char m_seed[CRYPT_GAMESEED_LENGTH];
    int m_table_index = 0;
    int m_block_pos = 0;
    int m_stream_pos = 0;

    void InitTables();
    void RawEncrypt(uint32_t *values, int table);

public:
    CBlowfishCrypt() = default;
    ~CBlowfishCrypt() = default;

    void Encrypt(uint8_t *in, uint8_t *out, int len);
    void Init();
};

class CTwofishCrypt
{
    keyInstance ki;
    cipherInstance ci;
    unsigned char m_subData3[256];
    uint32_t m_IP = 0; // seed
    uint32_t dwIndex = 0;
    // int tabEnable;
    int m_pos = 0;
    unsigned char sm_bData[16]; // int numRounds[4];

    MD5Crypt *m_md5 = nullptr;
    bool m_use_md5 = false;

public:
    CTwofishCrypt() = default;
    ~CTwofishCrypt() = default;

    void Init(uint8_t seed[4]);
    void Init_MD5();

    void Encrypt(const uint8_t *in, uint8_t *out, int size);
    void Decrypt(const uint8_t *in, uint8_t *out, int size);
};

extern CBlowfishCrypt g_BlowfishCrypt;
extern CTwofishCrypt g_TwofishCrypt;
