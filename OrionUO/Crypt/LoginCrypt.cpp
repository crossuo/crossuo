// MIT License

#include "LoginCrypt.h"
#include "../Misc.h"
#include "../Wisp/WispDataStream.h"
#include "aes.h"
#include "../Config.h"

CLoginCrypt g_LoginCrypt;

CLoginCrypt::CLoginCrypt()
{
    memset(&m_seed, 0, sizeof(m_seed));
}

void CLoginCrypt::Init(uint8_t ps[4])
{
    memcpy(m_seed, ps, 4);
    const uint32_t seed = (ps[0] << 24) | (ps[1] << 16) | (ps[2] << 8) | ps[3];

    m_k1 = g_Config.Key1;
    m_k2 = g_Config.Key2;
    m_k3 = g_Config.Key3;
    const uint32_t seed_key = g_Config.Seed;

    m_key[0] = (((~seed) ^ seed_key) << 16) | ((seed ^ 0xffffaaaa) & 0x0000ffff);
    m_key[1] = ((seed ^ 0x43210000) >> 16) | (((~seed) ^ 0xabcdffff) & 0xffff0000);
}

void CLoginCrypt::Encrypt(const uint8_t *in, uint8_t *out, int size)
{
    for (int i = 0; i < size; i++)
    {
        out[i] = in[i] ^ static_cast<uint8_t>(m_key[0]);

        const uint32_t table0 = m_key[0];
        const uint32_t table1 = m_key[1];

        m_key[1] = (((((table1 >> 1) | (table0 << 31)) ^ m_k1) >> 1) | (table0 << 31)) ^ m_k2;
        m_key[0] = ((table0 >> 1) | (table1 << 31)) ^ m_k3;
    }
}

void CLoginCrypt::Encrypt_Old(const uint8_t *in, uint8_t *out, int size)
{
    for (int i = 0; i < size; i++)
    {
        out[i] = in[i] ^ static_cast<unsigned char>(m_key[0]);

        const uint32_t table0 = m_key[0];
        const uint32_t table1 = m_key[1];

        m_key[0] = ((table0 >> 1) | (table1 << 31)) ^ m_k2;
        m_key[1] = ((table1 >> 1) | (table0 << 31)) ^ m_k1;
    }
}

void CLoginCrypt::Encrypt_1_25_36(const uint8_t *in, uint8_t *out, int size)
{
    for (int i = 0; i < size; i++)
    {
        out[i] = in[i] ^ static_cast<unsigned char>(m_key[0]);

        const uint32_t table0 = m_key[0];
        const uint32_t table1 = m_key[1];

        m_key[0] = ((table0 >> 1) | (table1 << 31)) ^ m_k2;
        m_key[1] = ((table1 >> 1) | (table0 << 31)) ^ m_k1;

        m_key[1] = (m_k1 >> ((5 * table1 * table1) & 0xFF)) + (table1 * m_k1) +
                   (table0 * table0 * 0x35ce9581) + 0x07afcc37;
        m_key[0] = (m_k2 >> ((3 * table0 * table0) & 0xFF)) + (table0 * m_k2) +
                   (m_key[1] * m_key[1] * 0x4c3a1353) + 0x16ef783f;
    }
}
