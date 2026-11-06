// SPDX-License-Identifier: AGPL-3.0-or-later

// Standalone Asset Viewer application: window + ImGui loop for the shared
// viewer core, with its own data-folder picker (SDL3 dialog) and audio.

#include "app.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>
// SDL3 relocates main; with the -municode link flag (SDL3 on mingw) the CRT
// entry is wWinMain, which SDL_main.h provides in the TU defining main
// (mirrors src/Main.cpp)
#include <SDL3/SDL_main.h>

#include <common/fs.h>
#include <common/logging/logging.h>
#include <common/str.h>

#include <external/gfx/gfx.h>
#include <external/gfx/imgui/imgui.h>
#include <external/gfx/ui.h>

#include <xuocore/client_info.h>
#include <xuocore/uodata.h>
#include <xuocore/uodata_reader.h>

#include "../../src/AssetViewer/AssetViewer.h"
#include "audio.h"

namespace
{
// where the last-used data folder is remembered (per user, like the client's
// <appdata>/<CLIENT_APPNAME> tree)
fs_path SettingsPath()
{
    return FilePath("assetviewer/crossuo-assetviewer.ini");
}

fs_path SettingsDir()
{
    return fs_directory(SettingsPath());
}

bool FileHoldsClientData(const fs_path &dir)
{
    // the index files the loader needs first (LoadCommon): a folder without
    // them is not a usable UO data set. names are compared case-insensitively
    // against the top-level listing only - installs mix the stock client's
    // mixed-case names (Skills.idx) with lowercase ones, and the folder that
    // holds them is a flat install, not a tree. This must stay a single
    // directory read: the picker can be pointed at an arbitrary folder (a
    // downloads or builds directory), and the recursive case-insensitive scan
    // below would walk the whole thing
    static const char *const required[] = {
        "anim.idx", "lightidx.mul", "skills.idx", "texidx.mul", "hues.mul", "light.mul",
        "tiledata.mul",
    };
    std::vector<fs_path> entries;
    fs_path_list_shallow(dir, entries);
    if (entries.empty())
    {
        return false;
    }
    for (const char *name : required)
    {
        bool found = false;
        for (const fs_path &entry : entries)
        {
            const astr_t &full = entry.real_path;
            const size_t slash = full.find_last_of('/');
            const astr_t base = slash == astr_t::npos ? full : full.substr(slash + 1);
            if (base.size() != strlen(name))
            {
                continue;
            }
            bool same = true;
            for (size_t i = 0; i < base.size() && same; i++)
            {
                same = tolower((unsigned char)base[i]) == (unsigned char)tolower((unsigned char)name[i]);
            }
            if (same)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}
} // namespace

// static state shared with the SDL dialog callback (see app.h)
char AssetViewerApp::s_PickedPath[1024] = {};
int AssetViewerApp::s_PickedState = 0;

AssetViewerApp &AssetViewerApp::Instance()
{
    static AssetViewerApp s_App;
    return s_App;
}

void AssetViewerApp::PrintUsage() const
{
    printf(
        "Usage: xuoassetviewer [options] [data-folder]\n"
        "\n"
        "Standalone UO asset browser: art, land art, tiledata, hues, gumps,\n"
        "effects, animations, sounds, music and multis of a client install.\n"
        "\n"
        "Options:\n"
        "  --data <folder>   UO data folder to browse (default: the last one\n"
        "                    used, or the folder picker)\n"
        "  --help, -h        this message\n");
}

bool AssetViewerApp::ParseCommandLine(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i] != nullptr ? argv[i] : "";
        if (strcmp(arg, "--data") == 0 && i + 1 < argc)
        {
            m_Options.DataPath = argv[++i] != nullptr ? argv[i] : "";
            m_DataFromCommandLine = true;
        }
        else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
        {
            PrintUsage();
            return false;
        }
        else if (arg[0] != '-' && arg[0] != '\0')
        {
            m_Options.DataPath = arg;
            m_DataFromCommandLine = true;
        }
        else
        {
            printf("unknown option: %s\n", arg);
            PrintUsage();
            return false;
        }
    }
    return true;
}

bool AssetViewerApp::InitWindow()
{
    m_Win.title = "X:UO Asset Viewer";
    m_Win.width = 1280;
    m_Win.height = 800;
    m_Win.vsync = 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == false)
    {
        fprintf(stderr, "failed to initialize SDL: %s\n", SDL_GetError());
        return false;
    }
    // window + GL context (win_init runs SDL_Window/context creation, the
    // loader and the context-attribute setup)
    m_Win.inifile = nullptr;
    if (win_init(&m_Win) != 0)
    {
        fprintf(stderr, "failed to create the viewer window\n");
        return false;
    }

    m_Ui = ui_init(m_Win, /*enable_viewports=*/false);
    if (ImGui::GetCurrentContext() == nullptr)
    {
        fprintf(stderr, "failed to initialize the UI backend\n");
        return false;
    }
    ui_style_launcher();
    // no OS cursor juggling: the viewer always shows the normal pointer
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // docking/size memory lives next to the settings file, not in the data
    // folder the loader may have changed into
    static const std::string s_IniFile = fs_path_str(SettingsPath()) + ".imgui.ini";
    ImGui::GetIO().IniFilename = s_IniFile.c_str();
    return true;
}

bool AssetViewerApp::InitAudio()
{
    if (avaudio::Init())
    {
        m_AudioStatus = "audio ready";
        return true;
    }
    m_AudioStatus = "audio unavailable";
    return false;
}

void AssetViewerApp::ShutdownAudio()
{
    avaudio::Shutdown();
}

bool AssetViewerApp::Init()
{
    if (m_Initialized)
    {
        return true;
    }
    if (!InitWindow())
    {
        return false;
    }
    InitAudio();
    m_Initialized = true;

    if (m_DataFromCommandLine && !m_Options.DataPath.empty())
    {
        LoadDataPath(m_Options.DataPath);
    }
    else
    {
        LoadRememberedData();
    }
    if (!m_DataLoaded)
    {
        RequestDataPathPicker();
    }
    return true;
}

void AssetViewerApp::DestroyData()
{
    if (m_DataLoaded)
    {
        AssetViewer_DataUnload();
        m_DataLoaded = false;
    }
}

bool AssetViewerApp::LoadDataPath(const std::string &path)
{
    if (path.empty())
    {
        return false;
    }
    const fs_path dir = fs_path_from(path);
    if (!fs_path_exists(dir) || !fs_path_is_dir(dir))
    {
        SetStatus("folder not found: %s", path.c_str());
        return false;
    }
    // installs mix the stock client's mixed-case names (Gumpart.mul,
    // Gumpidx.mul, Skills.idx) with lowercase ones, so the lookup table has to
    // exist before anything is probed by name. the client does this at boot;
    // the viewer redoes it per folder, and fs_case_insensitive_init replaces
    // (rather than extends) the previous scan
    fs_case_insensitive_init(dir);
    if (!FileHoldsClientData(dir))
    {
        SetStatus("no client data files in %s", path.c_str());
        return false;
    }

    DestroyData();

    const uint32_t version = uo_data_detect_client_version(path.c_str());
    char versionStr[32] = {};
    client_version_string(version, versionStr, sizeof(versionStr));
    // the loaders resolve every file against the current directory
    if (!fs_path_change(dir))
    {
        SetStatus("cannot enter %s", path.c_str());
        return false;
    }
    uo_data_init(path.c_str(), version, false);
    if (!g_FileManager.Load())
    {
        SetStatus("failed: the data files could not be loaded from %s", path.c_str());
        fs_path_change(fs_path_process());
        return false;
    }
    // the loader only logs per-file failures: prove the essential tables are
    // there so a half-loaded install is reported instead of browsed empty
    if (g_FileManager.m_BlockData[0].empty() || g_FileManager.m_Hues.empty())
    {
        SetStatus("failed: %s is missing the client data tables", path.c_str());
        g_FileManager.Unload();
        uo_index_prepare_reload();
        fs_path_change(fs_path_process());
        return false;
    }
    m_DataLoaded = true;
    m_DataPath = fs_path_str(dir);
    SetStatus("version %s", version == CV_UNKNOWN ? "unknown" : versionStr);
    RememberDataPath(m_DataPath);
    Info(Client, "asset viewer: loaded %s (client %s)", m_DataPath.c_str(), versionStr);
    return true;
}

void AssetViewerApp::LoadRememberedData()
{
    const fs_path settings = SettingsPath();
    if (!fs_path_exists(settings))
    {
        return;
    }
    char line[1024] = {};
    if (FILE *file = fopen(fs_path_ascii(settings), "r"))
    {
        while (fgets(line, sizeof(line), file) != nullptr)
        {
            const size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n')
            {
                line[len - 1] = '\0';
            }
            if (strncmp(line, "DataPath=", 9) == 0)
            {
                const char *value = line + 9;
                if (value[0] != '\0' && LoadDataPath(value))
                {
                    fclose(file);
                    return;
                }
            }
        }
        fclose(file);
    }
}

void AssetViewerApp::RememberDataPath(const std::string &path)
{
    fs_path_create(SettingsDir());
    FILE *file = fopen(fs_path_ascii(SettingsPath()), "w");
    if (file == nullptr)
    {
        return;
    }
    fprintf(file, "DataPath=%s\n", path.c_str());
    fclose(file);
}

void AssetViewerApp::RequestDataPathPicker()
{
    if (m_PickerOpen)
    {
        return;
    }
    // SDL3: asynchronous native folder dialog (portal/zenity/win32/AppKit);
    // the callback may run on another thread, see OnFolderPicked
    SDL_ShowOpenFolderDialog(
        &AssetViewerApp::OnFolderPicked,
        this,
        m_Win.window,
        m_DataPath.empty() ? nullptr : m_DataPath.c_str(),
        false);
    m_PickerOpen = true;
}

void AssetViewerApp::OnFolderPicked(void *userdata, const char *const *filelist, int filter)
{
    (void)userdata;
    (void)filter;
    // ASYNC: may run on a different thread; only queue the outcome. SDL3
    // reports failures as filelist == NULL (see SDL_ShowOpenFolderDialog)
    if (filelist == nullptr || filelist[0] == nullptr)
    {
        s_PickedState = 2;
        return;
    }
    SDL_strlcpy(s_PickedPath, filelist[0], sizeof(s_PickedPath));
    s_PickedState = 1;
}

void AssetViewerApp::SetStatus(const char *format, ...)
{
    char buffer[512] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    va_end(args);
    m_DataStatus = buffer;
}

void AssetViewerApp::PumpEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        // the backend keeps its ImGui state in sync (multi-viewport off, so
        // every event belongs to this window)
        ui_process_event(m_Ui, &event);
        if (event.type == SDL_EVENT_QUIT)
        {
            m_Quit = true;
        }
    }

    // consume a finished folder pick
    if (m_PickerOpen && s_PickedState != 0)
    {
        const int state = s_PickedState;
        s_PickedState = 0;
        m_PickerOpen = false;
        if (state == 1)
        {
            LoadDataPath(s_PickedPath);
        }
        else
        {
            SetStatus("no folder chosen");
        }
    }
}

void AssetViewerApp::DrawUi()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("##AssetViewerHost", nullptr, flags))
    {
        if (m_DataLoaded)
        {
            AssetViewer_Show(true);
            AssetViewer_Draw();
        }
        else
        {
            ImGui::TextUnformatted("X:UO Asset Viewer");
            ImGui::Separator();
            ImGui::TextWrapped(
                "No UO data folder is loaded. Choose a folder that holds the "
                "client data files (art.mul / artLegacyMUL.uop, tiledata.mul, "
                "hues.mul, gumpart.mul, ...).");
            if (ImGui::Button("Choose data folder..."))
            {
                RequestDataPathPicker();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", m_DataStatus.c_str());
            ImGui::Separator();
            if (!m_AudioStatus.empty())
            {
                ImGui::TextDisabled("%s", m_AudioStatus.c_str());
            }
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q, false))
        {
            m_Quit = true; // app shortcut: end the viewer session
        }
    }
    ImGui::End();
}

bool AssetViewerApp::RunFrame()
{
    PumpEvents();

    ui_newframe(m_Ui);
    if (ImGui::IsKeyPressed(ImGuiKey_O, false) &&
        (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper))
    {
        RequestDataPathPicker();
    }
    DrawUi();
    const bool wantText = ImGui::GetIO().WantTextInput;
    if (wantText && !m_WantTextInput)
    {
        SDL_StartTextInput(m_Win.window);
    }
    else if (!wantText && m_WantTextInput)
    {
        SDL_StopTextInput(m_Win.window);
    }
    m_WantTextInput = wantText;
    ui_draw(m_Ui);
    win_flip(&m_Win);

    return !m_Quit;
}

void AssetViewerApp::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }
    // the viewer's textures live in this ImGui context: release them (CPU
    // side) after the context is gone, so the backend teardown can recycle the
    // GL names first
    AssetViewer_Destroy();
    DestroyData();
    ui_shutdown(m_Ui);
    AssetViewer_Shutdown();
    ShutdownAudio();
    win_shutdown(&m_Win);
    // ui_context has NSDMIs: assignment keeps the type-safe zeroing the old
    // memset provided without tripping -Wclass-memaccess
    m_Ui = ui_context{};
    m_Initialized = false;
}

int main(int argc, char **argv)
{
    AssetViewerApp &app = AssetViewerApp::Instance();
    if (!app.ParseCommandLine(argc, argv))
    {
        return 0;
    }
    if (!app.Init())
    {
        return 1;
    }
    while (app.RunFrame())
    {
    }
    app.Shutdown();
    return 0;
}
