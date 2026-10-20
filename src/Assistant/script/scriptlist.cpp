// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scriptlist.h: the loaded-scripts list, its file helpers and the path
// identity rules (the run manager keys handles by these keys).

#include "scriptlist.h"

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include "../config/configio.h" // AtomicFileWrite

#include <stdio.h>

namespace xuoassist {

// --- file helpers -------------------------------------------------------------

bool ScriptList_ReadFile(const astr_t &path, astr_t &out)
{
    out.clear();
    FILE *file = fopen(path.c_str(), "rb");
    if (file == nullptr)
    {
        return false;
    }
    char buffer[4096];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        out.append(buffer, read);
    }
    const bool ok = ferror(file) == 0;
    fclose(file);
    return ok;
}

bool ScriptList_WriteFile(const astr_t &path, const astr_t &text)
{
    return AtomicFileWrite(fs_path_from(path), text.data(), text.size());
}

uint32_t ScriptList_ContentHash(const astr_t &text)
{
    // FNV-1a 32: cheap, stable across runs (no random seed), and only used to
    // spot "this save dialog picked the file we already have".
    uint32_t hash = 2166136261u;
    for (unsigned char c : text)
    {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

// --- paths --------------------------------------------------------------------

astr_t ScriptList_NormalizePath(const astr_t &path)
{
    const bool absolute = !path.empty() && (path[0] == '/' || path[0] == '\\');
#if defined(_WIN32)
    const bool drive = path.size() >= 2 && path[1] == ':';
#else
    const bool drive = false;
#endif

    vector<astr_t> parts;
    size_t start = 0;
    while (start <= path.size())
    {
        size_t end = path.find_first_of("/\\", start);
        if (end == astr_t::npos)
        {
            end = path.size();
        }
        const astr_t part = path.substr(start, end - start);
        if (part.empty() || part == ".")
        {
            // skip
        }
        else if (part == "..")
        {
            if (!parts.empty() && parts.back() != "..")
            {
                parts.pop_back();
            }
            else if (!absolute)
            {
                parts.push_back(part); // relative: keep the leading ..
            }
        }
        else
        {
            parts.push_back(part);
        }
        start = end + 1;
    }

    astr_t result;
    if (drive)
    {
        result = path.substr(0, 2); // "c:"
        result += '/';
    }
    else if (absolute)
    {
        result = "/";
    }
    for (size_t i = 0; i < parts.size(); i++)
    {
        if (i > 0)
        {
            result += '/';
        }
        result += parts[i];
    }
#if defined(_WIN32)
    // Windows paths compare case-insensitively: a normalized key is lowercase
    result = str_lower(result);
#endif
    if (result.empty())
    {
        result = ".";
    }
    return result;
}

astr_t ScriptList_DisplayName(const astr_t &path)
{
    if (path.empty())
    {
        return "untitled";
    }
    const size_t slash = path.find_last_of("/\\");
    return slash == astr_t::npos ? path : path.substr(slash + 1);
}

astr_t ScriptList_DirName(const astr_t &path)
{
    const size_t slash = path.find_last_of("/\\");
    if (slash == astr_t::npos)
    {
        return ".";
    }
    if (slash == 0)
    {
        return path.substr(0, 1); // root
    }
    return path.substr(0, slash);
}

// --- CLoadedScriptList --------------------------------------------------------

CLoadedScriptList g_LoadedScripts;

bool CLoadedScriptList::Contains(const astr_t &key) const
{
    return Find(key) != nullptr;
}

const CLoadedScript *CLoadedScriptList::Find(const astr_t &key) const
{
    for (const CLoadedScript &script : m_Scripts)
    {
        if (script.Key == key)
        {
            return &script;
        }
    }
    return nullptr;
}

astr_t CLoadedScriptList::Add(const astr_t &path, uint32_t hash, bool exists)
{
    const astr_t key = ScriptList_NormalizePath(path);
    for (CLoadedScript &script : m_Scripts)
    {
        if (script.Key == key)
        {
            // re-import: refresh the fields, keep the position
            script.Path = path;
            script.ContentHash = hash;
            script.bExists = exists;
            return key;
        }
    }
    CLoadedScript script;
    script.Path = path;
    script.Key = key;
    script.ContentHash = hash;
    script.bExists = exists;
    m_Scripts.push_back(script);
    return key;
}

void CLoadedScriptList::Remove(const astr_t &key)
{
    for (size_t i = 0; i < m_Scripts.size(); i++)
    {
        if (m_Scripts[i].Key == key)
        {
            m_Scripts.erase(m_Scripts.begin() + i);
            return;
        }
    }
}

void CLoadedScriptList::Clear()
{
    m_Scripts.clear();
    m_PendingRestore.clear();
}

void CLoadedScriptList::SetPendingRestore(const vector<astr_t> &paths)
{
    m_PendingRestore = paths;
}

void CLoadedScriptList::DrainPendingRestore()
{
    if (m_PendingRestore.empty())
    {
        return;
    }
    // take a copy: Add may reallocate nothing, but the drain clears the
    // source list and callers may hold references
    const vector<astr_t> pending = m_PendingRestore;
    m_PendingRestore.clear();
    for (const astr_t &path : pending)
    {
        if (path.empty())
        {
            continue;
        }
        astr_t text;
        const bool read = ScriptList_ReadFile(path, text);
        Add(path, read ? ScriptList_ContentHash(text) : 0u, read);
    }
}

void CLoadedScriptList::NotifySaved(const astr_t &key, const astr_t &text)
{
    for (CLoadedScript &script : m_Scripts)
    {
        if (script.Key == key)
        {
            script.ContentHash = ScriptList_ContentHash(text);
            script.bExists = true;
            return;
        }
    }
}

const CLoadedScript *CLoadedScriptList::FindByHash(uint32_t hash) const
{
    if (hash == 0)
    {
        return nullptr;
    }
    for (const CLoadedScript &script : m_Scripts)
    {
        if (script.ContentHash == hash)
        {
            return &script;
        }
    }
    return nullptr;
}

} // namespace xuoassist
