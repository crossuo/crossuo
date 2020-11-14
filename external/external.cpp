// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// xxhash lib
#ifndef XXH_LIB
#define XXH_LIB
#define XXH_STATIC_LINKING_ONLY // enable xxh3
#include "xxhash.c"
#endif // XXH_LIB

// ass.h lib
#ifndef ASS_DEPS
#define ASS_DEPS
#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "tsf.h"
#define TML_IMPLEMENTATION
#define TML_NO_STDIO
#include "tml.h"
#endif // ASS_DEPS

// tinyxml2
#include "tinyxml2.cpp"

// inih - https://github.com/benhoyt/inih/tree/0a87bf16693e35ed172ce8738404d9acd9545a5a/
#define INIH_IMPLEMENTATION
#include "inih.h"

// cbase64 - https://github.com/SizzlingCalamari/cbase64/tree/a353cab21c021aa4b16135baf909452b3ddc1173
#define CBASE64_IMPLEMENTATION
#include "cbase64.h"
