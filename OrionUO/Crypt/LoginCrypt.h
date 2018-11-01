
#ifndef LOGINCRYPT_H
#define LOGINCRYPT_H

#include "../Wisp/WispGlobal.h"

class CLoginCrypt
{
private:
    uint32_t m_k1;
    uint32_t m_k2;
    uint32_t m_k3;
    uint32_t m_key[2];

public:
    CLoginCrypt();
    ~CLoginCrypt() {}

    uint8_t m_seed[4];

    void Init(uint8_t *ps);

    void Encrypt(uint8_t *in, uint8_t *out, int size);
    void Encrypt_Old(uint8_t *in, uint8_t *out, int size);
    void Encrypt_1_25_36(uint8_t *in, uint8_t *out, int size);
};

extern CLoginCrypt g_LoginCrypt;

#endif
