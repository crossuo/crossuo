// GPLv3 License
// Copyright (c) 2018 Danny Angelo Carminati Grein

#include "Platform.h"
#include "Globals.h"

namespace Platform
{
#if _WIN32

// FIXME: Add support to all Langcode.iff codes.
void SetLanguageFromSystemLocale()
{
    DEBUG_TRACE_FUNCTION;
    //char buf[4];
    //if (GetProfileStringA("intl", "sLanguage", "default", buf, sizeof(buf)) == 0)
    // https://docs.microsoft.com/en-us/windows/desktop/Intl/language-identifier-constants-and-strings
    auto langId = GetSystemDefaultLangID();
    if (langId == 0x0c0c)
    {
        g_Language = "FRC";
    }
    else
    {
        switch (langId & 0xff)
        {
            case LANG_RUSSIAN:
            {
                g_Language = "RUS";
                break;
            }
            case LANG_FRENCH:
            {
                g_Language = "FRA";
                break;
            }
            case LANG_GERMAN:
            {
                g_Language = "DEU";
                break;
            }
            case LANG_SPANISH:
            {
                g_Language = "ESP";
                break;
            }
            case LANG_JAPANESE:
            {
                g_Language = "JPN";
                break;
            }
            case LANG_KOREAN:
            {
                g_Language = "KOR";
                break;
            }
            case LANG_PORTUGUESE:
            {
                g_Language = "PTB";
                break;
            }
            default:
            {
                g_Language = "ENU";
                break;
            }
        }
    }
    Info(Client, "Locale: %s", g_Language.c_str());
}

void OpenBrowser(const string &url)
{
    ShellExecuteA(0, "Open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

#else // Linux and MacOSX

#include <stdlib.h>

void SetLanguageFromSystemLocale()
{
    DEBUG_TRACE_FUNCTION;

    char *lang;
    lang = getenv("LANG");
    if (lang == nullptr)
    {
        return;
    }

    if (strstr(lang, "fr_CA."))
    {
        g_Language = "FRC";
    }
    else if (strstr(lang, "fr_"))
    {
        g_Language = "FRA";
    }
    else if (strstr(lang, "ru_"))
    {
        g_Language = "RUS";
    }
    else if (strstr(lang, "de_"))
    {
        g_Language = "DEU";
    }
    else if (strstr(lang, "ko_"))
    {
        g_Language = "KOR";
    }
    else if (strstr(lang, "es_"))
    {
        g_Language = "ESP";
    }
    else if (strstr(lang, "ja_"))
    {
        g_Language = "JPN";
    }
    else if (strstr(lang, "pt_BR"))
    {
        g_Language = "PTB";
    }
    else if (strstr(lang, "pt_"))
    {
        g_Language = "PTG";
    }

    Info(Client, "locale: %s", g_Language.c_str());
}

void OpenBrowser(const string &url)
{
#if __APPLE__
#define OPEN_CMD "open "
#else
#define OPEN_CMD "xdg-open "
#endif

    auto cmd = string(OPEN_CMD) + url;
    system(cmd.c_str()); // crossuo adds http in the url, we're a bit safer.
}
#endif
} // namespace Platform
