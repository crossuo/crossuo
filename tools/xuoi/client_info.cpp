// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include <stdio.h>
#include <inttypes.h>
#include <xuocore/client_info.h>

#define FS_IMPLEMENTATION
#include <common/fs.h>

#define CHECKSUM_IMPLEMENTATION
#include <common/checksum.h>

int main(int argc, char **argv)
{
    if (argc != 2 || !argv[1])
    {
        fprintf(stdout, "%s: <input file>\n", argv[0]);
        return -1;
    }

    client_info info;
    client_version(argv[1], info);
    if (!info.version)
    {
        // custom special case based on our organization of existing samples
        // parse the filename to retrieve the version
        char base[512], other[512];
        uint32_t a = 0, b = 0, c = 0, d = 0;
        char dc = 0;
        int r = sscanf(argv[1], "%[^_]_%d.%d.%d.%d%s.exe", base, &a, &b, &c, &d, other);
        if (r < 5)
        {
            r = sscanf(argv[1], "%[^_]_%d.%d.%d%c%s.exe", base, &a, &b, &c, &dc, other);
            d = (uint32_t)dc;
        }
        if (r >= 4)
        {
            info.version = VERSION(a, b, c, d);
        }
        else
        {
            info.version = -1;
        }
    }
    char str[32] = {};
    client_version_string(info.version, str, sizeof(str));
    fprintf(
        stdout,
        "    { 0x%016" PRIx64 ", 0x%08x, 0x%08x }, // %s\n",
        info.xxh3,
        info.crc32,
        info.version,
        str);
    return 0;
}
