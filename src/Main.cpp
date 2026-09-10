// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include <algorithm>
#include <time.h>
#include <common/str.h>
#include "Config.h"
#include "revision.h"
#include "Constants.h" // CPU_USAGE_DELAY
#include "Globals.h" // g_Ticks
#include "Misc.h" // g_MainThread
#include "CrossUO.h"
#include "GameWindow.h"
#include "Platform.h"
#include "SDL_wrapper.h"
#include "Wisp/WispThread.h"
#include <external/popts.h>
#include <external/process.h>
#include <xuocore/client_info.h>
#include <xuocore/uodata.h>
#include <xuocore/vfs.h>
#include <common/logging/logging.h>
#include "Managers/ConfigManager.h"
#include "Managers/ConnectionManager.h"
#include "Managers/PacketManager.h"

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

    // streamed install; typed so both --streaming and
    // --streaming=<dir> parse (empty value = the default streaming dir)
    g_cli["streaming"].type(po::string).description(
        "streamed install mode, optional value = install dir");
    g_cli["streaming-offline"].description(
        "streaming: never hit the network, cache only");
    g_cli["streaming-zerolat"].description(
        "streaming: serve placeholders instantly and heal in the background ");
    g_cli["streaming-trap"].type(po::string).description(
        "streaming: set to 'off' to disable hole traps - reads of undownloaded"
        " data return zeros (escape hatch)");
    g_cli["streaming-workers"].type(po::u32).description(
        "streaming: download workers (default cpu-1, max 8)");
    g_cli["streaming-record"].type(po::string).description(
        "streaming: record asset accesses to <file.csv>");
    g_cli["streaming-record-scenario"].type(po::string).description(
        "streaming recorder scenario name");
    g_cli["streaming-backend"].type(po::string).description(
        "streaming: 'arena' for the memory granule store (web groundwork)");
    g_cli["streaming-subset"].type(po::string).description(
        "streaming: 'web' plans the web product subset (drops the large"
        " legacy files)");

    g_cli["config"]
        .abbreviation('c')
        .type(po::string)
        .description("Use a different configuration file");
    g_cli(argc, argv);

    g_var_DisableRenderGame = g_cli["DisableRenderGame"].was_set();

    return g_cli["help"].size() == 0;
}

#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES) || defined(RENDERER_LEGACY))
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

#if defined(RENDERER_LEGACY)
    const bool immediateMode = true;
#else
    const bool immediateMode = !g_cli["indirect-rendering"].was_set();
#endif //#if defined(RENDERER_LEGACY)
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
#endif // #if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES) || defined(RENDERER_LEGACY))

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

    // builds the streaming params from the command line flags
    auto streaming_params = []() {
        vfs_params params;
        params.enabled = true;
        params.offline = g_cli["streaming-offline"].was_set();
        params.zerolat = g_cli["streaming-zerolat"].was_set();
        if (g_cli["streaming-trap"].was_set())
        {
            // --streaming-trap=off: holes stay readable (zeros) - the escape
            // hatch when the fault handler misbehaves
            params.traps = g_cli["streaming-trap"].get().string != "off";
        }
        if (g_cli["streaming-workers"].was_set())
            params.workers = g_cli["streaming-workers"].get().u32;
        // web groundwork: in-memory granule backend + web product subset
        // (excludes the large legacy files from the plan)
        if (g_cli["streaming-backend"].was_set())
            params.store_backend = g_cli["streaming-backend"].get().string == "arena"
                                       ? vfs_store_backend_arena
                                       : vfs_store_backend_sparse;
        if (g_cli["streaming-subset"].was_set())
            params.subset =
                g_cli["streaming-subset"].get().string == "web" ? vfs_subset_web : vfs_subset_none;
        // popts: .get() on an option that was never set reads out of bounds
        // (release builds compile the guard out) - always check was_set first
        astr_t dir;
        if (g_cli["streaming"].was_set())
            dir = g_cli["streaming"].get().string;
        if (dir.empty())
            dir = fs_path_ascii(FilePath("streamed"));
        params.install_dir = fs_path_from(dir.c_str());
        // cache (blob store + state db) lives under the streaming dir
        params.blobs_dir = fs_path_join(params.install_dir, "cache", "blobs");
        if (g_cli["streaming-record"].was_set())
        {
            params.record_path =
                fs_path_from(g_cli["streaming-record"].get().string.c_str());
            if (g_cli["streaming-record-scenario"].was_set())
                params.record_scenario =
                    g_cli["streaming-record-scenario"].get().string;
        }
        return params;
    };

    const bool config_exists = LoadGlobalConfig();

    // streaming-mode selection, in order of precedence:
    //  1] --streaming on the command line: stream into the given (or
    //     default) directory, even when a data path is configured
    //  2] a config file that sets the data path (CustomPath): use that
    //     installation, no streaming (the original behavior)
    //  3] otherwise (no config file, or a config without a data path):
    //     stream automatically - the streamed install replaces the
    //     launcher-driven setup
    const bool streaming =
        g_cli["streaming"].was_set() || !(config_exists && g_Config.CustomPathSet);
    if (streaming)
    {
        vfs_set_params(streaming_params());
        g_UOPath = fs_path_from(vfs_install_path());
        if (!config_exists)
        {
            // no config: assume the good defaults the streamed install runs
            // with (latest client version, and everything derived from it -
            // cryptography, client type, packet layout, view range)
            ApplyClientVersionDefaults();
        }
        uo_data_init(fs_path_ascii(g_UOPath), g_Config.ClientVersion, g_Config.UseVerdata);
        Info(Client, "\tStreaming Data Path: %s", fs_path_ascii(g_UOPath));
        // the streamed product is the modern uop distribution; a config
        // pinned to a pre-uop client version cannot consume it
        if (g_Config.ClientVersion < VERSION(7, 0, 0, 0))
        {
            g_Config.ClientVersion = CV_LATEST;
            char str[32] = {};
            client_version_string(g_Config.ClientVersion, str, sizeof(str));
            g_Config.ClientVersionString = str;
            Warning(Client, "streaming requires a 7.x client version, using %s instead", str);
        }
        return true;
    }

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
        const auto uopath = fs_path_ascii(g_UOPath);
        bool valid_client = uopath && uopath[0] && fs_path_exists(g_UOPath);
        if (!valid_client)
        {
            Warning(Client, "Couldn't find data files in the path: %s", uopath);
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

        const auto client_exe = fs_path_join(uopath, "client.exe");
        const bool client_exe_exists = fs_path_exists(client_exe);
        const bool check_valid_data = !g_Config.ClientVersionModified;
        if (check_valid_data)
        {
            Info(
                Client,
                "client version was not set in config, trying to auto-detect from '%s'",
                fs_path_ascii(client_exe));
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
            else
            {
                Info(
                    Client,
                    "a valid client.exe was not found, please set 'ClientVersion' in the config file or in launcher.");
            }
        }

        Info(Client, "validating data compatibility:");
        const bool new_client = fs_path_exists(fs_path_join(uopath, "MainMisc.uop")); // >=7
        Info(Client, "  >=7.x MainMisc.uop:%s found", new_client ? "" : " not");
        const bool old_client3 = fs_path_exists(fs_path_join(uopath, "chardata.mul")); // <=3
        Info(Client, "  <=3.x chardata.mul:%s found", old_client3 ? "" : " not");
        const bool old_client6 = fs_path_exists(fs_path_join(uopath, "map4.mul")) &&
                                 !fs_path_exists(fs_path_join(uopath, "map5.mul")); // <=6
        Info(Client, "  >3.x && <= 6.x map4.mul:%s found", old_client6 ? "" : " not");
        const bool old_client = old_client3 || old_client6;
        valid_client &= g_Config.ClientVersion >= VERSION(7, 0, 0, 0) ? new_client : old_client;
        if (!valid_client)
        {
            if (check_valid_data)
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
            else
            {
                Warning(
                    Client,
                    "couldn't validate that your data files are compatible with client version");
                Warning(
                    Client, "client will continue but errors may happen - please report any issue");
            }
        }
    }

    return true;
}

// Optional frame statistics, enabled with the XUO_FPS_LOG environment variable.
// Prints aggregated frame timings and renderer counters every 5 seconds to stdout.
static void FrameStatsTick()
{
#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES))
    static const bool enabled = getenv("XUO_FPS_LOG") != nullptr;
    if (!enabled)
    {
        return;
    }

    static uint32_t lastFrameTick = 0;
    static uint32_t windowTick = 0;
    static uint32_t frames = 0;
    static uint32_t minMs = 0xFFFFFFFF;
    static uint32_t maxMs = 0;
    static uint64_t sumMs = 0;

    const uint32_t now = SDL_GetTicks();
    if (lastFrameTick != 0)
    {
        const uint32_t ms = now - lastFrameTick;
        frames++;
        sumMs += ms;
        if (ms < minMs)
        {
            minMs = ms;
        }
        if (ms > maxMs)
        {
            maxMs = ms;
        }
    }
    lastFrameTick = now;

    if (windowTick == 0)
    {
        windowTick = now;
        return;
    }

    if (now - windowTick >= 5000)
    {
        const uint32_t elapsed = now - windowTick;
        RenderFrameStats stats;
        Render_GetFrameStats(&stats);
        printf(
            "[fps] %ums: frames=%u fps=%.1f frame avg=%.2fms min=%ums max=%ums | "
            "draws=%llu uploads=%llu bytes=%llukB batches=%llu batchedQuads=%llu\n",
            elapsed,
            frames,
            frames * 1000.0 / elapsed,
            frames ? double(sumMs) / frames : 0.0,
            minMs == 0xFFFFFFFF ? 0 : minMs,
            maxMs,
            (unsigned long long)stats.drawCalls,
            (unsigned long long)stats.vertexUploads,
            (unsigned long long)(stats.bytesUploaded / 1024),
            (unsigned long long)stats.batchFlushes,
            (unsigned long long)stats.batchedQuads);
        fflush(stdout);
        Render_ResetFrameStats();
        frames = 0;
        sumMs = 0;
        minMs = 0xFFFFFFFF;
        maxMs = 0;
        windowTick = now;
    }
#endif // NEW_RENDERER_ENABLED && (USE_GL3 || USE_GLES)
}

static int Run()
{
    bool quit = false;
    while (!quit)
    {
        FrameStatsTick();

        // 1] Handle all SDL events. Mouse, keyboard, windows etc..
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            quit = Wisp::g_WispWindow->OnWindowProc(event);
            if (quit)
            {
                break;
            }
        }

        // 2] Run main loop, packets, rendering etc..
        g_Ticks = SDL_GetTicks();
        g_ConnectionManager.Recv();
        g_PacketManager.ProcessPluginPackets();
        g_PacketManager.SendMegaClilocRequests();
        g_Game.Process(true);

        // 3] Calculate proper delay based on settings and frame time.
        // the test harness (XUO_TEST_LOGIN) runs uncapped: the window is not
        // focused during automated runs, which would apply the inactive-window
        // delay and mask the real frame cost being benchmarked
        if (!TestAutoLogin())
        {
            int32_t iDelay = g_GameWindow.GetRenderDelay();

            // a frame delay of 0 means uncapped; otherwise only sleep to hit
            // the configured cap (the old 2ms floor forced ~140fps ceilings)
            if (iDelay > 0)
            {
                const int32_t delay = iDelay - (SDL_GetTicks() - g_Ticks);
                if (delay > 0)
                {
                    SDL_Delay(delay);
                }
            }
        }
    }
    return EXIT_SUCCESS;
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

    str_init();
    Info(Client, "initializing application");
    g_MainThread = Wisp::CThread::GetCurrentThreadId();
    g_ExePath = fs_path_current();
    g_UOPath = fs_path_current();

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
#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES) || defined(RENDERER_LEGACY))
        gfx_render_list_init();
#endif // #if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES) || defined(RENDERER_LEGACY))
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
    auto ret = Run();
    SDL_Quit();
    str_terminate();
#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES) || defined(RENDERER_LEGACY))
    gfx_render_list_destroy();
#endif // #if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES) || defined(RENDERER_LEGACY))
    return ret;
}

#if !defined(XUO_WINDOWS)
XUO_EXPORT void set_install(REVERSE_PLUGIN_INTERFACE *p)
{
    g_oaReverse.Install = p->Install;
}
#endif
