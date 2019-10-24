#ifndef _UI_H_
#define _UI_H_

#include "gfx.h"
#include "imgui/imgui.h"

struct ui_context
{
    win_context *win = nullptr;
    bool show_stats_window = false;
    bool show_demo_window = false;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
};

ui_context ui_init(win_context &win);
void ui_process_event(ui_context &, const SDL_Event *event);
void ui_draw(ui_context &ctx);
void ui_update(ui_context &ctx);
void ui_shutdown(ui_context &ctx);

#endif // _UI_H_
