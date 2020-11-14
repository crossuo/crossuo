/*
SPDX-License-Identifier: MIT
SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

The MIT License (MIT)
Copyright (c) 2020 Danny Angelo Carminati Grein
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#if CHECKSUM_IMPLEMENTATION_PRIVATE
#define CHECKSUM_PRIVATE static
#else
#define CHECKSUM_PRIVATE
#endif

#if defined(CHECKSUM_IMPLEMENTATION) || defined(CHECKSUM_IMPLEMENTATION_PRIVATE)
#include <stdint.h>
#include <stddef.h>

CHECKSUM_PRIVATE uint64_t uo_jenkins_hash(const char *s)
{
    uint32_t l = 0;
    for (; s && s[l]; ++l)
        ;
    uint32_t eax, ecx, edx, ebx, esi, edi;

    eax = ecx = edx = ebx = esi = edi = 0;
    ebx = edi = esi = l + 0xDEADBEEF;

    uint32_t i = 0;
    for (i = 0; i + 12 < l; i += 12)
    {
        edi = (uint32_t)((s[i + 7] << 24) | (s[i + 6] << 16) | (s[i + 5] << 8) | s[i + 4]) + edi;
        esi = (uint32_t)((s[i + 11] << 24) | (s[i + 10] << 16) | (s[i + 9] << 8) | s[i + 8]) + esi;
        edx = (uint32_t)((s[i + 3] << 24) | (s[i + 2] << 16) | (s[i + 1] << 8) | s[i]) - esi;

        edx = (edx + ebx) ^ (esi >> 28) ^ (esi << 4);
        esi += edi;
        edi = (edi - edx) ^ (edx >> 26) ^ (edx << 6);
        edx += esi;
        esi = (esi - edi) ^ (edi >> 24) ^ (edi << 8);
        edi += edx;
        ebx = (edx - esi) ^ (esi >> 16) ^ (esi << 16);
        esi += edi;
        edi = (edi - ebx) ^ (ebx >> 13) ^ (ebx << 19);
        ebx += esi;
        esi = (esi - edi) ^ (edi >> 28) ^ (edi << 4);
        edi += ebx;
    }

    if (l - i > 0)
    {
        switch (l - i)
        {
            case 12:
                esi += static_cast<uint32_t>(s[i + 11]) << 24;
                goto case_11;
                break;
            case 11:
            case_11:
                esi += static_cast<uint32_t>(s[i + 10]) << 16;
                goto case_10;
                break;
            case 10:
            case_10:
                esi += static_cast<uint32_t>(s[i + 9]) << 8;
                goto case_9;
                break;
            case 9:
            case_9:
                esi += s[i + 8];
                goto case_8;
                break;
            case 8:
            case_8:
                edi += static_cast<uint32_t>(s[i + 7]) << 24;
                goto case_7;
                break;
            case 7:
            case_7:
                edi += static_cast<uint32_t>(s[i + 6]) << 16;
                goto case_6;
                break;
            case 6:
            case_6:
                edi += static_cast<uint32_t>(s[i + 5]) << 8;
                goto case_5;
                break;
            case 5:
            case_5:
                edi += s[i + 4];
                goto case_4;
                break;
            case 4:
            case_4:
                ebx += static_cast<uint32_t>(s[i + 3]) << 24;
                goto case_3;
                break;
            case 3:
            case_3:
                ebx += static_cast<uint32_t>(s[i + 2]) << 16;
                goto case_2;
                break;
            case 2:
            case_2:
                ebx += static_cast<uint32_t>(s[i + 1]) << 8;
                goto case_1;
            case 1:
            case_1:
                ebx += s[i];
                break;
        }

        esi = (esi ^ edi) - ((edi >> 18) ^ (edi << 14));
        ecx = (esi ^ ebx) - ((esi >> 21) ^ (esi << 11));
        edi = (edi ^ ecx) - ((ecx >> 7) ^ (ecx << 25));
        esi = (esi ^ edi) - ((edi >> 16) ^ (edi << 16));
        edx = (esi ^ ecx) - ((esi >> 28) ^ (esi << 4));
        edi = (edi ^ edx) - ((edx >> 18) ^ (edx << 14));
        eax = (esi ^ edi) - ((edi >> 8) ^ (edi << 24));

        return (static_cast<uint64_t>(edi) << 32) | eax;
    }

    return (static_cast<uint64_t>(esi) << 32) | eax;
}

#if 1 // USE_CLIENT_CRC32

static uint32_t crc32_table[256];
static uint32_t crc32_reflect(uint32_t source, int c)
{
    uint32_t value = 0;
    for (int i = 1; i < c + 1; i++)
    {
        if ((source & 0x1) != 0u)
        {
            value |= (1 << (c - i));
        }
        source >>= 1;
    }
    return value;
}

CHECKSUM_PRIVATE void crc32_init()
{
    for (int i = 0; i < 256; i++)
    {
        crc32_table[i] = crc32_reflect((int)i, 8) << 24;
        for (int j = 0; j < 8; j++)
        {
            crc32_table[i] =
                (crc32_table[i] << 1) ^ ((crc32_table[i] & (1 << 31)) != 0u ? 0x04C11DB7 : 0);
        }
        crc32_table[i] = crc32_reflect(crc32_table[i], 32);
    }

#if 0
    printf ("static const unsigned int crc32_table[] =\n{\n");
    for (int i = 0; i < 256; i += 4)
    {
        printf("  0x%08x, 0x%08x, 0x%08x, 0x%08x",
            crc32_table[i + 0],
            crc32_table[i + 1],
            crc32_table[i + 2],
            crc32_table[i + 3]);

        if (i + 4 < 256)
            putchar(',');
        putchar('\n');
    }
     printf("};\n");
#endif
}

CHECKSUM_PRIVATE uint32_t crc32_checksum(uint8_t *ptr, size_t size)
{
    uint32_t crc = 0xFFFFFFFF;
    while (size > 0)
    {
        crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ *ptr];
        ptr++;
        size--;
    }
    return (crc & 0xFFFFFFFF);
}

#else // #if USE_CLIENT_CRC32

// clang-format off
static const unsigned int crc32_table[] =
{
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
    0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
    0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
    0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
    0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
    0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
    0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
    0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
    0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
    0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
    0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
    0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
    0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
    0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
    0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
    0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
    0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
    0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
    0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
    0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
    0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
    0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};
// clang-format on

CHECKSUM_PRIVATE uint32_t crc32_checksum(uint8_t *ptr, size_t size)
{
    uint32_t crc = ~0u;
    while (size--)
    {
        crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *ptr++) & 255];
    }
    return crc;
}

#endif // #else // #if USE_CLIENT_CRC32

CHECKSUM_PRIVATE uint32_t adler32(uint8_t *ptr, size_t size)
{
    uint32_t a = 1;
    uint32_t b = 0;
    for (int i = 0; i < size; i++)
    {
        a = (a + ptr[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

#endif // CHECKSUM_IMPLEMENTATION

#ifndef CHECKSUM_HEADER
#define CHECKSUM_HEADER

#include <stdint.h>
#include <stddef.h>

CHECKSUM_PRIVATE uint64_t uo_jenkins_hash(const char *s);
CHECKSUM_PRIVATE void crc32_init();
CHECKSUM_PRIVATE uint32_t crc32_checksum(uint8_t *ptr, size_t size);
CHECKSUM_PRIVATE uint32_t adler32(uint8_t *ptr, size_t size);
#endif // CHECKSUM_HEADER
