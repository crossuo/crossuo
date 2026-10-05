// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Launcher

#include <stdio.h>
#include <string.h>

#include <external/gfx/gfx.h>
#include <xuocore/common.h>

#include "icon_launcher.h"
#include "xuol_app.h"

// window identity for the standalone launcher host (the embedded host maps it
// to its own title, see src/LauncherWindow.cpp)
#define XUOL_AGENT_NAME "EAMythic Patch Client"

int main(int argc, char **argv)
{
    LOG_INFO("started %s in %s", argv[0], fs_path_ascii(fs_path_current()));

    win_context win;
    memset(&win, 0, sizeof(win_context));
    win.title = XUOL_AGENT_NAME;
    win.width = 640;
    win.height = 420;
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
