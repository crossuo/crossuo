// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

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

#include "common.h"
#include "accounts.h"
#include "shards.h"
#include "ui_model.h"
#include "ui_shards.h"
#include "http.h"

#include "xuo_updater.h"

// to avoid issues self-updating, launcher will copy-itself into another binary and relaunch it
// this avoid the file being locked to write while updating iself
#define XUOL_ENABLE_SAFE_SELF_UPDATE

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
static int s_update_backup_index = -1;
static bool s_has_update = false;
static bool s_updated = false;
static fs_path s_launcher_binary;
static uint64_t s_launcher_timestamp = 0;

void XUODefaultStyle()
{
    ImVec4 *colors = ImGui::GetStyle().Colors;
    /* clang-format off */
    colors[ImGuiCol_Text]                   = ImVec4(0.62f, 0.58f, 0.58f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.25f, 0.25f, 0.25f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.43f, 0.25f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.50f, 0.50f, 0.50f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.50f, 0.50f, 0.50f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.22f, 0.22f, 0.22f, 0.86f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.32f, 0.31f, 0.29f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.22f, 0.22f, 0.22f, 0.86f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.50f, 0.50f, 0.50f, 0.63f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.71f, 0.71f, 0.71f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.33f, 0.33f, 0.33f, 0.63f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.50f, 0.50f, 0.50f, 0.40f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.50f, 0.50f, 0.50f, 0.63f);
    colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.50f, 0.50f, 0.50f, 0.80f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.47f, 0.47f, 0.47f, 0.39f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.50f, 0.50f, 0.50f, 0.80f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.22f, 0.22f, 0.22f, 0.86f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.50f, 0.50f, 0.50f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.22f, 0.22f, 0.22f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.31f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.49f, 0.49f, 0.49f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.80f, 0.80f, 0.80f, 0.78f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    colors[ImGuiCol_SelectedEntryBg]        = ImVec4(0.47f, 0.47f, 0.47f, 0.39f);
    /* clang-format on */
}

void HoverToolTip(const char *desc)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetTextLineHeightWithSpacing() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void HelpMarker(const char *desc)
{
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::GetColorU32(ImGuiCol_Text));
    ImGui::TextDisabled("" ICON_FK_QUESTION_CIRCLE);
    ImGui::PopStyleColor();
    HoverToolTip(desc);
}

void InputText(
    const char *id,
    const char *label,
    float w,
    char *buf,
    size_t buf_size,
    ImGuiInputTextFlags flags = 0,
    ImGuiInputTextCallback callback = nullptr,
    void *user_data = nullptr)
{
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(w);
    ImGui::InputText(id, buf, buf_size, flags, callback, user_data);
    ImGui::PopItemWidth();
}

bool ComboBox(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    const char *const items[],
    int items_count,
    int height_in_items = -1)
{
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(w);
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_SelectedEntryBg));
    const bool changed = ImGui::Combo(id, current_item, items, items_count, height_in_items);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    return changed;
}

bool ComboBox(
    const char *id,
    const char *label,
    float w,
    int *current_item,
    bool (*items_getter)(void *data, int idx, const char **out_text),
    void *data,
    int items_count,
    int height_in_items = -1)
{
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(w);
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_SelectedEntryBg));
    const bool changed =
        ImGui::Combo(id, current_item, items_getter, data, items_count, height_in_items);
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
    return changed;
}

void view_changelog()
{
    open_url("http://crossuo.com/changelog.html");
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

static bool backup_getter(void *data, int idx, const char **out_text)
{
    auto *items = (std::vector<releases> *)data;
    assert(items);
    assert(idx < items->size());
    if (out_text)
        *out_text = items->at(idx).display.c_str();
    return true;
}

void ui_backups(ui_model &m)
{
    const auto line_size = ImGui::GetTextLineHeightWithSpacing();
    const auto items = (m.area.y / (line_size + 2) - 2);
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
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();
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

static inline bool ui_modal(const char *title, const char *msg)
{
    bool yes = false;
    ImGui::OpenPopup(title);
    if (ImGui::BeginPopupModal(title))
    {
        ImGui::Text("%s", msg);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Button("Yes", ImVec2(80, 0)))
        {
            s_launcher_restart = false;
            ImGui::CloseCurrentPopup();
            yes = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(80, 0)))
        {
            s_launcher_restart = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    return yes;
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
        launcher::dump(&s_config.entries[0]);
    else
        s_config.entries.push_back(launcher::default_entry());
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

void xuol_launch_quit()
{
    if (config().global_auto_close)
        s_launcher_quit = true;
}

bool xuol_launch_assist()
{
    return config().global_enable_assist;
}

static ui_model model;

static bool run_self_update_instance(int argc, char **argv)
{
    if (!argv[0])
        return true;

    const char *bin = argv[0];
    auto len = strlen(bin);
    if (!len)
        return true;

    s_launcher_binary = fs_path_from(bin);
    s_launcher_timestamp = fs_timestamp_write(s_launcher_binary);
#if defined(XUOL_ENABLE_SAFE_SELF_UPDATE)
    if (bin[len - 1] != '_')
    {
        astr_t filename{ bin };
        filename += '_';
        auto file = fs_path_from(filename);
        fs_del(file);
        fs_copy(fs_path_from(bin), file);

        bin = filename.c_str();
        const char *args[] = { bin, 0 };
        LOG_INFO("running %s", bin);
        process_s process;
        const auto options = process_option_inherit_environment | process_option_child_detached;
        if (process_create(args, options, &process) != 0)
        {
            LOG_ERROR("could not relaunch %s for self-updates", bin);
        }
        return false;
    }
#endif // #if defined(XUO_WINDOWS)
    return true;
}

int main(int argc, char **argv)
{
#if !defined(XUO_DEBUG)
    const bool self_relaunch = true;
#else
    const bool self_relaunch = argc > 1 && strcmp(argv[1], "--self") == 0;
#endif
    if (self_relaunch && !run_self_update_instance(argc, argv))
    {
        LOG_INFO("terminating %s", argv[0]);
        return 0;
    }

    LOG_INFO("started %s in %s", argv[0], fs_path_ascii(fs_path_current()));
    crc32_init();
    http_init();
    static const auto ini = fs_path_join(xuol_data_path(), "xuolauncher.ini");

    win_context win;
    win.title = XUOL_AGENT_NAME;
    win.width = 550;
    win.height = 284;
    win.vsync = 0;
    win.inifile = fs_path_ascii(ini);
    win_init(&win);

    auto ui = ui_init(win);
    ui.userdata = &model;
#if defined(XUO_DEBUG)
    ui.show_stats_window = true;
#endif
    XUODefaultStyle();

    load_config();
    load_accounts();
    load_shards();

#if !defined(XUO_DEPLOY)
    config().global_auto_update = false;
    config().global_check_updates = false;
#endif // defined(XUO_DEPLOY)

    auto updater_init = []() {
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
    };
    auto update_run = []() {
        s_updated = xuo_update_apply(s_ctx);
        const auto timestamp = fs_timestamp_write(s_launcher_binary);
        s_launcher_restart = s_launcher_timestamp && s_launcher_timestamp != timestamp;
        s_update_started = false;
        s_has_update = false;
        model.view = ui_view::accounts;
    };
    auto update_backup = []() {
        assert(
            s_update_backup_index >= 0 && s_update_backup_index < s_releases.size() &&
            "invalid backup index");
        auto &e = s_releases[s_update_backup_index];
        LOG_INFO("downloading package %s %s", e.name, e.version);
        s_update_backup_index = -1;
        s_updated = xuo_release_get(s_ctx, e.name, e.version);
        const auto timestamp = fs_timestamp_write(s_launcher_binary);
        s_launcher_restart = s_launcher_timestamp != timestamp;
        s_update_started = false;
        s_has_update = false;
        model.view = ui_view::accounts;
    };
    auto update_worker = std::thread(updater_init);

    // Main loop
    while (!s_launcher_quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ui_process_event(ui, &event);
            if (event.type == SDL_QUIT)
                s_launcher_quit = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(win.window))
                s_launcher_quit = true;
        }

        glClearColor(ui.clear_color.x, ui.clear_color.y, ui.clear_color.z, ui.clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        int x, y;
        SDL_GetWindowPosition(win.window, &x, &y);
        ImVec2 pos = { float(x), float(y) };
        ImVec2 size = { float(win.width), float(win.height) };
        //LOG_INFO("%d, %d", win.width, win.height);
        ui_update(ui);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::Begin(
            XUOL_AGENT_NAME,
            nullptr,
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
        {
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("View"))
                {
                    bool selected = model.view == ui_view::accounts;
                    if (ImGui::MenuItem(ICON_FK_USER " Accounts", nullptr, &selected))
                        model.view = ui_view::accounts;
                    selected = model.view == ui_view::updates;
                    if (ImGui::MenuItem(ICON_FK_CLOUD_DOWNLOAD " Updates", nullptr, &selected))
                        model.view = ui_view::updates;
                    selected = model.view == ui_view::backups;
                    if (ImGui::MenuItem(ICON_FK_FILE_ARCHIVE_O " Backups", nullptr, &selected))
                        model.view = ui_view::backups;
                    selected = model.view == ui_view::shards;
                    if (ImGui::MenuItem(ICON_FK_GLOBE_W " Shards", nullptr, &selected))
                        model.view = ui_view::shards;
                    ImGui::EndMenu();
                }
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
                    ImGui::Separator();
                    if (ImGui::MenuItem(ICON_FK_QUESTION_CIRCLE " About", nullptr))
                    {
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            if (s_update_request)
            {
                s_update_request = false;
                s_update_started = true;
                if (update_worker.joinable())
                    update_worker.join();
                if (s_update_backup_index != -1)
                {
                    update_worker = std::thread(update_backup);
                }
                else
                {
                    update_worker = std::thread(update_run);
                }
            }
            else if (s_update_check)
            {
                s_apply_update = true;
                if (update_worker.joinable())
                    update_worker.join();
                update_worker = std::thread(updater_init);
                s_update_check = false;
            }

            model.area = ImVec2(ImGui::GetWindowContentRegionWidth(), size.y - 35); // FIXME
            if (model.view == ui_view::accounts)
                ui_accounts(model);
            if (model.view == ui_view::updates)
                ui_updates(model);
            else if (model.view == ui_view::backups)
                ui_backups(model);
            else if (model.view == ui_view::shards)
                ui_shards(model);
            else if (model.view == ui_view::shard_picker)
                ui_shards(model, true);

            if (s_launcher_restart)
                s_launcher_quit =
                    ui_modal("Update", "A restart is required, do you want to close?");
        }
        ImGui::End();
        ui_draw(ui);
        win_flip(&win);
    }

    if (update_worker.joinable())
        update_worker.join();

    if (s_ctx)
        xuo_shutdown(s_ctx);
    save_config();
    ui_shutdown(ui);
    win_shutdown(&win);

    http_shutdown();
    return 0;
}
