/*
The MIT License (MIT)
Copyright (c) 2017 Danny Angelo Carminati Grein
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

#endif // CHECKSUM_IMPLEMENTATION

#ifndef CHECKSUM_HEADER
#define CHECKSUM_HEADER

#include <stdint.h>
#include <stddef.h>

CHECKSUM_PRIVATE uint64_t uo_jenkins_hash(const char *s);
CHECKSUM_PRIVATE void crc32_init();
CHECKSUM_PRIVATE uint32_t crc32_checksum(uint8_t *ptr, size_t size);

#endif // CHECKSUM_HEADER
