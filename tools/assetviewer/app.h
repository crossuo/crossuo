// SPDX-License-Identifier: AGPL-3.0-or-later

// Standalone Asset Viewer: the in-client asset browser as its own application.
// It links the same viewer core (src/AssetViewer) and decodes UO data through
// the shared xuocore readers, so it browses any data folder (picked with the
// SDL3 folder dialog) without a game client around it.

#pragma once

#include <stdint.h>

#include <string>

#include <external/gfx/gfx.h>
#include <external/gfx/ui.h>

#include <xuocore/uodata_reader.h> // (kept for the host's own decoders)

struct AssetViewerAppOptions
{
    // data folder from the command line (--data, or the first positional
    // argument): loaded immediately, the picker is skipped when it holds a
    // usable install
    std::string DataPath;
};

class AssetViewerApp
{
public:
    static AssetViewerApp &Instance();

    // parses the command line; returns false when only --help ran
    bool ParseCommandLine(int argc, char **argv);
    void PrintUsage() const;

    // SDL window + GL context + the ImGui frame pipeline. Returns false when
    // the window or the GL context could not be created.
    bool Init();
    // one frame: events, viewer UI, present. Returns false when the app was
    // asked to quit.
    bool RunFrame();
    // tears the viewer, the ImGui context, the audio backend and the window
    void Shutdown();

    // --- data set ------------------------------------------------------------
    // loads the asset indexes of `path` (SDL/ImGui calls are allowed); returns
    // false when the folder has no usable data files
    bool LoadDataPath(const std::string &path);
    // opens the SDL3 folder picker (async; the result arrives in PumpEvents)
    void RequestDataPathPicker();
    const std::string &DataPath() const { return m_DataPath; }
    const std::string &DataStatus() const { return m_DataStatus; }

    // --- host services (consumed by the viewer through AssetViewerHost.cpp) ---
    // window-level input guard (text fields own the keyboard while enabled)
    bool WantTextInput() const { return m_WantTextInput; }
    void SetWantTextInput(bool want) { m_WantTextInput = want; }

    win_context &Window() { return m_Win; }

private:
    AssetViewerApp() = default;

    bool InitAudio();
    void ShutdownAudio();
    bool InitWindow();
    void LoadRememberedData();
    void RememberDataPath(const std::string &path);
    void DestroyData();
    void PumpEvents();
    void DrawUi();
    void DrawDataMenuBar();
    void SetStatus(const char *format, ...);

    // SDL file dialog callback (ASYNCHRONOUS: may run on another thread, so
    // it only stores the result for PumpEvents to consume)
    static void OnFolderPicked(void *userdata, const char *const *filelist, int filter);

    AssetViewerAppOptions m_Options;
    win_context m_Win = {};
    ui_context m_Ui = {};

    bool m_Initialized = false;
    bool m_DataLoaded = false;
    bool m_Quit = false;
    bool m_WantTextInput = false;

    // --data / positional argument wins over the remembered folder
    bool m_DataFromCommandLine = false;
    std::string m_DataPath;
    std::string m_DataStatus;
    std::string m_AudioStatus;

    // folder-pick result handoff (written by the dialog callback)
    static char s_PickedPath[1024];
    static int s_PickedState; // 0 = none, 1 = picked, 2 = cancelled/error
    bool m_PickerOpen = false;
};
