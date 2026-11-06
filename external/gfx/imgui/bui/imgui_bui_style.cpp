// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Blender Foundation / ppmpreetham BUI
//
// dear imgui on BUI: Blender's look. Colors from Blender's default theme
// (release/datafiles/userdef/userdef_default_theme.c, the values BUI's kit
// theme uses), metrics from its widget unit (20 px at scale 1) and uiStyle
// spacing (editors/interface/interface_style.cc), and its UI font, Inter, at
// its UI text size.
//
// Adapted from https://github.com/ppmpreetham/BUI
// bindings/imgui/imgui_bui_style.cpp for CrossUO's vendored ImGui 1.92.9:
// the BUI backend (imgui_impl_bui.h), kit theme lookups (BUI_kit.h) and font
// provider (GPU_raster.h) were dropped in favor of the embedded Inter font
// and the hard-coded dark-theme defaults. The 1.93-only
// ImGuiCol_CheckboxSelectedBg entry was removed.

#include "imgui_bui_style.h"

#include "../imgui.h"
#ifndef IMGUI_DISABLE

#include "../imgui_internal.h" // ImMin, ImMax, ImFormatString, IM_ROUND
#include "font_inter.h"           // g_font_inter_variable (Inter variable font)

#include <cmath>
#include <cstdint>
#include <cstring>

// 0xRRGGBBAA (Blender's RGBA() theme notation)
static ImVec4 Rgba(uint32_t c)
{
    return ImVec4(((c >> 24) & 0xFF) / 255.0f, ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f);
}

// Blender's hover highlight (interface_widgets.cc widget_active_color): HSL saturation
// x1.15, lightness x1.2 (light text on a dark widget).
static ImVec4 Hover(ImVec4 c)
{
    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(c.x, c.y, c.z, h, s, v);
    // RGB -> HSL lightness / saturation from HSV
    float l = v * (1.0f - s * 0.5f);
    float sl = (l > 0.0f && l < 1.0f) ? (v - l) / ImMin(l, 1.0f - l) : 0.0f;
    l = ImMin(l * 1.2f, 1.0f);
    sl = ImMin(sl * 1.15f, 1.0f);
    // HSL -> HSV -> RGB
    const float nv = l + sl * ImMin(l, 1.0f - l);
    const float ns = nv > 0.0f ? 2.0f * (1.0f - l / nv) : 0.0f;
    ImVec4 out(0, 0, 0, c.w);
    ImGui::ColorConvertHSVtoRGB(h, ns, nv, out.x, out.y, out.z);
    return out;
}

// Inter's hhea ascent - descent over its em (ImGui sizes fonts by ascent - descent).
static float InterHeightPerEm()
{
    const int len = (int)sizeof(g_font_inter_variable);
    const unsigned char *f = g_font_inter_variable;
    float ratio = 2472.0f / 2048.0f; // Inter 4: (1984 + 488) / 2048
    if (f == nullptr || len < 12)
        return ratio;
    auto u16 = [&](int o) { return (int)((f[o] << 8) | f[o + 1]); };
    auto u32 = [&](int o) { return (uint32_t)((f[o] << 24) | (f[o + 1] << 16) | (f[o + 2] << 8) | f[o + 3]); };
    const int ntables = u16(4);
    int head = -1, hhea = -1;
    for (int i = 0; i < ntables && 12 + 16 * i + 16 <= len; i++)
    {
        const int rec = 12 + 16 * i;
        const int off = (int)u32(rec + 8);
        if (memcmp(f + rec, "head", 4) == 0) head = off;
        if (memcmp(f + rec, "hhea", 4) == 0) hhea = off;
    }
    if (head < 0 || hhea < 0 || head + 20 > len || hhea + 8 > len)
        return ratio;
    const int upem = u16(head + 18);
    const int asc = (int16_t)u16(hhea + 4), desc = (int16_t)u16(hhea + 6);
    if (upem > 0 && asc - desc > 0)
        ratio = (float)(asc - desc) / (float)upem;
    return ratio;
}

namespace bui
{

float BlenderFontSize(float scale)
{
    // UI_DEFAULT_TEXT_POINTS (11) at 72 dpi * scale: an 11 px em (BUI's bk_font_size)
    static const float per_em = InterHeightPerEm();
    return 11.0f * per_em * scale;
}

ImFont *AddBlenderFont(float size_pixels)
{
    const int len = (int)sizeof(g_font_inter_variable);
    const void *data = g_font_inter_variable;
    if (data == nullptr || len <= 0)
        return nullptr;
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false; // static in this file's translation unit
    ImFormatString(cfg.Name, IM_COUNTOF(cfg.Name), "Inter (Blender UI)");
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<void *>(data), len,
                                                      size_pixels > 0.0f ? size_pixels : BlenderFontSize(1.0f), &cfg);
}

void StyleBlender(ImGuiStyle *dst, float scale)
{
    ImGuiStyle &style = dst ? *dst : ImGui::GetStyle();
    style = ImGuiStyle();
    ImGui::StyleColorsDark(&style);

    // -- metrics, at scale 1 (U = widget unit, 20 px) --
    const float font = BlenderFontSize(1.0f);
    const float unit = 20.0f;
    style.FontSizeBase = font;
    style.FontScaleDpi = scale;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 0.4f * unit * 0.5f;          // panel_roundness 0.4 (panel corners)
    style.WindowBorderSize = 1.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);         // panel headers: left
    style.WindowMenuButtonPosition = ImGuiDir_Left;      // the collapse arrow first, like panels
    style.ChildRounding = 0.2f * unit;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.2f * unit;                   // wcol_menu_back.roundness
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(6.0f, ImMax(1.0f, (unit - font) * 0.5f)); // frames one widget unit tall
    style.FrameRounding = 0.2f * unit;                   // wcol_*.roundness 0.2 * widget unit
    style.FrameBorderSize = 1.0f;                        // widget outline
    style.ItemSpacing = ImVec2(8.0f, 2.0f);              // uiStyle buttonspacex / buttonspacey
    style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);         // uiStyle templatespace
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = unit;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 0.45f * unit + 2.0f;           // V2D_SCROLL_WIDTH
    style.ScrollbarRounding = 0.5f * unit;               // wcol_scroll.roundness 0.5: round ends
    style.ScrollbarPadding = 2.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 0.2f * unit - 1.0f;             // inside the frame outline
    style.TabRounding = 0.2f * unit;                     // wcol_tab.roundness
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 1.0f;
    style.TabBarOverlineSize = 1.0f;
    style.MenuItemRounding = 0.2f * unit;
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = ImVec2(unit, 2.0f);
    style.TreeLinesSize = 1.0f;
    style.DisabledAlpha = 0.5f;                          // widget_alpha_factor for disabled buttons
    style.ScaleAllSizes(scale);
    // Frames exactly one widget unit tall: ImGui rounds the font to whole pixels (13 px at
    // scale 1) and ScaleAllSizes() truncates the padding, which made frames 19 px.
    style.FramePadding.y = ImMax(1.0f, (IM_ROUND(unit * scale) - IM_ROUND(font * scale)) * 0.5f);

    // -- colors: Blender's dark theme defaults (BUI kit theme defaults) --
    auto A = [](ImVec4 c, float a) { return ImVec4(c.x, c.y, c.z, a); };
    const ImVec4 text = Rgba(0xe6e6e6ffu);          // wcol_regular.text
    const ImVec4 region = Rgba(0x303030ffu);        // space back (region background)
    const ImVec4 inner = Rgba(0x545454ffu);         // wcol_regular / num / option .inner
    const ImVec4 outline = Rgba(0x3d3d3dffu);       // wcol_*.outline
    const ImVec4 select = Rgba(0x4772b3ffu);        // inner_sel: the selection blue
    const ImVec4 select_menu = Rgba(0x4772b3b3u);   // wcol_menu.inner_sel
    const ImVec4 panel_header = Rgba(0x3d3d3dffu);
    const ImVec4 dark = Rgba(0x1d1d1dffu);          // wcol_tab.inner
    ImVec4 *c = style.Colors;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = Rgba(0x999999ffu); // muted
    c[ImGuiCol_WindowBg] = region;
    c[ImGuiCol_ChildBg] = Rgba(0x00000000);
    c[ImGuiCol_PopupBg] = Rgba(0x181818ffu);
    c[ImGuiCol_Border] = outline;
    c[ImGuiCol_BorderShadow] = Rgba(0x00000000);
    c[ImGuiCol_FrameBg] = inner;                          // wcol_num.inner
    c[ImGuiCol_FrameBgHovered] = Hover(inner);
    c[ImGuiCol_FrameBgActive] = Rgba(0x222222ffu); // dragging / editing
    c[ImGuiCol_TitleBg] = dark;
    c[ImGuiCol_TitleBgActive] = panel_header;
    c[ImGuiCol_TitleBgCollapsed] = A(dark, dark.w * 0.8f);
    c[ImGuiCol_MenuBarBg] = dark;
    c[ImGuiCol_ScrollbarBg] = Rgba(0x22222200);           // wcol_scroll.inner
    c[ImGuiCol_ScrollbarGrab] = inner;                    // wcol_scroll.item
    c[ImGuiCol_ScrollbarGrabHovered] = Hover(inner);
    c[ImGuiCol_ScrollbarGrabActive] = Hover(Hover(inner));
    c[ImGuiCol_CheckMark] = Rgba(0xffffffffu);
    c[ImGuiCol_SliderGrab] = Rgba(0x4772b3ffu);
    c[ImGuiCol_SliderGrabActive] = Hover(c[ImGuiCol_SliderGrab]);
    c[ImGuiCol_Button] = inner;                           // wcol_regular.inner
    c[ImGuiCol_ButtonHovered] = Hover(inner);
    c[ImGuiCol_ButtonActive] = select;                    // wcol_regular.inner_sel
    c[ImGuiCol_Header] = panel_header;                    // panel_header (collapsing headers)
    c[ImGuiCol_HeaderHovered] = select_menu;              // wcol_menu.inner_sel (menu hover)
    c[ImGuiCol_HeaderActive] = select;                    // wcol_menu_item.inner_sel
    c[ImGuiCol_Separator] = ImVec4(text.x, text.y, text.z, 0.15f);
    c[ImGuiCol_SeparatorHovered] = select_menu;
    c[ImGuiCol_SeparatorActive] = select;
    c[ImGuiCol_ResizeGrip] = Rgba(0xffffff1a);
    c[ImGuiCol_ResizeGripHovered] = select_menu;
    c[ImGuiCol_ResizeGripActive] = select;
    c[ImGuiCol_InputTextCursor] = Rgba(0x71a8ffffu); // widget_text_cursor
    c[ImGuiCol_Tab] = dark;                               // wcol_tab.inner
    c[ImGuiCol_TabHovered] = panel_header;
    c[ImGuiCol_TabSelected] = Rgba(0x303030ffu);
    c[ImGuiCol_TabSelectedOverline] = select;
    c[ImGuiCol_TabDimmed] = dark;
    c[ImGuiCol_TabDimmedSelected] = c[ImGuiCol_TabSelected];
    c[ImGuiCol_TabDimmedSelectedOverline] = A(select, 128.0f / 255.0f);
    c[ImGuiCol_PlotLines] = Rgba(0xccccccffu);
    c[ImGuiCol_PlotLinesHovered] = Rgba(0xffffffff);
    c[ImGuiCol_PlotHistogram] = Rgba(0x4772b3ffu);
    c[ImGuiCol_PlotHistogramHovered] = Hover(c[ImGuiCol_PlotHistogram]);
    c[ImGuiCol_TableHeaderBg] = panel_header;
    c[ImGuiCol_TableBorderStrong] = dark;
    c[ImGuiCol_TableBorderLight] = outline;
    c[ImGuiCol_TableRowBg] = Rgba(0x00000000);
    c[ImGuiCol_TableRowBgAlt] = Rgba(0xffffff0a);
    c[ImGuiCol_TextLink] = Rgba(0x6fa9e6ff);              // theme link
    c[ImGuiCol_TextSelectedBg] = A(select, 0.8f);
    c[ImGuiCol_TreeLines] = Rgba(0xffffff1f);
    c[ImGuiCol_DragDropTarget] = select;
    c[ImGuiCol_DragDropTargetBg] = A(select, 0.2f);
    c[ImGuiCol_UnsavedMarker] = text;
    c[ImGuiCol_NavCursor] = select;
    c[ImGuiCol_NavWindowingHighlight] = Rgba(0xffffffb3);
    c[ImGuiCol_NavWindowingDimBg] = Rgba(0x00000033);
    c[ImGuiCol_ModalWindowDimBg] = Rgba(0x00000059);
}

} // namespace bui

#endif // #ifndef IMGUI_DISABLE
