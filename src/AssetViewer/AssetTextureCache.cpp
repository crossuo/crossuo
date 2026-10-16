// SPDX-License-Identifier: AGPL-3.0-or-later

#include "AssetTextureCache.h"

#if defined(XUO_ASSISTANT)

#include <functional>
#include <string.h>
#include <vector>

#include <xuocore/uodata.h> // g_FileManager (hue palettes)

#include "../Managers/ColorManager.h" // g_ColorManager

// The retire/teardown lifecycle mirrors src/Assistant/managers/assmap.cpp:
// retired textures stay registered (RefCount == 1) until the backend reports
// them Destroyed (then UnregisterUserTexture + delete); at shutdown the GL
// names are reclaimed by the backend teardown, so only the CPU memory is
// freed here.

namespace
{
struct CacheEntry
{
    ImTextureData *Tex = nullptr;
    uint32_t LastUseTicks = 0;
    uint32_t Bytes = 0; // RGBA payload (0 = cached miss)
    int MissFrame = -1; // ImGui frame of the decode failure (cached misses only)
};

const size_t ASSET_TEXTURE_LRU_CAP = 256;
const size_t ASSET_TEXTURE_BYTE_BUDGET = 64u * 1024 * 1024; // ~64 MB of RGBA
const int ASSET_TEXTURE_MAX_DIM = 2048;
const int ASSET_TEXTURE_MISS_RETRY_FRAMES = 30; // transient failures are retried

std::map<AssetTextureKey, CacheEntry> s_Entries;
std::vector<ImTextureData *> s_Retired;
int s_LastFrame = -1;
uint32_t s_AcquireCounter = 0;
uint32_t s_FrameStartStamp = 0; // first acquire stamp of the current ImGui frame
size_t s_Bytes = 0;

void ReapDestroyed()
{
    // reap the retirements the backend finished destroying
    for (size_t i = 0; i < s_Retired.size();)
    {
        ImTextureData *tex = s_Retired[i];
        if (tex->Status == ImTextureStatus_Destroyed)
        {
            ImGui::UnregisterUserTexture(tex);
            delete tex;
            s_Retired.erase(s_Retired.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

void Retire(CacheEntry &entry)
{
    // queue the texture for destroy (the atlas's own lifecycle sequence)
    entry.Tex->WantDestroyNextFrame = true;
    s_Retired.push_back(entry.Tex);
    entry.Tex = nullptr;
}

void EraseEntry(std::map<AssetTextureKey, CacheEntry>::iterator it)
{
    if (it->second.Tex != nullptr)
    {
        Retire(it->second);
    }
    s_Bytes -= it->second.Bytes;
    s_Entries.erase(it);
}

void EvictOverCap()
{
    while ((s_Entries.size() >= ASSET_TEXTURE_LRU_CAP || s_Bytes > ASSET_TEXTURE_BYTE_BUDGET) &&
           !s_Entries.empty())
    {
        auto victim = s_Entries.begin();
        for (auto it = s_Entries.begin(); it != s_Entries.end(); ++it)
        {
            if (it->second.LastUseTicks < victim->second.LastUseTicks)
            {
                victim = it;
            }
        }
        // never evict a texture touched in this same frame: the draw list of
        // this frame still references it (LRU thrash when many entries are
        // acquired per frame above the cap)
        if (victim->second.LastUseTicks >= s_FrameStartStamp)
        {
            break; // nothing eligible: allow temporary over-cap this frame
        }
        EraseEntry(victim);
    }
}

} // namespace

uint32_t
AssetViewer_Convert16ToRgba32(const uint16_t *src, uint32_t *dst, uint32_t count, uint16_t hue)
{
    const bool hued = hue != 0;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint16_t c = src[i];
        if (c == 0)
        {
            dst[i] = 0; // fully transparent (the UO color-key)
            continue;
        }
        uint16_t cc = c;
        const uint32_t rgb =
            hued ? g_ColorManager.GetPartialHueColor(cc, hue) : g_ColorManager.Color16To32(cc);
        dst[i] = rgb | 0xFF000000u;
    }
    return count;
}

ImTextureRef AssetTextureCache_Acquire(
    const AssetTextureKey &key, const uint16_t *pixels16, int w, int h, uint16_t hue)
{
    if (pixels16 == nullptr || w <= 0 || h <= 0)
    {
        return ImTextureRef();
    }
    (void)hue; // key.Hue carries the partial hue into the conversion
    return AssetTextureCache_AcquireLazy(
        key,
        [&](std::vector<uint16_t> &out)
        {
            out.assign(pixels16, pixels16 + (size_t)w * h);
            return true;
        });
}

bool AssetTextureCache_IsReady(const ImTextureRef &ref)
{
    // a fresh ImTextureData has TexID == ImTextureID_Invalid until the backend
    // uploads it during RenderDrawData (after the draw loop) - but _TexData is
    // already set, so the ref will resolve during rendering this very frame
    return ref.GetTexID() != 0 || ref._TexData != nullptr;
}

bool AssetTextureCache_Contains(const AssetTextureKey &key)
{
    return s_Entries.find(key) != s_Entries.end();
}

ImTextureRef
AssetTextureCache_AcquireLazy(const AssetTextureKey &key, const AssetTextureDecodeFn &decode)
{
    if (ImGui::GetCurrentContext() == nullptr || !decode)
    {
        return ImTextureRef();
    }

    ReapDestroyed();

    if (ImGui::GetFrameCount() != s_LastFrame)
    {
        s_LastFrame = ImGui::GetFrameCount();
        s_FrameStartStamp = s_AcquireCounter + 1;
    }

    const uint32_t stamp = ++s_AcquireCounter;
    auto it = s_Entries.find(key);
    if (it != s_Entries.end())
    {
        if (it->second.Tex == nullptr)
        {
            // cached miss: retry when stale (transient decode failures must
            // not stay sticky forever)
            if (ImGui::GetFrameCount() - it->second.MissFrame < ASSET_TEXTURE_MISS_RETRY_FRAMES)
            {
                return ImTextureRef();
            }
            s_Entries.erase(it);
        }
        else
        {
            it->second.LastUseTicks = stamp;
            return it->second.Tex->GetTexRef();
        }
    }

    // decode only on a cache miss
    const int w = key.Width;
    const int h = key.Height;
    std::vector<uint16_t> pixels16;
    if (w <= 0 || h <= 0 || w > ASSET_TEXTURE_MAX_DIM || h > ASSET_TEXTURE_MAX_DIM ||
        !decode(pixels16) || pixels16.size() != (size_t)w * h)
    {
        // cache the miss with a frame stamp so failures are not re-decoded
        // every frame (but are retried after a while)
        CacheEntry miss;
        miss.LastUseTicks = stamp;
        miss.MissFrame = ImGui::GetFrameCount();
        s_Entries.emplace(key, miss);
        return ImTextureRef();
    }

    EvictOverCap();

    ImTextureData *tex = new ImTextureData();
    tex->Create(ImTextureFormat_RGBA32, w, h);
    tex->SetStatus(ImTextureStatus_WantCreate);
    tex->UseColors = true;
    // the create upload carries the pixels: fill before registration and do
    // NOT downgrade the status afterwards (the backend asserts if a texture is
    // never created - see assmap.cpp)
    AssetViewer_Convert16ToRgba32(
        pixels16.data(), (uint32_t *)tex->GetPixels(), (uint32_t)((size_t)w * h), key.Hue);
    ImGui::RegisterUserTexture(tex);

    CacheEntry entry;
    entry.Tex = tex;
    entry.LastUseTicks = stamp;
    entry.Bytes = (uint32_t)w * h * 4;
    s_Bytes += entry.Bytes;
    s_Entries.emplace(key, entry);
    return tex->GetTexRef();
}

void AssetTextureCache_Shutdown()
{
    // called after the assistant shutdown: the ImGui context is gone and the
    // backend teardown walked every registered texture with RefCount == 1 to
    // release the GL names - the ImTextureData objects are plain CPU memory
    // now
    for (auto &kv : s_Entries)
    {
        delete kv.second.Tex;
    }
    s_Entries.clear();
    s_Bytes = 0;
    for (ImTextureData *tex : s_Retired)
    {
        delete tex;
    }
    s_Retired.clear();
    s_AcquireCounter = 0;
}

#endif // XUO_ASSISTANT
