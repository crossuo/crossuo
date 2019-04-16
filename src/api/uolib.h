#pragma once

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

extern void uo_log(const char *type, ...);

#if LIBUO == 1

#include <assert.h>
#include <stdint.h>

template <typename T, typename U>
static inline T checked_cast(U value)
{
    auto result = static_cast<T>(value);
    assert(static_cast<U>(result) == value && "Type conversion loses information");
    return result;
}

template <typename T, typename U>
static inline T checked_cast(U *value)
{
    auto result = checked_cast<T>((intptr_t)value);
    assert(static_cast<intptr_t>(result) == (intptr_t)value && "Type conversion loses information");
    return result;
}

#define countof(xarray) (sizeof(xarray) / sizeof(xarray[0]))

#define DEBUG_TRACE_FUNCTION

#define LIBUO_STRINGIFY(x) #x
#define LIBUO_TOSTRING(x) LIBUO_STRINGIFY(x)

#define PRINT_LOG(type, ...) uo_log(type, __VA_ARGS__)
#define TRACE(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#define DEBUG(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#define Info(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#define Warning(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)
#define Error(type, ...) PRINT_LOG(LIBUO_TOSTRING(type), __VA_ARGS__)

#define os_path std::string
#define ToPath(x) x
#define StringFromPath(x) x
#define CStringFromPath(x) x.c_str()
#define PATH_SEP std::string("/")

#else

#if defined(XUO_WINDOWS)
#define os_path std::wstring
#define ToPath(x) ToWString(x)
#define StringFromPath(x) ToString(x)
#define CStringFromPath(x) ToString(x).c_str()
#define PATH_SEP ToPath("/")
#else
#define os_path std::string
#define ToPath(x) x
#define StringFromPath(x) x
#define CStringFromPath(x) x.c_str()
#define PATH_SEP std::string("/")
#endif

#endif
