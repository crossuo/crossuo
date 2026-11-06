# BUI (Blender) theme for Dear ImGui

Vendored subset of [ppmpreetham/BUI](https://github.com/ppmpreetham/BUI)
("Blender's GPU Accelerated UI") that provides Blender's look for Dear ImGui:

- `imgui_bui_style.cpp/.h` - adapted from `bindings/imgui/imgui_bui_style.cpp`.
  Blender's default-theme colors, widget-unit metrics and the
  `bui::StyleBlender()` / `bui::AddBlenderFont()` / `bui::BlenderFontSize()`
  entry points. The upstream BUI window/render backend (`imgui_impl_bui`),
  kit theme lookups (`BUI_kit.h`) and font provider (`GPU_raster.h`) are not
  vendored; CrossUO keeps its own SDL/OpenGL/DX11 backends and uses the
  hard-coded dark-theme defaults.
- `font_inter.h` - from `gpu/extern/font_inter.h`: the Inter variable font
  (Blender's UI font) embedded as a byte array.
- `LICENSE` - upstream license (README declares GPL-2.0-or-later; the shipped
  LICENSE file is GPL-3).

Used by `external/gfx/ui.cpp` (`ui_init` / `ui_style_launcher`) so
xuolauncher, xuoassist and assetviewer share the Blender theme.
