// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#define LOGGER_MODULE Client

#include "LauncherWindow.h"

#if defined(XUO_LAUNCHER_EMBEDDED)

#include <external/gfx/gfx.h>
#include <external/gfx/ui.h>
#include <external/process.h>
#include <common/logging/logging.h>
#include <xuocore/common.h>
#include "Config.h"
#include "CrossUO.h"
#include "GameWindow.h"
#include "ScreenStages/MainScreen.h"
#include "icon_launcher.h"
#include "xuol_app.h"
#include <revision.h>

#define XUOL_AGENT_NAME LAUNCHER_TITLE

static win_context s_launcher_win;
static bool s_launcher_initialized = false;
static bool s_launcher_visible = false;
static int s_argc = 0;
static char **s_argv = nullptr;

// the launcher is a login-screen tool only
static bool at_login_screen()
{
    return g_CurrentScreen == &g_MainScreen;
}

bool LauncherWindow_Init()
{
    if (s_launcher_initialized)
        return true;

    // the game renderer assumes its GL context is always the current one on
    // the main thread; save it, the launcher setup makes its own current
    SDL_Window *bak_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext bak_context = SDL_GL_GetCurrentContext();

    memset(&s_launcher_win, 0, sizeof(s_launcher_win));
    s_launcher_win.title = XUOL_AGENT_NAME;
    s_launcher_win.width = 640;
    s_launcher_win.height = 420;
    s_launcher_win.vsync = 0;
    s_launcher_win.icon = &g_icon_xuolauncher;
    if (win_init(&s_launcher_win) != 0)
    {
        Error(Client, "could not create the launcher window");
        if (bak_window && bak_context)
            SDL_GL_MakeCurrent(bak_window, bak_context);
        return false;
    }

    if (!xuol_app_init(s_launcher_win, "xuolauncher-embedded.ini", false))
    {
        SDL_GL_DeleteContext(s_launcher_win.context);
        SDL_DestroyWindow(s_launcher_win.window);
        memset(&s_launcher_win, 0, sizeof(s_launcher_win));
        if (bak_window && bak_context)
            SDL_GL_MakeCurrent(bak_window, bak_context);
        return false;
    }

    // give the game its context back before rendering anything else
    if (bak_window && bak_context)
        SDL_GL_MakeCurrent(bak_window, bak_context);

    s_launcher_initialized = true;
    s_launcher_visible = true;
    return true;
}

void LauncherWindow_Shutdown()
{
    if (!s_launcher_initialized)
        return;

    xuol_app_shutdown();
    // win_shutdown() calls SDL_Quit() which would take the client window down
    // with it, so release the launcher resources manually instead
    SDL_GL_DeleteContext(s_launcher_win.context);
    SDL_DestroyWindow(s_launcher_win.window);
    memset(&s_launcher_win, 0, sizeof(s_launcher_win));
    s_launcher_initialized = false;
    s_launcher_visible = false;
}

void LauncherWindow_Show(bool show)
{
    if (!s_launcher_initialized)
        return;
    s_launcher_visible = show;
    if (show)
    {
        SDL_ShowWindow(s_launcher_win.window);
        SDL_RaiseWindow(s_launcher_win.window);
    }
    else
    {
        SDL_HideWindow(s_launcher_win.window);
    }
}

bool LauncherWindow_IsVisible()
{
    return s_launcher_initialized && s_launcher_visible;
}

void LauncherWindow_Open()
{
    if (!s_launcher_initialized && !LauncherWindow_Init())
    {
        return;
    }
    LauncherWindow_Show(true);
}

static bool event_belongs_to_launcher(const SDL_Event &ev)
{
    switch (ev.type)
    {
        case SDL_WINDOWEVENT:
            return ev.window.windowID == SDL_GetWindowID(s_launcher_win.window);
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            return ev.key.windowID == SDL_GetWindowID(s_launcher_win.window);
        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:
            return ev.text.windowID == SDL_GetWindowID(s_launcher_win.window);
        case SDL_MOUSEMOTION:
            return ev.motion.windowID == SDL_GetWindowID(s_launcher_win.window);
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            return ev.button.windowID == SDL_GetWindowID(s_launcher_win.window);
        case SDL_MOUSEWHEEL:
            return ev.wheel.windowID == SDL_GetWindowID(s_launcher_win.window);
        default:
            return false;
    }
}

bool LauncherWindow_ProcessEvent(const SDL_Event &ev)
{
    // global toggle hotkey: Alt+L; works even when the launcher was never
    // created (valid configuration at boot), creating it on demand. The
    // launcher is only available at the login screen: anywhere else the key
    // is handed over to the client untouched.
    if (ev.type == SDL_KEYDOWN && !ev.key.repeat && ev.key.keysym.sym == SDLK_l &&
        (ev.key.keysym.mod & KMOD_ALT) != 0)
    {
        if (!at_login_screen())
            return false;
        if (!s_launcher_initialized)
        {
            if (!LauncherWindow_Init())
                return true;
        }
        else
        {
            LauncherWindow_Show(!s_launcher_visible);
        }
        return true;
    }

    if (!s_launcher_initialized)
        return false;

    if (!event_belongs_to_launcher(ev))
        return false;

    if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)
    {
        // closing the launcher only hides it; the client keeps running
        LauncherWindow_Show(false);
        return true;
    }

    xuol_app_process_event(&ev);
    return true;
}

void LauncherWindow_Render()
{
    if (!s_launcher_initialized)
        return;

    // the app can request to close itself (close after launch, restarts);
    // embedded that means hiding the window, the client stays up
    if (xuol_app_wants_close())
    {
        xuol_app_clear_close_intent();
        LauncherWindow_Show(false);
    }

    // keep the launcher a login-screen tool: as soon as the client moves on
    // (connect flow, character screen, in game) put it away
    if (s_launcher_visible && g_CurrentScreen != nullptr && !at_login_screen())
    {
        LauncherWindow_Show(false);
    }

    if (!LauncherWindow_IsVisible())
        return;

    // save the client GL state, the launcher has its own context
    SDL_Window *bak_window = SDL_GL_GetCurrentWindow();
    SDL_GLContext bak_context = SDL_GL_GetCurrentContext();

    SDL_GL_MakeCurrent(s_launcher_win.window, s_launcher_win.context);
    float clear[4];
    xuol_ui_clear_color(clear);
    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    xuol_app_frame();

    // the imgui SDL backend toggles the process-global OS cursor visibility
    // every frame; hide it back whenever the mouse is not over the launcher
    // so the game keeps drawing its own cursor only
    if (SDL_GetMouseFocus() != s_launcher_win.window)
    {
        SDL_ShowCursor(SDL_DISABLE);
    }

    SDL_GL_SwapWindow(s_launcher_win.window);
    if (bak_window && bak_context)
        SDL_GL_MakeCurrent(bak_window, bak_context);
}

bool LauncherWindow_Pump()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev) != 0)
    {
        if (ev.type == SDL_QUIT)
            return false;
        if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE &&
            ev.window.windowID == SDL_GetWindowID(s_launcher_win.window))
            return false;
        LauncherWindow_ProcessEvent(ev);
    }
    LauncherWindow_Render();
    SDL_Delay(16);
    return true;
}

//
// Client-side bridge used by the launcher app (see xuol_app.h)
//

namespace xuol {

apply_result client_apply_profile(const client_profile &profile)
{
    xuol_app_notify_profile_applied();
    const bool live = ApplyClientProfile(
        profile.login,
        profile.password,
        profile.remember_pw,
        profile.autologin,
        profile.character,
        profile.loginserver,
        profile.data_path,
        profile.clientversion,
        profile.clienttype,
        profile.crypt,
        profile.profile); // the launcher profile names the shard
    if (live)
    {
        return apply_result::applied;
    }
    // the profile was persisted to crossuo.cfg, but the running client state
    // does not allow a safe live application (in game, connected, or the data
    // path changed after the client data was loaded)
    return apply_result::needs_restart;
}

apply_result client_apply_shard(const shard_profile &profile)
{
    xuol_app_notify_profile_applied();
    ::shard_profile shard;
    shard.name = profile.name;
    shard.loginserver = profile.loginserver;
    shard.clientversion = profile.clientversion;
    shard.clienttype = profile.clienttype;
    shard.crypt = profile.crypt;
    shard.product_url = profile.product_url;
    shard.web = profile.web;
    shard.account_page = profile.account_page;
    shard.help = profile.help;
    shard.video = profile.video;
    shard.credits = profile.credits;
    shard.other = profile.other;
    shard.community = profile.community;
    shard.seed_override = profile.seed_override;
    shard.system_check_override = profile.system_check_override;
    shard.assistant = profile.assistant;
    shard.language = profile.language;
    shard.report_language = profile.report_language;
    //a shard switch is a restart-class change: the shard profile carries the
    //client version/type/crypt/data identity, which cannot be safely
    //re-fixed-up on a running session (and a live address-only swap does not
    //reliably retarget an in-flight login state). Always report needs_restart
    //so the caller latches a client reload; ApplyShardProfile persisted the
    //new shard already - to the default crossuo.cfg when the client was
    //started with --config - and the relaunched client (restart args drop
    //--config) boots straight into it
    ApplyShardProfile(shard);
    return apply_result::needs_restart;
}

void client_set_restart_args(int argc, char **argv)
{
    s_argc = argc;
    s_argv = argv;
}

void client_request_restart()
{
    std::vector<char *> args;
    bool drop_next = false;
    for (int i = 0; i < s_argc && s_argv && s_argv[i]; i++)
    {
        //the profile that came in via -c/--config is a read-only input: the
        //shard change that triggered this restart was persisted to the
        //default crossuo.cfg (see SaveGlobalConfig), so the relaunched client
        //must boot from the default file or it would reload the old shard
        if (drop_next)
        {
            drop_next = false;
            continue;
        }
        if (strcmp(s_argv[i], "-c") == 0 || strcmp(s_argv[i], "--config") == 0)
        {
            //both "--config <path>" and "--config=<path>" forms
            if (!strchr(s_argv[i], '='))
            {
                drop_next = true;
            }
            continue;
        }
        args.push_back(s_argv[i]);
    }
    if (args.empty() || !s_argv)
    {
        const auto bin = fs_path_process();
        args.push_back(const_cast<char *>(fs_path_ascii(bin)));
    }
    args.push_back(nullptr);

    LOG_INFO("restarting %s", args[0]);
    process_s process;
    const auto options = process_option_inherit_environment | process_option_child_detached;
    if (process_create(args.data(), options, &process) == 0)
    {
        g_GameWindow.Destroy();
    }
    else
    {
        Error(Client, "could not relaunch the client");
    }
}

} // namespace xuol

#endif // XUO_LAUNCHER_EMBEDDED
