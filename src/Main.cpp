// AGPLv3 License
// Copyright (C) 2019 Danny Angelo Carminati Grein

#include "Config.h"
#include "CrossUO.h"
#include "GameWindow.h"
#include "Application.h"
#include "SDL_wrapper.h"
#include <time.h>
#include <external/popts.h>
#include <common/logging/logging.h>
#include "Managers/ConfigManager.h"

#if !defined(XUO_WINDOWS)
#include <dlfcn.h>
#define XUO_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define XUO_EXPORT
#endif

//#include <xuocore/plugininterface.h>

extern bool g_var_DisableRenderGame;
bool g_var_DisableRenderGame = false;

extern po::parser g_cli;
po::parser g_cli;

static bool InitCli(int argc, char *argv[])
{
    g_cli["dump-uop"].type(po::string).description("Dump information about a UOP file");
    g_cli["headless"].description("Headless mode, no window is created");
    g_cli["nocrypt"].description("Disable network cryptography");
    g_cli["help"].abbreviation('h').description("print this help screen").callback([&] {
        std::cout << g_cli << '\n';
    });

    // xuolauncher
    g_cli["host"].abbreviation('s').type(po::string).description("LoginServer address");
    g_cli["port"]
        .abbreviation('p')
        .type(po::u32)
        .description("LoginServer port, defaults to 2593")
        .fallback(2593);
    g_cli["login"].abbreviation('l').type(po::string).description("Account username/login");
    g_cli["password"]
        .abbreviation('w')
        .type(po::string)
        .description("Account password")
        .fallback("");
    g_cli["savepassword"].type(po::u32).description(
        "Force saving the password in crossuo.cfg (0 to remove, 1 to save)");

    g_cli["autologin"].type(po::u32).description("1 to log in automatically");
    g_cli["character"].type(po::string).description("When using auto login, use this character");
    g_cli["fastlogin"].type(po::u32).description("1 to do a fast login");

    g_cli["proxy-host"]
        .type(po::string)
        .description("Use proxy, host address. Requires 'proxy-port'");
    g_cli["proxy-port"].type(po::u32).description("Proxy port to use with 'proxy-host'");
    g_cli["proxy-user"].type(po::string).description("Proxy user");
    g_cli["proxy-password"].type(po::string).description("Proxy password");
    g_cli["indirect-rendering"].description("uses indirect rendering pipeline");
    g_cli["quiet"]
        .abbreviation('q')
        .description("Disable logging (almost) completely")
        .callback([&] { g_LogEnabled = eLogSystem::LogSystemNone; });

    // vars
    g_cli["DisableRenderGame"].description("disable game canvas rendering");

    g_cli["config"]
        .abbreviation('c')
        .type(po::string)
        .description("Use a different configuration file");
    g_cli(argc, argv);

    g_var_DisableRenderGame = g_cli["DisableRenderGame"].was_set();

    return g_cli["help"].size() == 0;
}

RenderCmdList *g_renderCmdList = nullptr;
static void *s_renderCmdListData = nullptr;

void gfx_render_list_init()
{
    if (s_renderCmdListData)
    {
        return;
    }

    static const uint32_t s_renderCmdListSize = 512 * 1024;
    s_renderCmdListData = malloc(s_renderCmdListSize);
    assert(s_renderCmdListData);

    // use indirect-rendering to enable delayed render cmds (commands are pushed to the GPU when RenderDraw_Execute is called)
    // don't use this until text resources lifetime isn't fixed (see CFontsManager::DrawA)
    const bool immediateMode = !g_cli["indirect-rendering"].was_set();
    static RenderCmdList s_renderCmdList(
        s_renderCmdListData, s_renderCmdListSize, Render_DefaultState(), immediateMode);
    g_renderCmdList = &s_renderCmdList;
}

void gfx_render_list_destroy()
{
    if (s_renderCmdListData)
    {
        free(s_renderCmdListData);
        s_renderCmdListData = nullptr;
    }
    g_renderCmdList = nullptr;
}

void fatal_error_dialog(const char *message)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message, nullptr);
}

#if !defined(XUO_WINDOWS)
XUO_EXPORT int plugin_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    g_fatalErrorCb = &fatal_error_dialog;
    if (!InitCli(argc, argv))
    {
        return 0;
    }

    LogInit(argc, argv, "crossuo.log");

    if (SDL_Init(SDL_INIT_TIMER) < 0)
    {
        Fatal(
            Client,
            "Window initialization failure.",
            "unable to initialize SDL %s",
            SDL_GetError());
        return EXIT_FAILURE;
    }
    Info(Client, "SDL initialized");

    g_App.Init();
    if (!LoadGlobalConfig())
    {
        return EXIT_FAILURE;
    }

    if (g_cli["dump-uop"].was_set())
    {
        g_dumpUopFile = g_cli["dump-uop"].get().string;
    }

    const bool isHeadless = g_cli["headless"].was_set();
    if (!isHeadless)
    {
        gfx_render_list_init();
        if (!g_GameWindow.Create("CrossUO Client", "Ultima Online", false, 640, 480))
        {
            const char *errMsg =
                "Failed to create CrossUO client window. May be caused by a missing configuration file.";
            Fatal(Client, errMsg, "error initializing game window");
            return -1;
        }
    }
    g_ConfigManager.Init();

    // FIXME: headless: lets end here so we can run on travis for now
    if (isHeadless)
    {
        return EXIT_SUCCESS;
    }

    g_Game.ProcessCommandLine();
    g_Game.LoadPlugins();
    auto ret = g_App.Run();
    SDL_Quit();
    gfx_render_list_destroy();
    return ret;
}

#if !defined(XUO_WINDOWS)
XUO_EXPORT void set_install(REVERSE_PLUGIN_INTERFACE *p)
{
    g_oaReverse.Install = p->Install;
}
#endif
