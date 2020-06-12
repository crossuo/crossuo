// AGPLv3 License
// Copyright (c) 2018 Danny Angelo Carminati Grein

#include "Platform.h"
#include "Globals.h"
#include <common/logging/logging.h>

#if _WIN32
#include <Shellapi.h>
#endif

namespace Platform
{
#if _WIN32

// FIXME: Add support to all Langcode.iff codes.
void SetLanguageFromSystemLocale()
{
    //char buf[4];
    //if (GetProfileStringA("intl", "sLanguage", "default", buf, sizeof(buf)) == 0)
    // https://docs.microsoft.com/en-us/windows/desktop/Intl/language-identifier-constants-and-strings
    auto langId = GetSystemDefaultLangID();
    if (langId == 0x0c0c)
    {
        g_Language = "frc";
    }
    else
    {
        switch (langId & 0xff)
        {
            case LANG_RUSSIAN:
            {
                g_Language = "rus";
                break;
            }
            case LANG_FRENCH:
            {
                g_Language = "fra";
                break;
            }
            case LANG_GERMAN:
            {
                g_Language = "deu";
                break;
            }
            case LANG_SPANISH:
            {
                g_Language = "esp";
                break;
            }
            case LANG_JAPANESE:
            {
                g_Language = "jpn";
                break;
            }
            case LANG_KOREAN:
            {
                g_Language = "kor";
                break;
            }
            case LANG_PORTUGUESE:
            {
                g_Language = "ptb";
                break;
            }
            default:
            {
                g_Language = "enu";
                break;
            }
        }
    }
    Info(Client, "Locale: %s", g_Language.c_str());
}

void OpenBrowser(const astr_t &url)
{
    ShellExecuteA(0, "Open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

#else // Linux and MacOSX

#include <stdlib.h>

void SetLanguageFromSystemLocale()
{
    char *lang;
    lang = getenv("LANG");
    if (lang == nullptr)
    {
        return;
    }

    if (strstr(lang, "fr_CA."))
    {
        g_Language = "frc";
    }
    else if (strstr(lang, "fr_"))
    {
        g_Language = "fra";
    }
    else if (strstr(lang, "ru_"))
    {
        g_Language = "rus";
    }
    else if (strstr(lang, "de_"))
    {
        g_Language = "deu";
    }
    else if (strstr(lang, "ko_"))
    {
        g_Language = "kor";
    }
    else if (strstr(lang, "es_"))
    {
        g_Language = "esp";
    }
    else if (strstr(lang, "ja_"))
    {
        g_Language = "jpn";
    }
    else if (strstr(lang, "pt_BR"))
    {
        g_Language = "ptb";
    }
    else if (strstr(lang, "pt_"))
    {
        g_Language = "ptg";
    }

    Info(Client, "locale: %s", g_Language.c_str());
}

void OpenBrowser(const astr_t &url)
{
#if __APPLE__
#define OPEN_CMD "open "
#else
#define OPEN_CMD "xdg-open "
#endif

    auto cmd = astr_t(OPEN_CMD) + url;
    system(cmd.c_str()); // crossuo adds http in the url, we're a bit safer.
}
#endif
} // namespace Platform
