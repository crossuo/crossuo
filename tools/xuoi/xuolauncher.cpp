// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include <stdio.h>
#include <vector>
#include <sstream>
#if _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <external/gfx/gfx.h>
#include <external/gfx/ui.h>
#include <external/gfx/sokol_gfx.h>
#include <external/gfx/imgui/imgui.h>
#include <external/inih.h>

#include "common.h"
#include "accounts.h"
#include "shards.h"
#include "ui_model.h"

bool valid_url(const std::string &url)
{
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

#if defined(_MSC_VER)
void open_url(const std::string &url)
{
    assert(valid_url(url) && "invalid url format");
    ShellExecuteA(0, "Open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
#else
void open_url(const std::string &url)
{
#if __APPLE__
#define OPEN_CMD "open "
#else
#define OPEN_CMD "xdg-open "
#endif
    assert(valid_url(url) && "invalid url format");
    auto cmd = std::string(OPEN_CMD) + url;
    system(cmd.c_str());
}
#endif

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

#if defined(XUO_RELEASE)
#define XUOL_UPDATER_HOST "http://update.crossuo.com/"
#else
#define XUOL_UPDATER_HOST "http://update.crossuo.com/"
//#define XUOL_UPDATER_HOST "http://192.168.2.14:8089/"
#endif

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
    open_url(XUOL_UPDATER_HOST "release/changelog.html");
}

void ui_updates(ui_model &m)
{
    // see: https://forkaweso.me/Fork-Awesome/icons/
    ImGui::Text(ICON_FK_CHECK_SQUARE " No updates found at the moment");
    if (ImGui::Button(ICON_FK_FILE_TEXT_O " View Changelog"))
        view_changelog();
}

void ui_backups(ui_model &m)
{
    const auto line_size = ImGui::GetTextLineHeightWithSpacing();
    const auto items = int(m.area.y / (line_size + 2));
    const char *accounts[] = { "Apple",  "Banana",    "Cherry",     "Kiwi",      "Mango",
                               "Orange", "Pineapple", "Strawberry", "Watermelon" };
    const int last_item = 0;
    static int cur_item = last_item;

    ImGuiWindowFlags window_flags = 0;
    auto area = ImGui::GetWindowContentRegionMax();
    ImGui::BeginChild("##left", area, false, window_flags);
    ImGui::Text(ICON_FK_FILE_ARCHIVE_O " Packages / Versions");
    ImGui::PushItemWidth(m.area.x);
    ImGui::ListBox("##pkg", &cur_item, accounts, IM_ARRAYSIZE(accounts), items);
    ImGui::PopItemWidth();
    ImGui::EndChild();
}

#define CFG_SECTION_FILTER_NAME "global"
#define CFG_NAME launcher
#define CFG_FILE xuolauncher
#define CFG_DEFINITION "cfg_launcher.h"
#include "cfg_loader.h"

static launcher::data s_config;

launcher::entry &config()
{
    return s_config.entries[0];
}

void load_config()
{
    s_config = launcher::cfg();
    if (s_config.entries.size())
        launcher::dump(&s_config.entries[0]);
    else
        s_config.entries.push_back({});
}

void write_config(void *_fp)
{
    auto fp = (FILE *)_fp;
    const auto cur = config();
    launcher::write(fp, cur, "Global");
}

void save_config()
{
    FILE *fp = fopen("xuolauncher2.cfg", "wt");
    if (!fp)
    {
        LOG_ERROR("failed to write configuration");
        return;
    }
    write_config(fp);
    write_accounts(fp);
    fclose(fp);
}

static ui_model model;

int main(int argc, char **argv)
{
    win_context win;
    win.title = "X:UO Launcher";
    //win.width = 680;
    //win.height = 440;
    win.width = 550;
    win.height = 284;
    win.vsync = 0;
    win_init(&win);

    auto ui = ui_init(win);
    ui.userdata = &model;
    ui.show_stats_window = true;
    XUODefaultStyle();

    load_config();
    load_accounts();
    load_shards();

    if (!config().global_check_updates)
        model.view = ui_view::accounts;

    // Main loop
    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ui_process_event(ui, &event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(win.window))
                done = true;
        }

        glClearColor(ui.clear_color.x, ui.clear_color.y, ui.clear_color.z, ui.clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        int x, y;
        SDL_GetWindowPosition(win.window, &x, &y);
        ImVec2 pos = { float(x), float(y) };
        ImVec2 size = { float(win.width), float(win.height) };
        //LOG_INFO("%d, %d\n", win.width, win.height);
        ui_update(ui);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::Begin(
            "X:UO Launcher",
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
                    selected = model.view == ui_view::backups;
                    if (ImGui::MenuItem(ICON_FK_FILE_ARCHIVE_O " Backups", nullptr, &selected))
                        model.view = ui_view::backups;
                    selected = model.view == ui_view::shards;
                    if (ImGui::MenuItem(ICON_FK_GLOBE_W " Shards", nullptr, &selected))
                        model.view = ui_view::shards;
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("About"))
                {
                    if (ImGui::MenuItem(ICON_FK_FILE_TEXT_O " Changelog", nullptr))
                        view_changelog();
                    if (ImGui::MenuItem("Demo", nullptr, &ui.show_demo_window))
                        ;
                    ImGui::Separator();
                    if (ImGui::MenuItem(ICON_FK_QUESTION_CIRCLE " About", nullptr))
                        ;
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
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
        }
        ImGui::End();
        ui_draw(ui);
        win_flip(&win);
    }
    save_config();
    ui_shutdown(ui);
    win_shutdown(&win);
    return 0;
}