// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: Blender Foundation / ppmpreetham BUI
//
// Blender (BUI) theme for Dear ImGui: colors from Blender's default theme,
// metrics from its widget unit (20 px at scale 1) and its UI font (Inter).
//
// Adapted from https://github.com/ppmpreetham/BUI bindings/imgui/imgui_bui_style.cpp
// (the BUI window/render backend and its kit theme lookups were dropped; the
// hard-coded dark-theme defaults are used, matching BUI's defaults).

#ifndef BUI_IMGUI_STYLE_H
#define BUI_IMGUI_STYLE_H

struct ImGuiStyle;
struct ImFont;

namespace bui
{
// Inter's UI text size: 11 pt at 72 dpi scaled by the font's height per em.
float BlenderFontSize(float scale = 1.0f);

// Loads the Inter variable font embedded in font_inter.h into the current
// ImGui font atlas (call before the atlas is built, e.g. in ui_init).
ImFont *AddBlenderFont(float size_pixels = 0.0f);

// Applies Blender's style metrics and colors (dark preset) to dst, or the
// current context style when dst is null.
void StyleBlender(ImGuiStyle *dst = nullptr, float scale = 1.0f);
} // namespace bui

#endif // BUI_IMGUI_STYLE_H
