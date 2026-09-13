// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scriptgump.h. The Qt flow was textwindowengine.cpp (emit + PauseWaiting
// per op) into commandmanager.cpp OnCommandTextWindow* (709-761, 4426-4439)
// over the g_TextDialog window (script text list + position/size + save).

#include "scriptgump.h"
#include "scriptfile.h"

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
vector<string> s_Lines;

void add_line(const string &text)
{
    s_Lines.push_back(text);
    if (s_Lines.size() > TEXT_WINDOW_LINE_CAP)
    {
        s_Lines.erase(s_Lines.begin()); // drop the oldest
    }
}

} // namespace

void ScriptTextWindow_Command(
    int op, const string &text, int intA, int intB, const string &path)
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

void ScriptTextWindow_SaveToFile(const string &path)
{
    // the g_TextDialog->SaveToFile port: raw lines, one per line
    const string clean = ScriptFile_Sanitize(path, "textwindow save");
    if (clean.empty())
    {
        return;
    }
    FILE *file = fopen(clean.c_str(), "w");
    if (file == nullptr)
    {
        Warning(Assistant, "script text window: save failed (%s)\n", clean.c_str());
        return;
    }
    for (const string &line : s_Lines)
    {
        fputs(line.c_str(), file);
        fputc('\n', file);
    }
    fclose(file);
}

const vector<string> &ScriptTextWindow_Lines()
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
