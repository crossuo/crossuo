// AGPLv3 License
// Copyright (C) 2026 Danny Angelo Carminati Grein

#if defined(NEW_RENDERER_ENABLED)
#include "RenderTypes.h"
#include <assert.h>
#include <string.h>

#define countof(xarray) (sizeof(xarray) / sizeof(xarray[0]))

float float4::operator[](size_t i) const
{
    assert(i < countof(rgba));
    return rgba[i];
}

bool float4::operator==(const float4 &other) const
{
    return memcmp(rgba, other.rgba, sizeof(rgba)) == 0;
}

bool float4::operator!=(const float4 &other) const
{
    return !(*this == other);
}

float float3::operator[](size_t i) const
{
    assert(i < countof(rgb));
    return rgb[i];
}

bool float3::operator==(const float3 &other) const
{
    return memcmp(rgb, other.rgb, sizeof(rgb)) == 0;
}

bool float3::operator!=(const float3 &other) const
{
    return !(*this == other);
}
#endif // #if defined(NEW_RENDERER_ENABLED)
