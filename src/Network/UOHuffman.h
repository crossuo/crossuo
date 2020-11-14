// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Hotride

#ifndef UOHuffmanH
#define UOHuffmanH

#include <stdint.h>

class CDecompressingCopier
{
private:
    static int tree[512];
    int value;
    int mask;
    int bit_num;
    int treepos;

public:
    CDecompressingCopier() { initialise(); }

    void initialise()
    {
        bit_num = 8;
        treepos = 0;
    }

    virtual void operator()(char *dest, const char *src, int &dest_size, intptr_t &src_size);
};

#endif
