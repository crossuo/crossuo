// SPDX-License-Identifier: AGPL-3.0-or-later

#include "AssetViewer.h"

#if defined(XUO_ASSISTANT)

#include <stdio.h>

#include <string.h>

#include <ctype.h>
#include <float.h>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <common/fs.h>
#include <common/logging/logging.h>
#include <common/str.h>

#include <external/gfx/imgui/imgui.h>

#include <xuocore/mappedfile.h> // CDataReader (UOP multi block parsing)
#include <xuocore/mulstruct.h>  // MULTI_BLOCK / MULTI_BLOCK_NEW
#include <xuocore/uodata.h>     // g_Index, g_Data, g_FileManager

#include "../Config.h"                 // g_Config (client version gate)
#include "../Managers/ColorManager.h"  // g_ColorManager (radar color chip)
#include "../Managers/ConfigManager.h" // g_ConfigManager (sound/music enables)
#include "../Managers/SoundManager.h"  // g_SoundManager (preview playback)
#include "../Managers/UOFileReader.h"  // g_UOFileReader.GetGumpPixels
#include "AssetDecode.h"
#include "AssetGrid.h"
#include "AssetTextureCache.h"

namespace
{
bool s_Visible = false;
bool s_Shutdown = false;

// clip a status string (with an ellipsis) so it fits the given pixel width
std::string EllipsizeToFit(const char *text, float width)
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
    int len = (int)strlen(text);
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

// cross-tab jump requests: the detail buttons/Enter key set these, the shell
// tab bar consumes them on the next frame (ImGuiTabItemFlags_SetSelected for
// exactly one frame); -1 = nothing pending
enum PendingTab
{
    PendingNone = -1,
    PendingStatics = 0,
    PendingLand,
    PendingTiledata
};
int s_PendingTab = PendingNone;
uint32_t s_PendingSelect = 0;  // graphic id for Statics/Land jumps
int s_PendingTiledataSub = -1; // 0 = Items, 1 = Land sub-tab
uint32_t s_PendingTiledataId = 0;

void RequestStaticsShow(uint32_t id)
{
    s_PendingTab = PendingStatics;
    s_PendingSelect = id;
}

void RequestLandShow(uint32_t id)
{
    s_PendingTab = PendingLand;
    s_PendingSelect = id;
}

void RequestTiledataShow(uint32_t id, int subTab)
{
    s_PendingTab = PendingTiledata;
    s_PendingTiledataSub = subTab;
    s_PendingTiledataId = id;
}

// the viewer needs the asset indexes; it must not crash when toggled before
// the client loaded the UO data (login screen / missing data path)
bool UoDataAvailable()
{
    // the file manager Load()/LoadHues() products: non-empty once a data
    // path was loaded (works from the login screen, MUL and UOP alike)
    return !g_FileManager.m_BlockData[0].empty() || !g_FileManager.m_Hues.empty();
}

} // namespace

namespace
{
// nearest-neighbor rescale to an exact target size (used by the detail
// preview zoom, which is not limited to integer factors)
void Scale16Exact(const uint16_t *src, int srcW, int srcH, uint16_t *dst, int dstW, int dstH)
{
    for (int y = 0; y < dstH; y++)
    {
        const uint16_t *srow = &src[(std::min(y * srcH / dstH, srcH - 1)) * srcW];
        uint16_t *drow = &dst[y * dstW];
        for (int x = 0; x < dstW; x++)
        {
            drow[x] = srow[std::min(x * srcW / dstW, srcW - 1)];
        }
    }
}

// tiledata names are 20 non-terminated bytes: copy into a scratch buffer
// (single-threaded UI, one buffer per tab is enough)
const char *StaticTileName(uint16_t id)
{
    static char buf[21] = {};
    if (id >= g_Data.m_Static.size())
    {
        return "";
    }
    strncpy(buf, g_Data.m_Static[id].Name, 20);
    buf[20] = '\0';
    return buf;
}

const char *LandTileName(uint16_t id)
{
    static char buf[21] = {};
    if (id >= g_Data.m_Land.size())
    {
        return "";
    }
    strncpy(buf, g_Data.m_Land[id].Name, 20);
    buf[20] = '\0';
    return buf;
}

bool StaticNodraw(uint16_t id)
{
    const CIndexObjectStatic &io = g_Index.m_Static[id];
    return io.Address == 0 && io.UopBlock == nullptr;
}

bool LandNodraw(uint16_t id)
{
    const CIndexObjectLand &io = g_Index.m_Land[id];
    return io.Address == 0 && io.UopBlock == nullptr;
}

// hue combo over the loaded hue entries (1-based game ids, 0 = none); the
// valid entry count is m_Hues.size() (one HUES_GROUP per 8 entries is
// initialized by LoadHues, the rest of the vector stays unused)
void DrawHueSelector(AssetGridState &state)
{
    const int entries = (int)g_FileManager.m_Hues.size();
    if (state.Hue >= entries)
    {
        state.Hue = 0;
    }
    char preview[48];
    if (state.Hue == 0)
    {
        snprintf(preview, sizeof(preview), "None");
    }
    else
    {
        snprintf(
            preview,
            sizeof(preview),
            "%u: %.20s",
            state.Hue,
            g_FileManager.m_Hues[(state.Hue - 1) / 8].Entries[(state.Hue - 1) % 8].Name);
    }
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("Hue", preview))
    {
        if (ImGui::Selectable("None", state.Hue == 0))
        {
            state.Hue = 0;
        }
        for (int e = 1; e <= entries; e++)
        {
            char label[48];
            snprintf(
                label,
                sizeof(label),
                "%d: %.20s",
                e,
                g_FileManager.m_Hues[(e - 1) / 8].Entries[(e - 1) % 8].Name);
            if (ImGui::Selectable(label, state.Hue == (uint16_t)e))
            {
                state.Hue = (uint16_t)e;
            }
        }
        ImGui::EndCombo();
    }
}

// enlarged, zoomable, hueable preview from the shared texture cache; shows
// a placeholder when the art is missing or beyond the cache limits
void DrawPreview(
    AssetTextureKind kind,
    uint16_t id,
    const std::vector<uint16_t> &pixels,
    int w,
    int h,
    uint16_t hue,
    float zoom)
{
    if (w > 2048 || h > 2048)
    {
        ImGui::TextDisabled("Preview too large (%dx%d)", w, h);
        return;
    }
    const int dstW = std::max(1, std::min(1024, (int)(w * zoom)));
    const int dstH = std::max(1, std::min(1024, (int)(h * zoom)));
    std::vector<uint16_t> scaled((size_t)dstW * dstH, 0);
    Scale16Exact(pixels.data(), w, h, scaled.data(), dstW, dstH);
    AssetTextureKey key{ kind, id, hue, (uint16_t)dstW, (uint16_t)dstH };
    const ImTextureRef ref = AssetTextureCache_Acquire(key, scaled.data(), dstW, dstH, hue);
    if (!AssetTextureCache_IsReady(ref))
    {
        ImGui::TextDisabled("Preview unavailable");
        return;
    }
    ImGui::Image(ref, ImVec2((float)dstW, (float)dstH));
}

void DrawStaticsDetail(uint16_t id, AssetGridState &state)
{
    ImGui::SeparatorText("Detail");
    ImGui::Text("Id: 0x%04X (%u)", id, id);
    DrawHueSelector(state);
    ImGui::SliderFloat("##Zoom", &state.DetailZoom, 1.0f, 8.0f, "zoom %.0fx");

    std::vector<uint16_t> pixels;
    int w = 0;
    int h = 0;
    if (!AssetGrid_DecodeStatic(id, pixels, w, h))
    {
        ImGui::TextDisabled("nodraw / no art for 0x%04X", id);
    }
    else
    {
        DrawPreview(AssetTextureKind::Static, id, pixels, w, h, state.Hue, state.DetailZoom);
        ImGui::Text("Size: %dx%d", w, h);
    }

    if (id >= g_Data.m_Static.size())
    {
        ImGui::TextDisabled("No tiledata entry");
        return;
    }
    const MulStaticTile2 &tile = g_Data.m_Static[id];
    ImGui::SeparatorText("Tiledata");
    if (ImGui::Button("Show in Tiledata"))
    {
        RequestTiledataShow(id, 0);
    }
    ImGui::Text("Name: %s", StaticTileName(id));
    ImGui::Text("Flags: 0x%08llX", (unsigned long long)tile.Flags);
    AssetGrid_DrawFlagNames(tile.Flags);
    ImGui::Text("Weight: %u", tile.Weight);
    ImGui::Text("Layer: %u", tile.Layer);
    ImGui::Text("Quantity: %u", tile.Count);
    ImGui::Text("Animation: 0x%04X", tile.AnimID);
    ImGui::Text("Hue: %u", tile.Hue);
    ImGui::Text("LightIndex: %u", tile.LightIndex);
    ImGui::Text("Height: %u", tile.Height);
}

void DrawLandDetail(uint16_t id)
{
    ImGui::SeparatorText("Detail");

    std::vector<uint16_t> pixels;
    int w = 0;
    int h = 0;
    if (!AssetGrid_DecodeLand(id, pixels, w, h))
    {
        ImGui::TextDisabled("nodraw / no art for 0x%04X", id);
    }
    else
    {
        DrawPreview(AssetTextureKind::Land, id, pixels, w, h, 0, 4.0f);
        ImGui::Text("Size: %dx%d", w, h);
    }

    if (id >= g_Data.m_Land.size())
    {
        ImGui::TextDisabled("No tiledata entry");
        return;
    }
    const MulLandTile2 &tile = g_Data.m_Land[id];
    ImGui::SeparatorText("Tiledata");
    if (ImGui::Button("Show in Tiledata"))
    {
        RequestTiledataShow(id, 1);
    }
    ImGui::Text("Name: %s", LandTileName(id));
    ImGui::Text("TexID: 0x%04X", tile.TexID);
    ImGui::Text("Flags: 0x%08llX", (unsigned long long)tile.Flags);
    AssetGrid_DrawFlagNames(tile.Flags);

    // radar color chip (radarcol.mul through the color manager)
    const uint32_t rgb = g_ColorManager.Color16To32(g_ColorManager.GetRadarColorData(id));
    const ImVec4 color(
        (float)(rgb & 0xFF) / 255.0f,
        (float)((rgb >> 8) & 0xFF) / 255.0f,
        (float)((rgb >> 16) & 0xFF) / 255.0f,
        1.0f);
    ImGui::ColorButton(
        "##RadarColor",
        color,
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
        ImVec2(48.0f, 24.0f));
    ImGui::SameLine();
    ImGui::Text(
        "Radar: 0x%04X (%u, %u, %u)",
        g_ColorManager.GetRadarColorData(id),
        (unsigned)(rgb & 0xFF),
        (unsigned)((rgb >> 8) & 0xFF),
        (unsigned)((rgb >> 16) & 0xFF));
}

AssetGridState &StaticsState()
{
    static AssetGridState state;
    return state;
}

AssetGridState &LandState()
{
    static AssetGridState state;
    return state;
}

// Gumps tab

bool GumpNodraw(uint16_t id)
{
    const CIndexGump &io = g_Index.m_Gump[id];
    return io.Address == 0 && io.UopBlock == nullptr;
}

bool GumpDecode(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h)
{
    if ((uint32_t)id >= (uint32_t)MAX_GUMP_DATA_INDEX_COUNT)
    {
        return false;
    }
    CIndexGump &io = g_Index.m_Gump[id];
    pixels = g_UOFileReader.GetGumpPixels(io);
    // GetGumpPixels learns/caches the layout on the entry: read the
    // dimensions back from it instead of trusting prior state
    w = io.Width;
    h = io.Height;
    return !pixels.empty() && w > 0 && h > 0;
}

AssetGridState &GumpsState()
{
    static AssetGridState state;
    return state;
}

// shared detail pane body for the hueable tabs (gumps, texmaps): id, size,
// hue selector, zoomable preview and a single-tile PNG export
// draggable vertical splitter between browser and detail panes; returns the
// detail pane width clamped to [220, availW - 220]
float DrawDetailSplitter(const char *id, float &detailW)
{
    const float availW = ImGui::GetContentRegionAvail().x;
    detailW = std::max(220.0f, std::min(availW - 220.0f, detailW));
    ImGui::SameLine();
    ImGui::InvisibleButton(id, ImVec2(4.0f, ImGui::GetContentRegionAvail().y));
    if (ImGui::IsItemActive())
    {
        detailW =
            std::max(220.0f, std::min(availW - 220.0f, detailW - ImGui::GetIO().MouseDelta.x));
    }
    ImGui::SameLine();
    return detailW;
}

void DrawHuedDetail(
    const char *prefix,
    AssetTextureKind kind,
    uint16_t id,
    AssetGridState &state,
    bool (*decode)(uint16_t, std::vector<uint16_t> &, int &, int &))
{
    ImGui::SeparatorText("Detail");
    DrawHueSelector(state);
    // zoom <= 0 means "Fit": scale the preview to the detail pane; a positive
    // zoom is used as-is (scrollbars appear when it overflows)
    static bool fit = true; // per-session default: Fit
    ImGui::Checkbox("Fit", &fit);
    ImGui::SameLine();
    if (!fit)
    {
        if (state.DetailZoom <= 0.0f)
        {
            state.DetailZoom = 1.0f;
        }
        ImGui::SliderFloat(
            "##Zoom", &state.DetailZoom, 0.25f, 8.0f, "zoom %.2fx", ImGuiSliderFlags_Logarithmic);
    }
    else
    {
        state.DetailZoom = 0.0f;
        ImGui::TextDisabled("zoom: fit");
    }

    std::vector<uint16_t> pixels;
    int w = 0;
    int h = 0;
    if (!decode(id, pixels, w, h))
    {
        ImGui::TextDisabled("nodraw / no art for 0x%04X", id);
        return;
    }
    ImGui::Text("Size: %dx%d", w, h);
    float zoom = state.DetailZoom;
    if (fit)
    {
        const float availW = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float availH =
            std::max(1.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing());
        zoom = std::min(availW / (float)w, availH / (float)h);
    }
    DrawPreview(kind, id, pixels, w, h, state.Hue, zoom);
    if (ImGui::Button("Export PNG"))
    {
        const std::string path = AssetGrid_ExportPng(prefix, id, pixels, w, h);
        ImGui::TextDisabled("%s", path.empty() ? "Export failed" : path.c_str());
    }
}

void DrawGumpsTab()
{
    AssetGridSource src;
    src.Kind = AssetTextureKind::Gump;
    src.Count = MAX_GUMP_DATA_INDEX_COUNT;
    src.Decode = GumpDecode;
    src.NameOf = [](uint16_t) { return ""; };
    src.Nodraw = GumpNodraw;
    src.ExportPrefix = "gump";

    AssetGridState &state = GumpsState();
    uint16_t selected = 0;
    bool activated = false;
    static float detailW = 380.0f; // splitter position, persists per session
    const float availW = ImGui::GetContentRegionAvail().x;
    detailW = std::max(220.0f, std::min(availW - 220.0f, detailW));
    const float browserW = std::max(200.0f, availW - detailW - 4.0f);
    ImGui::BeginChild("##gumpsBrowser", ImVec2(browserW, 0.0f), ImGuiChildFlags_None);
    AssetGrid_DrawBrowser(src, state, &selected, &activated);
    ImGui::EndChild();
    DrawDetailSplitter("##gumpsSplitter", detailW);
    ImGui::BeginChild("##gumpsDetail", ImVec2(detailW, 0.0f), ImGuiChildFlags_None);
    if (state.Selected >= 0)
    {
        DrawHuedDetail("gump", AssetTextureKind::Gump, (uint16_t)state.Selected, state, GumpDecode);
    }
    else
    {
        ImGui::TextDisabled("Select a gump to inspect it");
    }
    ImGui::EndChild();
    (void)selected;
    (void)activated;
}

// Effects tab: Lights + Texmaps sections

bool LightNodraw(uint16_t id)
{
    const CIndexLight &io = g_Index.m_Light[id];
    return io.Address == 0 && io.UopBlock == nullptr;
}

bool TexmapNodraw(uint16_t id)
{
    const CIndexObject &io = g_Index.m_Texture[id];
    return io.Address == 0 && io.UopBlock == nullptr;
}

AssetGridState &LightsState()
{
    static AssetGridState state;
    return state;
}

AssetGridState &TexmapsState()
{
    static AssetGridState state;
    return state;
}

// additive lightmaps carry no hue: keep the state hue pinned to none
void DrawLightsDetail(uint16_t id, AssetGridState &state)
{
    ImGui::SeparatorText("Detail");
    state.Hue = 0;
    ImGui::SliderFloat("##Zoom", &state.DetailZoom, 1.0f, 8.0f, "zoom %.0fx");
    ImGui::TextDisabled("Additive lightmap (no hue)");

    std::vector<uint16_t> pixels;
    int w = 0;
    int h = 0;
    if (!AssetViewer_DecodeLight(id, pixels, w, h))
    {
        ImGui::TextDisabled("nodraw / no light for 0x%04X", id);
        return;
    }
    ImGui::Text("Size: %dx%d", w, h);
    DrawPreview(AssetTextureKind::Light, id, pixels, w, h, 0, state.DetailZoom);
    if (ImGui::Button("Export PNG"))
    {
        const std::string path = AssetGrid_ExportPng("light", id, pixels, w, h);
        ImGui::TextDisabled("%s", path.empty() ? "Export failed" : path.c_str());
    }
}

void DrawTexmapsDetail(uint16_t id, AssetGridState &state)
{
    DrawHuedDetail("texmap", AssetTextureKind::Texmap, id, state, AssetViewer_DecodeTexture);
}

void DrawEffectsSection(
    const char *childBase,
    AssetGridSource &src,
    AssetGridState &state,
    void (*drawDetail)(uint16_t, AssetGridState &))
{
    uint16_t selected = 0;
    bool activated = false;
    static float detailW = 380.0f; // splitter position, persists per session
    const float availW = ImGui::GetContentRegionAvail().x;
    detailW = std::max(220.0f, std::min(availW - 220.0f, detailW));
    const float browserW = std::max(200.0f, availW - detailW - 4.0f);
    char label[64];
    snprintf(label, sizeof(label), "##%sBrowser", childBase);
    ImGui::BeginChild(label, ImVec2(browserW, 0.0f), ImGuiChildFlags_None);
    AssetGrid_DrawBrowser(src, state, &selected, &activated);
    ImGui::EndChild();
    snprintf(label, sizeof(label), "##%sSplitter", childBase);
    DrawDetailSplitter(label, detailW);
    snprintf(label, sizeof(label), "##%sDetail", childBase);
    ImGui::BeginChild(label, ImVec2(detailW, 0.0f), ImGuiChildFlags_None);
    if (state.Selected >= 0)
    {
        drawDetail((uint16_t)state.Selected, state);
    }
    else
    {
        ImGui::TextDisabled("Select an entry to inspect it");
    }
    ImGui::EndChild();
    (void)selected;
    (void)activated;
}

void DrawEffectsTab()
{
    if (ImGui::BeginTabBar("##EffectsSections", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Lights"))
        {
            AssetGridSource src;
            src.Kind = AssetTextureKind::Light;
            src.Count = MAX_LIGHTS_DATA_INDEX_COUNT;
            src.Decode = AssetViewer_DecodeLight;
            src.NameOf = [](uint16_t) { return ""; };
            src.Nodraw = LightNodraw;
            src.ExportPrefix = "light";
            DrawEffectsSection("lights", src, LightsState(), DrawLightsDetail);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Texmaps"))
        {
            AssetGridSource src;
            src.Kind = AssetTextureKind::Texmap;
            src.Count = MAX_LAND_TEXTURES_DATA_INDEX_COUNT;
            src.Decode = AssetViewer_DecodeTexture;
            src.NameOf = [](uint16_t) { return ""; };
            src.Nodraw = TexmapNodraw;
            src.ExportPrefix = "texmap";
            DrawEffectsSection("texmaps", src, TexmapsState(), DrawTexmapsDetail);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void DrawStaticsTab()
{
    AssetGridSource src;
    src.Kind = AssetTextureKind::Static;
    src.Count = 0x4000;
    src.Decode = AssetGrid_DecodeStatic;
    src.NameOf = StaticTileName;
    src.Nodraw = StaticNodraw;
    src.ExportPrefix = "static";

    AssetGridState &state = StaticsState();
    if (s_PendingTab == PendingStatics)
    {
        state.Selected = (int)s_PendingSelect;
        state.NavIndex = -1;
        state.ScrollToId = (int)s_PendingSelect;
        s_PendingTab = PendingNone;
    }
    uint16_t selected = 0;
    bool activated = false;
    const float detailW = 360.0f;
    const float browserW = std::max(200.0f, ImGui::GetContentRegionAvail().x - detailW);
    ImGui::BeginChild("##staticsBrowser", ImVec2(browserW, 0.0f), ImGuiChildFlags_None);
    AssetGrid_DrawBrowser(src, state, &selected, &activated);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##staticsDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (state.Selected >= 0)
    {
        DrawStaticsDetail((uint16_t)state.Selected, state);
    }
    else
    {
        ImGui::TextDisabled("Select a tile to inspect it");
    }
    ImGui::EndChild();
    (void)selected;
    (void)activated;
}

void DrawLandTab()
{
    AssetGridSource src;
    src.Kind = AssetTextureKind::Land;
    src.Count = MAX_LAND_DATA_INDEX_COUNT;
    src.Decode = AssetGrid_DecodeLand;
    src.NameOf = LandTileName;
    src.Nodraw = LandNodraw;
    src.ExportPrefix = "land";

    AssetGridState &state = LandState();
    if (s_PendingTab == PendingLand)
    {
        state.Selected = (int)s_PendingSelect;
        state.NavIndex = -1;
        state.ScrollToId = (int)s_PendingSelect;
        s_PendingTab = PendingNone;
    }
    uint16_t selected = 0;
    bool activated = false;
    const float detailW = 360.0f;
    const float browserW = std::max(200.0f, ImGui::GetContentRegionAvail().x - detailW);
    ImGui::BeginChild("##landBrowser", ImVec2(browserW, 0.0f), ImGuiChildFlags_None);
    AssetGrid_DrawBrowser(src, state, &selected, &activated);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##landDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (state.Selected >= 0)
    {
        DrawLandDetail((uint16_t)state.Selected);
    }
    else
    {
        ImGui::TextDisabled("Select a tile to inspect it");
    }
    ImGui::EndChild();
    (void)selected;
    (void)activated;
}

// ---------------------------------------------------------------------------
// Tiledata tab
// ---------------------------------------------------------------------------

// per-sub-tab state (Items / Land): filter, filtered id list and selection;
// the table itself reads g_Data.m_Static / g_Data.m_Land directly (clipper,
// no per-frame copies of the vectors)
struct TiledataTabState
{
    char Filter[64] = {};
    std::vector<uint32_t> Filtered;
    std::string AppliedFilter = "\1"; // forces the first rebuild
    int Selected = -1;
    // one-shot cross-tab jump: show this id (clearing the filter) and scroll
    // its row into view; consumed by the next DrawSubTab frame
    int ExternalSelect = -1;
    char Progress[192] = {};
};

TiledataTabState &TiledataItemsState()
{
    static TiledataTabState state;
    return state;
}

TiledataTabState &TiledataLandState()
{
    static TiledataTabState state;
    return state;
}

// case-insensitive id/name filter over the tiledata entries themselves
bool TiledataMatch(uint32_t id, const char *name, const std::string &lower)
{
    if (lower.empty())
    {
        return true;
    }
    if (strncmp(lower.c_str(), "0x", 2) == 0 ||
        lower.find_first_not_of("0123456789") == std::string::npos)
    {
        const uint32_t parsed = (uint32_t)strtoul(lower.c_str(), nullptr, 0);
        return parsed == id;
    }
    if (name == nullptr || name[0] == '\0')
    {
        return false;
    }
    char buf[21];
    strncpy(buf, name, 20);
    buf[20] = '\0';
    for (char *p = buf; *p != '\0'; ++p)
    {
        *p = (char)tolower(*p);
    }
    return strstr(buf, lower.c_str()) != nullptr;
}

void TiledataRebuildFiltered(
    TiledataTabState &state, uint32_t count, const std::function<const char *(uint32_t)> &nameOf)
{
    std::string lower(state.Filter);
    for (char &c : lower)
    {
        c = (char)tolower(c);
    }
    state.Filtered.clear();
    for (uint32_t id = 0; id < count; id++)
    {
        if (TiledataMatch(id, nameOf(id), lower))
        {
            state.Filtered.push_back(id);
        }
    }
    state.AppliedFilter = state.Filter;
    state.Selected = -1;
}

// read-only checkbox matrix over the named flag word (bits 32+ are reported
// in the hex line; the shipped client data only uses the low 32 bits)
void TiledataDrawFlagCheckboxes(uint64_t flags)
{
    ImGui::Text("Flags: 0x%08llX", (unsigned long long)flags);
    if ((flags >> 32) != 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(high word: 0x%08llX)", (unsigned long long)(flags >> 32));
    }
    ImGui::BeginDisabled(true);
    const int bits = AssetGrid_FlagBits();
    const int cols = 2;
    const int rows = (bits + cols - 1) / cols;
    if (ImGui::BeginTable("##flags", cols, ImGuiTableFlags_SizingStretchSame))
    {
        for (int row = 0; row < rows; row++)
        {
            ImGui::TableNextRow();
            for (int col = 0; col < cols; col++)
            {
                ImGui::TableNextColumn();
                const int bit = col * rows + row;
                if (bit >= bits)
                {
                    continue;
                }
                bool value = ((flags >> bit) & 1) != 0;
                ImGui::Checkbox(AssetGrid_FlagBitName(bit), &value);
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndDisabled();
}

bool TiledataExportCsv(const char *fileName, const std::string &content)
{
    const fs_path dir = fs_path_from(AssetViewerExportDir());
    const fs_path path = fs_path_from(fs_path_str(dir) + "/" + fileName);
    FILE *file = fopen(fs_path_ascii(path), "wb");
    if (file == nullptr)
    {
        Warning(Assistant, "asset viewer: CSV export failed: %s\n", fs_path_ascii(path));
        return false;
    }
    fwrite(content.data(), 1, content.size(), file);
    fclose(file);
    Info(Assistant, "asset viewer: exported %s\n", fs_path_ascii(path));
    return true;
}

// quoted CSV field: doubles embedded quotes, keeps the (possibly garbage)
// tiledata name bytes as-is otherwise
void TiledataCsvField(std::string &out, const char *name)
{
    out += '"';
    for (const char *p = name; p != nullptr && *p != '\0'; p++)
    {
        if (*p == '"')
        {
            out += '"';
        }
        out += *p;
    }
    out += '"';
}

// per-kind field columns and read-only detail panes
void TiledataItemColumns(uint32_t id)
{
    if (id >= g_Data.m_Static.size())
    {
        return;
    }
    const MulStaticTile2 &tile = g_Data.m_Static[id];
    ImGui::Text(
        "W %u | L %u | Q %u | Anim 0x%04X | Hue %d | LI %u | H %u",
        tile.Weight,
        tile.Layer,
        tile.Count,
        (uint16_t)tile.AnimID,
        (int)tile.Hue,
        tile.LightIndex,
        tile.Height);
}

void TiledataLandColumns(uint32_t id)
{
    if (id >= g_Data.m_Land.size())
    {
        return;
    }
    ImGui::Text("TexID 0x%04X", g_Data.m_Land[id].TexID);
}

void TiledataItemDetail(uint32_t id)
{
    if (id >= g_Data.m_Static.size())
    {
        ImGui::TextDisabled("No tiledata entry");
        return;
    }
    const MulStaticTile2 &tile = g_Data.m_Static[id];
    ImGui::Text("Name: %.20s", tile.Name);
    ImGui::Text("Weight: %u", tile.Weight);
    ImGui::Text("Layer: %u", tile.Layer);
    ImGui::Text("Quantity: %u", tile.Count);
    ImGui::Text("Animation: 0x%04X", (uint16_t)tile.AnimID);
    ImGui::Text("Hue: %d", (int)tile.Hue);
    ImGui::Text("LightIndex: %u", tile.LightIndex);
    ImGui::Text("Height: %u", tile.Height);
    if (ImGui::Button("View in Statics"))
    {
        RequestStaticsShow(id);
    }
    TiledataDrawFlagCheckboxes(tile.Flags);
}

void TiledataLandDetail(uint32_t id)
{
    if (id >= g_Data.m_Land.size())
    {
        ImGui::TextDisabled("No tiledata entry");
        return;
    }
    const MulLandTile2 &tile = g_Data.m_Land[id];
    ImGui::Text("Name: %.20s", tile.Name);
    ImGui::Text("TexID: 0x%04X", tile.TexID);
    if (ImGui::Button("View in Land"))
    {
        RequestLandShow(id);
    }
    TiledataDrawFlagCheckboxes(tile.Flags);
}

// full flag word of a tiledata entry (0 when the id is out of range)
uint64_t TileFlagsOf(uint32_t id)
{
    if (id < g_Data.m_Static.size())
    {
        return g_Data.m_Static[id].Flags;
    }
    if (id < g_Data.m_Land.size())
    {
        return g_Data.m_Land[id].Flags;
    }
    return 0;
}

// one Tiledata sub-tab: toolbar + clipper table (left) + detail child (right)
void TiledataDrawSubTab(
    const char *childId,
    TiledataTabState &state,
    uint32_t count,
    bool land, // false = Items sub-tab (static art), true = Land
    const std::function<const char *(uint32_t)> &nameOf,
    void (*drawColumns)(uint32_t),
    void (*drawDetail)(uint32_t),
    const char *csvName,
    bool (*exportCsv)(const TiledataTabState &))
{
    if (strcmp(state.AppliedFilter.c_str(), state.Filter) != 0)
    {
        TiledataRebuildFiltered(state, count, nameOf);
    }
    // toolbar: filter + CSV export of the filtered set
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint(
        "##Filter", "id (0x.. / dec) or name", state.Filter, sizeof(state.Filter));
    ImGui::SameLine();
    if (ImGui::Button("Export CSV (filtered)"))
    {
        if (exportCsv(state))
        {
            snprintf(
                state.Progress,
                sizeof(state.Progress),
                "Exported %d rows to %s/%s",
                (int)state.Filtered.size(),
                AssetViewerExportDir().c_str(),
                csvName);
        }
        else
        {
            snprintf(
                state.Progress,
                sizeof(state.Progress),
                "Export failed: %s/%s",
                AssetViewerExportDir().c_str(),
                csvName);
        }
    }
    if (state.Progress[0] != '\0')
    {
        // status on the button row when it fits, own line otherwise
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

    const float detailW = 320.0f;
    const float tableW = std::max(240.0f, ImGui::GetContentRegionAvail().x - detailW);
    ImGui::BeginChild(childId, ImVec2(tableW, 0.0f), ImGuiChildFlags_Borders);
    if (state.Filtered.empty())
    {
        ImGui::TextWrapped("No tiledata entries match the current filter.");
    }
    else if (
        ImGui::BeginTable(
            "##TiledataTable",
            5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Art", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("Fields", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableHeadersRow();

        // keyboard navigation while the table child has focus
        const int rowCount = (int)state.Filtered.size();
        int selRow = -1;
        for (int i = 0; i < rowCount; i++)
        {
            if (state.Filtered[i] == state.Selected)
            {
                selRow = i;
                break;
            }
        }
        bool scrollToSel = false;
        const int prevRow = selRow;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && rowCount > 0)
        {
            int sel = selRow;
            int delta = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                delta = -1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                delta = 1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            {
                delta = -20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            {
                delta = 20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Home))
            {
                sel = 0;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_End))
            {
                sel = rowCount - 1;
            }
            if (delta != 0)
            {
                sel = (sel < 0) ? 0 : std::max(0, std::min(rowCount - 1, sel + delta));
            }
            if (sel >= 0 && sel < rowCount)
            {
                if (sel != prevRow)
                {
                    scrollToSel = true;
                }
                selRow = sel;
                state.Selected = (int)state.Filtered[sel];
            }
            if ((ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                 ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) &&
                ImGui::GetActiveID() == 0)
            {
                // Enter cross-jumps to the matching grid tab (gated on no
                // active item so a nav-focused Selectable does not also
                // activate and fire the jump twice)
                if (sel >= 0 && sel < rowCount)
                {
                    const uint32_t id = state.Filtered[sel];
                    if (land)
                    {
                        RequestLandShow(id);
                    }
                    else
                    {
                        RequestStaticsShow(id);
                    }
                }
            }
        }

        // one-shot cross-tab jump: clear the filter, select and scroll once
        if (state.ExternalSelect >= 0)
        {
            const int want = state.ExternalSelect;
            state.ExternalSelect = -1;
            for (int i = 0; i < rowCount; i++)
            {
                if (state.Filtered[i] == (uint32_t)want)
                {
                    state.Selected = want;
                    scrollToSel = true;
                    selRow = i;
                    break;
                }
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin(rowCount);
        // only pin the scroll target on frames where keyboard nav moved it,
        // so the user can still scroll away freely
        if (scrollToSel && selRow >= 0)
        {
            clipper.IncludeItemsByIndex(selRow, selRow + 1);
        }
        else
        {
            scrollToSel = false;
        }
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const uint32_t id = state.Filtered[row];
                if (id >= count)
                {
                    continue; // tiledata smaller than expected: bounds-check
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // thumbnail through the shared texture cache (decode-on-miss,
                // visible rows only thanks to the clipper); the SpanAllColumns
                // selectable in the Id column keeps handling row clicks
                const bool nodraw = land ? LandNodraw((uint16_t)id) : StaticNodraw((uint16_t)id);
                const ImTextureRef ref =
                    nodraw ? ImTextureRef() :
                             AssetGrid_AcquireThumb(
                                 land ? AssetTextureKind::Land : AssetTextureKind::Static,
                                 (uint16_t)id,
                                 48);
                if (AssetTextureCache_IsReady(ref))
                {
                    ImGui::Image(ref, ImVec2(48.0f, 48.0f));
                }
                else
                {
                    ImGui::TextDisabled("(nodraw)");
                }
                ImGui::TableNextColumn();
                char label[24];
                snprintf(label, sizeof(label), "%u (0x%04X)", id, id);
                // SpanAllColumns: any cell click selects the row
                if (ImGui::Selectable(
                        label, state.Selected == (int)id, ImGuiSelectableFlags_SpanAllColumns))
                {
                    state.Selected = (int)id;
                }
                if (row == selRow && scrollToSel)
                {
                    ImGui::SetScrollFromPosY(
                        ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y - ImGui::GetScrollY(),
                        0.3f);
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(nameOf(id));
                ImGui::TableNextColumn();
                char flags[16];
                snprintf(flags, sizeof(flags), "0x%08llX", (unsigned long long)TileFlagsOf(id));
                ImGui::TextUnformatted(flags);
                ImGui::TableNextColumn();
                drawColumns(id);
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##TiledataDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    if (state.Selected >= 0)
    {
        drawDetail((uint32_t)state.Selected);
    }
    else
    {
        ImGui::TextDisabled("Select a row to inspect it");
    }
    ImGui::EndChild();
}

const char *TiledataStaticName(uint32_t id)
{
    return StaticTileName((uint16_t)id);
}

const char *TiledataLandName(uint32_t id)
{
    return LandTileName((uint16_t)id);
}

bool TiledataItemsCsv(const TiledataTabState &state)
{
    std::string csv = "Id,IdHex,Name,Flags,Weight,Layer,Quantity,AnimID,Hue,LightIndex,Height\n";
    char row[160];
    for (uint32_t id : state.Filtered)
    {
        if (id >= g_Data.m_Static.size())
        {
            continue;
        }
        const MulStaticTile2 &tile = g_Data.m_Static[id];
        csv += std::to_string(id);
        snprintf(row, sizeof(row), ",0x%04X,", id);
        csv += row;
        TiledataCsvField(csv, tile.Name);
        snprintf(
            row,
            sizeof(row),
            ",0x%08llX,%u,%u,%u,0x%04X,%d,%u,%u\n",
            (unsigned long long)tile.Flags,
            tile.Weight,
            tile.Layer,
            tile.Count,
            (uint16_t)tile.AnimID,
            (int)tile.Hue,
            tile.LightIndex,
            tile.Height);
        csv += row;
    }
    return TiledataExportCsv("tiledata_items.csv", csv);
}

bool TiledataLandCsv(const TiledataTabState &state)
{
    std::string csv = "Id,IdHex,Name,Flags,TexID\n";
    char row[96];
    for (uint32_t id : state.Filtered)
    {
        if (id >= g_Data.m_Land.size())
        {
            continue;
        }
        const MulLandTile2 &tile = g_Data.m_Land[id];
        csv += std::to_string(id);
        snprintf(row, sizeof(row), ",0x%04X,", id);
        csv += row;
        TiledataCsvField(csv, tile.Name);
        snprintf(row, sizeof(row), ",0x%08llX,%u\n", (unsigned long long)tile.Flags, tile.TexID);
        csv += row;
    }
    return TiledataExportCsv("tiledata_land.csv", csv);
}

void DrawTiledataTab()
{
    // consume a pending cross-tab jump: force the filter cleared, select the
    // id and activate the right sub-tab for one frame
    ImGuiTabItemFlags itemsFlags = ImGuiTabItemFlags_None;
    ImGuiTabItemFlags landFlags = ImGuiTabItemFlags_None;
    if (s_PendingTab == PendingTiledata && s_PendingTiledataSub >= 0)
    {
        TiledataTabState &st =
            (s_PendingTiledataSub == 1) ? TiledataLandState() : TiledataItemsState();
        st.Filter[0] = '\0';
        st.AppliedFilter = "\1"; // force the rebuild with the cleared filter
        st.ExternalSelect = (int)s_PendingTiledataId;
        if (s_PendingTiledataSub == 1)
        {
            landFlags = ImGuiTabItemFlags_SetSelected;
        }
        else
        {
            itemsFlags = ImGuiTabItemFlags_SetSelected;
        }
        s_PendingTiledataSub = -1;
        s_PendingTiledataId = 0;
        s_PendingTab = PendingNone; // this tab consumed the request
    }
    if (ImGui::BeginTabBar("##TiledataSubTabs", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("Items", nullptr, itemsFlags))
        {
            TiledataDrawSubTab(
                "##tdItems",
                TiledataItemsState(),
                (uint32_t)g_Data.m_Static.size(),
                false,
                TiledataStaticName,
                TiledataItemColumns,
                TiledataItemDetail,
                "tiledata_items.csv",
                TiledataItemsCsv);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Land", nullptr, landFlags))
        {
            TiledataDrawSubTab(
                "##tdLand",
                TiledataLandState(),
                (uint32_t)g_Data.m_Land.size(),
                true,
                TiledataLandName,
                TiledataLandColumns,
                TiledataLandDetail,
                "tiledata_land.csv",
                TiledataLandCsv);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// ---------------------------------------------------------------------------
// Hues tab
// ---------------------------------------------------------------------------

// LoadHues fills one HUES_GROUP per 8 game hue ids (the vector keeps spare
// capacity that is never initialized): the valid entries are therefore
// m_Hues.size() game hue ids 1..m_Hues.size()
int HuesEntryCount()
{
    return (int)g_FileManager.m_Hues.size();
}

// hue names are 20 non-terminated bytes (may hold garbage): bounded copy
void HuesEntryName(int entry, char (&buf)[21])
{
    buf[0] = '\0';
    const int group = entry / 8;
    if (group < 0 || group >= (int)g_FileManager.m_Hues.size())
    {
        return;
    }
    strncpy(buf, g_FileManager.m_Hues[group].Entries[entry % 8].Name, 20);
    buf[20] = '\0';
}

// uint16 RGB555 -> ImGui color (matches g_ColorManager::Color16To32 layout)
ImU32 HuesColor(uint16_t c)
{
    const uint32_t rgb = g_ColorManager.Color16To32(c);
    return IM_COL32(rgb & 0xFF, (rgb >> 8) & 0xFF, (rgb >> 16) & 0xFF, 255);
}

struct HuesTabState
{
    char Filter[64] = {};
    std::vector<int> Filtered; // entry indices (0-based)
    std::string AppliedFilter = "\1";
    int Selected = -1;
};

HuesTabState &HuesState()
{
    static HuesTabState state;
    return state;
}

void HuesRebuildFiltered(HuesTabState &state)
{
    std::string lower(state.Filter);
    for (char &c : lower)
    {
        c = (char)tolower(c);
    }
    const bool numeric =
        !lower.empty() && (strncmp(lower.c_str(), "0x", 2) == 0 ||
                           lower.find_first_not_of("0123456789") == std::string::npos);
    const uint32_t parsed = numeric ? (uint32_t)strtoul(lower.c_str(), nullptr, 0) : 0;
    state.Filtered.clear();
    for (int entry = 0; entry < HuesEntryCount(); entry++)
    {
        if (numeric)
        {
            // accept both the raw and the ingame numbering
            if (parsed != (uint32_t)entry && parsed != (uint32_t)entry + 1)
            {
                continue;
            }
        }
        else if (!lower.empty())
        {
            char name[21];
            HuesEntryName(entry, name);
            char buf[21];
            strncpy(buf, name, 20);
            buf[20] = '\0';
            for (char *p = buf; *p != '\0'; ++p)
            {
                *p = (char)tolower(*p);
            }
            if (strstr(buf, lower.c_str()) == nullptr)
            {
                continue;
            }
        }
        state.Filtered.push_back(entry);
    }
    state.AppliedFilter = state.Filter;
    state.Selected = -1;
}

// pure draw-list version: paints the 32-column palette strip into the given
// screen-space rect (solid color quads - no textures involved)
void HuesDrawStrip(const ImVec2 &min, const ImVec2 &max, int entry)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const float cellW = (max.x - min.x) / 32.0f;
    const int group = entry / 8;
    const HUES_BLOCK &block = g_FileManager.m_Hues[group].Entries[entry % 8];
    for (int i = 0; i < 32; i++)
    {
        dl->AddRectFilled(
            ImVec2(min.x + i * cellW, min.y),
            ImVec2(min.x + (i + 1) * cellW, max.y),
            HuesColor(block.ColorTable[i]));
    }
}

// apply-to-sample preview: a few well-known statics rendered unhued and with
// the selected hue through the shared texture cache (hue is part of the key)
void HuesDrawSamples(int entry, uint16_t hueId)
{
    static const uint16_t kSamples[] = { 0x1515, 0x0FAB };
    for (uint16_t graphic : kSamples)
    {
        std::vector<uint16_t> pixels;
        int w = 0;
        int h = 0;
        if (!AssetGrid_DecodeStatic(graphic, pixels, w, h))
        {
            ImGui::TextDisabled("sample 0x%04X: no art", graphic);
            continue;
        }
        ImGui::Text("Graphic 0x%04X", graphic);
        ImGui::BeginGroup();
        ImGui::TextDisabled("raw");
        ImGui::SameLine();
        DrawPreview(AssetTextureKind::Static, graphic, pixels, w, h, 0, 1.0f);
        ImGui::TextDisabled("hue %u", hueId);
        ImGui::SameLine();
        DrawPreview(AssetTextureKind::Static, graphic, pixels, w, h, hueId, 1.0f);
        ImGui::EndGroup();
        ImGui::Separator();
    }
}

void DrawHuesTab()
{
    HuesTabState &state = HuesState();
    if (strcmp(state.AppliedFilter.c_str(), state.Filter) != 0)
    {
        HuesRebuildFiltered(state);
    }

    // toolbar: search
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint(
        "##HueFilter", "index (dec) or name", state.Filter, sizeof(state.Filter));
    ImGui::SameLine();
    ImGui::TextDisabled("%d hues", (int)state.Filtered.size());

    const float detailW = 380.0f;
    const float listW = std::max(240.0f, ImGui::GetContentRegionAvail().x - detailW);
    ImGui::BeginChild("##HuesList", ImVec2(listW, 0.0f), ImGuiChildFlags_Borders);
    if (state.Filtered.empty())
    {
        ImGui::TextWrapped("No hues match the current filter.");
    }
    else
    {
        // strip takes most of the list width; index/name text sits left of it
        const float maxStripW = std::max(
            120.0f,
            std::min(
                480.0f,
                0.7f * (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x)));
        const float rowH = 2.0f * ImGui::GetFrameHeight(); // identical for every row

        // keyboard navigation while the list child has focus
        const int count = (int)state.Filtered.size();
        bool scrollToSel = false;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && count > 0)
        {
            int sel = -1;
            for (int i = 0; i < count; i++)
            {
                if (state.Filtered[i] == state.Selected)
                {
                    sel = i;
                    break;
                }
            }
            int delta = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                delta = -1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                delta = 1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            {
                delta = -20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            {
                delta = 20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Home))
            {
                sel = 0;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_End))
            {
                sel = count - 1;
            }
            if (delta != 0)
            {
                sel = (sel < 0) ? 0 : std::max(0, std::min(count - 1, sel + delta));
            }
            if (sel >= 0 && sel < count && state.Filtered[sel] != state.Selected)
            {
                state.Selected = state.Filtered[sel];
                scrollToSel = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                // Enter confirms the current selection (nothing extra to do)
                if (sel >= 0 && sel < count)
                {
                    state.Selected = state.Filtered[sel];
                }
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin(count, rowH);
        // only pin the scroll target on frames where keyboard nav moved it,
        // so the user can still scroll away freely
        int selRow = -1;
        for (int i = 0; i < count; i++)
        {
            if (state.Filtered[i] == state.Selected)
            {
                selRow = i;
                break;
            }
        }
        if (!(scrollToSel && selRow >= 0))
        {
            scrollToSel = false;
        }
        if (scrollToSel)
        {
            clipper.IncludeItemsByIndex(selRow, selRow + 1);
        }
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const int entry = state.Filtered[row];
                char name[21];
                HuesEntryName(entry, name);
                char label[64];
                snprintf(label, sizeof(label), "%d: %.20s", entry + 1, name);
                // clamp the strip so it never overlays the row label text
                const float labelWidth = ImGui::CalcTextSize(label).x;
                const float stripW = std::max(
                    60.0f,
                    std::min(maxStripW, ImGui::GetContentRegionAvail().x - labelWidth - 16.0f));
                // Selectable sized to the exact row height reserves the whole
                // row; the strip is painted into the right part of its rect
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                const bool clicked = ImGui::Selectable(
                    label, state.Selected == entry, ImGuiSelectableFlags_None, ImVec2(0.0f, rowH));
                ImGui::PopStyleVar();
                if (clicked)
                {
                    state.Selected = entry;
                }
                const ImVec2 rMin = ImGui::GetItemRectMin();
                const ImVec2 rMax = ImGui::GetItemRectMax();
                const float stripH = rMax.y - rMin.y - 8.0f;
                HuesDrawStrip(
                    ImVec2(rMax.x - stripW - 4.0f, rMin.y + 4.0f),
                    ImVec2(rMax.x - 4.0f, rMin.y + 4.0f + stripH),
                    entry);
                if (row == selRow && scrollToSel)
                {
                    ImGui::SetScrollFromPosY(
                        ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y - ImGui::GetScrollY(),
                        0.3f);
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##HuesDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
    if (state.Selected < 0 || state.Selected >= HuesEntryCount())
    {
        ImGui::TextDisabled("Select a hue to inspect it");
        ImGui::EndChild();
        return;
    }
    const int entry = state.Selected;
    const HUES_BLOCK &block = g_FileManager.m_Hues[entry / 8].Entries[entry % 8];
    char name[21];
    HuesEntryName(entry, name);
    ImGui::Text("Hue %d", entry + 1);
    ImGui::Text("Name: %.20s", name);
    ImGui::Text("Table range: %u - %u", block.TableStart, block.TableEnd);
    ImGui::SeparatorText("Palette");
    if (ImGui::BeginTable("##HuePalette", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        for (int i = 0; i < 32; i++)
        {
            const uint16_t c = block.ColorTable[i];
            char label[16];
            snprintf(label, sizeof(label), "%d", i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            const uint32_t rgb = g_ColorManager.Color16To32(c);
            ImGui::ColorButton(
                label,
                ImVec4(
                    (float)(rgb & 0xFF) / 255.0f,
                    (float)((rgb >> 8) & 0xFF) / 255.0f,
                    (float)((rgb >> 16) & 0xFF) / 255.0f,
                    1.0f),
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                ImVec2(48.0f, 18.0f));
            ImGui::TableNextColumn();
            snprintf(label, sizeof(label), "0x%04X", c);
            ImGui::TextUnformatted(label);
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Sample preview");
    if (entry + 1 >= (int)g_FileManager.m_Hues.size())
    {
        ImGui::TextDisabled("engine ignores this hue id (out of range)");
        ImGui::SameLine();
    }
    HuesDrawSamples(entry, (uint16_t)(entry + 1));
    ImGui::EndChild();
}
} // namespace

namespace
{
// ---- Animations tab ---------------------------------------------------------
//
// Works directly on the animation index instead of the draw-oriented
// CAnimationManager: a body has animation data when any of its index groups
// carries a direction block (uo_animation_exists). Decoding goes through
// CFileManager::LoadAnimation with a viewer-owned 16-bit pixel callback, so
// frames never become client CSprites/textures.
//
// LoadAnimation registers its result in the shared g_Index.m_Animations map
// (keyed by AnimationId) and skips decoding when an entry is already there -
// with CSprite UserData owned by the game. To stay read-only for the client,
// a decode temporarily borrows the map slot (erasing any game-owned entry and
// restoring it afterwards), then moves the freshly decoded frames into this
// viewer-local LRU cache and removes its own entry again.

// decoded pixels of one animation frame (stored as AnimationFrame::UserData)
struct AnimFramePixels
{
    int W = 0;
    int H = 0;
    std::vector<uint16_t> Pixels;
};

void *AnimLoadPixels(int width, int height, uint16_t *pixels)
{
    auto *frame = new AnimFramePixels();
    frame->W = width;
    frame->H = height;
    if (width > 0 && height > 0)
    {
        frame->Pixels.assign(pixels, pixels + (size_t)width * height);
    }
    return frame;
}

void AnimDeletePixels(void *ptr)
{
    delete (AnimFramePixels *)ptr;
}

struct AnimDirEntry
{
    AnimationDirFrames *Dir = nullptr; // viewer-owned, frames hold AnimFramePixels
    bool Ok = false;                   // false = decode failed (cached miss)
};

std::unordered_map<AnimationId, AnimDirEntry> s_AnimDirs;
std::vector<AnimationId> s_AnimLru;
const size_t ANIM_DIR_CACHE_CAP = 64;

// frees the decoded pixel/frame data of an entry (does not touch the map)
void AnimDirFreePixels(AnimDirEntry &entry)
{
    if (entry.Dir != nullptr)
    {
        for (int i = 0; i < entry.Dir->FrameCount; i++)
        {
            AnimDeletePixels(entry.Dir->Frames[i].UserData);
        }
        delete[] entry.Dir->Frames;
        delete entry.Dir;
        entry.Dir = nullptr;
    }
}

void AnimDirRelease(AnimationId id, AnimDirEntry &entry)
{
    AnimDirFreePixels(entry);
    s_AnimDirs.erase(id);
}

// decodes (graphic, group, dir) into the viewer cache; returns the cached
// entry (entry.Ok false = no data available for this direction)
const AnimDirEntry &AnimDirAcquire(uint16_t graphic, uint8_t group, uint8_t dir)
{
    const AnimationId id = AnimId(graphic, group, dir);
    const auto it = s_AnimDirs.find(id);
    if (it != s_AnimDirs.end())
    {
        // LRU touch
        auto lru = std::find(s_AnimLru.begin(), s_AnimLru.end(), id);
        if (lru != s_AnimLru.end())
        {
            std::rotate(lru, lru + 1, s_AnimLru.end());
        }
        return it->second;
    }

    // evict the oldest entry once the cache is full
    while (s_AnimLru.size() >= ANIM_DIR_CACHE_CAP)
    {
        const AnimationId victim = s_AnimLru.front();
        s_AnimLru.erase(s_AnimLru.begin());
        auto vit = s_AnimDirs.find(victim);
        if (vit != s_AnimDirs.end())
        {
            AnimDirRelease(victim, vit->second);
        }
    }

    const AnimationState anim{ graphic, group, dir };
    AnimationDirFrames *gameOwned = uo_animation_get(id);
    if (gameOwned != nullptr)
    {
        // borrow the map slot: LoadAnimation early-returns on an existing
        // entry, so move the game's aside for the duration of the decode
        g_Index.m_Animations.erase(id);
    }
    const bool loaded = g_FileManager.LoadAnimation(anim, AnimLoadPixels);
    AnimationDirFrames *mine = uo_animation_get(id);
    g_Index.m_Animations.erase(id); // viewer frames never stay in the shared map
    if (gameOwned != nullptr)
    {
        g_Index.m_Animations.emplace(id, gameOwned); // restore the game's entry
    }

    AnimDirEntry entry;
    entry.Ok = loaded && mine != nullptr;
    entry.Dir = mine;
    s_AnimDirs.emplace(id, entry);
    s_AnimLru.push_back(id);
    return s_AnimDirs.find(id)->second;
}

void AnimShutdown()
{
    // no AnimDirRelease here: it erases from s_AnimDirs and would invalidate
    // the iterator mid-loop
    for (auto &kv : s_AnimDirs)
    {
        AnimDirFreePixels(kv.second);
    }
    s_AnimDirs.clear();
    s_AnimLru.clear();
}

// bodies with at least one index group carrying animation data
const std::vector<uint16_t> &AnimBodyList()
{
    static std::vector<uint16_t> bodies;
    static bool built = false;
    if (!built)
    {
        built = true;
        for (int g = 0; g < MAX_ANIMATIONS_DATA_INDEX_COUNT; g++)
        {
            for (int group = 0; group < MAX_ANIMATION_GROUPS_COUNT; group++)
            {
                if (uo_animation_exists((uint16_t)g, (uint8_t)group))
                {
                    bodies.push_back((uint16_t)g);
                    break;
                }
            }
        }
    }
    return bodies;
}

// numeric filter: matches the decimal or hex text of the id as a substring
bool AnimIdMatches(uint16_t id, const std::string &lower)
{
    char dec[16];
    char hex[16];
    snprintf(dec, sizeof(dec), "%u", id);
    snprintf(hex, sizeof(hex), "0x%04x", id);
    return strstr(dec, lower.c_str()) != nullptr || strstr(hex, lower.c_str()) != nullptr;
}

struct AnimTabState
{
    AssetGridState Grid;              // Hue selector (DrawHueSelector) reuse
    char Filter[64] = {};             // body filter (numeric id text)
    std::vector<uint16_t> Filtered;   // filtered body list
    std::string AppliedFilter = "\1"; // filter text the Filtered set mirrors
    int SelectedGraphic = -1;         // selected body (-1 = none)
    int SelectedGroup = -1;           // selected action/group index
    int SelectedDir = 0;              // selected direction
    int FrameIndex = 0;               // playback / scrub position
    float Fps = 10.0f;                // playback speed
    bool Playing = true;              // play/pause
    double LastAdvance = 0.0;         // ImGui::GetTime of the last frame step
    float Zoom = 2.0f;                // frame viewport zoom
    char Progress[192] = {};          // last export result line
};

// available directions of a group (index blocks can carry fewer than all)
int AnimDirCount(uint16_t graphic, uint8_t group)
{
    int count = 0;
    for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
    {
        const auto &direction = g_Index.m_Anim[graphic].Groups[group].Direction[d];
        if (direction.Size != 0 || direction.IsUOP)
        {
            count = d + 1; // directions are contiguous in the index
        }
    }
    return count;
}

// replica of CAnimationManager::GetAnimDirection (AnimationManager.cpp:288):
// maps a facing direction (0-7) onto a stored base direction (0-4) and tells
// whether the sprite must be drawn horizontally mirrored
void AnimClientDirection(uint8_t &dir, bool &mirror)
{
    switch (dir)
    {
        case 2:
        case 4:
        {
            mirror = (dir == 2);
            dir = 1;
            break;
        }
        case 1:
        case 5:
        {
            mirror = (dir == 1);
            dir = 2;
            break;
        }
        case 0:
        case 6:
        {
            mirror = (dir == 0);
            dir = 3;
            break;
        }
        case 3:
        {
            dir = 0;
            break;
        }
        case 7:
        {
            dir = 4;
            break;
        }
        default:
            break;
    }
}

// top / bottom / left / right offsets of a frame relative to its pivot, in
// source pixels — the exact expressions the draw path uses (verbatim from
// CAnimationManager::Draw, AnimationManager.cpp:545-554:
// x -= mirror ? Width - CenterX : CenterX; y -= Height + CenterY), so the
// pivot-extent computation in DrawAnimationsTab can never diverge from the
// playback positioning
int AnimFrameTopOffset(const AnimationFrame &f, const AnimFramePixels *fp)
{
    return fp != nullptr && fp->H > 0 ? fp->H + (int)f.CenterY : 0;
}

int AnimFrameBottomOffset(const AnimationFrame &f)
{
    return -(int)f.CenterY;
}

int AnimFrameLeftOffset(const AnimationFrame &f)
{
    return (int)f.CenterX;
}

int AnimFrameRightOffset(const AnimationFrame &f, const AnimFramePixels *fp)
{
    return fp != nullptr && fp->W > 0 ? fp->W - (int)f.CenterX : 0;
}

void AnimDrawFrame(
    AnimTabState &state,
    uint16_t graphic,
    const AnimFramePixels *frame,
    int16_t centerX,
    int16_t centerY,
    bool mirror,
    int maxLeft,
    int maxRight,
    int maxUp,
    int maxDown)
{
    // size the bordered viewport to the content so oversized frames stay
    // anchored inside it (a fixed canvas would start them off-canvas), while
    // still capping both axes to the playback panel so the window never grows
    const bool empty = frame == nullptr || frame->W <= 0 || frame->H <= 0 || frame->Pixels.empty();
    const float w = empty ? 0.0f : frame->W * state.Zoom;
    const float h = empty ? 0.0f : frame->H * state.Zoom;
    constexpr float minView = 600.0f;
    constexpr float pad = 8.0f;
    const float availX = ImGui::GetContentRegionAvail().x;
    // reserve one line below for the zoom slider so the child can't push it
    // into a scroll/grow of the playback panel
    const float availY = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing();
    // size the viewport to the pivot extent of the whole (group, dir) so the
    // anchor point stays at a fixed position across frames and playback never
    // resizes the viewport (capped to the panel as before)
    const float vw = std::min(
        std::max(minView, (maxLeft + maxRight) * state.Zoom + 2.0f * pad), std::max(1.0f, availX));
    const float vh = std::min(
        std::max(minView, (maxUp + maxDown) * state.Zoom + 2.0f * pad), std::max(1.0f, availY));
    // center the viewport child horizontally in the leftover panel width
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (availX - vw) * 0.5f));
    ImGui::BeginChild("##AnimViewport", ImVec2(vw, vh), ImGuiChildFlags_Borders);
    if (empty)
    {
        ImGui::SetCursorPos(ImVec2(vw / 2 - 60.0f, vh / 2 - 8.0f));
        ImGui::TextDisabled("empty frame");
        ImGui::EndChild();
        return;
    }
    // the pivot is drawn at a fixed point in the viewport (like the character
    // position in the client); sign convention verbatim from
    // CAnimationManager::Draw (AnimationManager.cpp:545-554):
    // x -= mirror ? Width - CenterX : CenterX; y -= Height + CenterY.
    // The frame pixels are flipped for mirrored dirs below, which restores the
    // CenterX-from-left anchor, so one offset formula serves both cases.
    // the pivot is placed so the largest frame of the (group, dir) fits below
    // and above it: anchorY must stay >= maxUp * zoom or the tops of tall
    // frames are clipped (the panel-height cap below only limits how much
    // headroom remains, never steals the required top extent)
    const float anchorX = std::min(maxLeft * state.Zoom + pad, vw * 0.5f);
    const float anchorY =
        std::min(maxUp * state.Zoom + pad, std::max(maxUp * state.Zoom, vh - pad));
    const float x = anchorX - centerX * state.Zoom;
    const float y = anchorY - (frame->H + centerY) * state.Zoom;
    AssetTextureKey key{ AssetTextureKind::Animation,
                         graphic,
                         state.Grid.Hue,
                         (uint16_t)frame->W,
                         (uint16_t)frame->H,
                         ((uint32_t)state.SelectedGroup << 16) |
                             ((uint32_t)state.SelectedDir << 8) | (uint32_t)state.FrameIndex |
                             ((uint32_t)mirror << 24) };
    const ImTextureRef ref = AssetTextureCache_AcquireLazy(
        key,
        [&](std::vector<uint16_t> &out)
        {
            // apply the partial hue here so state.Grid.Hue recolors the
            // frames: gray palette entries are remapped through the hue
            // table (same rule as AssetViewer_Convert16ToRgba32, kept in
            // 16-bit since the cache converts to RGBA32 afterwards). The
            // cache's own key.Hue pass is a no-op for the already-tinted
            // non-gray results, and hue 0 leaves the pixels raw.
            const uint16_t hue = state.Grid.Hue;
            out.resize(frame->Pixels.size());
            for (int row = 0; row < frame->H; row++)
            {
                for (int col = 0; col < frame->W; col++)
                {
                    // mirrored dirs flip the decoded pixels (the client flips
                    // the texture coordinates at render time instead)
                    const size_t src = size_t(row) * frame->W + (mirror ? frame->W - 1 - col : col);
                    uint16_t c = frame->Pixels[src];
                    if (c == 0)
                    {
                        continue; // fully transparent (the UO color-key)
                    }
                    if (hue != 0 && hue < g_ColorManager.GetHuesCount())
                    {
                        const uint32_t rgb = g_ColorManager.Color16To32(c);
                        const uint8_t r = (uint8_t)(rgb & 0xFF);
                        const uint8_t g = (uint8_t)((rgb >> 8) & 0xFF);
                        const uint8_t b = (uint8_t)((rgb >> 16) & 0xFF);
                        if (r == g && b == g)
                        {
                            const uint16_t h = (uint16_t)(hue - 1);
                            c = g_FileManager.m_Hues[h / 8]
                                    .Entries[h % 8]
                                    .ColorTable[(c >> 10) & 0x1F] |
                                (c & 0x8000); // keep the alpha bit
                        }
                    }
                    out[size_t(row) * frame->W + col] = c;
                }
            }
            return true;
        });
    if (AssetTextureCache_IsReady(ref))
    {
        ImGui::SetCursorPos(ImVec2(x, y));
        ImGui::Image(ref, ImVec2(w, h));
    }
    else
    {
        ImGui::SetCursorPos(ImVec2(vw / 2 - 60.0f, vh / 2 - 8.0f));
        ImGui::TextDisabled("frame unavailable");
    }
    ImGui::EndChild();
}

std::string AnimExportDirName(uint16_t graphic, uint8_t group, uint8_t dir)
{
    char name[64];
    snprintf(name, sizeof(name), "anim_0x%04X_%u_%u", graphic, group, dir);
    return std::string(name);
}

void AnimExportFrame(
    AnimTabState &state,
    uint16_t graphic,
    uint8_t group,
    uint8_t dir,
    const AnimFramePixels *frame,
    int frameIndex)
{
    if (frame == nullptr || frame->W <= 0 || frame->H <= 0 || frame->Pixels.empty())
    {
        snprintf(state.Progress, sizeof(state.Progress), "Frame %d is empty", frameIndex);
        return;
    }
    char rel[96];
    snprintf(
        rel,
        sizeof(rel),
        "%s/frame_%03d.png",
        AnimExportDirName(graphic, group, dir).c_str(),
        frameIndex);
    const std::string path = AssetGrid_ExportPngFile(rel, frame->Pixels, frame->W, frame->H);
    snprintf(
        state.Progress,
        sizeof(state.Progress),
        "%s",
        path.empty() ? "Export failed" : path.c_str());
}

void AnimExportSequence(
    AnimTabState &state,
    uint16_t graphic,
    uint8_t group,
    uint8_t dir,
    const AnimationDirFrames *dirFrames)
{
    int written = 0;
    for (int i = 0; i < dirFrames->FrameCount; i++)
    {
        const auto *frame = (const AnimFramePixels *)dirFrames->Frames[i].UserData;
        if (frame == nullptr || frame->W <= 0 || frame->H <= 0 || frame->Pixels.empty())
        {
            continue;
        }
        char rel[96];
        snprintf(
            rel,
            sizeof(rel),
            "%s/frame_%03d.png",
            AnimExportDirName(graphic, group, dir).c_str(),
            i);
        if (!AssetGrid_ExportPngFile(rel, frame->Pixels, frame->W, frame->H).empty())
        {
            written++;
        }
    }
    snprintf(
        state.Progress,
        sizeof(state.Progress),
        "Exported %d of %d frames to %s",
        written,
        dirFrames->FrameCount,
        AnimExportDirName(graphic, group, dir).c_str());
}

void DrawAnimationsTab()
{
    static AnimTabState state;

    // lazily rebuild the filtered body list
    const std::string filter = state.Filter;
    if (filter != state.AppliedFilter)
    {
        state.AppliedFilter = filter;
        state.Filtered.clear();
        std::string lower = filter;
        for (auto &c : lower)
        {
            c = (char)tolower(c);
        }
        for (uint16_t id : AnimBodyList())
        {
            if (lower.empty() || AnimIdMatches(id, lower))
            {
                state.Filtered.push_back(id);
            }
        }
    }

    // left column: body list
    ImGui::BeginChild("##AnimBodies", ImVec2(150.0f, 0.0f));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##AnimFilter", "graphic id", state.Filter, sizeof(state.Filter));
    if (ImGui::BeginListBox("##AnimBodyList", ImVec2(-1, -1)))
    {
        // keyboard navigation while the list child has focus
        const int count = (int)state.Filtered.size();
        int selIdx = -1;
        for (int i = 0; i < count; i++)
        {
            if (state.Filtered[i] == state.SelectedGraphic)
            {
                selIdx = i;
                break;
            }
        }
        bool scrollToSel = false;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && count > 0)
        {
            int sel = selIdx;
            int delta = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                delta = -1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                delta = 1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            {
                delta = -20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            {
                delta = 20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Home))
            {
                sel = 0;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_End))
            {
                sel = count - 1;
            }
            if (delta != 0)
            {
                sel = (sel < 0) ? 0 : std::max(0, std::min(count - 1, sel + delta));
            }
            if (sel >= 0 && sel < count && state.Filtered[sel] != state.SelectedGraphic)
            {
                scrollToSel = true;
                state.SelectedGraphic = state.Filtered[sel];
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                // Enter confirms the selection (same reset as a click)
                if (sel >= 0 && sel < count)
                {
                    state.SelectedGraphic = state.Filtered[sel];
                    state.SelectedGroup = -1;
                    state.SelectedDir = 0;
                    state.FrameIndex = 0;
                }
            }
            selIdx = -1;
            for (int i = 0; i < count; i++)
            {
                if (state.Filtered[i] == state.SelectedGraphic)
                {
                    selIdx = i;
                    break;
                }
            }
        }
        for (uint16_t id : state.Filtered)
        {
            char label[16];
            snprintf(label, sizeof(label), "0x%04X", id);
            if (ImGui::Selectable(label, state.SelectedGraphic == id))
            {
                state.SelectedGraphic = id;
                state.SelectedGroup = -1;
                state.SelectedDir = 0;
                state.FrameIndex = 0;
            }
            if (state.SelectedGraphic == id && scrollToSel)
            {
                // scroll the newly selected row into view (keyboard nav only)
                ImGui::SetScrollFromPosY(
                    ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y - ImGui::GetScrollY(),
                    0.3f);
            }
        }
        ImGui::EndListBox();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // middle column: action/group list
    ImGui::BeginChild("##AnimGroups", ImVec2(170.0f, 0.0f));
    if (state.SelectedGraphic < 0)
    {
        ImGui::TextDisabled("select a graphic");
    }
    else
    {
        const uint16_t graphic = (uint16_t)state.SelectedGraphic;
        if (ImGui::BeginListBox("##AnimGroupList", ImVec2(-1, -1)))
        {
            // existing groups in display order (keyboard nav needs an indexable list)
            std::vector<int> groups;
            for (int group = 0; group < MAX_ANIMATION_GROUPS_COUNT; group++)
            {
                if (uo_animation_exists(graphic, (uint8_t)group))
                {
                    groups.push_back(group);
                }
            }
            // keyboard navigation while the list child has focus
            const int count = (int)groups.size();
            int selIdx = -1;
            for (int i = 0; i < count; i++)
            {
                if (groups[i] == state.SelectedGroup)
                {
                    selIdx = i;
                    break;
                }
            }
            bool scrollToSel = false;
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && count > 0)
            {
                int sel = selIdx;
                int delta = 0;
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                {
                    delta = -1;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                {
                    delta = 1;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
                {
                    delta = -20;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
                {
                    delta = 20;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Home))
                {
                    sel = 0;
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_End))
                {
                    sel = count - 1;
                }
                if (delta != 0)
                {
                    sel = (sel < 0) ? 0 : std::max(0, std::min(count - 1, sel + delta));
                }
                if (sel >= 0 && sel < count && groups[sel] != state.SelectedGroup)
                {
                    scrollToSel = true;
                    state.SelectedGroup = groups[sel];
                    state.SelectedDir = 0;
                    state.FrameIndex = 0;
                    state.Playing = true;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                    ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
                {
                    // Enter confirms the selection (same reset as a click)
                    if (sel >= 0 && sel < count)
                    {
                        state.SelectedGroup = groups[sel];
                        state.SelectedDir = 0;
                        state.FrameIndex = 0;
                        state.Playing = true;
                    }
                }
                selIdx = -1;
                for (int i = 0; i < count; i++)
                {
                    if (groups[i] == state.SelectedGroup)
                    {
                        selIdx = i;
                        break;
                    }
                }
            }
            for (int group : groups)
            {
                char label[16];
                snprintf(label, sizeof(label), "Group %d", group);
                if (ImGui::Selectable(label, state.SelectedGroup == group))
                {
                    state.SelectedGroup = group;
                    state.SelectedDir = 0;
                    state.FrameIndex = 0;
                    state.Playing = true;
                }
                if (state.SelectedGroup == group && scrollToSel)
                {
                    // scroll the newly selected row into view (keyboard nav only)
                    ImGui::SetScrollFromPosY(
                        ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y - ImGui::GetScrollY(),
                        0.3f);
                }
            }
            ImGui::EndListBox();
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // right column: playback
    ImGui::BeginChild("##AnimPlayback", ImVec2(0.0f, 0.0f));
    if (state.SelectedGraphic < 0 || state.SelectedGroup < 0)
    {
        ImGui::TextDisabled("select an action/group");
        ImGui::EndChild();
        return;
    }
    const uint16_t graphic = (uint16_t)state.SelectedGraphic;
    const uint8_t group = (uint8_t)state.SelectedGroup;
    const int dirCount = AnimDirCount(graphic, group);
    // the direction selector covers all facings (0-7); stored base dirs come
    // straight from the index, the rest are mirrored via AnimClientDirection
    const int dir = std::min(state.SelectedDir, MAX_MOBILE_DIRECTIONS - 1);
    bool mirror = false;
    uint8_t fileDir = (uint8_t)dir;
    const auto &dirIndex = g_Index.m_Anim[graphic].Groups[group].Direction[fileDir];
    if (dirIndex.Size == 0 && !dirIndex.IsUOP)
    {
        AnimClientDirection(fileDir, mirror);
    }
    const AnimDirEntry &entry = AnimDirAcquire(graphic, group, fileDir);
    const AnimationDirFrames *dirFrames = entry.Dir;

    int frameCount = dirFrames != nullptr ? dirFrames->FrameCount : 0;
    if (frameCount <= 0)
    {
        ImGui::TextDisabled(
            entry.Ok ? "no frames for group %d dir %d" : "no animation data (group %d dir %d)",
            group,
            dir);
        ImGui::EndChild();
        return;
    }
    if (state.FrameIndex >= frameCount)
    {
        state.FrameIndex = 0; // frame counts can differ per direction
    }

    // playback controls (a hidden tab never runs this code: no timer advance)
    ImGui::BeginDisabled(frameCount <= 1);
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::BeginCombo("Direction", dirCount > 0 ? "" : "-"))
    {
        for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
        {
            char dirLabel[16];
            snprintf(dirLabel, sizeof(dirLabel), "%d", d);
            ImGui::Selectable(dirLabel, state.SelectedDir == d);
            if (ImGui::IsItemHovered() || state.SelectedDir == d)
            {
                ImGui::SetTooltip("%s", d < dirCount ? "stored direction" : "mirrored direction");
            }
            if (ImGui::IsItemClicked())
            {
                state.SelectedDir = d;
                state.FrameIndex = 0; // frame counts can differ per direction
            }
        }
        ImGui::EndCombo();
    }
    if (dirCount < MAX_MOBILE_DIRECTIONS)
    {
        ImGui::TextDisabled(
            "only %d of %d dirs in index (client mirrors for facing)",
            dirCount,
            MAX_MOBILE_DIRECTIONS);
    }
    ImGui::SameLine();
    if (ImGui::Button(state.Playing ? "Pause" : "Play"))
    {
        state.Playing = !state.Playing;
        state.LastAdvance = ImGui::GetTime();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderFloat("##AnimFps", &state.Fps, 1.0f, 30.0f, "%.0f fps");
    ImGui::SameLine();
    ImGui::Text("%d/%d", state.FrameIndex + 1, frameCount);

    if (state.Playing && frameCount > 1)
    {
        const double now = ImGui::GetTime();
        const double interval = 1.0 / std::max(1.0f, state.Fps);
        if (now - state.LastAdvance > interval) // long stall (e.g. breakpoint):
        {                                       // resync instead of catch-up flood
            state.LastAdvance = now - interval;
        }
        while (now - state.LastAdvance >= interval)
        {
            state.LastAdvance += interval;
            state.FrameIndex = (state.FrameIndex + 1) % frameCount;
        }
    }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderInt("##AnimFrame", &state.FrameIndex, 0, frameCount - 1))
    {
        state.Playing = false;
    }

    const AnimationFrame &frame = dirFrames->Frames[state.FrameIndex];
    // pivot extent of the whole (graphic, group, fileDir): keeps the anchor at
    // a fixed viewport point across frames of different sizes
    int maxLeft = 0;
    int maxRight = 0;
    int maxUp = 0;
    int maxDown = 0;
    for (int i = 0; i < frameCount; i++)
    {
        const auto *fp = (const AnimFramePixels *)dirFrames->Frames[i].UserData;
        if (fp == nullptr || fp->W <= 0 || fp->H <= 0)
        {
            continue;
        }
        maxLeft = std::max(maxLeft, AnimFrameLeftOffset(dirFrames->Frames[i]));
        maxRight = std::max(maxRight, AnimFrameRightOffset(dirFrames->Frames[i], fp));
        maxUp = std::max(maxUp, AnimFrameTopOffset(dirFrames->Frames[i], fp));
        maxDown = std::max(maxDown, AnimFrameBottomOffset(dirFrames->Frames[i]));
    }
    AnimDrawFrame(
        state,
        graphic,
        (const AnimFramePixels *)frame.UserData,
        frame.CenterX,
        frame.CenterY,
        mirror,
        maxLeft,
        maxRight,
        maxUp,
        maxDown);
    ImGui::SliderFloat("##AnimZoom", &state.Zoom, 0.25f, 4.0f, "zoom %.2fx");
    DrawHueSelector(state.Grid);

    ImGui::SeparatorText("Export");
    if (ImGui::Button("Export frame PNG"))
    {
        AnimExportFrame(
            state,
            graphic,
            group,
            (uint8_t)dir,
            (const AnimFramePixels *)frame.UserData,
            state.FrameIndex);
    }
    ImGui::SameLine();
    if (ImGui::Button("Export animation"))
    {
        AnimExportSequence(state, graphic, group, (uint8_t)dir, dirFrames);
    }
    if (state.Progress[0] != '\0')
    {
        // status on the button row when it fits, own line otherwise
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
    const auto *framePixels = (const AnimFramePixels *)frame.UserData;
    ImGui::Text(
        "Graphic: 0x%04X, Group: %d, Dir: %d%s, Size: %dx%d, Center: %d,%d",
        graphic,
        group,
        dir,
        mirror ? " (mirrored)" : "",
        framePixels != nullptr ? framePixels->W : 0,
        framePixels != nullptr ? framePixels->H : 0,
        frame.CenterX,
        frame.CenterY);
    ImGui::EndChild();
}
} // namespace

namespace
{
// ---------------------------------------------------------------------------
// Sound tab: preview + WAV export over g_Index.m_Sound (MUL and UOP alike).
// Playback mirrors CGame::PlaySoundEffect's minimal path through the public
// CSoundManager API: LoadSoundEffect resolves the sound block (including the
// UOP pack hash / legacy mul variants handled inside the core) and returns a
// stream handle that stays owned by this module (the game's index entries are
// left untouched except for the wave cache LoadSoundEffect already maintains).
// Only one preview plays at a time; the handle is freed with FreeSound.
//
// Sound block layout (see CSoundManager::CreateWaveFile): a 32-byte header
// (16-byte name + 4 unknown u32 words), then a 16-byte wave hint carrying the
// real channel count (byte at +8) and sample rate (u32 at +12), then the raw
// 16-bit PCM payload. The client playback hardcodes mono/22050/16-bit; the
// WAV export honors the hint instead so the exported file sounds right.
constexpr int SOUND_BLOCK_HEADER_SIZE = 32;
constexpr int SOUND_WAVE_HINT_SIZE = 16;

struct SoundTabState
{
    char Filter[64] = {};
    std::vector<uint16_t> Filtered;
    std::string AppliedFilter;
    std::unordered_map<int, std::string> Names; // lazily resolved block names
    int Selected = -1;
    SoundHandle Playing = SOUND_NULL; // owned preview stream (FreeSound'd)
    int PlayingId = -1;
    std::string Status; // last export/play feedback line
};

SoundTabState &SoundState()
{
    static SoundTabState s;
    return s;
}

bool SoundAvailable(uint16_t id)
{
    const CIndexSound &is = g_Index.m_Sound[id];
    return is.Address != 0 || is.UopHash != 0;
}

// The sound name lives in the block header (16 bytes), not in the index:
// resolve lazily (visible rows only) through the core block resolver so both
// MUL and UOP sources work, and cache the (sanitized) result per id. An empty
// cache entry means "no usable name" and renders as "(unnamed)".
const char *SoundName(uint16_t id)
{
    SoundTabState &st = SoundState();
    auto found = st.Names.find(id);
    if (found == st.Names.end())
    {
        std::string name;
        if (SoundAvailable(id))
        {
            std::vector<uint8_t> fetched;
            size_t blockSize = 0;
            const uint8_t *block = uo_sound_block_data(g_Index.m_Sound[id], fetched, blockSize);
            if (block != nullptr && blockSize >= 16)
            {
                for (int i = 0; i < 16 && block[i] != 0; i++)
                {
                    const unsigned char c = block[i];
                    name += char(isprint(c) != 0 ? c : ' ');
                }
            }
        }
        found = st.Names.emplace(id, name).first;
    }
    return found->second.empty() ? "(unnamed)" : found->second.c_str();
}

bool SoundMatch(uint16_t id, const std::string &lower, const std::string &lowerName)
{
    if (lower.empty())
    {
        return true;
    }
    char dec[16];
    snprintf(dec, sizeof(dec), "%u", id);
    if (strstr(dec, lower.c_str()) != nullptr)
    {
        return true;
    }
    char hex[16];
    snprintf(hex, sizeof(hex), "0x%04x", id);
    if (strstr(hex, lower.c_str()) != nullptr)
    {
        return true;
    }
    return lowerName.find(lower) != std::string::npos;
}

void SoundRebuildFiltered(SoundTabState &st, const std::string &lower)
{
    st.Filtered.clear();
    for (int id = 0; id < MAX_SOUND_DATA_INDEX_COUNT; id++)
    {
        if (!SoundAvailable((uint16_t)id))
        {
            continue;
        }
        // cheap pass: match by id only; name matching applies to already-
        // cached names so filtering never triggers a block fetch/decode per
        // candidate id (names resolve lazily for rendered rows instead)
        std::string lowerName;
        auto found = st.Names.find(id);
        if (found != st.Names.end() && !found->second.empty())
        {
            lowerName = str_lower(found->second);
        }
        if (SoundMatch((uint16_t)id, lower, lowerName))
        {
            st.Filtered.push_back((uint16_t)id);
        }
    }
}

void SoundStopPreview()
{
    SoundTabState &st = SoundState();
    if (st.Playing != SOUND_NULL)
    {
        g_SoundManager.FreeSound(st.Playing);
        st.Playing = SOUND_NULL;
        st.PlayingId = -1;
    }
}

void SoundPlay(uint16_t id)
{
    SoundTabState &st = SoundState();
    SoundStopPreview();
    if (!SoundAvailable(id))
    {
        st.Status = "No sound data for this id";
        return;
    }
    SoundHandle handle = g_SoundManager.LoadSoundEffect(g_Index.m_Sound[id]);
    if (handle == SOUND_NULL)
    {
        st.Status = "Failed to decode the sound block";
        return;
    }
    // one preview at a time: the previous stream was stopped above, this one
    // plays at the client's configured sound volume (game-consistent)
    g_SoundManager.PlaySoundEffect(handle, g_SoundManager.GetVolumeValue());
    st.Playing = handle;
    st.PlayingId = id;
    st.Status.clear();
}

void WavAppendU16(std::string &out, uint16_t v)
{
    out.append((const char *)&v, 2);
}

void WavAppendU32(std::string &out, uint32_t v)
{
    out.append((const char *)&v, 4);
}

// Writes the decoded PCM payload as a canonical 44-byte-header RIFF/WAVE
// (PCM 16-bit); odd data chunks get the mandatory pad byte so the RIFF
// size stays consistent.
std::string
WavWrite(const char *fileName, const uint8_t *data, uint32_t dataSize, int channels, uint32_t rate)
{
    const uint32_t byteRate = rate * uint32_t(channels) * 2u;
    std::string wav;
    wav.reserve(44 + dataSize + 1);
    wav += "RIFF";
    WavAppendU32(wav, 36u + dataSize);
    wav += "WAVE";
    wav += "fmt ";
    WavAppendU32(wav, 16u); // fmt chunk size
    WavAppendU16(wav, 1u);  // PCM
    WavAppendU16(wav, (uint16_t)channels);
    WavAppendU32(wav, rate);
    WavAppendU32(wav, byteRate);
    WavAppendU16(wav, (uint16_t)(channels * 2)); // block align
    WavAppendU16(wav, 16u);                      // bits per sample
    wav += "data";
    WavAppendU32(wav, dataSize);
    wav.append((const char *)data, dataSize);
    if ((dataSize & 1u) != 0)
    {
        wav += '\0';
    }

    const fs_path dir = fs_path_from(AssetViewerExportDir());
    const fs_path path = fs_path_from(fs_path_str(dir) + "/" + fileName);
    FILE *file = fopen(fs_path_ascii(path), "wb");
    if (file == nullptr)
    {
        Warning(Assistant, "asset viewer: WAV export failed: %s\n", fs_path_ascii(path));
        return std::string();
    }
    fwrite(wav.data(), 1, wav.size(), file);
    fclose(file);
    Info(Assistant, "asset viewer: exported %s\n", fs_path_ascii(path));
    return fs_path_str(path);
}

std::string SoundExportWav(uint16_t id, std::string &error)
{
    error.clear();
    if (!SoundAvailable(id))
    {
        error = "No sound data for this id";
        return std::string();
    }
    std::vector<uint8_t> fetched;
    size_t blockSize = 0;
    const uint8_t *block = uo_sound_block_data(g_Index.m_Sound[id], fetched, blockSize);
    if (block == nullptr || blockSize < size_t(SOUND_BLOCK_HEADER_SIZE + SOUND_WAVE_HINT_SIZE))
    {
        error = "Sound block fetch failed";
        return std::string();
    }
    // wave hint after the block header: channel count and sample rate of the
    // embedded PCM (bits are always 16 in the classic format, as the client's
    // CreateWaveFile playback path assumes as well)
    const uint8_t *hint = block + SOUND_BLOCK_HEADER_SIZE;
    int channels = hint[8];
    uint32_t rate = hint[12] | (uint32_t(hint[13]) << 8) | (uint32_t(hint[14]) << 16) |
                    (uint32_t(hint[15]) << 24);
    bool hintInvalid = false;
    if (channels < 1 || channels > 2 || rate < 8000 || rate > 96000)
    {
        // unusable hint: fall back to the client's playback assumption
        // (mono/22050/16-bit) instead of failing the export outright
        channels = 1;
        rate = 22050;
        hintInvalid = true;
    }
    const uint32_t dataSize = uint32_t(blockSize - SOUND_BLOCK_HEADER_SIZE - SOUND_WAVE_HINT_SIZE);
    char fileName[32];
    snprintf(fileName, sizeof(fileName), "sound_0x%04X.wav", id);
    std::string path = WavWrite(
        fileName, block + SOUND_BLOCK_HEADER_SIZE + SOUND_WAVE_HINT_SIZE, dataSize, channels, rate);
    if (path.empty())
    {
        error = "Write failed";
    }
    else if (hintInvalid)
    {
        path += " (hint invalid, assumed mono/22050)";
        Warning(
            Assistant, "asset viewer: sound 0x%04X wave hint invalid, assumed mono/22050\n", id);
    }
    return path;
}

void DrawSoundTab()
{
    SoundTabState &st = SoundState();

    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##soundFilter", "Filter (id or name)", st.Filter, sizeof(st.Filter));
    const std::string filter = str_lower(st.Filter);
    if (filter != st.AppliedFilter)
    {
        st.AppliedFilter = filter;
        SoundRebuildFiltered(st, filter);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu sounds", st.Filtered.size());
    if (!st.Status.empty())
    {
        ImGui::TextDisabled("%s", st.Status.c_str());
    }

    const float detailW = 320.0f;
    const float listW = std::max(240.0f, ImGui::GetContentRegionAvail().x - detailW);
    ImGui::BeginChild("##soundList", ImVec2(listW, 0.0f), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable(
            "##soundTable",
            3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        // keyboard navigation while the table child has focus
        const int rowCount = (int)st.Filtered.size();
        int selRow = -1;
        for (int i = 0; i < rowCount; i++)
        {
            if (st.Filtered[i] == st.Selected)
            {
                selRow = i;
                break;
            }
        }
        bool scrollToSel = false;
        const int prevRow = selRow;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && rowCount > 0)
        {
            int sel = selRow;
            int delta = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                delta = -1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                delta = 1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            {
                delta = -20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            {
                delta = 20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Home))
            {
                sel = 0;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_End))
            {
                sel = rowCount - 1;
            }
            else if (
                ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
            {
                // Enter plays the selected sound (same as the detail Play button)
                if (sel >= 0 && sel < rowCount)
                {
                    const uint16_t id = st.Filtered[sel];
                    if (SoundAvailable(id) && g_ConfigManager.GetSound())
                    {
                        SoundPlay(id);
                    }
                }
            }
            if (delta != 0)
            {
                sel = (sel < 0) ? 0 : std::max(0, std::min(rowCount - 1, sel + delta));
            }
            if (sel >= 0 && sel < rowCount)
            {
                if (sel != prevRow)
                {
                    scrollToSel = true;
                }
                selRow = sel;
                st.Selected = st.Filtered[sel];
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin(rowCount);
        // only pin the scroll target on frames where keyboard nav moved it,
        // so the user can still scroll away freely
        if (scrollToSel && selRow >= 0)
        {
            clipper.IncludeItemsByIndex(selRow, selRow + 1);
        }
        else
        {
            scrollToSel = false;
        }
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const uint16_t id = st.Filtered[row];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                char label[24];
                snprintf(label, sizeof(label), "%u (0x%04X)", id, id);
                if (ImGui::Selectable(label, st.Selected == (int)id))
                {
                    st.Selected = id;
                }
                if (row == selRow && scrollToSel)
                {
                    ImGui::SetScrollFromPosY(
                        ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y - ImGui::GetScrollY(),
                        0.3f);
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(SoundName(id));
                ImGui::TableNextColumn();
                if (st.PlayingId == (int)id)
                {
                    ImGui::TextUnformatted("playing");
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##soundDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (st.Selected < 0)
    {
        ImGui::TextDisabled("Select a sound to preview or export it");
        ImGui::EndChild();
        return;
    }

    const uint16_t id = (uint16_t)st.Selected;
    ImGui::Text("Id: %u (0x%04X)", id, id);
    ImGui::Text("Name: %s", SoundName(id));
    if (!SoundAvailable(id))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "no sound data");
    }
    else if (!g_ConfigManager.GetSound())
    {
        ImGui::TextDisabled("sound is disabled in the client settings");
    }
    if (st.PlayingId == (int)id)
    {
        ImGui::TextUnformatted("Status: playing");
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(!SoundAvailable(id) || !g_ConfigManager.GetSound());
    if (ImGui::Button("Play"))
    {
        SoundPlay(id);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        SoundStopPreview();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save WAV..."))
    {
        std::string error;
        const std::string path = SoundExportWav(id, error);
        st.Status = !path.empty()  ? "Exported " + path :
                    !error.empty() ? error :
                                     std::string("Export failed");
    }
    ImGui::Spacing();
    ImGui::TextWrapped(
        "Exports decode the sound block through the client's "
        "own resolver (MUL and UOP) and write it as 16-bit PCM WAV to "
        "the configured export folder (default: Pictures/Screenshots).");
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Music tab: g_Index.m_MP3 (music/digital/*.mp3 from the client's music
// config). Play goes through CSoundManager::PlayMP3 (which fades out the
// previous lane itself); Stop uses the client's StopMusic. The loop flag is a
// viewer-local toggle (the index entries are not modified). Files missing on
// disk render a clear "file missing" state with Play disabled.
struct MusicTabState
{
    int Selected = -1;
    bool Loop = false;
    int PlayingId = -1;                   // viewer-local: the last track Play was issued for
    std::unordered_map<int, bool> Exists; // lazily resolved file presence
};

MusicTabState &MusicState()
{
    static MusicTabState s;
    return s;
}

bool MusicEntryExists(int index)
{
    MusicTabState &st = MusicState();
    auto found = st.Exists.find(index);
    if (found == st.Exists.end())
    {
        const CIndexMusic &mp3 = g_Index.m_MP3[index];
        bool exists = false;
        if (!mp3.FilePath.empty())
        {
            // plain stdio probe: the viewer never loads the music itself
            if (FILE *file = fopen(fs_path_ascii(fs_path_from(mp3.FilePath)), "rb"))
            {
                fclose(file);
                exists = true;
            }
        }
        found = st.Exists.emplace(index, exists).first;
    }
    return found->second;
}

const char *MusicFileName(int index)
{
    const CIndexMusic &mp3 = g_Index.m_MP3[index];
    if (mp3.FilePath.empty())
    {
        return "(unnamed)";
    }
    const size_t slash = mp3.FilePath.find_last_of("/\\");
    return mp3.FilePath.c_str() + (slash == std::string::npos ? 0 : slash + 1);
}

// Stop uses the client's StopMusic; the viewer-local playing marker is
// cleared here so the "playing" state never goes stale (CurrentMusicIndex is
// not relied upon after a stop).
void MusicStop()
{
    g_SoundManager.StopMusic();
    MusicState().PlayingId = -1;
}

void DrawMusicTab()
{
    MusicTabState &st = MusicState();

    ImGui::Checkbox("Loop", &st.Loop);
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        MusicStop();
    }
    ImGui::SameLine();
    if (!g_ConfigManager.GetMusic())
    {
        ImGui::TextDisabled("music is disabled in the client settings");
    }
    else
    {
        ImGui::TextDisabled("current: %d", g_SoundManager.CurrentMusicIndex);
    }

    const float detailW = 360.0f;
    const float listW = std::max(240.0f, ImGui::GetContentRegionAvail().x - detailW);
    ImGui::BeginChild("##musicList", ImVec2(listW, 0.0f), ImGuiChildFlags_Borders);
    if (ImGui::BeginTable(
            "##musicTable",
            3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        // keyboard navigation while the table child has focus
        int selRow = -1;
        if (st.Selected >= 0 && st.Selected < MAX_MUSIC_DATA_INDEX_COUNT)
        {
            selRow = st.Selected;
        }
        bool scrollToSel = false;
        const int prevRow = selRow;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
        {
            int sel = selRow;
            int delta = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                delta = -1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                delta = 1;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            {
                delta = -20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            {
                delta = 20;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Home))
            {
                sel = 0;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_End))
            {
                sel = MAX_MUSIC_DATA_INDEX_COUNT - 1;
            }
            else if (
                ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
            {
                // Enter plays the selected track (same as the detail Play button)
                if (sel >= 0 && sel < MAX_MUSIC_DATA_INDEX_COUNT)
                {
                    const CIndexMusic &mp3 = g_Index.m_MP3[sel];
                    if (!mp3.FilePath.empty() && MusicEntryExists(sel) &&
                        g_ConfigManager.GetMusic())
                    {
                        g_SoundManager.PlayMP3(mp3.FilePath.c_str(), sel, st.Loop, false);
                        st.PlayingId = sel;
                    }
                }
            }
            if (delta != 0)
            {
                sel = (sel < 0) ?
                          0 :
                          std::max(0, std::min(MAX_MUSIC_DATA_INDEX_COUNT - 1, sel + delta));
            }
            if (sel >= 0 && sel < MAX_MUSIC_DATA_INDEX_COUNT)
            {
                if (sel != prevRow)
                {
                    scrollToSel = true;
                }
                selRow = sel;
                st.Selected = sel;
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin(MAX_MUSIC_DATA_INDEX_COUNT);
        // only pin the scroll target on frames where keyboard nav moved it,
        // so the user can still scroll away freely
        if (scrollToSel && selRow >= 0)
        {
            clipper.IncludeItemsByIndex(selRow, selRow + 1);
        }
        else
        {
            scrollToSel = false;
        }
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const CIndexMusic &mp3 = g_Index.m_MP3[row];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                char label[16];
                snprintf(label, sizeof(label), "%d", row);
                if (ImGui::Selectable(label, st.Selected == row))
                {
                    st.Selected = row;
                }
                if (row == selRow && scrollToSel)
                {
                    ImGui::SetScrollFromPosY(
                        ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y - ImGui::GetScrollY(),
                        0.3f);
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(MusicFileName(row));
                ImGui::TableNextColumn();
                if (mp3.FilePath.empty())
                {
                    ImGui::TextDisabled("not configured");
                }
                else if (!MusicEntryExists(row))
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "file missing");
                }
                else if (st.PlayingId == row)
                {
                    ImGui::TextUnformatted("playing");
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##musicDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (st.Selected < 0)
    {
        ImGui::TextDisabled("Select a track to inspect it");
        ImGui::EndChild();
        return;
    }

    const int index = st.Selected;
    const CIndexMusic &mp3 = g_Index.m_MP3[index];
    ImGui::Text("Id: %d", index);
    ImGui::Text("File: %s", MusicFileName(index));
    ImGui::TextWrapped(
        "Path: %s", mp3.FilePath.empty() ? "(not configured)" : mp3.FilePath.c_str());
    if (mp3.FilePath.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "track not configured");
    }
    else if (!MusicEntryExists(index))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "file missing on disk");
    }

    ImGui::Spacing();
    const bool playable =
        !mp3.FilePath.empty() && MusicEntryExists(index) && g_ConfigManager.GetMusic();
    ImGui::BeginDisabled(!playable);
    if (ImGui::Button("Play"))
    {
        g_SoundManager.PlayMP3(mp3.FilePath.c_str(), index, st.Loop, false);
        st.PlayingId = index;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        MusicStop();
    }
    ImGui::EndChild();
}
} // namespace

namespace
{
// ---------------------------------------------------------------------------
// Multis tab: multi list over g_Index.m_Multi + component table + composed
// preview of the whole structure drawn from static art. The multi blocks are
// parsed read-only exactly like the client's CTarget::LoadMulti (MUL blocks
// and the MultiCollection UOP format), no client state is touched.
// ---------------------------------------------------------------------------

// preview canvas bound: big houses are clamped, the truncation is reported
constexpr int MULTI_CANVAS_MAX = 1024;

struct MultiComponent
{
    uint16_t Graphic = 0;
    int16_t X = 0;
    int16_t Y = 0;
    int16_t Z = 0;
    uint32_t Flags = 0;
};

struct MultiTabState
{
    char Filter[64] = "";
    std::vector<uint32_t> Filtered;
    std::string AppliedFilter = "\1"; // forces the first rebuild
    int Selected = -1;                // multi index (0-based into g_Index.m_Multi)
    // composed preview, rebuilt only when the selection changes
    int BuildMulti = -1;
    std::vector<uint16_t> Canvas; // 16-bit pixels, 0 = empty
    int CanvasW = 0;
    int CanvasH = 0;
    std::vector<MultiComponent> Components;
    int Skipped = 0;        // components with no usable static art
    int Stride = 0;         // per-component record size used by the last parse
    bool Truncated = false; // extents clamped to MULTI_CANVAS_MAX
    float Zoom = 1.0f;
    bool Fit = true;    // zoom-to-fit the preview pane (slider used when off)
    std::string Status; // last export result
};

MultiTabState &MultisState()
{
    static MultiTabState st;
    return st;
}

bool MultiEntryExists(uint32_t id)
{
    if (id >= (uint32_t)g_Index.m_MultiIndexCount)
    {
        return false;
    }
    const CIndexMulti &idx = g_Index.m_Multi[id];
    // UOP entries carry a hash-table block, MUL entries a mapped address (the
    // stream-file lane keeps relative offsets, so 0 is possible) + a size
    return idx.Count != 0 && (idx.UopBlock != nullptr || idx.Address != 0 || idx.DataSize != 0);
}

// Parses the multi's component list, mirroring CTarget::LoadMulti: UOP
// entries (MultiCollection) use the extended per-component format with a
// cliloc list, MUL entries are plain MULTI_BLOCK/MULTI_BLOCK_NEW records
// depending on the client version.
bool MultiParse(uint16_t id, std::vector<MultiComponent> &comps, int *outStride = nullptr)
{
    comps.clear();
    if (outStride != nullptr)
    {
        *outStride = 0;
    }
    if (!MultiEntryExists(id))
    {
        return false;
    }
    const CIndexMulti &idx = g_Index.m_Multi[id];
    if (idx.UopBlock != nullptr)
    {
        std::vector<uint8_t> data = g_FileManager.m_MultiCollection.GetData(idx.UopBlock);
        if (data.empty())
        {
            return false;
        }
        if (outStride != nullptr)
        {
            *outStride = 14; // extended UOP component record (without clilocs)
        }
        CDataReader reader(&data[0], data.size());
        comps.reserve(idx.Count);
        // manual cursor: CDataReader has no end-of-data query on native
        size_t pos = 8; // ID + Count header
        for (uint32_t i = 0; i < idx.Count && pos + 14 <= data.size(); i++)
        {
            MultiComponent mc;
            mc.Graphic = reader.ReadUInt16LE((intptr_t)pos);
            mc.X = reader.ReadInt16LE((intptr_t)(pos + 2));
            mc.Y = reader.ReadInt16LE((intptr_t)(pos + 4));
            mc.Z = (int8_t)reader.ReadInt16LE((intptr_t)(pos + 6));
            mc.Flags = reader.ReadUInt16LE((intptr_t)(pos + 8));
            const uint32_t clilocsCount = reader.ReadUInt32LE((intptr_t)(pos + 10));
            pos += 14 + (size_t)clilocsCount * 4;
            comps.push_back(mc);
        }
        return !comps.empty();
    }

    // derive the stride from the index entry: Count was computed at index
    // time with the file manager's client version, so DataSize / Count is the
    // authoritative record size (a g_Config-based guess can diverge and yield
    // garbage components); accept it only for valid MULTI_BLOCK sizes (12 or
    // 16) and keep the version check as a fallback otherwise
    const int derivedStride =
        (idx.Count > 0 && idx.DataSize >= idx.Count) ? (int)(idx.DataSize / idx.Count) : 0;
    const int fallbackStride =
        g_Config.ClientVersion >= CV_7090 ? sizeof(MULTI_BLOCK_NEW) : sizeof(MULTI_BLOCK);
    int itemOffset =
        (derivedStride == sizeof(MULTI_BLOCK) || derivedStride == sizeof(MULTI_BLOCK_NEW)) ?
            derivedStride :
            fallbackStride;
    if (outStride != nullptr)
    {
        *outStride = itemOffset;
    }
    const uint8_t *base = (const uint8_t *)idx.Address;
    const size_t itemSize = (size_t)itemOffset;
    uint32_t count = idx.Count;
    if (idx.DataSize && (uint64_t)count * itemSize > (uint64_t)idx.DataSize)
    {
        count = (uint32_t)((uint64_t)idx.DataSize / itemSize);
    }
    comps.reserve(count);
    for (uint32_t i = 0; i < count; i++)
    {
        MULTI_BLOCK block;
        memcpy(&block, base + (size_t)i * itemOffset, sizeof(MULTI_BLOCK));
        MultiComponent mc;
        mc.Graphic = block.ID;
        mc.X = block.X;
        mc.Y = block.Y;
        mc.Z = block.Z;
        mc.Flags = block.Flags;
        comps.push_back(mc);
    }
    return !comps.empty();
}

void MultiRebuildFiltered(MultiTabState &st)
{
    std::string filter = st.Filter;
    for (char &c : filter)
    {
        c = (char)tolower(c);
    }
    if (filter == st.AppliedFilter)
    {
        return;
    }
    st.AppliedFilter = filter;
    st.Filtered.clear();
    // an exact id filter (decimal or 0x-hex) narrows to that entry
    bool idFilter = false;
    uint32_t idValue = 0;
    if (!filter.empty())
    {
        const char *text = filter.c_str();
        if (filter.compare(0, 2, "0x") == 0)
        {
            idValue = (uint32_t)strtoul(text + 2, nullptr, 16);
            idFilter = true;
        }
        else if (strspn(text, "0123456789") == filter.size())
        {
            idValue = (uint32_t)strtoul(text, nullptr, 10);
            idFilter = true;
        }
    }
    for (uint32_t i = 0; i < (uint32_t)g_Index.m_MultiIndexCount; i++)
    {
        if (!MultiEntryExists(i))
        {
            continue;
        }
        if (idFilter && i != idValue)
        {
            continue;
        }
        st.Filtered.push_back(i);
    }
    if (st.Selected >= 0 && !MultiEntryExists((uint32_t)st.Selected))
    {
        st.Selected = -1;
    }
}

// Composes the whole multi into a 16-bit canvas: components are stamped in
// painter's order (y, then x), static art decoded once per rebuild through
// the same GetArtPixels run=true path the Statics tab uses. Components whose
// art is missing/nodraw are counted in Skipped.
void MultiRebuildPreview(MultiTabState &st, int id)
{
    st.BuildMulti = id;
    st.Components.clear();
    st.Skipped = 0;
    st.Stride = 0;
    st.Truncated = false;
    st.Canvas.clear();
    st.CanvasW = st.CanvasH = 0;
    if (id < 0 || !MultiParse((uint16_t)id, st.Components, &st.Stride))
    {
        st.Components.clear();
        return;
    }
    // painter's order like the client's map render: y first, then x
    std::stable_sort(
        st.Components.begin(),
        st.Components.end(),
        [](const MultiComponent &a, const MultiComponent &b)
        {
            if (a.Y != b.Y)
            {
                return a.Y < b.Y;
            }
            return a.X < b.X;
        });

    // rejected components (nodraw/land-piece/outlier coords) must not inflate
    // the bounding box, otherwise a single misparsed record shrinks the whole
    // preview to a pixel; compute the extents from accepted components only
    int rawMinX = INT16_MAX;
    int rawMinY = INT16_MAX;
    for (const MultiComponent &mc : st.Components)
    {
        rawMinX = std::min(rawMinX, (int)mc.X);
        rawMinY = std::min(rawMinY, (int)mc.Y);
    }
    std::vector<const MultiComponent *> accepted;
    accepted.reserve(st.Components.size());
    for (const MultiComponent &mc : st.Components)
    {
        const int rx = std::abs((int)mc.X - rawMinX); // reject misparsed outliers
        const int ry = std::abs((int)mc.Y - rawMinY);
        if (mc.Graphic >= 0x4000 || StaticNodraw(mc.Graphic) || rx > 4096 || ry > 4096)
        {
            st.Skipped++;
            continue;
        }
        accepted.push_back(&mc);
    }

    int minX = INT16_MAX;
    int minY = INT16_MAX;
    int maxX = INT16_MIN;
    int maxY = INT16_MIN;
    for (const MultiComponent *mc : accepted)
    {
        minX = std::min(minX, (int)mc->X);
        minY = std::min(minY, (int)mc->Y);
        maxX = std::max(maxX, (int)mc->X);
        maxY = std::max(maxY, (int)mc->Y);
    }
    if (accepted.empty())
    {
        st.Skipped = (int)st.Components.size();
        return;
    }
    if (maxX - minX + 1 > MULTI_CANVAS_MAX || maxY - minY + 1 > MULTI_CANVAS_MAX)
    {
        st.Truncated = true;
        maxX = std::min(maxX, minX + MULTI_CANVAS_MAX - 1);
        maxY = std::min(maxY, minY + MULTI_CANVAS_MAX - 1);
    }
    st.CanvasW = maxX - minX + 1;
    st.CanvasH = maxY - minY + 1;
    st.Canvas.assign((size_t)st.CanvasW * st.CanvasH, 0);

    std::vector<uint16_t> pixels;
    int w = 0;
    int h = 0;
    for (const MultiComponent *mcp : accepted) // already in painter's order
    {
        const MultiComponent &mc = *mcp;
        const int px = mc.X - minX;
        const int py = mc.Y - minY;
        pixels.clear();
        if (!AssetGrid_DecodeStatic(mc.Graphic, pixels, w, h))
        {
            st.Skipped++;
            continue;
        }
        // straight src-over stamp: the decoded art is opaque (color-key 0 =
        // transparent), so later components simply overwrite earlier ones
        for (int y = 0; y < h; y++)
        {
            const int dy = py + y;
            if (dy >= st.CanvasH)
            {
                break;
            }
            for (int x = 0; x < w; x++)
            {
                const int dx = px + x;
                if (dx >= st.CanvasW)
                {
                    break;
                }
                const uint16_t color = pixels[(size_t)y * w + x];
                if (color != 0)
                {
                    st.Canvas[(size_t)dy * st.CanvasW + dx] = color;
                }
            }
        }
    }
}

bool MultiExportCsv(MultiTabState &st, int id)
{
    std::string csv = "Graphic,X,Y,Z,Flags,Name\n";
    char row[192];
    for (const MultiComponent &mc : st.Components)
    {
        TiledataCsvField(csv, StaticTileName(mc.Graphic));
        snprintf(row, sizeof(row), ",%u,%d,%d,%d,0x%08X\n", mc.Graphic, mc.X, mc.Y, mc.Z, mc.Flags);
        csv += row;
    }
    char name[64];
    snprintf(name, sizeof(name), "multi_%u_components.csv", id);
    return TiledataExportCsv(name, csv);
}

void DrawMultisTab()
{
    MultiTabState &st = MultisState();
    MultiRebuildFiltered(st);
    if (g_Index.m_MultiIndexCount == 0)
    {
        ImGui::TextDisabled("No multis available (multi.mul / MultiCollection.uop not loaded)");
        return;
    }

    // filter bar lives outside the list child so the tab stays interactive
    // even while the filter matches nothing
    ImGui::SetNextItemWidth(230.0f);
    ImGui::InputText("##multisFilter", st.Filter, sizeof(st.Filter));
    if (st.Filtered.empty())
    {
        ImGui::TextDisabled("No multis match this filter");
        return;
    }

    ImGui::BeginChild("##multisList", ImVec2(230.0f, 0.0f), ImGuiChildFlags_None);
    const int rowCount = (int)st.Filtered.size();
    int selRow = -1; // keyboard nav: track the selected multi's row
    for (int r = 0; r < rowCount && selRow < 0; r++)
    {
        if (st.Filtered[r] == (uint32_t)st.Selected)
        {
            selRow = r;
        }
    }
    bool scrollToSel = false;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && rowCount > 0)
    {
        int sel = selRow;
        int delta = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        {
            delta = -1;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        {
            delta = 1;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
        {
            delta = -20;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
        {
            delta = 20;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_Home))
        {
            sel = 0;
        }
        else if (ImGui::IsKeyPressed(ImGuiKey_End))
        {
            sel = rowCount - 1;
        }
        if (delta != 0)
        {
            sel = (sel < 0) ? 0 : std::max(0, std::min(rowCount - 1, sel + delta));
        }
        if (sel != selRow && sel >= 0 && sel < rowCount)
        {
            st.Selected = (int)st.Filtered[sel];
            selRow = sel;
            scrollToSel = true;
        }
        else if (
            ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
        {
            // Enter selects the highlighted entry (same as a click)
            if (sel >= 0 && sel < rowCount)
            {
                st.Selected = (int)st.Filtered[sel];
                selRow = sel;
                scrollToSel = true;
            }
        }
    }
    ImGui::BeginChild("##multisListScroll", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    ImGuiListClipper clipper;
    clipper.Begin((int)st.Filtered.size());
    // scroll the selected row into view only when keyboard nav moved it
    if (scrollToSel && selRow >= 0)
    {
        clipper.IncludeItemsByIndex(selRow, selRow + 1);
    }
    else
    {
        scrollToSel = false;
    }
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            const uint32_t id = st.Filtered[row];
            char label[64];
            snprintf(
                label, sizeof(label), "0x%04X (%u) - %u items", id, id, g_Index.m_Multi[id].Count);
            if (ImGui::Selectable(label, st.Selected == (int)id))
            {
                st.Selected = (int)id;
            }
            if (row == selRow && scrollToSel)
            {
                ImGui::SetScrollFromPosY(
                    ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y - ImGui::GetScrollY(),
                    0.3f);
            }
        }
    }
    clipper.End();
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##multisDetail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (st.Selected < 0)
    {
        ImGui::TextDisabled("Select a multi to inspect it");
        ImGui::EndChild();
        return;
    }

    // rebuild only when the selection changed; the composed canvas is cached
    // as an ImGui texture keyed by (Multi, id, size) until evicted
    if (st.BuildMulti != st.Selected)
    {
        MultiRebuildPreview(st, st.Selected);
    }
    const CIndexMulti &idx = g_Index.m_Multi[st.Selected];
    ImGui::Text(
        "Multi 0x%04X (%d) - %d components%s%s",
        st.Selected,
        st.Selected,
        (int)st.Components.size(),
        st.Skipped != 0 ? ", " : "",
        st.Skipped != 0 ? std::to_string(st.Skipped).append(" skipped").c_str() : "");
    ImGui::TextDisabled(
        "Source: %s", idx.UopBlock != nullptr ? "MultiCollection.uop" : "multi.mul");
    ImGui::TextDisabled(
        "%d components, %d skipped, stride %d", (int)st.Components.size(), st.Skipped, st.Stride);
    if (st.Components.empty())
    {
        ImGui::TextDisabled("This multi has no readable components");
        ImGui::EndChild();
        return;
    }

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Zoom", &st.Zoom, 0.05f, 8.0f, "%.2fx");
    ImGui::SameLine();
    ImGui::Checkbox("Fit", &st.Fit);
    ImGui::SameLine();
    if (ImGui::Button("Export PNG"))
    {
        char name[64];
        snprintf(name, sizeof(name), "multi_%u_preview.png", st.Selected);
        const std::string path = AssetGrid_ExportPngFile(name, st.Canvas, st.CanvasW, st.CanvasH);
        st.Status = path.empty() ? "Export failed" : "Exported " + path;
    }
    ImGui::SameLine();
    if (ImGui::Button("Export CSV"))
    {
        MultiExportCsv(st, st.Selected);
    }
    if (!st.Status.empty())
    {
        ImGui::TextDisabled("%s", st.Status.c_str());
    }
    if (st.Truncated)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
            "Preview truncated to %dx%d (multi is larger)",
            st.CanvasW,
            st.CanvasH);
    }

    if (!st.Canvas.empty() && st.CanvasW > 0)
    {
        AssetTextureKey key{}; // zero-init: uninitialized Hue/Extra = garbage tint + cache churn
        key.Kind = AssetTextureKind::Multi;
        key.Id = (uint16_t)st.Selected;
        key.Width = (uint16_t)st.CanvasW;
        key.Height = (uint16_t)st.CanvasH;
        const ImTextureRef ref =
            AssetTextureCache_Acquire(key, st.Canvas.data(), st.CanvasW, st.CanvasH, 0);
        if (!AssetTextureCache_IsReady(ref))
        {
            ImGui::TextDisabled("Preview unavailable");
        }
        else
        {
            // default zoom = fit the preview pane; the slider takes over when
            // "Fit" is off (big houses at 1x would be a few screen pixels)
            const float paneW = std::max(1.0f, ImGui::GetContentRegionAvail().x);
            const float paneH = std::max(
                1.0f,
                ImGui::GetContentRegionAvail().y -
                    ImGui::GetTextLineHeightWithSpacing()); // leave the status line room
            float effZoom = st.Zoom;
            if (st.Fit)
            {
                const float fit = std::min(paneW / st.CanvasW, paneH / st.CanvasH);
                effZoom = std::min(8.0f, std::max(0.05f, fit));
            }
            ImGui::Image(ref, ImVec2(st.CanvasW * effZoom, st.CanvasH * effZoom));
            ImGui::TextDisabled(
                "canvas %dx%d, scale %.2fx%s",
                st.CanvasW,
                st.CanvasH,
                effZoom,
                st.Fit ? " (fit)" : "");
        }
    }
    ImGui::Separator();
    ImGui::TextDisabled("Graphic | X | Y | Z | Flags");
    ImGui::BeginChild("##multisComponents", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
    if (ImGui::BeginTable("##multisTable", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
    {
        ImGui::TableSetupColumn("Graphic");
        ImGui::TableSetupColumn("X");
        ImGui::TableSetupColumn("Y");
        ImGui::TableSetupColumn("Z");
        ImGui::TableSetupColumn("Flags");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        ImGuiListClipper tableClipper;
        tableClipper.Begin((int)st.Components.size());
        while (tableClipper.Step())
        {
            for (int row = tableClipper.DisplayStart; row < tableClipper.DisplayEnd; row++)
            {
                const MultiComponent &mc = st.Components[row];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                char glabel[80];
                snprintf(
                    glabel, sizeof(glabel), "0x%04X %.20s", mc.Graphic, StaticTileName(mc.Graphic));
                ImGui::TextUnformatted(glabel);
                ImGui::TableNextColumn();
                ImGui::Text("%d", mc.X);
                ImGui::TableNextColumn();
                ImGui::Text("%d", mc.Y);
                ImGui::TableNextColumn();
                ImGui::Text("%d", mc.Z);
                ImGui::TableNextColumn();
                char flags[16];
                snprintf(flags, sizeof(flags), "0x%08X", mc.Flags);
                ImGui::TextUnformatted(flags);
                if (ImGui::IsItemHovered() && mc.Flags != 0)
                {
                    ImGui::BeginTooltip();
                    AssetGrid_DrawFlagNames(mc.Flags);
                    ImGui::EndTooltip();
                }
            }
        }
        tableClipper.End();
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::EndChild();
}
} // namespace

bool AssetViewer_Toggle()
{
    return AssetViewer_Show(!AssetViewer_IsVisible());
}

bool AssetViewer_Show(bool show)
{
    if (s_Shutdown)
    {
        return false;
    }

    s_Visible = show;
    if (!show)
    {
        SoundStopPreview(); // no preview outlives the viewer window
        MusicStop();        // ... and no music either
    }
    return true;
}

bool AssetViewer_IsVisible()
{
    return s_Visible;
}

void AssetViewer_Draw()
{
    // closed: submit nothing (mirrors the assistant shell early-out in
    // assistantui.cpp); the draw runs inside the assistant's ImGui frame
    if (!s_Visible || ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Asset Viewer", &s_Visible))
    {
        ImGui::End();
        return;
    }
    if (!s_Visible)
    {
        // closed through the title bar X: stop any playing preview + music
        SoundStopPreview();
        MusicStop();
    }

    if (!UoDataAvailable())
    {
        ImGui::TextWrapped(
            "No UO data loaded: configure a UO data path in the client "
            "settings and log in to browse its assets.");
        ImGui::End();
        return;
    }

    // footer row, drawn left-to-right as ONE row with the checkbox first so
    // its visibility never depends on the remaining width: placed here, right
    // under the header and above the tab bar, so it reads as an extra top line
    bool hideNodraw = AssetGrid_GetHideNodraw();
    if (ImGui::Checkbox("Hide nodraw", &hideNodraw))
    {
        AssetGrid_SetHideNodraw(hideNodraw);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputText("##ExportDir", AssetGrid_ExportDirOverride(), ASSETVIEWER_EXPORT_DIR_MAX);
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Export folder override (absolute path);\nempty = default (%s)",
            AssetViewerExportDir().c_str());
    }
    ImGui::SameLine();
    {
        // hint ellipsized to whatever width is left after the two items
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float avail = ImGui::GetContentRegionAvail().x - spacing - 4.0f; // reserve a few px
        ImGui::TextDisabled(
            "%s",
            EllipsizeToFit(
                (std::string("Export folder (empty = default: ") + AssetViewerExportDir().c_str() +
                 ")")
                    .c_str(),
                std::max(1.0f, avail))
                .c_str());
    }
    ImGui::Separator();

    if (ImGui::BeginTabBar("##AssetViewerTabs", ImGuiTabBarFlags_None))
    {
        const ImGuiTabItemFlags staticsFlags = (s_PendingTab == PendingStatics) ?
                                                   ImGuiTabItemFlags_SetSelected :
                                                   ImGuiTabItemFlags_None;
        const ImGuiTabItemFlags landFlags =
            (s_PendingTab == PendingLand) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        const ImGuiTabItemFlags tiledataFlags = (s_PendingTab == PendingTiledata) ?
                                                    ImGuiTabItemFlags_SetSelected :
                                                    ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem("Statics", nullptr, staticsFlags))
        {
            DrawStaticsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Land", nullptr, landFlags))
        {
            DrawLandTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Tiledata", nullptr, tiledataFlags))
        {
            DrawTiledataTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Hues"))
        {
            DrawHuesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Gumps"))
        {
            DrawGumpsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Effects"))
        {
            DrawEffectsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Animations"))
        {
            DrawAnimationsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sound"))
        {
            DrawSoundTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Music"))
        {
            DrawMusicTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Multis"))
        {
            DrawMultisTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void AssetViewer_Shutdown()
{
    s_Visible = false;
    s_Shutdown = true;
    SoundStopPreview();
    AnimShutdown();
    AssetTextureCache_Shutdown();
}

#endif // XUO_ASSISTANT
