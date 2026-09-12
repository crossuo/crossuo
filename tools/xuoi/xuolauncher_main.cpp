// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Launcher

#include <stdio.h>
#include <string.h>
#if defined(XUO_WINDOWS)
#include <windows.h>
#include <shellapi.h>
#endif

#include <external/gfx/gfx.h>
#include <external/process.h>
#include <xuocore/common.h>

#include "icon_launcher.h"
#include "xuo_updater.h"
#include "xuol_app.h"

static bool run_self_update_instance(int argc, char **argv)
{
    const char *bin = argv[0];
    const auto len = strlen(bin);
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

    win_context win;
    memset(&win, 0, sizeof(win_context));
    win.title = XUOL_AGENT_NAME;
    win.width = 550;
    win.height = 284;
    win.vsync = 0;
    win.icon = &g_icon_xuolauncher;
    if (win_init(&win) != 0)
    {
        LOG_ERROR("could not create launcher window");
        return 1;
    }

    if (!xuol_app_init(win))
    {
        win_shutdown(&win);
        return 1;
    }

    // Main loop
    bool quit = false;
    while (!quit)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            xuol_app_process_event(&event);
            if (event.type == SDL_QUIT)
                quit = true;
            if (
                event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(win.window))
                quit = true;
        }

        float clear[4];
        xuol_ui_clear_color(clear);
        glClearColor(clear[0], clear[1], clear[2], clear[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        xuol_app_frame();
        win_flip(&win);

        if (xuol_app_wants_close())
            quit = true;
    }

    xuol_app_shutdown();
    win_shutdown(&win);
    return 0;
}
