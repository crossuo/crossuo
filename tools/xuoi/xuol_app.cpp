// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Launcher

#include <stdio.h>
#include <sstream> // used by cfg_converters.h via cfg_loader.h
#include <vector>

#include <external/gfx/gfx.h>
#include <external/gfx/ui.h>
#include <external/gfx/sokol_gfx.h>
#include <external/gfx/imgui/imgui.h>
#include <external/inih.h>
#include <xuocore/http.h>
#include <xuocore/common.h>

#include "icon_launcher.h"
#include "accounts.h"
#include "shards.h"
#include "widgets.h"
#include "ui_model.h"
#include "ui_shards.h"

#include "xuol_app.h"
#include "white_label.h"

// window identity for the standalone launcher host (the embedded host maps it
// to its own title, see src/LauncherWindow.cpp)
#define XUOL_AGENT_NAME "EAMythic Patch Client"

namespace xuol {

static ui_context s_ui;
static win_context *s_win = nullptr;

static bool s_launcher_close_intent = false;
#if defined(XUO_LAUNCHER_EMBEDDED)
static bool s_reload_request = false;
#endif
const fs_path &xuol_data_path()
{
    static bool initialized = false;
    static fs_path dir;
    if (initialized)
        return dir;

    dir = fs_path_join(fs_path_appdata(), "xuolauncher");
    if (!fs_path_create(dir))
    {
        LOG_ERROR("failed to create directory: %s", fs_path_ascii(dir));
        return dir;
    }

    initialized = true;
    return dir;
}

#define CFG_NAME launcher
#define CFG_SECTION_FILTER_NAME "global"
#define CFG_DEFINITION "cfg_launcher.h"
#include "cfg_loader.h"

static launcher::data s_config;

launcher::entry &config()
{
    return s_config.entries[0];
}

void load_config()
{
    const auto fname = fs_path_join(xuol_data_path(), "xuolauncher.cfg");

    LOG_INFO("loading settings from %s", fs_path_ascii(fname));
    auto fp = fs_open(fname, FS_READ);
    launcher::cfg(fp, s_config);
    if (fp)
        fs_close(fp);
    if (s_config.entries.size())
    {
        launcher::dump(&s_config.entries[0]);
    }
    else
    {
        s_config.entries.push_back(launcher::default_entry());
    }
}

void write_config(void *_fp)
{
    auto fp = (FILE *)_fp;
    const auto cur = config();
    launcher::write(fp, cur, "Global");
}

void save_config()
{
    const auto fname = fs_path_join(xuol_data_path(), "xuolauncher.cfg");
    auto fp = fs_open(fname, FS_WRITE);
    if (!fp)
    {
        LOG_ERROR("failed to write configuration");
        return;
    }
    write_config(fp);
    write_accounts(fp);
    fclose(fp);
}

void xuol_set_last_used(int account_index)
{
    config().global_last_used = account_index;
}

int xuol_last_used()
{
    return config().global_last_used;
}

void request_close()
{
    s_launcher_close_intent = true;
}

void xuol_launch_quit()
{
    // the launcher closes after every successful client spawn/apply; the
    // auto_close key is kept parse-only so old configs still load
    request_close();
}

#if defined(XUO_LAUNCHER_EMBEDDED)
// a profile that cannot be applied live asked for a client reload; the request
// is latched here and acted upon by the app frame, which owns the client
// bridge and runs even after the view that requested it has been dismissed
void xuol_request_reload()
{
    s_reload_request = true;
}
#endif

static ui_model model;

} // namespace xuol

using namespace xuol;

bool xuol_app_init(win_context &win, const char *ini_name, bool enable_viewports)
{
    LOG_INFO("launcher app initializing");
    http_init();
    s_win = &win;
    static const auto ini =
        fs_path_join(xuol_data_path(), ini_name ? ini_name : "xuolauncher.ini");
    win.inifile = fs_path_ascii(ini);

    s_ui = ui_init(win, enable_viewports);
    s_ui.userdata = &model;
#if defined(XUO_DEBUG)
    s_ui.show_stats_window = true;
#endif
    ui_style_launcher();

    load_config();
    load_accounts();
    load_shards();

    return true;
}

void xuol_app_process_event(const SDL_Event *event)
{
    ui_process_event(s_ui, event);
}

bool xuol_app_wants_close()
{
    return s_launcher_close_intent;
}

void xuol_app_clear_close_intent()
{
    s_launcher_close_intent = false;
}

void xuol_app_shutdown()
{
    LOG_INFO("launcher app shutting down");
    save_config();
    ui_shutdown(s_ui);

    http_shutdown();
}

static bool s_profile_applied = false;

void xuol_app_notify_profile_applied()
{
    s_profile_applied = true;
}

bool xuol_app_consume_profile_applied()
{
    const bool applied = s_profile_applied;
    s_profile_applied = false;
    return applied;
}

void xuol_ui_clear_color(float out[4])
{
    out[0] = s_ui.clear_color.x;
    out[1] = s_ui.clear_color.y;
    out[2] = s_ui.clear_color.z;
    out[3] = s_ui.clear_color.w;
}


void xuol_app_frame()
{
    win_context &win = *s_win;
    auto &ui = s_ui;

    ui_newframe(ui);

    // with viewports enabled (standalone host) the pinned window is placed
    // in screen coordinates; without them (embedded host) the imgui viewport
    // starts at the client area origin
    ImVec2 pos = { 0.0f, 0.0f };
    if (ui.viewports_enabled)
    {
        int x, y;
        SDL_GetWindowPosition(win.window, &x, &y);
        pos = { float(x), float(y) };
    }
    ImVec2 size = { float(win.width), float(win.height) };
    //LOG_INFO("%d, %d", win.width, win.height);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin(XUOL_AGENT_NAME, nullptr, window_flags);
    {
        // the shard picker is a pushed sub-view of Accounts, not a peer tab;
        // it renders on its own with its Pick/Cancel footer. Keep s_shown_view
        // in sync here as well so returning from it to a different view would
        // still be picked up by the tab bar below.
        static ui_view s_shown_view = ui_view::count;

        // The views lay themselves out absolutely from model.area, so its height
        // must already exclude everything above the body: the tab strip.
        // That is the height BeginTabBar() gives its bar
        // rect, fontSize + 2 * framePadding (see BeginTabBar in
        // imgui_widgets.cpp), plus the ItemSpacing.y that separates the bar from
        // the body (the bar is submitted with ItemSize(..., FramePadding.y)).
        // The picker has no tab bar, so nothing to subtract.
        const ImVec2 region_max = ImGui::GetWindowContentRegionMax();
        const ImVec2 region_min = ImGui::GetWindowContentRegionMin();
        float body_height = region_max.y - region_min.y;
        if (model.view != ui_view::shard_picker)
        {
            const ImGuiStyle &style = ImGui::GetStyle();
            body_height -= ImGui::GetFontSize() + style.FramePadding.y * 2.0f + style.ItemSpacing.y;
        }
        model.area = ImVec2(region_max.x - region_min.x, body_height);

        if (model.view == ui_view::shard_picker)
        {
            s_shown_view = model.view;
            ui_shards(model, true);
        }
        else if (ImGui::BeginTabBar("##views"))
        {
            // model.view is owned outside this function (boot picks the initial
            // view, ui_stack push/pop restores it when leaving the shard
            // picker), so the tab bar has to follow it. _SetSelected is a
            // trigger, not a state: passing it every frame would re-queue the
            // focus after an earlier tab was already submitted, swallowing a
            // click on a tab that precedes the current one. Track the view we
            // rendered last and only force the selection when it actually
            // changed underneath us.
            const bool view_changed = s_shown_view != model.view;
            s_shown_view = model.view;

            // Every tab must be submitted on every frame: imgui culls from the
            // tab bar any tab that was not submitted in the current frame
            // (ImGuiTabBar::Tabs is rebuilt from the BeginTabItem calls). So
            // these cannot be chained with else-if: only the selected tab
            // returns true, and short-circuiting would make the remaining tabs
            // disappear entirely. Submit all of them, and draw each view's
            // content inside its own Begin/End pair, which is also what lets
            // EndTabBar() measure the contents height correctly.
            const auto tab = [view_changed](ui_view v, const char *label, void (*body)(ui_model &)) {
                const ImGuiTabItemFlags flags = (view_changed && model.view == v)
                                                    ? ImGuiTabItemFlags_SetSelected
                                                    : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(label, nullptr, flags))
                {
                    model.view = v;
                    body(model);
                    ImGui::EndTabItem();
                }
            };

            // ui_shards() takes an optional picker argument, so it cannot decay
            // to the plain ui_model& signature; wrap it.
            const auto shards_body = [](ui_model &m) { ui_shards(m); };

            // Shards is the home tab: picking a shard launches without any
            // account setup; Accounts stays for the advanced profile flow.
            tab(ui_view::shards, ICON_FK_GLOBE_W " Shards", shards_body);
            tab(ui_view::accounts, ICON_FK_USER " Accounts", ui_accounts);
            ImGui::EndTabBar();
        }

#if defined(XUO_LAUNCHER_EMBEDDED)
        // a profile that could not be applied live asked for a reload; the
        // restart always happens, there is no confirmation step
        if (s_reload_request)
        {
            s_reload_request = false;
            xuol::client_request_restart();
        }
#endif
    }
    ImGui::End();
    ui_draw(ui);
}
