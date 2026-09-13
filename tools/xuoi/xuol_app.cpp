// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Launcher

#include <stdio.h>
#include <vector>
#include <sstream>
#include <thread>
#if defined(XUO_WINDOWS)
#include <windows.h>
#include <shellapi.h>
#endif

#include <external/gfx/gfx.h>
#include <external/gfx/ui.h>
#include <external/gfx/sokol_gfx.h>
#include <external/gfx/imgui/imgui.h>
#include <external/inih.h>
#include <external/process.h>
#include <xuocore/http.h>
#include <xuocore/common.h>

#include "icon_launcher.h"
#include "accounts.h"
#include "shards.h"
#include "widgets.h"
#include "ui_model.h"
#include "ui_shards.h"

#include "xuol_app.h"
#include "xuo_updater.h"
#include "white_label.h"

namespace xuol {

static ui_context s_ui;
static win_context *s_win = nullptr;

struct releases
{
    const char *name;
    const char *version;
    astr_t display;
};
static std::vector<releases> s_releases;
static std::vector<releases> s_releases_updated;

static xuo_context *s_ctx = nullptr;
static bool s_update_check = false;
static bool s_apply_update = false;
static bool s_update_request = false;
static bool s_update_started = false;
static bool s_launcher_restart = false;
static bool s_launcher_quit = false;
static bool s_launcher_close_intent = false;
static int s_update_backup_index = -1;
static bool s_has_update = false;
static bool s_updated = false;
static fs_path s_launcher_binary;
static uint64_t s_launcher_timestamp = 0;
#if defined(XUO_LAUNCHER_EMBEDDED)
static bool s_restart_answer = false;
static bool s_reload_request = false;
#endif

void view_changelog()
{
    open_url(XUO_DEFAULT_SHARD_CHANGELOG);
}

void ui_updates(ui_model &m)
{
    // see: https://forkaweso.me/Fork-Awesome/icons/
    if (!s_ctx)
        ImGui::Text(ICON_FK_REFRESH " Checking for updates");
    else if (s_updated)
        ImGui::Text(ICON_FK_CHECK_SQUARE " Update finished");
    else if (!s_has_update)
        ImGui::Text(ICON_FK_CHECK_SQUARE " No updates found at the moment");
    else if (s_update_started)
    {
        const ImU32 col = ImGui::GetColorU32(ImGuiCol_PlotLinesHovered);
        ImGui::Spinner("##spinner", 7, 3, col);
        ImGui::SameLine();
        ImGui::Text(" Update in progress");
    }
    else if (s_has_update)
        ImGui::Text(ICON_FK_CHECK " An update is available");

    ImGui::NewLine();
    if (ImGui::Button(ICON_FK_FILE_TEXT_O " View Changelog"))
        view_changelog();
    ImGui::SameLine();
    if (s_has_update)
    {
        if (ImGui::Button(ICON_FK_CLOUD_DOWNLOAD " Apply"))
            s_update_request = true;
    }
    else
    {
        if (ImGui::Button(ICON_FK_REFRESH " Force Check"))
            s_update_check = true;
    }
}

static const char *backup_getter(void *data, int idx)
{
    auto *items = (std::vector<releases> *)data;
    assert(items);
    assert(idx < (int)items->size());
    return items->at(idx).display.c_str();
}

void ui_backups(ui_model &m)
{
    const auto line_size = ImGui::GetTextLineHeightWithSpacing();
    const auto items = int(m.area.y / (line_size + 2) - 2);
    const int last_item = 0;
    static int cur_item = last_item;

    ImGuiWindowFlags window_flags = 0;
    ImGui::Text(ICON_FK_FILE_ARCHIVE_O " Packages / Versions");
    auto area = ImGui::GetWindowContentRegionMax();
    const float y = m.area.y - ImGui::GetCursorPosY();
    ImGui::BeginChild("##left", { area.x - 7, y }, false, window_flags);
    ImGui::PushItemWidth(m.area.x);
    //    ImGui::ListBox("##pkg", &cur_item, accounts, IM_ARRAYSIZE(accounts), items);
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_SelectedEntryBg));
    ImGui::ListBox("##bkp", &cur_item, backup_getter, &s_releases, int(s_releases.size()), items);
    ImGui::PopStyleColor();    ImGui::PopItemWidth();
    ImGui::EndChild();
    const bool in_progress = s_update_started || s_update_request;
    if (in_progress)
    {
        const ImU32 col = ImGui::GetColorU32(ImGuiCol_PlotLinesHovered);
        ImGui::Spinner("##spinner", 7, 3, col);
    }
    else
    {
        ImGui::NewLine();
    }
    ImGui::SameLine(m.area.x - 55.0f);
    if (ImGui::Button(ICON_FK_CLOUD_DOWNLOAD " Apply") && !in_progress && !s_releases.empty())
    {
        s_update_backup_index = cur_item;
        s_update_request = true;
    }
}

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
    s_launcher_quit = true;
    s_launcher_close_intent = true;
}

void xuol_launch_quit()
{
    if (config().global_auto_close)
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

bool xuol_launch_assist()
{
    return config().global_enable_assist;
}

static ui_model model;

static void updater_init();
static void update_run();
static void update_backup();
static std::thread s_update_worker;

static void updater_init()
{
    s_updated = false;
    s_update_backup_index = -1;
    xuo_shutdown(s_ctx);
    const auto p = fs_path_current();
    const auto install_path = fs_path_ascii(p);
    s_ctx = xuo_init(install_path, false);
    if (s_ctx)
    {
        if (s_apply_update || config().global_check_updates || config().global_auto_update)
        {
            s_has_update = xuo_update_check(s_ctx);
            xuo_release_cb func = [](const char *name, const char *version, bool latest) {
                if (latest)
                    return;
                if (!strcasecmp(name, "all"))
                    return;
                s_releases_updated.push_back({ name, version, astr_t(name) + " " + version });
            };
            xuo_releases_iterate(s_ctx, func);
            s_releases.swap(s_releases_updated);
            s_releases_updated.clear();
        }
        if (s_has_update && config().global_auto_update)
            s_update_request = true;
        const auto timestamp = fs_timestamp_write(s_launcher_binary);
        s_launcher_restart = s_launcher_timestamp && s_launcher_timestamp != timestamp;
    }
    else
    {
        LOG_ERROR("could not initialize updater, ignoring updates");
    }
    model.view = s_has_update ? ui_view::updates : ui_view::accounts;
}

static void update_run()
{
    s_updated = xuo_update_apply(s_ctx);
    const auto timestamp = fs_timestamp_write(s_launcher_binary);
    s_launcher_restart = s_updated || (s_launcher_timestamp && s_launcher_timestamp != timestamp);
    s_update_started = false;
    s_has_update = false;
    model.view = ui_view::accounts;
}

static void update_backup()
{
    assert(
        s_update_backup_index >= 0 && s_update_backup_index < (int)s_releases.size() &&
        "invalid backup index");
    auto &e = s_releases[s_update_backup_index];
    LOG_INFO("downloading package %s %s", e.name, e.version);
    s_update_backup_index = -1;
    s_updated = xuo_release_get(s_ctx, e.name, e.version);
    const auto timestamp = fs_timestamp_write(s_launcher_binary);
    s_launcher_restart = s_updated || s_launcher_timestamp != timestamp;
    s_update_started = false;
    s_has_update = false;
    model.view = ui_view::accounts;
}

} // namespace xuol

using namespace xuol;

bool xuol_app_init(win_context &win, const char *ini_name, bool enable_viewports)
{
    LOG_INFO("launcher app initializing");
    http_init();
    s_win = &win;
    // used by the updater to detect that the launcher binary changed on disk
    s_launcher_binary = fs_path_process();
    s_launcher_timestamp = fs_timestamp_write(s_launcher_binary);
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

#if !defined(XUO_DEPLOY)
    config().global_auto_update = false;
    config().global_check_updates = false;
#endif // defined(XUO_DEPLOY)

    auto init_worker = []() { updater_init(); };
    s_update_worker = std::thread(init_worker);
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
    if (s_update_worker.joinable())
        s_update_worker.join();

    if (s_ctx)
        xuo_shutdown(s_ctx);
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
    // the embedded launcher is a minimal login-screen tool: it has no menu bar,
    // so its window flags are built without ImGuiWindowFlags_MenuBar
#if defined(XUO_LAUNCHER_EMBEDDED)
    const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoScrollbar;
#else
    const ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar |
                                          ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoScrollbar;
#endif
    ImGui::Begin(XUOL_AGENT_NAME, nullptr, window_flags);
    {
#if !defined(XUO_LAUNCHER_EMBEDDED)
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Options"))
            {
                auto &cfg = config();
                if (ImGui::MenuItem("Check updates", nullptr, &cfg.global_check_updates))
                {
                }
                if (ImGui::MenuItem("Auto update", nullptr, &cfg.global_auto_update))
                {
                }
                if (ImGui::MenuItem("Enable assitant", nullptr, &cfg.global_enable_assist))
                {
                }
                if (ImGui::MenuItem("Close after launch", nullptr, &cfg.global_auto_close))
                {
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("About"))
            {
                if (ImGui::MenuItem(ICON_FK_FILE_TEXT_O " Changelog", nullptr))
                    view_changelog();
#if defined(XUO_DEBUG)
                if (ImGui::MenuItem("Demo", nullptr, &ui.show_demo_window))
                {
                }
#endif
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
#endif // !XUO_LAUNCHER_EMBEDDED

        if (s_update_request)
        {
            s_update_request = false;
            s_update_started = true;
            if (s_update_worker.joinable())
                s_update_worker.join();
            if (s_update_backup_index != -1)
            {
                s_update_worker = std::thread(update_backup);
            }
            else
            {
                s_update_worker = std::thread(update_run);
            }
        }
        else if (s_update_check)
        {
            s_apply_update = true;
            if (s_update_worker.joinable())
                s_update_worker.join();
            s_update_worker = std::thread(updater_init);
            s_update_check = false;
        }

        // the shard picker is a pushed sub-view of Accounts, not a peer tab;
        // it renders on its own with its Pick/Cancel footer. Keep s_shown_view
        // in sync here as well so returning from it to a different view would
        // still be picked up by the tab bar below.
        static ui_view s_shown_view = ui_view::count;

        // The views lay themselves out absolutely from model.area, so its height
        // must already exclude everything above the body: the menu bar and the
        // tab strip. GetWindowContentRegionMax() already accounts for the menu
        // bar (ImGuiWindow::DecoOuterSizeY1 includes MenuBarHeight), so only the
        // tab strip is subtracted. That is the height BeginTabBar() gives its bar
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

            tab(ui_view::accounts, ICON_FK_USER " Accounts", ui_accounts);
            tab(ui_view::updates, ICON_FK_CLOUD_DOWNLOAD " Updates", ui_updates);
            tab(ui_view::backups, ICON_FK_FILE_ARCHIVE_O " Backups", ui_backups);
            tab(ui_view::shards, ICON_FK_GLOBE_W " Shards", shards_body);
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

        if (s_launcher_restart)
        {
#if defined(XUO_LAUNCHER_EMBEDDED)
            // embedded in the client: restarting means relaunching crossuo so
            // the updated binaries take over (the client exits by itself).
            // the answer lives in a static: DialogYesNo() only returns true on
            // "Yes", so a per-frame local would never observe a "No" and the
            // dialog would reopen forever.
            s_restart_answer = true; // keeps the modal up until answered
            DialogYesNo(
                "Update", "A restart is required, do you want to restart now?", s_restart_answer);
            if (!s_restart_answer)
            {
                s_launcher_restart = false;
                LOG_WARN("client restart postponed, the update applies on the next start");
            }
#else
            s_launcher_quit = DialogYesNo(
                "Update", "A restart is required, do you want to close?", s_launcher_restart);
#endif
        }
    }
    ImGui::End();
    ui_draw(ui);
}
