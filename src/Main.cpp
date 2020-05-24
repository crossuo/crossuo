// AGPLv3 License
// Copyright (C) 2019 Danny Angelo Carminati Grein

#include "Config.h"
#include "revision.h"
#include "CrossUO.h"
#include "GameWindow.h"
#include "Application.h"
#include "Platform.h"
#include "SDL_wrapper.h"
#include <time.h>
#include <external/popts.h>
#include <external/process.h>
#include <xuocore/client_info.h>
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

bool load_config()
{
#if defined(XUO_WINDOWS)
#define LAUNCHER_EXE "xuolauncher.exe"
#else
#define LAUNCHER_EXE "xuolauncher"
#endif

    const bool config_exists = LoadGlobalConfig();
    const auto bin_fs = fs_path_join(fs_path_current(), LAUNCHER_EXE);
    const auto bin = fs_path_ascii(bin_fs);
    const bool launcher_exists = fs_path_exists(bin_fs);
    if (!config_exists && !launcher_exists)
    {
        Platform::OpenBrowser("https://crossuo.com/#download");
        ErrorMessage(
            Client,
            "You need X:UO Launcher to launch CrossUO.\n"
            "Please download from https://crossuo.com/#download",
            "Could not find X:UO Launcher.");
        return false;
    }

    auto launcher = [bin]() {
        // TODO: check if one instance is already running and do nothing.
        const char *args[] = { bin, 0 };
        Info(Client, "Launching X:UO Launcher %s due missing config", bin);
        process_s process;
        const auto options = process_option_inherit_environment | process_option_child_detached;
        if (process_create(args, options, &process) != 0)
        {
            ErrorMessage(Client, "Could not launch X:UO Launcher", "could not launch: %s", bin);
        }
        SDL_Delay(500);
    };

    if (!config_exists && launcher_exists)
    {
        launcher();
        return false;
    }

    if (config_exists)
    {
        const auto uopath = fs_path_ascii(g_App.m_UOPath);
        bool valid_client = uopath && uopath[0] && fs_path_exists(g_App.m_UOPath);
        if (!valid_client)
        {
            if (launcher_exists)
            {
                launcher();
            }
            ErrorMessage(
                Client,
                "Couldn't find Ultima Online(tm) data files.\n"
                "Please download the original ultima client and configure CrossUO to use the "
                "original's client installation directory.",
                "could not find data path: %s.",
                uopath);
            return false;
        }

        if (!g_Config.ClientVersionModified)
        {
            const auto client_exe = fs_path_join(uopath, "client.exe");
            const bool client_exe_exists = fs_path_exists(client_exe);
            if (client_exe_exists)
            {
                client_info info;
                client_version(fs_path_ascii(client_exe), info);
                if (info.version)
                {
                    g_Config.ClientVersion = info.version;
                    Info(
                        Client,
                        "Client version wasn't set, using auto-detected version from data.");

                    char str[32] = {};
                    client_version_string(info.version, str, sizeof(str));
                    Info(Client, "Using client version: %s.", str);
                    g_Config.ClientVersionString = str;
                }
                else
                {
                    Warning(
                        Client,
                        "Client version not found, signature: 0x%016lx, 0x%08x",
                        info.xxh3,
                        info.crc32);
                }
            }
        }

        const bool new_client = fs_path_exists(fs_path_join(uopath, "MainMisc.uop"));  // 7+
        const bool old_client3 = fs_path_exists(fs_path_join(uopath, "chardata.mul")); // <3
        const bool old_client6 = fs_path_exists(fs_path_join(uopath, "map5.mul"));     // 6+
        const bool old_client = old_client3 || old_client6;
        valid_client &= g_Config.ClientVersion >= VERSION(7, 0, 0, 0) ? new_client : old_client;
        if (!valid_client)
        {
            if (launcher_exists)
            {
                launcher();
            }
            ErrorMessage(
                Client,
                "Couldn't find valid Ultima Online(tm) data files matching your current configuration.\n"
                "Make sure your configuration is pointing to the correct data files for the"
                "correct version of your client installation directory.",
                "could not find data files for client version %s.",
                g_Config.ClientVersionString.c_str());
            return false;
        }
    }

    return true;
}

#if !defined(XUO_WINDOWS)
XUO_EXPORT int plugin_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
#if 0 // TEST encoding
    str_init();
    // Add some tests, this cliloc message seems to use broken UTF8
    // "Hey buddy... Looking for work?"
    uint8_t d[] = {
        0x48, 0x65, 0x79, 0x20, 0x62, 0x75, 0x64, 0x64, 0x79, 0x2E, 0xC2,
        0xA0, 0x20, 0x4C, 0x6F, 0x6F, 0x6B, 0x69, 0x6E, 0x67, 0x20, 0x66,
        0x6F, 0x72, 0x20, 0x77, 0x6F, 0x72, 0x6B, 0x3F
    };
    // cp1250 - issue #221
    //unsigned char a[] = { 0x56, 0xE1, 0x6c, 0x65, 0xE8, 0x6E, 0xED, 0x6B }; // input
    astr_t s1( reinterpret_cast< char const* >( d ), sizeof( d ) );
    str_terminate();
    exit(0);
#endif

    g_fatalErrorCb = &fatal_error_dialog;
    if (!InitCli(argc, argv))
    {
        return 0;
    }

    LogInit(argc, argv, "crossuo.log");

    if (SDL_Init(SDL_INIT_TIMER) < 0)
    {
        ErrorMessage(
            Client,
            "Window initialization failure.",
            "unable to initialize SDL %s",
            SDL_GetError());
        return EXIT_FAILURE;
    }
    Info(Client, "SDL initialized");

    g_App.Init();
    if (!load_config())
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
        if (!g_GameWindow.Create(CLIENT_TITLE, false, 640, 480))
        {
            const char *errMsg =
                "Failed to create CrossUO client window. May be caused by a missing configuration file.";
            ErrorMessage(Client, errMsg, "error initializing game window");
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
