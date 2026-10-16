// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#if defined(XUO_ASSISTANT)

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include <external/gfx/imgui/imgui.h>

#include "AssetTextureCache.h"

// The shared virtual-scrolling tile-grid browser used by the asset viewer
// tabs (statics, land, gumps, effects, ...). The widget is source-agnostic:
// a tab supplies an item count plus per-graphic pixel decoder / tiledata name
// / nodraw callbacks, and the grid takes care of the thumbnail size slider,
// the id/name filter, visible-only decoding, selection and export.

struct AssetGridSource
{
    AssetTextureKind Kind = AssetTextureKind::Static;
    uint32_t Count = 0; // graphics are addressed 0..Count-1
    // Decodes the 16-bit pixels for a graphic (false = no art available).
    // The returned buffer is sized w*h and must not carry a stride.
    std::function<bool(uint16_t, std::vector<uint16_t> &, int &, int &)> Decode;
    // Tiledata name (may be empty; used by the filter and the status line).
    std::function<const char *(uint16_t)> NameOf;
    // True when the entry has no art at all (index Address == 0): the grid
    // draws a placeholder instead of decoding.
    std::function<bool(uint16_t)> Nodraw;
    const char *ExportPrefix = "asset"; // exported file name component
};

struct AssetGridState
{
    char Filter[64] = {};
    float ThumbSize = 64.0f; // 32..256 thumbnail slider
    float DetailZoom = 2.0f; // detail preview zoom (pixels per sprite pixel)
    uint16_t Hue = 0;        // partial hue id (1-based, 0 = none)
    ImVec4 Background = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    int Selected = -1; // selection as a raw graphic id (-1 = none)
    std::vector<uint32_t> Filtered;
    // sentinel ("\1") so the initial frame always rebuilds with the empty
    // filter ("show all"); mirrors HuesTabState/AnimTabState in AssetViewer.cpp
    std::string AppliedFilter = "\1";
    bool AppliedHideNodraw = false; // mirrors the global hide-nodraw toggle
    int NavIndex = -1; // keyboard-nav index into Filtered (-1 = none); kept in sync with Selected
    // one-shot scroll request: scroll the grid to the row showing this graphic
    // id (cross-tab jumps); consumed by the next DrawBrowser frame
    int ScrollToId = -1;
    char Progress[192] = {}; // last "Export All (filtered)" result line
};

// Draws the browser (toolbar + scrolled grid + status line) into the current
// region. Reports the current selection id through outSelected (when not
// null) and sets *outActivated on double-click/Enter (detail pane trigger).
void AssetGrid_DrawBrowser(
    const AssetGridSource &src, AssetGridState &state, uint16_t *outSelected, bool *outActivated);

// size of the export-folder override buffer (see AssetGrid_ExportDirOverride)
#define ASSETVIEWER_EXPORT_DIR_MAX 256

// Global "Hide nodraw" toggle (session-persistent, shared by every grid tab):
// when enabled, cells whose source reports Nodraw() are not rendered and
// cannot be reached by keyboard navigation.
void AssetGrid_SetHideNodraw(bool hide);
bool AssetGrid_GetHideNodraw();

// The session-persistent export folder override (owned here); empty = use the
// default (Pictures/Screenshots). Absolute paths only.
char *AssetGrid_ExportDirOverride();

// Resolves the export directory: the override when non-empty, otherwise the
// user's pictures folder + "Screenshots" (XDG_PICTURES_DIR / xdg-user-dir /
// $HOME/Pictures on Linux, %USERPROFILE%\\Pictures on Windows, $HOME/Pictures
// on macOS); the directory is created on first export.
std::string AssetViewerExportDir();

// Pixel decoders for the two art families: statics go through the RLE
// (run = true) path of GetArtPixels over g_Index.m_Static, land tiles through
// the raw 44x44 branch over g_Index.m_Land. Return false when the entry has
// no usable art (nodraw, missing data, decode failure).
bool AssetGrid_DecodeStatic(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h);
bool AssetGrid_DecodeLand(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h);

// Writes one tile as a PNG (RGBA) into the export directory; returns the
// written path (empty on failure) and logs it.
std::string AssetGrid_ExportPng(
    const char *prefix, uint16_t id, const std::vector<uint16_t> &pixels, int w, int h);

// Writes a 16-bit pixel buffer as a PNG (RGBA) to <export dir>/<relPath>
// (creating parent folders); returns the written path (empty on failure).
std::string
AssetGrid_ExportPngFile(const char *relPath, const std::vector<uint16_t> &pixels, int w, int h);

// Batch export of the currently filtered set (capped, with a result line
// written into progress for the toolbar).
void AssetGrid_ExportFilteredPngs(
    const AssetGridSource &src, const AssetGridState &state, char *progress, size_t progressSize);

// Decode-on-miss thumbnail texture (size x size, centered, hue 0) for the
// art families, shared with the grid browser (same dim cache and texture
// cache). Returns a null reference while the art is missing or not decoded.
ImTextureRef AssetGrid_AcquireThumb(AssetTextureKind kind, uint16_t id, int size);

// Appends the tiledata flag names of `flags` (one per line) to the current
// ImGui text output; nothing is drawn when no bit is set.
void AssetGrid_DrawFlagNames(uint64_t flags);

// The tiledata flag word is fully named: bits 0..AssetGrid_FlagBits()-1 map
// to AssetGrid_FlagBitName(bit) ("UnknownN" for the reserved bits), which
// lets other tabs render the whole word as read-only checkboxes.
int AssetGrid_FlagBits();
const char *AssetGrid_FlagBitName(int bit);

#endif // XUO_ASSISTANT
