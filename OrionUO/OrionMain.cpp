#include "stdafx.h"

#include "FileSystem.h"
#include <SDL.h>
#include <time.h>

#if USE_WISP

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
    DEBUG_TRACE_FUNCTION;
    INITLOGGER(L"uolog.txt");

    //ParseCommandLine(); // FIXME
    if (SDL_Init(SDL_INIT_TIMER) < 0)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "Unable to initialize SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    auto path = g_App.ExeFilePath("crashlogs");
    fs_path_create(path);

    char buf[100]{};
    auto t = time(nullptr);
    auto now = *localtime(&t);
    sprintf_s(buf, "/crash_%d%d%d_%d_%d_%d.txt", now.tm_year + 1900, now.tm_mon, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
    path += ToPath(buf);
    INITCRASHLOGGER(path);

    socket_init();
    g_OrionWindow.hInstance = hInstance;
    if (!g_OrionWindow.Create("Orion UO Client", "Ultima Online", true, 640, 480))
    {
        socket_shutdown();
        return 0;
    }

    g_OrionWindow.ShowWindow(true);
    g_OrionWindow.NoResize = true;

    g_Orion.LoadPluginConfig();

    auto r = g_App.Run(hInstance);
    socket_shutdown();
    SDL_Quit();
    return r;
}

#else

#if USE_ORIONDLL
ENCRYPTION_TYPE g_EncryptionType;
#endif

static bool g_isHeadless = false;
extern ENCRYPTION_TYPE g_EncryptionType;

extern COrionWindow g_OrionWindow;

int main(int argc, char **argv)
{
    DEBUG_TRACE_FUNCTION;

    // TODO: good cli parsing api
    // keep this simple for now just for travis-ci
    for (int i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i], "--headless"))
            g_isHeadless = true;
        else if (!strcmp(argv[i], "--nocrypt"))
            g_EncryptionType = ET_NOCRYPT;
    }

    if (SDL_Init(SDL_INIT_TIMER) < 0)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "Unable to initialize SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    SDL_Log("SDL Initialized.");

    INITLOGGER("uolog.txt");
    auto path = g_App.ExeFilePath("crashlogs");
    fs_path_create(path);

    // FIXME: log stuff
    /*
    char buf[100]{};
    auto t = time(nullptr);
    auto now = *localtime(&t);
    sprintf_s(buf, "/crash_%d%d%d_%d_%d_%d.txt", now.tm_year + 1900, now.tm_mon, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
    path += ToPath(buf);
    INITCRASHLOGGER(path);
	*/

    if (!g_isHeadless)
    {
        if (!g_OrionWindow.Create("Orion UO Client", "Ultima Online", false, 640, 480))
        {
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
                "Failed to create OrionUO client window. Fallbacking to headless mode.\n");
            g_isHeadless = true;
        }
    }

    // FIXME: headless: lets end here so we can run on travis for now
    if (g_isHeadless)
        return EXIT_SUCCESS;

    g_Orion.LoadPluginConfig();
    auto ret = g_App.Run();
    SDL_Quit();
    return ret;
}

#endif
