// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "Platform.h"
#include <cstring>

#if defined(XUO_EMSC)
#include <emscripten.h>
#endif
#include "Globals.h"
#include <common/logging/logging.h>
#include <common/fs.h>
#include <common/str.h>

#if _WIN32
#include <shellapi.h>
#include <shlwapi.h>
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
#if defined(XUO_EMSC)
    if (url.empty())
    {
        return;
    }
    // the game runs on a pthread worker where `window` does not exist:
    // proxied execution opens the tab on the page, inside the click's
    // user activation (popup blockers require it)
    MAIN_THREAD_EM_ASM(
    {
        var url = UTF8ToString($0);
        if (url)
        {
            var w = window.open(url, '_blank');
            if (!w)
                console.warn('[main] popup blocked:', url);
        }
    }, (intptr_t)url.c_str());
#else
    ShellExecuteA(0, "Open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

static astr_t UrlSchemeExecutablePath()
{
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return astr_t(buf);
}

static astr_t UrlSchemeCommandValue()
{
    char buf[1024] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueA(
            HKEY_CURRENT_USER,
            "Software\\Classes\\crossuo\\shell\\open\\command",
            nullptr,
            RRF_RT_REG_SZ,
            nullptr,
            buf,
            &size) != ERROR_SUCCESS)
    {
        return {};
    }
    return astr_t(buf);
}

// scheme registration is user-scope (HKCU\\Software\\Classes), so no
// elevation is needed; the registered command must match this executable
// for the install to count as registered (a stale registration from a
// moved install would still fire the old binary).
bool IsUrlSchemeRegistered()
{
    const auto cmd = UrlSchemeCommandValue();
    return !cmd.empty() && cmd.find(UrlSchemeExecutablePath()) != astr_t::npos;
}

bool RegisterUrlScheme()
{
    const auto exe = UrlSchemeExecutablePath();
    if (exe.empty())
    {
        return false;
    }
    HKEY key = nullptr;
    if (RegCreateKeyExA(
            HKEY_CURRENT_USER,
            "Software\\Classes\\crossuo",
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS)
    {
        return false;
    }
    RegSetValueExA(key, nullptr, 0, REG_SZ, (const BYTE *)"URL:CrossUO", sizeof("URL:CrossUO"));
    // the presence of the URL Protocol value marks the key as a protocol handler
    RegSetValueExA(key, "URL Protocol", 0, REG_SZ, (const BYTE *)"", 1);
    RegCloseKey(key);

    if (RegCreateKeyExA(
            HKEY_CURRENT_USER,
            "Software\\Classes\\crossuo\\shell\\open\\command",
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS)
    {
        return false;
    }
    const auto cmd = "\"" + exe + "\" \"%1\"";
    RegSetValueExA(key, nullptr, 0, REG_SZ, (const BYTE *)cmd.c_str(), DWORD(cmd.size() + 1));
    RegCloseKey(key);
    return true;
}

#else // Linux and MacOSX

#include <stdlib.h>
#include <unistd.h>

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

#if defined(XUO_EMSC)
// the web build runs inside a browser page: scheme handling is owned by the
// page, native OS registration does not apply
bool IsUrlSchemeRegistered()
{
    return false;
}

bool RegisterUrlScheme()
{
    return false;
}
#elif __APPLE__
#include <mach-o/dyld.h>

// scheme registration on macOS is owned by the application bundle's
// Info.plist (CFBundleURLTypes); LaunchServices picks it up when the bundle
// is registered. Registration here re-registers the containing bundle
// (lsregister) so a freshly built bundle becomes live without a re-install;
// detection reads the plist. A bare (non-bundle) build cannot receive
// crossuo:// links.
static astr_t UrlSchemeExecutablePath()
{
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    astr_t buf(size + 1, '\0');
    if (_NSGetExecutablePath(&buf[0], &size) != 0)
    {
        return {};
    }
    buf.resize(strlen(buf.c_str()));
    return buf;
}

static astr_t UrlSchemeAppBundlePath()
{
    auto exe = UrlSchemeExecutablePath();
    const auto marker = "/Contents/MacOS/";
    const auto pos = exe.rfind(marker);
    if (pos == astr_t::npos)
    {
        return {};
    }
    return exe.substr(0, pos);
}

bool IsUrlSchemeRegistered()
{
    const auto bundle = UrlSchemeAppBundlePath();
    if (bundle.empty())
    {
        return false;
    }
    // the bundle is live for crossuo:// links when its Info.plist declares
    // the scheme (CFBundleURLTypes)
    const auto plist = bundle + "/Contents/Info.plist";
    std::string text;
    FILE *f = fopen(plist.c_str(), "rb");
    if (f != nullptr)
    {
        char buf[4096];
        size_t read = 0;
        while ((read = fread(buf, 1, sizeof(buf), f)) > 0)
        {
            text.append(buf, read);
        }
        fclose(f);
    }
    return text.find("crossuo") != std::string::npos;
}

bool RegisterUrlScheme()
{
    const auto bundle = UrlSchemeAppBundlePath();
    if (bundle.empty())
    {
        Info(
            Client,
            "url scheme: not a .app bundle, crossuo:// registration is"
            " not possible (add CFBundleURLTypes to Info.plist)");
        return false;
    }
    const auto cmd = "/System/Library/Frameworks/CoreServices.framework/Frameworks/"
                     "LaunchServices.framework/Support/lsregister -f \"" +
                     bundle + "\"";
    const int code = system(cmd.c_str());
    return code == 0 && IsUrlSchemeRegistered();
}
#else // Linux

// registration is user-scope: a desktop entry in $XDG_DATA_HOME/applications
// (default ~/.local/share/applications) declaring the x-scheme-handler, plus
// the xdg-mime default pick. Re-running is idempotent (the file is
// rewritten with the current executable path).
static astr_t UrlSchemeExecutablePath()
{
    char buf[4096] = {};
    const ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0)
    {
        return {};
    }
    return astr_t(buf, size_t(len));
}

static astr_t UrlSchemeDesktopFilePath()
{
    const char *dataHome = getenv("XDG_DATA_HOME");
    if (dataHome != nullptr && dataHome[0] != '\0')
    {
        return astr_t(dataHome) + "/applications/crossuo.desktop";
    }
    const char *home = getenv("HOME");
    if (home == nullptr)
    {
        return {};
    }
    return astr_t(home) + "/.local/share/applications/crossuo.desktop";
}

bool IsUrlSchemeRegistered()
{
    FILE *f = popen("xdg-mime query default x-scheme-handler/crossuo 2>/dev/null", "r");
    if (f == nullptr)
    {
        return false;
    }
    char buf[512] = {};
    const bool registered =
        fgets(buf, sizeof(buf), f) != nullptr && strstr(buf, "crossuo") != nullptr;
    pclose(f);
    return registered;
}

bool RegisterUrlScheme()
{
    auto exe = UrlSchemeExecutablePath();
    auto file = UrlSchemeDesktopFilePath();
    if (exe.empty() || file.empty())
    {
        return false;
    }
    // shell-quote the executable path (spaces are legal in paths)
    auto quoted = exe;
    const bool quote = exe.find(' ') != astr_t::npos;
    if (quote)
    {
        quoted = "\"" + exe + "\"";
    }

    const auto dir = file.substr(0, file.rfind('/'));
    fs_path_create(fs_path_from(dir));

    FILE *f = fopen(file.c_str(), "w");
    if (f == nullptr)
    {
        return false;
    }
    fprintf(f, "[Desktop Entry]\n");
    fprintf(f, "Type=Application\n");
    fprintf(f, "Name=CrossUO\n");
    fprintf(f, "NoDisplay=true\n");
    fprintf(f, "Exec=%s %%u\n", quoted.c_str());
    fprintf(f, "MimeType=x-scheme-handler/crossuo;\n");
    fclose(f);

    const auto cmd = astr_t("xdg-mime default crossuo.desktop x-scheme-handler/crossuo");
    const int code = system(cmd.c_str());
    return code == 0 && IsUrlSchemeRegistered();
}
#endif
#endif
} // namespace Platform