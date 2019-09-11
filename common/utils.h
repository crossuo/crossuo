#pragma once

#ifndef checked_cast
#include <stdint.h>
#include <assert.h>

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
#endif

#ifndef countof
#define countof(xarray) (sizeof(xarray) / sizeof(xarray[0]))
#endif

#ifndef MACRO_STRINGIFY
#define MACRO_STRINGIFY(x) #x
#define MACRO_TOSTRING(x) MACRO_STRINGIFY(x)
#endif
