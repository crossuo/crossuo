// AGPLv3 License
// Copyright (c) 2026 The crossuo authors

#include "meta.h"

#include <string.h>

namespace meta
{
//
// SHA-256 (FIPS 180-4)
//

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static uint32_t rotr(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

static void sha256_block(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
               (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
    for (int i = 16; i < 64; i++)
    {
        const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; i++)
    {
        const uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = h + S1 + ch + sha256_k[i] + w[i];
        const uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    static const uint32_t init[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    uint32_t state[8];
    memcpy(state, init, sizeof(state));

    const size_t blocks = len / 64;
    for (size_t i = 0; i < blocks; i++)
        sha256_block(state, data + i * 64);

    uint8_t tail[128] = {};
    const size_t rem = len - blocks * 64;
    memcpy(tail, data + blocks * 64, rem);
    tail[rem] = 0x80;
    const size_t tail_blocks = rem + 9 > 64 ? 2 : 1;
    const uint64_t bits = uint64_t(len) * 8;
    for (int i = 0; i < 8; i++)
        tail[tail_blocks * 64 - 1 - i] = uint8_t(bits >> (8 * i));
    for (size_t i = 0; i < tail_blocks; i++)
        sha256_block(state, tail + i * 64);

    for (int i = 0; i < 8; i++)
    {
        out[i * 4] = uint8_t(state[i] >> 24);
        out[i * 4 + 1] = uint8_t(state[i] >> 16);
        out[i * 4 + 2] = uint8_t(state[i] >> 8);
        out[i * 4 + 3] = uint8_t(state[i]);
    }
}

//
// RSA public key operation with the Mythic launcher key (key_id 21)
//
// Key blob recovered from UO.exe (the constructor builds it on the stack):
//   [u8 len = 0x91][u8 0][u8 type = 0 (public)][u8 0][u8 ver = 1]
//   [u32 LE 129][129 byte modulus, big endian][u32 LE 3][3 byte exponent = 01 00 01]
// Note: the modulus really is 1026 bits (leading byte 0x02), not 1024.
//

static const uint8_t key21_modulus[129] = {
    0x02, 0x5e, 0xb6, 0xc9, 0xc8, 0xee, 0x58, 0xe2, 0x9d, 0x12, 0xce, 0x13, 0xf1, 0x3b, 0x5a, 0x84,
    0x2a, 0x06, 0x5a, 0xd1, 0x82, 0xfc, 0x0c, 0x74, 0x94, 0x41, 0x5d, 0xdd, 0x31, 0x39, 0x81, 0xd5,
    0x4b, 0xc9, 0x38, 0x1a, 0x12, 0xa5, 0x04, 0xda, 0x3c, 0x35, 0x65, 0x9e, 0xa9, 0x92, 0x38, 0x5e,
    0x9c, 0xe1, 0x84, 0xfe, 0xb1, 0xd4, 0xa9, 0xc8, 0xc2, 0xaa, 0xc7, 0x35, 0x11, 0x26, 0xf7, 0x6f,
    0x00, 0xa5, 0x0f, 0x5b, 0xf2, 0x8a, 0x14, 0x4c, 0xe2, 0x77, 0xd9, 0xd1, 0x07, 0xc3, 0xb5, 0xb6,
    0x9d, 0xfd, 0x20, 0x28, 0xda, 0xfb, 0x57, 0x26, 0x78, 0xcb, 0xb9, 0x3a, 0x24, 0xfa, 0xfa, 0x5f,
    0x92, 0xff, 0x7f, 0x9f, 0x51, 0xcf, 0xa5, 0x79, 0xa4, 0x27, 0x0f, 0x08, 0x8e, 0x91, 0xb5, 0xab,
    0x1c, 0xf9, 0xdd, 0x24, 0x30, 0x90, 0x44, 0xec, 0x3e, 0x6b, 0x8a, 0xae, 0x89, 0xbe, 0xb3, 0xa3,
    0x25
};

static const uint32_t key21_exponent = 65537;

// little-endian u32 limbs, enough for 132 bytes
static const int LIMBS = 33;

struct bignum
{
    uint32_t d[LIMBS] = {};
};

static void bn_from_be(bignum &r, const uint8_t *be, size_t len)
{
    r = {};
    for (size_t i = 0; i < len; i++)
        r.d[i / 4] |= uint32_t(be[len - 1 - i]) << (8 * (i % 4));
}

static void bn_to_be(const bignum &a, uint8_t *be, size_t len)
{
    for (size_t i = 0; i < len; i++)
        be[len - 1 - i] = uint8_t(a.d[i / 4] >> (8 * (i % 4)));
}

// r = a * b, r must not alias a or b, needs 2 * LIMBS limbs
static void bn_mul(const bignum &a, const bignum &b, uint64_t *r)
{
    memset(r, 0, 2 * LIMBS * sizeof(uint64_t));
    for (int i = 0; i < LIMBS; i++)
    {
        if (!a.d[i])
            continue;
        uint64_t carry = 0;
        for (int j = 0; j < LIMBS; j++)
        {
            const uint64_t t = uint64_t(a.d[i]) * b.d[j] + r[i + j] + carry;
            r[i + j] = t & 0xffffffffu;
            carry = t >> 32;
        }
        for (int k = i + LIMBS; carry && k < 2 * LIMBS; k++)
        {
            const uint64_t t = r[k] + carry;
            r[k] = t & 0xffffffffu;
            carry = t >> 32;
        }
    }
}

// r (2 * LIMBS limbs) %= n
static void bn_mod(uint64_t *r, const bignum &n)
{
    bignum rem;
    for (int bit = 2 * LIMBS * 32 - 1; bit >= 0; bit--)
    {
        // rem = rem * 2 + bit(r); all limbs shift, carry may not be dropped early
        uint32_t carry = uint32_t(r[bit / 32] >> (bit % 32)) & 1;
        for (int i = 0; i < LIMBS; i++)
        {
            const uint64_t t = (uint64_t(rem.d[i]) << 1) + carry;
            rem.d[i] = uint32_t(t);
            carry = uint32_t(t >> 32);
        }
        // if (rem >= n) rem -= n
        bool ge = false;
        for (int i = LIMBS - 1; i >= 0; i--)
        {
            if (rem.d[i] != n.d[i])
            {
                ge = rem.d[i] > n.d[i];
                break;
            }
        }
        if (ge)
        {
            uint64_t borrow = 0;
            for (int i = 0; i < LIMBS; i++)
            {
                const uint64_t t = uint64_t(rem.d[i]) - n.d[i] - borrow;
                rem.d[i] = uint32_t(t);
                borrow = (t >> 32) & 1;
            }
        }
    }
    memcpy(r, rem.d, sizeof(rem.d));
}

// r = b^e mod n
static void bn_modexp(bignum &r, const bignum &b, uint32_t e, const bignum &n)
{
    bignum result;
    result.d[0] = 1;
    uint64_t tmp[2 * LIMBS];
    bool first = true;
    for (int bit = 31; bit >= 0; bit--)
    {
        if (!first)
        {
            bn_mul(result, result, tmp);
            bn_mod(tmp, n);
            memcpy(result.d, tmp, sizeof(result.d));
        }
        if ((e >> bit) & 1)
        {
            bn_mul(result, b, tmp);
            bn_mod(tmp, n);
            memcpy(result.d, tmp, sizeof(result.d));
        }
        first = false;
    }
    r = result;
}

static bool rsa_recover(const uint8_t *sig, size_t sig_len, uint8_t em[129])
{
    if (sig_len == 0 || sig_len > sizeof(key21_modulus))
        return false;
    bignum s, n, m;
    bn_from_be(s, sig, sig_len);
    bn_from_be(n, key21_modulus, sizeof(key21_modulus));
    bool ge = false;
    for (int i = LIMBS - 1; i >= 0; i--)
    {
        if (s.d[i] != n.d[i])
        {
            ge = s.d[i] > n.d[i];
            break;
        }
    }
    if (ge)
        return false; // signature >= modulus
    bn_modexp(m, s, key21_exponent, n);
    bn_to_be(m, em, 129);
    return true;
}

bool verify_sig_block(const uint8_t *block, size_t block_len, const uint8_t sha256_digest[32])
{
    uint8_t em[129];
    if (!rsa_recover(block, block_len, em))
        return false;
    // em = 33 x 0x00 || hash slot A || SHA-256 || hash slot C
    for (int i = 0; i < 33; i++)
    {
        if (em[i] != 0)
            return false;
    }
    return memcmp(em + 65, sha256_digest, 32) == 0;
}

bool verify_meta(const uint8_t *meta, size_t meta_len, const uint8_t sha256_digest[32])
{
    // [u16 LE version][u16 LE payload_len][u16 BE key_id][u16 BE block_len][block]
    if (meta_len < 10)
        return false;
    const uint16_t version = uint16_t(meta[0] | (meta[1] << 8));
    const uint16_t payload_len = uint16_t(meta[2] | (meta[3] << 8));
    const uint16_t key_id = uint16_t((meta[4] << 8) | meta[5]);
    const uint16_t block_len = uint16_t((meta[6] << 8) | meta[7]);
    if (version != 4 || key_id != 21 || block_len == 0 || block_len > 129)
        return false;
    if (payload_len != meta_len - 4 || size_t(8) + block_len != meta_len)
        return false;
    return verify_sig_block(meta + 8, block_len, sha256_digest);
}

} // namespace meta
