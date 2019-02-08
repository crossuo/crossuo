// GPLv3 License
// Copyright (C) 2019 Danny Angelo Carminati Grein

#include "FileSystem.h"
#include "Config.h"
#include "CrossUO.h"
#include "GameWindow.h"
#include "Application.h"
#include <SDL.h>
#include <time.h>
#include "Managers/ConfigManager.h"

#if !defined(XUO_WINDOWS)
#include <dlfcn.h>
#define XUO_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define XUO_EXPORT
#endif

#include "api/plugininterface.h"

static bool g_isHeadless = false;

#if !defined(XUO_WINDOWS)
XUO_EXPORT int plugin_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
    LogInit("crossuo.log");
    DEBUG_TRACE_FUNCTION;

    if (SDL_Init(SDL_INIT_TIMER) < 0)
    {
        ERROR(Client, "unable to initialize SDL {}", SDL_GetError());
        return EXIT_FAILURE;
    }

    INFO(Client, "SDL Initialized.");
    g_App.Init();
    LoadGlobalConfig();

    // TODO: good cli parsing api
    // keep this simple for now just for travis-ci
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--headless") == 0)
        {
            g_isHeadless = true;
        }
        else if (strcmp(argv[i], "--nocrypt") == 0)
        {
            g_Config.EncryptionType = ET_NOCRYPT;
        }
    }

    if (!g_isHeadless)
    {
        if (!g_GameWindow.Create("CrossUO Client", "Ultima Online", false, 640, 480))
        {
            const char *errMsg =
                "Failed to create CrossUO client window. May be caused by a missing configuration file.";
            ERROR(Client, errMsg);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errMsg, nullptr);
            return -1;
        }
    }
    g_ConfigManager.Init();

    // FIXME: headless: lets end here so we can run on travis for now
    if (g_isHeadless)
    {
        return EXIT_SUCCESS;
    }

    g_Game.LoadPluginConfig();
    auto ret = g_App.Run();
    SDL_Quit();
    return ret;
}

#if !defined(XUO_WINDOWS)
XUO_EXPORT void set_install(REVERSE_PLUGIN_INTERFACE *p)
{
    g_oaReverse.Install = p->Install;
}
#endif
