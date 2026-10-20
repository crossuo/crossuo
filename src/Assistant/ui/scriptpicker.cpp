// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scriptpicker.h: the desktop backend (tinyfiledialogs) and the web
// backend (the page shell's <input type=file> bridge). Only this file opens
// a file dialog.

#include "scriptpicker.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <stdio.h>
#if defined(XUO_EMSC)
#include <atomic>
#include <stdint.h>
#endif

#include "assistantui.h" // AssistantUI_MarkOptionsDirty
#include "../config/assistantconfig.h"
#include "../script/scriptlist.h"

#if defined(XUO_EMSC)
#include <emscripten.h>

// The page (resources/shell.html) answers through the shared wasm heap: in a
// PROXY_TO_PTHREAD build the page cannot call into the module, so the worker
// polls these exports. __xuoScriptPick writes status (0 = still open,
// 1 = done) and the newline-separated absolute paths into this static pair;
// HEAPU32 reads it from the page (the vfs_web_progress_addrs pattern).
namespace {
// the pair the page reads through HEAPU32/HEAPU8 (keep contiguous and 8-aligned)
std::atomic<int> s_PickStatus{ 1 };
char s_PickPaths[4096] = { 0 };
}

extern "C" EMSCRIPTEN_KEEPALIVE uint32_t xuo_script_pick_addrs(void)
{
    return uint32_t(uintptr_t(&s_PickStatus));
}

static const char *xuo_script_pick_result(void)
{
    return s_PickPaths;
}

static int xuo_script_pick_status(void)
{
    return s_PickStatus.load();
}
#else
#include <external/tinyfiledialogs.h>
#endif

namespace xuoassist {

namespace {

bool s_LoadPending = false;
vector<astr_t> s_DesktopPicked; // the synchronous desktop result, until polled
#if defined(XUO_EMSC)
constexpr uint32_t WEB_PICK_TIMEOUT_MS = 120000;
uint32_t s_WebRequestAt = 0;
#endif

void split_paths(const char *text, vector<astr_t> &out)
{
    if (text == nullptr)
    {
        return;
    }
    const astr_t joined = text;
    size_t start = 0;
    while (start <= joined.size())
    {
        size_t end = joined.find_first_of("|\n", start);
        if (end == astr_t::npos)
        {
            end = joined.size();
        }
        const astr_t part = joined.substr(start, end - start);
        if (!part.empty())
        {
            out.push_back(part);
        }
        start = end + 1;
    }
}

void remember_dir(const astr_t &path)
{
    const astr_t dir = ScriptList_DirName(path);
    if (!dir.empty() && dir != ".")
    {
        g_AssistantConfig.SetScriptLastDir(dir);
        AssistantUI_MarkOptionsDirty();
    }
}

} // namespace

astr_t ScriptPicker_JoinPath(const astr_t &dir, const astr_t &name)
{
    if (dir.empty())
    {
        return name;
    }
    const char last = dir[dir.size() - 1];
    return last == '/' || last == '\\' ? dir + name : dir + "/" + name;
}

astr_t ScriptPicker_DefaultDir()
{
#if defined(XUO_EMSC)
    // the page's persistent filesystem: the only place a picked file can live
    return "/data/scripts";
#else
    const astr_t &dir = g_AssistantConfig.ScriptLastDir();
    return dir.empty() ? astr_t(".") : dir;
#endif
}

void ScriptPicker_RequestLoad()
{
#if defined(XUO_EMSC)
    // the page opens <input type=file>; the chosen files land in
    // /data/scripts (persisted with the rest of the page state) and the
    // paths come back through the exported result
    MAIN_THREAD_EM_ASM({
        if (typeof globalThis.__xuoScriptPick === 'function')
        {
            globalThis.__xuoScriptPick();
        }
        else
        {
            console.warn('[assist] __xuoScriptPick missing (stale shell.html)');
        }
    });
    s_PickStatus = 0;
    s_PickPaths[0] = 0;
    s_WebRequestAt = g_Ticks;
    s_LoadPending = true;
#else
    static const char *const PATTERNS[] = { "*.js", "*.txt" };
    const astr_t dir = ScriptPicker_DefaultDir();
    const char *picked = tinyfd_openFileDialog("Load script", dir.c_str(), 2, PATTERNS,
        "Script files", 1);
    if (picked == nullptr || picked[0] == 0)
    {
        // the user cancelled (headless runs never reach the picker)
        return;
    }
    s_DesktopPicked.clear();
    split_paths(picked, s_DesktopPicked);
    s_LoadPending = true;
#endif
}

bool ScriptPicker_Poll(vector<astr_t> &outPaths)
{
    if (!s_LoadPending)
    {
        return false;
    }
#if defined(XUO_EMSC)
    if (xuo_script_pick_status() == 0)
    {
        // a cancelled browser dialog never reports back: give up after a
        // while (the next Load re-arms the request with a fresh status)
        if (g_Ticks - s_WebRequestAt < WEB_PICK_TIMEOUT_MS)
        {
            return false;
        }
    }
    s_LoadPending = false;
    split_paths(xuo_script_pick_result(), outPaths);
#else
    s_LoadPending = false;
    outPaths = s_DesktopPicked;
    s_DesktopPicked.clear();
#endif
    if (outPaths.empty())
    {
        return false;
    }
    remember_dir(outPaths[0]);
    return true;
}

bool ScriptPicker_SaveAs(const astr_t &suggestedPath, astr_t &outPath, bool &failedOnce)
{
    (void)failedOnce;
#if defined(XUO_EMSC)
    // the page FS is the persistent store and the browser's download prompt
    // only saves a COPY to the user's machine: the script is written straight
    // into /data/scripts under the suggested name, which the editor's
    // untitled documents derive from their window name
    outPath = suggestedPath.empty()
        ? ScriptPicker_JoinPath(ScriptPicker_DefaultDir(), "script.js")
        : suggestedPath;
    if (ScriptList_DisplayName(outPath).find('.') == astr_t::npos)
    {
        outPath += ".js";
    }
    return true;
#else
    static const char *const PATTERNS[] = { "*.js" };
    const char *picked = tinyfd_saveFileDialog("Save script as",
        suggestedPath.empty() ? ScriptPicker_DefaultDir().c_str() : suggestedPath.c_str(), 1,
        PATTERNS, "Script files");
    if (picked == nullptr || picked[0] == 0)
    {
        return false;
    }
    astr_t path = picked;
    if (ScriptList_DisplayName(path).find('.') == astr_t::npos)
    {
        path += ".js"; // plain .js is the script extension
    }
    remember_dir(path);
    outPath = path;
    return true;
#endif
}

} // namespace xuoassist
