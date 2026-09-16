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

astr_t s_SandboxRoot; // empty = process cwd (the client's binary dir)

astr_t normalize_separators(const astr_t &path)
{
    astr_t result = path;
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
astr_t normalize_path(const astr_t &path)
{
    const bool absolute = !path.empty() && path[0] == '/';
    astr_t base = absolute ? astr_t() : s_SandboxRoot;
    if (!base.empty() && base[0] != '/')
    {
        base = "./" + base; // relative root: keep it recognizable below
    }

    astr_t combined;
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
    std::vector<astr_t> parts;
    size_t start = 0;
    while (start <= combined.size())
    {
        size_t slash = combined.find('/', start);
        astr_t part = combined.substr(
            start, slash == astr_t::npos ? astr_t::npos : slash - start);
        if (slash == astr_t::npos)
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
    std::vector<astr_t> resolved;
    for (const astr_t &part : parts)
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
    // so the result's absoluteness follows the JOINED astr_t, not the input
    astr_t result = combinedResultAbsolute ? "/" : "";
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

bool inside_sandbox(const astr_t &normalized)
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

    const astr_t root = normalize_separators(s_SandboxRoot);
    const astr_t rootNormalized = normalize_path(root);
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

void ScriptFile_SetSandboxRoot(const astr_t &root)
{
    s_SandboxRoot = normalize_separators(root);
}

astr_t ScriptFile_Sanitize(const astr_t &path, const char *api)
{
    const astr_t normalized = normalize_path(normalize_separators(path));
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
    const astr_t &path, bool append, bool checkExists, bool &outOpened, astr_t &outClean)
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

bool ScriptFile_Open(XA_SCRIPT_FILE *file, const astr_t &path, bool checkExists)
{
    if (file == nullptr)
    {
        return false;
    }
    ScriptFile_Close(file);
    bool opened = false;
    astr_t clean;
    file->Handle = open_sandboxed(path, false, checkExists, opened, clean);
    if (file->Handle != nullptr)
    {
        file->Path = clean;
        opened = true;
    }
    return opened;
}

bool ScriptFile_Append(XA_SCRIPT_FILE *file, const astr_t &path, bool checkExists)
{
    if (file == nullptr)
    {
        return false;
    }
    ScriptFile_Close(file);
    bool opened = false;
    astr_t clean;
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

astr_t ScriptFile_ReadLine(XA_SCRIPT_FILE *file)
{
    if (file == nullptr || file->Handle == nullptr)
    {
        return "";
    }
    astr_t line;
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

astr_t ScriptFile_Read(XA_SCRIPT_FILE *file)
{
    if (file == nullptr || file->Handle == nullptr)
    {
        return "";
    }
    // whitespace-delimited token, like the old stream extractor
    astr_t token;
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

void ScriptFile_WriteLine(XA_SCRIPT_FILE *file, const astr_t &text)
{
    if (file == nullptr || file->Handle == nullptr)
    {
        return;
    }
    fputs(text.c_str(), (FILE *)file->Handle);
    fputs("\r\n", (FILE *)file->Handle); // the original wrote \r\n
}

void ScriptFile_Write(XA_SCRIPT_FILE *file, const astr_t &text)
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

bool ScriptFile_RemovePath(const astr_t &path)
{
    const astr_t clean = ScriptFile_Sanitize(path, "file remove");
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
