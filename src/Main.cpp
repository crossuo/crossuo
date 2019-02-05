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

#include "GTypes.h"
#include "P7_Cproxy.h"
static hP7_Client       g_hClient = NULL;
static hP7_Telemetry    g_hTel    = NULL;
static hP7_Trace        g_hTrace  = NULL;
static hP7_Trace_Module g_pModule = NULL;
static tUINT8           g_bTID1   = 0;
static tUINT8           g_bTID2   = 0;
static tUINT64          g_qwI     = 0;

#define P7_CRITICAL(i_pFormat, ...)  P7_TRACE_ADD(g_hTrace, 0, P7_TRACE_LEVEL_CRITICAL, g_pModule, i_pFormat, __VA_ARGS__)
void MyLogFunction(const tXCHAR *i_pFormat, ...)
{
    va_list l_pVl;
    va_start(l_pVl, i_pFormat);
    P7_Trace_Embedded(g_hTrace, 0, P7_TRACE_LEVEL_INFO, g_pModule, (tUINT16)__LINE__, __FILE__, __FUNCTION__, &i_pFormat, &l_pVl);
    va_end(l_pVl);
}


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
    P7_Set_Crash_Handler();
    // Verb = 0 info, 1 debug, 2 warn, 3 error, 4 critical
    // Trace = 0 trace, 1 debug, 2 info, 3 warn, 4 error, 5 critical
    g_hClient = P7_Client_Create(TM("/P7.Sink=Console /P7.Name=crossuo /P7.On=1 /P7.Verb=0 /P7.Pool=4096 /P7.Dir=logs /P7.Format=\"%td [%lv] [%cn] [%tn] %fs:%fl@%fn %ms\""));
    //using the client create telemetry & trace channels
    g_hTel    = P7_Telemetry_Create(g_hClient, TM("TelemetryChannel"), NULL);
    g_hTrace  = P7_Trace_Create(g_hClient, TM("TraceChannel"), NULL);

    if ((NULL == g_hClient) || (NULL == g_hTel) || (NULL == g_hTrace))
    {
        printf("Initialization error\n");
    }

    //register current application module (it isn't obligatory)
    g_pModule = P7_Trace_Register_Module(g_hTrace, TM("Main"));
    //register current application thread (it isn't obligatory)
    P7_Trace_Register_Thread(g_hTrace, TM("Main"), 0);

    //adding 2 counters into common group
    if (!P7_Telemetry_Create_Counter(g_hTel, TM("Group/counter 1"), 0, 1023, 1000, 1, &g_bTID1))
    {
        printf("can't create counter\n");
    }

    if (!P7_Telemetry_Create_Counter(g_hTel, TM("Group/counter 2"), 0, 1023, 1000, 1, &g_bTID2))
    {
        printf("can't create counter\n");
    }

    //delivering trace messages
    for (g_qwI = 0ULL; g_qwI < 1000ULL; g_qwI ++)
    {
        P7_TRACE_ADD(g_hTrace, 0, P7_TRACE_LEVEL_TRACE, g_pModule, TM("Test trace message #%I64d"), g_qwI);
        P7_TRACE_ADD(g_hTrace, 0, P7_TRACE_LEVEL_INFO,  g_pModule, TM("Test info message #%I64d"), g_qwI);
        P7_TRACE_ADD(g_hTrace, 0, P7_TRACE_LEVEL_ERROR, g_pModule, TM("Test error message #%I64d"), g_qwI);
        //user defined macro
        P7_CRITICAL(TM("Test critical message #%I64d"), g_qwI);
    }
    MyLogFunction(TM("Test embedded function, iteration: %I64d"), g_qwI);
    //delivering telemetry samples
    for (g_qwI = 0ULL; g_qwI < 100000ULL; g_qwI ++)
    {
        P7_Telemetry_Put_Value(g_hTel, g_bTID1, (g_qwI & 0x3FFull));
        P7_Telemetry_Put_Value(g_hTel, g_bTID2, ((g_qwI + 11ull) & 0x3FFull));
    }



    DEBUG_TRACE_FUNCTION;

    if (SDL_Init(SDL_INIT_TIMER) < 0)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION, "Unable to initialize SDL: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_Log("SDL Initialized.");
    g_App.Init();
    INITLOGGER(ToPath("crossuo.log"));
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

    // FIXME: log stuff
    /*
    auto path = g_App.ExeFilePath("crashlogs");
    fs_path_create(path);
    char buf[100]{};
    auto t = time(nullptr);
    auto now = *localtime(&t);
    sprintf_s(buf, "/crash_%d%d%d_%d_%d_%d.txt", now.tm_year + 1900, now.tm_mon, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
    path += ToPath(buf);
    INITCRASHLOGGER(path);
	*/

    if (!g_isHeadless)
    {
        if (!g_GameWindow.Create("CrossUO Client", "Ultima Online", false, 640, 480))
        {
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                "Error",
                "Failed to create CrossUO client window. May be caused by a missing configuration file.\n",
                nullptr);
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

    //unregister current application thread (it is obligatory if thread was registered)
    P7_Trace_Unregister_Thread(g_hTrace, 0);
    if (g_hTel)
    {
        P7_Telemetry_Release(g_hTel);
        g_hTel = NULL;
    }
    if (g_hTrace)
    {
        P7_Trace_Release(g_hTrace);
        g_hTrace = NULL;
    }
    if (g_hClient)
    {
        P7_Client_Release(g_hClient);
        g_hClient = NULL;
    }
    P7_Clr_Crash_Handler();

    return ret;
}

#if !defined(XUO_WINDOWS)
XUO_EXPORT void set_install(REVERSE_PLUGIN_INTERFACE *p)
{
    g_oaReverse.Install = p->Install;
}
#endif
