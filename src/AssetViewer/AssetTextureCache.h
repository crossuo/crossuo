// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#if defined(XUO_ASSISTANT)

#include <stdint.h>
#include <functional>
#include <map>
#include <vector>

#include <external/gfx/imgui/imgui.h>
#include <external/gfx/imgui/imgui_internal.h> // RegisterUserTexture/UnregisterUserTexture

// The shared texture cache for the asset viewer: game sprites are decoded to
// 16-bit UO color, converted (optionally hue-mapped) to RGBA32 and uploaded as
// ImGui-owned ImTextureData objects, following the proven lifecycle in
// src/Assistant/managers/assmap.cpp (Create -> RegisterUserTexture -> retire
// via WantDestroyNextFrame + UnregisterUserTexture once the backend reports
// Destroyed). Entries are LRU-capped; AssetTextureCache_Shutdown frees the
// CPU-side pixels after the ImGui context is gone.

enum class AssetTextureKind : uint8_t
{
    Static = 0,
    Land,
    Gump,
    Texmap,
    Light,
    Animation,
    Multi
};

struct AssetTextureKey
{
    AssetTextureKind Kind;
    uint16_t Id;
    uint16_t Hue;   // partial hue id (0 = none)
    uint16_t Width; // disambiguates multi-scale renders of the same id
    uint16_t Height;
    // kind-specific discriminator; animations pack
    // (group<<16)|(dir<<8)|frameIndex, everything else leaves it 0
    uint32_t Aux = 0;

    bool operator<(const AssetTextureKey &o) const
    {
        if (Kind != o.Kind)
        {
            return Kind < o.Kind;
        }
        if (Id != o.Id)
        {
            return Id < o.Id;
        }
        if (Hue != o.Hue)
        {
            return Hue < o.Hue;
        }
        if (Width != o.Width)
        {
            return Width < o.Width;
        }
        if (Height != o.Height)
        {
            return Height < o.Height;
        }
        return Aux < o.Aux;
    }
};

// Converts a 16-bit UO pixel buffer to RGBA32, applying the partial hue
// `hue` through g_ColorManager (0 = straight conversion). Pixels equal to 0
// stay fully transparent. Returns the number of converted pixels.
uint32_t
AssetViewer_Convert16ToRgba32(const uint16_t *src, uint32_t *dst, uint32_t count, uint16_t hue);

// Returns an ImGui texture reference for (key) filled with the given 16-bit
// pixels (size w*h, converted internally). Returns a null reference when the
// ImGui context is not live. Buffers larger than 2048x2048 are rejected.
ImTextureRef AssetTextureCache_Acquire(
    const AssetTextureKey &key, const uint16_t *pixels16, int w, int h, uint16_t hue);

// True when a live entry (texture or cached miss) already exists for `key`.
bool AssetTextureCache_Contains(const AssetTextureKey &key);

// Decode-on-miss variant of AssetTextureCache_Acquire: `decode` is invoked
// only when `key` is not cached yet and must fill a 16-bit buffer of exactly
// key.Width * key.Height pixels. Returning false caches the miss (with a
// frame stamp) so failing graphics are not re-decoded every frame; stale
// misses are retried periodically so transient failures recover.
typedef std::function<bool(std::vector<uint16_t> &)> AssetTextureDecodeFn;
ImTextureRef
AssetTextureCache_AcquireLazy(const AssetTextureKey &key, const AssetTextureDecodeFn &decode);

// True when the texture behind the ref has been uploaded by the backend (or
// carries a low-level id): a freshly registered ImTextureData has an invalid
// TexID until RenderDrawData uploads it, so GetTexID() != 0 alone skips fresh
// textures for one frame.
bool AssetTextureCache_IsReady(const ImTextureRef &ref);

// Frees every cached ImTextureData (CPU pixels only - the GL names were
// reclaimed by the backend teardown during the ImGui context shutdown).
void AssetTextureCache_Shutdown();

#endif // XUO_ASSISTANT
