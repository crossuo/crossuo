// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef _UI_H_
#define _UI_H_

#include "gfx.h"
#include "imgui/imgui.h"
#include "imgui/IconsForkAwesome.h"

struct ui_context
{
    win_context *win = nullptr;
    void *userdata = nullptr;
    bool viewports_enabled = true;
    bool show_stats_window = false;
    bool show_demo_window = false;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
};

ui_context ui_init(win_context &win, bool enable_viewports = true);
// shared CrossUO launcher/assistant theme: colors + style metrics applied on
// top of ui_init's default style (fonts and app-specific tweaks stay local)
void ui_style_launcher();
void ui_process_event(ui_context &, const SDL_Event *event);
void ui_newframe(ui_context &ctx);
void ui_draw(ui_context &ctx);
void ui_update(ui_context &ctx);
void ui_shutdown(ui_context &ctx);

#endif // _UI_H_
