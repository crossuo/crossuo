// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scriptfile.h (the CScriptFileObject port, script/fileengine.cpp).

#include "scriptfile.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

namespace xuoassist {

namespace {

string s_SandboxRoot; // empty = process cwd (the client's binary dir)

string normalize_separators(const string &path)
{
    string result = path;
    for (size_t i = 0; i < result.size(); i++)
    {
        if (result[i] == '\\')
        {
            result[i] = '/';
        }
    }
    return result;
}

// Lexical normalization: resolves "." and ".." against `base` ("" = cwd for
// relative results). Returns the normalized absolute-ish path.
string normalize_path(const string &path)
{
    const bool absolute = !path.empty() && path[0] == '/';
    string base = absolute ? string() : s_SandboxRoot;
    if (!base.empty() && base[0] != '/')
    {
        base = "./" + base; // relative root: keep it recognizable below
    }

    string combined;
    if (!absolute && !base.empty())
    {
        combined = base;
        if (combined[combined.size() - 1] != '/')
        {
            combined += '/';
        }
        combined += path;
    }
    else
    {
        combined = path;
    }

    // split + resolve
    std::vector<string> parts;
    size_t start = 0;
    while (start <= combined.size())
    {
        size_t slash = combined.find('/', start);
        string part = combined.substr(
            start, slash == string::npos ? string::npos : slash - start);
        if (slash == string::npos)
        {
            if (!part.empty())
            {
                parts.push_back(part);
            }
            break;
        }
        if (!part.empty())
        {
            parts.push_back(part);
        }
        start = slash + 1;
    }

    const bool combinedResultAbsolute = !combined.empty() && combined[0] == '/';
    std::vector<string> resolved;
    for (const string &part : parts)
    {
        if (part == ".")
        {
            continue;
        }
        if (part == "..")
        {
            if (!resolved.empty() && resolved.back() != "..")
            {
                resolved.pop_back();
            }
            else if (!combinedResultAbsolute)
            {
                resolved.push_back(".."); // climbing above a relative root
            }
            continue;
        }
        resolved.push_back(part);
    }

    // a root-joined relative input resolves against an absolute sandbox root,
    // so the result's absoluteness follows the JOINED string, not the input
    string result = combinedResultAbsolute ? "/" : "";
    for (size_t i = 0; i < resolved.size(); i++)
    {
        if (i != 0)
        {
            result += "/";
        }
        result += resolved[i];
    }
    return result;
}

bool inside_sandbox(const string &normalized)
{
    if (s_SandboxRoot.empty())
    {
        // cwd sandbox: relative paths that stayed inside the cwd tree pass;
        // anything still climbing (leading "..") or absolute is outside
        if (!normalized.empty() && normalized[0] == '/')
        {
            return false;
        }
        return normalized.compare(0, 2, "..") != 0 ||
            (normalized.size() > 2 && normalized[2] != '/');
    }

    const string root = normalize_separators(s_SandboxRoot);
    const string rootNormalized = normalize_path(root);
    if (normalized == rootNormalized)
    {
        return true;
    }
    if (rootNormalized.empty())
    {
        return normalized[0] != '/';
    }
    return normalized.compare(0, rootNormalized.size(), rootNormalized) == 0 &&
        (normalized.size() <= rootNormalized.size() ||
            normalized[rootNormalized.size()] == '/');
}

} // namespace

void ScriptFile_SetSandboxRoot(const string &root)
{
    s_SandboxRoot = normalize_separators(root);
}

string ScriptFile_Sanitize(const string &path, const char *api)
{
    const string normalized = normalize_path(normalize_separators(path));
    if (!inside_sandbox(normalized))
    {
        Warning(Assistant, "script %s: sandbox escape blocked (%s)\n", api, path.c_str());
        return "";
    }
    return normalized;
}

namespace {

// Opens a sandboxed file; fills `outClean` with the normalized path even on
// failure so callers can report it.
FILE *open_sandboxed(
    const string &path, bool append, bool checkExists, bool &outOpened, string &outClean)
{
    outOpened = false;
    outClean = ScriptFile_Sanitize(path, append ? "file append" : "file open");
    if (outClean.empty())
    {
        return nullptr;
    }
    // The original modes: Append|Text -> create-or-append; ReadWrite|Text -> open an
    // existing file WITHOUT truncating, create when missing. stdio has no
    // single create-or-r+ mode, so probe first.
    FILE *probe = fopen(outClean.c_str(), "r");
    const bool exists = probe != nullptr;
    if (probe != nullptr)
    {
        fclose(probe);
    }
    if (checkExists && !exists)
    {
        return nullptr;
    }
    const char *mode = append ? "a" : (exists ? "r+" : "w+");
    FILE *handle = fopen(outClean.c_str(), mode);
    if (handle == nullptr)
    {
        Warning(Assistant, "script file: open failed (%s)\n", outClean.c_str());
    }
    return handle;
}

} // namespace

bool ScriptFile_Open(XA_SCRIPT_FILE *file, const string &path, bool checkExists)
{
    if (file == nullptr)
    {
        return false;
    }
    ScriptFile_Close(file);
    bool opened = false;
    string clean;
    file->Handle = open_sandboxed(path, false, checkExists, opened, clean);
    if (file->Handle != nullptr)
    {
        file->Path = clean;
        opened = true;
    }
    return opened;
}

bool ScriptFile_Append(XA_SCRIPT_FILE *file, const string &path, bool checkExists)
{
    if (file == nullptr)
    {
        return false;
    }
    ScriptFile_Close(file);
    bool opened = false;
    string clean;
    file->Handle = open_sandboxed(path, true, checkExists, opened, clean);
    if (file->Handle != nullptr)
    {
        file->Path = clean;
        opened = true;
    }
    return opened;
}

bool ScriptFile_Opened(const XA_SCRIPT_FILE *file)
{
    return file != nullptr && file->Handle != nullptr;
}

void ScriptFile_Close(XA_SCRIPT_FILE *file)
{
    if (file != nullptr && file->Handle != nullptr)
    {
        fclose((FILE *)file->Handle);
        file->Handle = nullptr;
    }
}

string ScriptFile_ReadLine(XA_SCRIPT_FILE *file)
{
    if (file == nullptr || file->Handle == nullptr)
    {
        return "";
    }
    string line;
    int c = 0;
    while ((c = fgetc((FILE *)file->Handle)) != EOF)
    {
        if (c == '\n')
        {
            break; // the trailing \n is dropped, like the old readLine
        }
        line += (char)c;
    }
    // \r\n text files: drop a trailing \r
    if (!line.empty() && line[line.size() - 1] == '\r')
    {
        line.erase(line.size() - 1);
    }
    return line;
}

string ScriptFile_Read(XA_SCRIPT_FILE *file)
{
    if (file == nullptr || file->Handle == nullptr)
    {
        return "";
    }
    // whitespace-delimited token, like the old stream extractor
    string token;
    int c = 0;
    do
    {
        c = fgetc((FILE *)file->Handle);
    } while (c != EOF && isspace(c));
    while (c != EOF && !isspace(c))
    {
        token += (char)c;
        c = fgetc((FILE *)file->Handle);
    }
    return token;
}

void ScriptFile_WriteLine(XA_SCRIPT_FILE *file, const string &text)
{
    if (file == nullptr || file->Handle == nullptr)
    {
        return;
    }
    fputs(text.c_str(), (FILE *)file->Handle);
    fputs("\r\n", (FILE *)file->Handle); // the original wrote \r\n
}

void ScriptFile_Write(XA_SCRIPT_FILE *file, const string &text)
{
    if (file == nullptr || file->Handle == nullptr)
    {
        return;
    }
    fputs(text.c_str(), (FILE *)file->Handle);
}

void ScriptFile_Remove(XA_SCRIPT_FILE *file)
{
    if (file == nullptr)
    {
        return;
    }
    ScriptFile_Close(file);
    ScriptFile_RemovePath(file->Path);
}

bool ScriptFile_RemovePath(const string &path)
{
    const string clean = ScriptFile_Sanitize(path, "file remove");
    if (clean.empty())
    {
        return false;
    }
    FILE *probe = fopen(clean.c_str(), "r");
    if (probe == nullptr)
    {
        return false; // missing-file gate
    }
    fclose(probe);
    return remove(clean.c_str()) == 0;
}

} // namespace xuoassist
