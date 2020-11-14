// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>

#define VERSION(a, b, c, d)                                                                        \
    (((a & 0xff) << 24) | ((b & 0xff) << 16) | ((c & 0xff) << 8) | (d & 0xff))

struct client_info
{
    uint64_t xxh3 = 0;
    uint32_t crc32 = 0;
    uint32_t version = 0;
};

void client_version_string(uint32_t version, char *output, int maxlen);
int client_version(const char *filename, client_info &info);
