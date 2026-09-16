// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

// Bridge contract between the client and the in-client assistant: the
// shared string carrier and the host-bridge function typedefs.

#pragma once

#include <string>
#include "enumlist.h"
#include <common/str.h>

#if defined(_MSC_VER)
#undef CDECL
#define CDECL __cdecl
#else
#define CDECL
#endif

class IGameString
{
public:
    IGameString();
    ~IGameString();

    IGameString &operator()(const astr_t &str);
    IGameString &operator()(const wstr_t &str);

    bool m_Unicode = false;
    char *m_DataA{ nullptr };
    wchar_t *m_DataW{ nullptr };
};

typedef uint64_t CDECL FUNCDEF_GET_STATIC_FLAGS(unsigned short);
typedef int CDECL FUNCDEF_GET_VALUE_INT(VALUE_KEY_INT, int);
typedef void CDECL FUNCDEF_SET_VALUE_INT(VALUE_KEY_INT, int);
typedef IGameString *CDECL FUNCDEF_GET_VALUE_STRING(VALUE_KEY_STRING, const char *);
typedef void CDECL FUNCDEF_SET_VALUE_STRING(VALUE_KEY_STRING, const char *);
