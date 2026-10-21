// SPDX-License-Identifier: AGPL-3.0-or-later

#include "AssetGrid.h"
#include <string>

#if defined(XUO_ASSISTANT)

#include <algorithm>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/fs.h>
#include <common/logging/logging.h>

#include <xuocore/uodata.h> // g_Index, g_Data, g_FileManager


#include "../Managers/UOFileReader.h" // g_UOFileReader::GetArtPixels

// the stb implementation is namespaced so it does not collide with the other
// in-tree translation unit that instantiates it (ScreenshotBuilder.cpp)
namespace AssetViewerStb
{
#define STBIWDEF static inline
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <external/stb_image_write.h>
} // namespace AssetViewerStb
using namespace AssetViewerStb;

// global "Hide nodraw" toggle + export folder override (see AssetGrid.h)
static bool s_HideNodraw = false;
static char s_ExportDirOverride[ASSETVIEWER_EXPORT_DIR_MAX] = {};

// clip a status string (with an ellipsis) so it fits the given pixel width
static std::string EllipsizeToFit(const char *text, float width)
{
    if (text == nullptr || text[0] == '\0')
    {
        return std::string();
    }
    ImFont *font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    if (font == nullptr || font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text).x <= width)
    {
        return std::string(text);
    }
    const char *ellipsis = "...";
    const float ellipsisW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, ellipsis).x;
    const int bytes = (int)strlen(text);
    int len = bytes;
    while (len > 0)
    {
        // never split a UTF-8 sequence
        while (len > 0 && (text[len] & 0xC0) == 0x80)
        {
            len--;
        }
        if (font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text, text + len).x + ellipsisW <= width)
        {
            break;
        }
        len--;
    }
    return std::string(text, len) + ellipsis;
}

void AssetGrid_SetHideNodraw(bool hide)
{
    s_HideNodraw = hide;
}

bool AssetGrid_GetHideNodraw()
{
    return s_HideNodraw;
}

char *AssetGrid_ExportDirOverride()
{
    return s_ExportDirOverride;
}

// platform default: <pictures>/Screenshots
static std::string DefaultExportDir()
{
#if defined(_WIN32)
    const char *profile = getenv("USERPROFILE");
    return profile != nullptr && profile[0] != '\0' ?
               std::string(profile) + "\\Pictures\\Screenshots" :
               std::string("Pictures\\Screenshots");
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    return home != nullptr && home[0] != '\0' ? std::string(home) + "/Pictures/Screenshots" :
                                                std::string("Pictures/Screenshots");
#else
    const char *xdg = getenv("XDG_PICTURES_DIR");
    if (xdg != nullptr && xdg[0] == '/')
    {
        return std::string(xdg) + "/Screenshots";
    }
    std::string base;
    if (FILE *pipe = popen("xdg-user-dir PICTURES", "r"))
    {
        char buf[512];
        if (fgets(buf, sizeof(buf), pipe) != nullptr)
        {
            std::string line(buf);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            {
                line.pop_back();
            }
            if (!line.empty() && line[0] == '/')
            {
                base = line;
            }
        }
        pclose(pipe);
    }
    if (base.empty())
    {
        const char *home = getenv("HOME");
        if (home != nullptr && home[0] != '\0')
        {
            base = std::string(home) + "/Pictures";
        }
    }
    return base.empty() ? std::string("Screenshots") : base + "/Screenshots";
#endif
}

std::string AssetViewerExportDir()
{
    if (s_ExportDirOverride[0] != '\0')
    {
        const fs_path dir = fs_path_from(s_ExportDirOverride);
        fs_path_create(dir);
        return fs_path_str(dir);
    }
    static std::string cached; // one-shot resolve + create of the default
    if (cached.empty())
    {
        const fs_path dir = fs_path_from(DefaultExportDir());
        fs_path_create(dir);
        cached = fs_path_str(dir);
    }
    return cached;
}

namespace
{
// nearest-neighbor rescale of a 16-bit pixel buffer into a w x h window;
// the sprite is centered and aspect-preserving, the padding stays at the
// UO color-key (0 = transparent)
void Scale16ToSize(const uint16_t *src, int srcW, int srcH, uint16_t *dst, int dstW, int dstH)
{
    memset(dst, 0, sizeof(uint16_t) * dstW * dstH);
    if (srcW <= 0 || srcH <= 0)
    {
        return;
    }
    const int scale = std::max(1, std::min(dstW / srcW, dstH / srcH));
    const int drawW = std::min(dstW, srcW * scale);
    const int drawH = std::min(dstH, srcH * scale);
    const int offX = (dstW - drawW) / 2;
    const int offY = (dstH - drawH) / 2;
    for (int y = 0; y < drawH; y++)
    {
        const uint16_t *srow = &src[(y / scale) * srcW];
        uint16_t *drow = &dst[(y + offY) * dstW + offX];
        for (int x = 0; x < drawW; x++)
        {
            drow[x] = srow[x / scale];
        }
    }
}

bool MatchFilter(const AssetGridSource &src, uint16_t id, const std::string &lower)
{
    if (lower.empty())
    {
        return true;
    }
    // hex (0x-prefixed) or decimal id match
    if (strncmp(lower.c_str(), "0x", 2) == 0 ||
        lower.find_first_not_of("0123456789") == std::string::npos)
    {
        const uint32_t parsed = (uint32_t)strtoul(lower.c_str(), nullptr, 0);
        return parsed == (uint32_t)id;
    }
    const char *name = src.NameOf != nullptr ? src.NameOf(id) : "";
    if (name == nullptr || name[0] == '\0')
    {
        return false;
    }
    // case-insensitive substring match against the tiledata name
    char buf[21];
    strncpy(buf, name, 20);
    buf[20] = '\0';
    for (char *p = buf; *p != '\0'; ++p)
    {
        *p = (char)tolower(*p);
    }
    return strstr(buf, lower.c_str()) != nullptr;
}

void RebuildFiltered(const AssetGridSource &src, AssetGridState &state)
{
    std::string lower(state.Filter);
    for (char &c : lower)
    {
        c = (char)tolower(c);
    }
    state.Filtered.clear();
    for (uint32_t id = 0; id < src.Count; id++)
    {
        // hidden ids keep their identity (selection/export stay stable), they
        // are just not offered as browser cells while "Hide nodraw" is on
        if (s_HideNodraw && src.Nodraw != nullptr && src.Nodraw((uint16_t)id))
        {
            continue;
        }
        if (MatchFilter(src, (uint16_t)id, lower))
        {
            state.Filtered.push_back(id);
        }
    }
    state.AppliedFilter = state.Filter;
    // keep the selection only when it is still part of the filtered set
    state.Selected = -1;
    state.NavIndex = -1;
}

// Scales a decoded 16-bit sprite into a size x size buffer (the cache entry
// holds the RGBA upload).
void AcquireScale(const uint16_t *pixels, int w, int h, std::vector<uint16_t> &out, int size)
{
    out.assign((size_t)size * size, 0);
    Scale16ToSize(pixels, w, h, out.data(), size, size);
}

// Per-kind decoded dimension cache (id -> w,h): the status line, the
// "too large" check and the cache probes must not re-decode the MUL every
// frame just to learn a sprite's size. Failed decodes are cached as (0,0)
// with a frame stamp so transient failures are retried (every
// kStaleRetryFrames) instead of being sticky forever.
struct DimEntry
{
    int W = 0;
    int H = 0;
    int Frame = 0; // ImGui frame of the last (failed) decode attempt
};

struct DimCache
{
    std::map<uint16_t, DimEntry> Map;
};

DimCache &Dims(AssetTextureKind kind)
{
    static std::map<AssetTextureKind, DimCache> s_Dims;
    return s_Dims[kind];
}

const int kStaleRetryFrames = 30;

// Rate-limited (per id, 5s) log for transient grid decode failures
void LogDecodeFailure(AssetTextureKind kind, uint16_t id)
{
    static std::map<std::pair<uint8_t, uint16_t>, double> s_LastLog;
    const double now = ImGui::GetTime();
    const std::pair<uint8_t, uint16_t> key{ (uint8_t)kind, id };
    auto it = s_LastLog.find(key);
    if (it != s_LastLog.end() && now - it->second < 5.0)
    {
        return;
    }
    s_LastLog[key] = now;
    Warning(Assistant, "asset viewer: decode failed for kind %d id 0x%04X\n", (int)kind, id);
}

// True (with w/h) when the graphic has usable art; decodes at most once per
// id, with a stale-stamp retry for previously failed decodes.
bool DimOf(const AssetGridSource &src, uint16_t id, int &w, int &h)
{
    auto &map = Dims(src.Kind).Map;
    const int now = ImGui::GetFrameCount();
    auto it = map.find(id);
    if (it != map.end())
    {
        if (it->second.W > 0 && it->second.H > 0)
        {
            w = it->second.W;
            h = it->second.H;
            return true;
        }
        if (now - it->second.Frame < kStaleRetryFrames)
        {
            return false;
        }
        // stale failure: fall through to a retry
    }
    std::vector<uint16_t> pixels;
    bool ok = src.Decode != nullptr && src.Decode(id, pixels, w, h);
    map[id] = { ok ? w : 0, ok ? h : 0, now };
    if (!ok)
    {
        LogDecodeFailure(src.Kind, id);
    }
    return ok;
}

std::string ExportDir()
{
    return AssetViewerExportDir();
}

} // namespace

bool AssetGrid_DecodeStatic(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h)
{
    if ((uint32_t)id >= (uint32_t)MAX_STATIC_DATA_INDEX_COUNT)
    {
        return false;
    }
    CIndexObjectStatic &io = g_Index.m_Static[id];
    if (io.Address == 0 && io.UopBlock == nullptr)
    {
        return false;
    }
    short sw = 0;
    short sh = 0;
    pixels = g_UOFileReader.GetArtPixels(id, io, true, sw, sh);
    w = sw;
    h = sh;
    return !pixels.empty() && w > 0 && h > 0;
}

bool AssetGrid_DecodeLand(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h)
{
    if ((uint32_t)id >= (uint32_t)MAX_LAND_DATA_INDEX_COUNT)
    {
        return false;
    }
    CIndexObjectLand &io = g_Index.m_Land[id];
    if (io.Address == 0 && io.UopBlock == nullptr)
    {
        return false;
    }
    short sw = 0;
    short sh = 0;
    pixels = g_UOFileReader.GetArtPixels(id, io, false, sw, sh);
    w = sw;
    h = sh;
    return !pixels.empty() && w > 0 && h > 0;
}

std::string AssetGrid_ExportPng(
    const char *prefix, uint16_t id, const std::vector<uint16_t> &pixels, int w, int h)
{
    if (pixels.empty() || w <= 0 || h <= 0 || w > 2048 || h > 2048)
    {
        return std::string();
    }
    std::vector<uint32_t> rgba((size_t)w * h);
    AssetViewer_Convert16ToRgba32(pixels.data(), rgba.data(), (uint32_t)rgba.size(), 0);
    const std::string dir = ExportDir();
    char name[128];
    snprintf(name, sizeof(name), "%s_0x%04X.png", prefix != nullptr ? prefix : "asset", id);
    const fs_path path = fs_path_from(dir + "/" + name);
    if (stbi_write_png(fs_path_ascii(path), w, h, 4, rgba.data(), w * 4) == 0)
    {
        Warning(Assistant, "asset viewer: PNG export failed: %s\n", fs_path_ascii(path));
        return std::string();
    }
    Info(Assistant, "asset viewer: exported %s\n", fs_path_ascii(path));
    return fs_path_str(path);
}

std::string
AssetGrid_ExportPngFile(const char *relPath, const std::vector<uint16_t> &pixels, int w, int h)
{
    if (relPath == nullptr || relPath[0] == '\0' || pixels.empty() || w <= 0 || h <= 0 ||
        w > 2048 || h > 2048)
    {
        return std::string();
    }
    std::vector<uint32_t> rgba((size_t)w * h);
    AssetViewer_Convert16ToRgba32(pixels.data(), rgba.data(), (uint32_t)rgba.size(), 0);
    const fs_path path = fs_path_from(ExportDir() + "/" + relPath);
    fs_path_create(fs_directory(path));
    if (stbi_write_png(fs_path_ascii(path), w, h, 4, rgba.data(), w * 4) == 0)
    {
        Warning(Assistant, "asset viewer: PNG export failed: %s\n", fs_path_ascii(path));
        return std::string();
    }
    Info(Assistant, "asset viewer: exported %s\n", fs_path_ascii(path));
    return fs_path_str(path);
}

void AssetGrid_ExportFilteredPngs(
    const AssetGridSource &src, const AssetGridState &state, char *progress, size_t progressSize)
{
    const int cap = 4096;
    const int total = (int)state.Filtered.size();
    int written = 0;
    for (int i = 0; i < total && written < cap; i++)
    {
        const uint16_t id = (uint16_t)state.Filtered[i];
        if (src.Nodraw != nullptr && src.Nodraw(id))
        {
            continue;
        }
        std::vector<uint16_t> pixels;
        int w = 0;
        int h = 0;
        if (src.Decode == nullptr || !src.Decode(id, pixels, w, h))
        {
            continue;
        }
        if (!AssetGrid_ExportPng(src.ExportPrefix, id, pixels, w, h).empty())
        {
            written++;
        }
    }
    if (progress != nullptr && progressSize > 0)
    {
        snprintf(
            progress,
            progressSize,
            "Exported %d of %d filtered tiles (cap %d) to %s",
            written,
            total,
            cap,
            AssetViewerExportDir().c_str());
    }
}

// the full 32-bit tiledata word, bit by bit (shared with the Tiledata tab,
// which renders every bit as a read-only checkbox)
namespace
{
const struct
{
    uint64_t Bit;
    const char *Name;
} kFlags[] = {
    { 0x00000001, "Background" },  { 0x00000002, "Weapon" },    { 0x00000004, "Transparent" },
    { 0x00000008, "Translucent" }, { 0x00000010, "Wall" },      { 0x00000020, "Damaging" },
    { 0x00000040, "Impassable" },  { 0x00000080, "Wet" },       { 0x00000100, "Unknown1" },
    { 0x00000200, "Surface" },     { 0x00000400, "Bridge" },    { 0x00000800, "Generic" },
    { 0x00001000, "Window" },      { 0x00002000, "NoShoot" },   { 0x00004000, "ArticleA" },
    { 0x00008000, "ArticleAn" },   { 0x00010000, "Foliage" },   { 0x00020000, "PartialHue" },
    { 0x00040000, "Unknown2" },    { 0x00080000, "Unknown3" },  { 0x00100000, "NoDiagonal" },
    { 0x00200000, "Unknown4" },    { 0x00400000, "Unknown5" },  { 0x00800000, "Animated" },
    { 0x01000000, "Unknown6" },    { 0x02000000, "Unknown7" },  { 0x04000000, "NoShade" },
    { 0x08000000, "Armor" },       { 0x10000000, "Unknown8" },  { 0x20000000, "Unknown9" },
    { 0x40000000, "Unknown10" },   { 0x80000000, "Unknown11" },
};
} // namespace

int AssetGrid_FlagBits()
{
    return (int)(sizeof(kFlags) / sizeof(kFlags[0]));
}

const char *AssetGrid_FlagBitName(int bit)
{
    if (bit < 0 || bit >= AssetGrid_FlagBits())
    {
        return nullptr;
    }
    return kFlags[bit].Name;
}

void AssetGrid_DrawFlagNames(uint64_t flags)
{
    for (const auto &f : kFlags)
    {
        if ((flags & f.Bit) != 0)
        {
            ImGui::TextUnformatted(f.Name);
        }
    }
}

ImTextureRef AssetGrid_AcquireThumb(AssetTextureKind kind, uint16_t id, int size)
{
    typedef bool (*DecodeFn)(uint16_t, std::vector<uint16_t> &, int &, int &);
    const DecodeFn decode = (kind == AssetTextureKind::Static) ? AssetGrid_DecodeStatic :
                            (kind == AssetTextureKind::Land)   ? AssetGrid_DecodeLand :
                                                                 nullptr;
    if (decode == nullptr)
    {
        return ImTextureRef();
    }
    int w = 0;
    int h = 0;
    std::vector<uint16_t> pixels;
    bool havePixels = false;
    auto &map = Dims(kind).Map;
    const int now = ImGui::GetFrameCount();
    auto it = map.find(id);
    if (it != map.end() && it->second.W > 0)
    {
        w = it->second.W;
        h = it->second.H;
    }
    else if (it != map.end() && now - it->second.Frame < kStaleRetryFrames)
    {
        return ImTextureRef(); // recent failure: do not hammer the MUL
    }
    else
    {
        if (!decode(id, pixels, w, h))
        {
            map[id] = { 0, 0, now };
            LogDecodeFailure(kind, id);
            return ImTextureRef();
        }
        map[id] = { w, h, now };
        havePixels = true; // decoded once: reused for the dims and the upload
    }
    if (w <= 0 || h <= 0 || w > 2048 || h > 2048)
    {
        return ImTextureRef();
    }
    const AssetTextureKey key{ kind, id, 0, (uint16_t)size, (uint16_t)size };
    return AssetTextureCache_AcquireLazy(
        key,
        [pixels = std::move(pixels), havePixels, w, h, decode, kind, id, size](
            std::vector<uint16_t> &out)
        {
            if (havePixels)
            {
                AcquireScale(pixels.data(), w, h, out, size);
                return true;
            }
            std::vector<uint16_t> fresh;
            int dw = 0;
            int dh = 0;
            if (!decode(id, fresh, dw, dh))
            {
                LogDecodeFailure(kind, id);
                return false;
            }
            AcquireScale(fresh.data(), dw, dh, out, size);
            return true;
        });
}

void AssetGrid_DrawBrowser(
    const AssetGridSource &src, AssetGridState &state, uint16_t *outSelected, bool *outActivated)
{
    if (outSelected != nullptr)
    {
        *outSelected = 0;
    }
    if (outActivated != nullptr)
    {
        *outActivated = false;
    }

    // toolbar: row 1 = thumbnail size, filter, background; row 2 = exports + status
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderFloat("##ThumbSize", &state.ThumbSize, 32.0f, 256.0f, "size %.0f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint(
        "##Filter", "id (0x.. / dec) or name", state.Filter, sizeof(state.Filter));
    ImGui::SameLine();
    ImGui::ColorEdit4(
        "##Bg", &state.Background.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    ImGui::NewLine();
    if (ImGui::Button("Export selected"))
    {
        std::vector<uint16_t> pixels;
        int w = 0;
        int h = 0;
        if (state.Selected >= 0 && src.Decode != nullptr &&
            src.Decode((uint16_t)state.Selected, pixels, w, h))
        {
            const std::string path =
                AssetGrid_ExportPng(src.ExportPrefix, (uint16_t)state.Selected, pixels, w, h);
            snprintf(
                state.Progress,
                sizeof(state.Progress),
                "%s",
                path.empty() ? "Export failed" : path.c_str());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Export All (filtered)"))
    {
        AssetGrid_ExportFilteredPngs(src, state, state.Progress, sizeof(state.Progress));
    }
    if (state.Progress[0] != '\0')
    {
        // status on the same row as the buttons when it fits, own line otherwise
        if (ImGui::GetContentRegionAvail().x < 200.0f)
        {
            ImGui::NewLine();
        }
        else
        {
            ImGui::SameLine();
        }
        ImGui::TextDisabled(
            "%s", EllipsizeToFit(state.Progress, ImGui::GetContentRegionAvail().x).c_str());
    }

    if (strcmp(state.AppliedFilter.c_str(), state.Filter) != 0 ||
        state.AppliedHideNodraw != s_HideNodraw)
    {
        const int prevSelected = state.Selected;
        RebuildFiltered(src, state);
        state.AppliedHideNodraw = s_HideNodraw;
        // keep the selection across a hide-nodraw flip when it is still shown
        for (int i = 0; prevSelected >= 0 && i < (int)state.Filtered.size(); i++)
        {
            if ((int)state.Filtered[i] == prevSelected)
            {
                state.Selected = prevSelected;
                state.NavIndex = i;
                break;
            }
        }
    }

    const int count = (int)state.Filtered.size();
    if (count == 0)
    {
        ImGui::TextWrapped("No tiles match the current filter.");
        return;
    }

    // grid (virtual scroll: only the visible rows decode/upload textures);
    // the cells are positioned absolutely and hit-tested through one
    // invisible button spanning the visible grid area
    const float cell = state.ThumbSize + 8.0f;
    if (ImGui::BeginChild("##Grid", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
    {
        const float availW = ImGui::GetContentRegionAvail().x;
        const int cols = std::max(1, (int)(availW / cell));
        const int rows = (count + cols - 1) / cols;
        const float scrollY = ImGui::GetScrollY();
        const float availH = ImGui::GetWindowHeight();
        const float top = ImGui::GetCursorPosY();
        const int firstRow = std::max(0, std::min((int)((scrollY - top) / cell), rows - 1));
        const int lastRow = std::min(rows, firstRow + (int)(availH / cell) + 2);

        // one-shot cross-tab jump: scroll to the requested graphic id; if the
        // id is filtered out (or hidden by nodraw), clear the filter so the
        // jump lands on the next frame (ScrollToId stays armed: one-shot on
        // the successful landing)
        if (state.ScrollToId >= 0)
        {
            bool found = false;
            for (int i = 0; i < count; i++)
            {
                if ((int)state.Filtered[i] == state.ScrollToId)
                {
                    state.NavIndex = i;
                    ImGui::SetScrollFromPosY(top + (float)(i / cols) * cell, 0.3f);
                    state.ScrollToId = -1;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                state.Filter[0] = '\0';
                state.AppliedFilter = "\1"; // force the rebuild next frame
            }
        }

        // keyboard navigation over the drawlist quads (only while the browser
        // child window is focused; never auto-scrolls on mouse hover)
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
        {
            int nav = state.NavIndex;
            if (nav >= count)
            {
                nav = count - 1;
            }
            const int visibleRows = std::max(1, (int)(availH / cell));
            int moved = nav;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            {
                moved = nav < 0 ? 0 : std::max(0, nav - 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            {
                moved = nav < 0 ? 0 : std::min(count - 1, nav + 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                moved = nav < 0 ? 0 : std::max(0, nav - cols);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                moved = nav < 0 ? 0 : std::min(count - 1, nav + cols);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            {
                moved = nav < 0 ? 0 : std::max(0, nav - cols * visibleRows);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            {
                moved = nav < 0 ? count - 1 : std::min(count - 1, nav + cols * visibleRows);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Home))
            {
                moved = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_End))
            {
                moved = count - 1;
            }
            if (moved != nav)
            {
                state.NavIndex = moved;
                state.Selected = (int)state.Filtered[moved];
                const int selRow = moved / cols;
                const float cellTop = top + (float)selRow * cell;
                const float cellBottom = cellTop + cell;
                if (cellTop < scrollY)
                {
                    ImGui::SetScrollFromPosY(cellTop, 0.0f);
                }
                else if (cellBottom > scrollY + availH)
                {
                    ImGui::SetScrollFromPosY(cellBottom, 1.0f);
                }
            }
        }

        ImDrawList *dl = ImGui::GetWindowDrawList();
        const ImU32 bgCol = ImGui::GetColorU32(state.Background);
        const ImU32 selCol = ImGui::GetColorU32(ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
        const ImU32 placeholderCol = ImGui::GetColorU32(ImVec4(0.4f, 0.2f, 0.2f, 0.5f));
        const ImVec2 origin = ImGui::GetCursorScreenPos();

        // reserve the whole scroll range so the scrollbar reflects every row
        ImGui::Dummy(ImVec2(availW, (float)rows * cell));

        for (int row = firstRow; row < lastRow; row++)
        {
            for (int col = 0; col < cols; col++)
            {
                const int index = row * cols + col;
                if (index >= count)
                {
                    break;
                }
                const uint16_t id = (uint16_t)state.Filtered[index];
                const ImVec2 tl(origin.x + (float)col * cell, origin.y + (float)row * cell);
                const ImVec2 br(tl.x + state.ThumbSize, tl.y + state.ThumbSize);
                dl->AddRectFilled(tl, br, bgCol);

                const bool nodraw = src.Nodraw != nullptr && src.Nodraw(id);
                bool tooLarge = false;
                if (!nodraw)
                {
                    int w = 0;
                    int h = 0;
                    if (DimOf(src, id, w, h))
                    {
                        if (w > 2048 || h > 2048)
                        {
                            // cache rejection path: oversized buffers never
                            // become textures
                            tooLarge = true;
                        }
                        else
                        {
                            // decode-on-miss: pixels are only produced when
                            // the texture is not cached yet
                            const AssetTextureKey key{ src.Kind,
                                                       id,
                                                       0,
                                                       (uint16_t)state.ThumbSize,
                                                       (uint16_t)state.ThumbSize };
                            const ImTextureRef ref = AssetTextureCache_AcquireLazy(
                                key,
                                [&src, id, &state](std::vector<uint16_t> &out)
                                {
                                    std::vector<uint16_t> pixels;
                                    int dw = 0;
                                    int dh = 0;
                                    if (src.Decode == nullptr || !src.Decode(id, pixels, dw, dh))
                                    {
                                        LogDecodeFailure(src.Kind, id);
                                        return false;
                                    }
                                    AcquireScale(pixels.data(), dw, dh, out, (int)state.ThumbSize);
                                    return true;
                                });
                            if (AssetTextureCache_IsReady(ref))
                            {
                                dl->AddImage(ref, tl, br);
                            }
                        }
                    }
                    if (tooLarge)
                    {
                        dl->AddRectFilled(tl, br, placeholderCol);
                        dl->AddText(tl, ImGui::GetColorU32(ImGuiCol_Text), "too large");
                    }
                }
                else
                {
                    dl->AddRectFilled(tl, br, placeholderCol);
                    dl->AddText(tl, ImGui::GetColorU32(ImGuiCol_Text), "nodraw");
                }

                if ((uint16_t)state.Selected == id)
                {
                    dl->AddRect(tl, br, selCol, 0.0f, 0, 2.0f);
                }

                const ImVec2 mouse = ImGui::GetIO().MousePos;
                if (ImGui::IsWindowHovered() && mouse.x >= tl.x && mouse.x < br.x &&
                    mouse.y >= tl.y && mouse.y < br.y)
                {
                    ImGui::SetTooltip(
                        "0x%04X\n%s", id, src.NameOf != nullptr ? src.NameOf(id) : "");
                    if (ImGui::IsMouseClicked(0))
                    {
                        state.Selected = id;
                        state.NavIndex = index;
                    }
                    if (ImGui::IsMouseDoubleClicked(0))
                    {
                        state.Selected = id;
                        state.NavIndex = index;
                        if (outActivated != nullptr)
                        {
                            *outActivated = true;
                        }
                    }
                }
            }
        }

        // keyboard: Enter opens the detail pane for the current selection
        if (ImGui::IsWindowFocused() && state.Selected >= 0 &&
            (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
             ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
        {
            if (outActivated != nullptr)
            {
                *outActivated = true;
            }
        }
    }
    ImGui::EndChild();

    // status line for the current selection
    if (outSelected != nullptr && state.Selected >= 0)
    {
        *outSelected = (uint16_t)state.Selected;
    }
    if (state.Selected >= 0)
    {
        int w = 0;
        int h = 0;
        const bool have = DimOf(src, (uint16_t)state.Selected, w, h);
        ImGui::Text(
            "Graphic: 0x%04X, Name: %s, %dx%d%s",
            (uint16_t)state.Selected,
            src.NameOf != nullptr ? src.NameOf((uint16_t)state.Selected) : "",
            have ? w : 0,
            have ? h : 0,
            src.Nodraw != nullptr && src.Nodraw((uint16_t)state.Selected) ? " (nodraw)" : "");
    }
    else
    {
        ImGui::TextDisabled("%d tiles (click a tile to select)", count);
    }
}

#endif // XUO_ASSISTANT
