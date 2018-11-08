// MIT License

#pragma once

class CLoginCrypt
{
private:
    uint32_t m_k1;
    uint32_t m_k2;
    uint32_t m_k3;
    uint32_t m_key[2];
    uint8_t m_seed[4];

public:
    CLoginCrypt();
    ~CLoginCrypt() {}

    void Init(uint8_t ps[4]);
    void Encrypt(const uint8_t *in, uint8_t *out, int size);
    void Encrypt_Old(const uint8_t *in, uint8_t *out, int size);
    void Encrypt_1_25_36(const uint8_t *in, uint8_t *out, int size);
};

extern CLoginCrypt g_LoginCrypt;
