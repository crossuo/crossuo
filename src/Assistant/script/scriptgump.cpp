// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scriptgump.h. Ops marshal through the command queue and the window
// renders with the script text list + position/size + save.

#include "scriptgump.h"
#include "scriptfile.h"
#include "../config/configio.h" // AtomicFileWrite (the crash-safe save path)

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <cstdio>

namespace xuoassist {

namespace {

constexpr size_t TEXT_WINDOW_LINE_CAP = 512; // runaway-print guard

bool s_Open = false;
int s_X = 40;
int s_Y = 40;
int s_Width = 400;
int s_Height = 300;
vector<astr_t> s_Lines;

void add_line(const astr_t &text)
{
    s_Lines.push_back(text);
    if (s_Lines.size() > TEXT_WINDOW_LINE_CAP)
    {
        s_Lines.erase(s_Lines.begin()); // drop the oldest
    }
}

} // namespace

void ScriptTextWindow_Command(
    int op, const astr_t &text, int intA, int intB, const astr_t &path)
{
    switch (op)
    {
        case TW_OPEN:
            s_Open = true;
            break;
        case TW_CLOSE:
            s_Open = false;
            break;
        case TW_CLEAR:
            s_Lines.clear();
            break;
        case TW_PRINT:
            add_line(text);
            break;
        case TW_SET_POS:
            s_X = intA;
            s_Y = intB;
            break;
        case TW_SET_SIZE:
            s_Width = intA > 0 ? intA : s_Width;
            s_Height = intB > 0 ? intB : s_Height;
            break;
        case TW_SAVE:
            ScriptTextWindow_SaveToFile(path);
            break;
        default:
            break;
    }
    (void)text;
    (void)intA;
    (void)intB;
}

void ScriptTextWindow_SaveToFile(const astr_t &path)
{
    // the g_TextDialog->SaveToFile port: raw lines, one per line
    const astr_t clean = ScriptFile_Sanitize(path, "textwindow save");
    if (clean.empty())
    {
        return;
    }

    // one buffer, one atomic temp+rename write (crash-safe like the config
    // saves)
    astr_t out;
    out.reserve(s_Lines.size() * 32);
    for (const astr_t &line : s_Lines)
    {
        out += line;
        out += '\n';
    }

    if (!AtomicFileWrite(fs_path_from(clean), out.data(), out.size()))
    {
        Warning(Assistant, "script text window: save failed (%s)\n", clean.c_str());
    }
}

const vector<astr_t> &ScriptTextWindow_Lines()
{
    return s_Lines;
}

bool ScriptTextWindow_Opened()
{
    return s_Open;
}

void ScriptTextWindow_Rect(int *x, int *y, int *width, int *height)
{
    if (x != nullptr)
    {
        *x = s_X;
    }
    if (y != nullptr)
    {
        *y = s_Y;
    }
    if (width != nullptr)
    {
        *width = s_Width;
    }
    if (height != nullptr)
    {
        *height = s_Height;
    }
}

} // namespace xuoassist
