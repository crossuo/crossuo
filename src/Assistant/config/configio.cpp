// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Assistant config I/O primitives (see configio.h for the schema rules).

#include "configio.h"
#include <cstdio>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/stat.h> // stat/chmod (atomic-save file mode preservation)
#if defined(_WIN32)
#include <io.h> // _commit
#else
#include <unistd.h>   // fsync
#endif
#else
#include <windows.h> // MoveFileExA / SetFileAttributesA
#include <io.h> // _commit
#endif

#include <common/logging/logging.h>
#include <common/str.h> // str_from

// inih tuning (read before the vendored single-header implementation):
//   - INI_MAX_LINE: the 512 default is tight for text-heavy values
//     (speech filters, replace rules); 4k is comfortably above any
//     assistant value.
//   - INI_ALLOW_MULTILINE off: an indented continuation line must never be
//     glued onto the previous value behind the reader's back.
//   - INI_ALLOW_INLINE_COMMENTS off: values may legitimately contain ';'.
//   - INI_STOP_ON_FIRST_ERROR: any malformed line fails the whole Load,
//     which fails safe to the in-memory defaults.
// external/external.cpp also compiles inih with the default tuning, so this
// TU's copy is compiled under private symbol names.
#define INI_MAX_LINE 4096
#define INI_ALLOW_MULTILINE 0
#define INI_ALLOW_INLINE_COMMENTS 0
#define INI_STOP_ON_FIRST_ERROR 1
#define ini_parse ini_parse_xuoassist
#define ini_parse_file ini_parse_file_xuoassist
#define ini_parse_stream ini_parse_stream_xuoassist
#define ini_parse_string ini_parse_string_xuoassist
#define INIH_IMPLEMENTATION
#include <external/inih.h>

namespace xuoassist {

// --- atomic save ------------------------------------------------------------

bool AtomicFileWrite(const fs_path &path, const void *data, size_t size)
{
    // <dir>/<name>.tmp - same directory so the rename stays on one filesystem
    const astr_t tmpName = astr_t(fs_path_ascii(path)) + ".tmp";
    const fs_path tmpPath = fs_path_from(tmpName);

    FILE *file = fs_open(tmpPath, FS_WRITE);
    if (file == nullptr)
    {
        Warning(Assistant, "atomic save: cannot open %s\n", tmpName.c_str());
        return false;
    }

    if (fwrite(data, 1, size, file) != size || fflush(file) != 0)
    {
        Warning(Assistant, "atomic save: write failed for %s\n", fs_path_ascii(path));
        fclose(file);
        fs_del(tmpPath);
        return false;
    }

    // flush to disk before the rename: after a crash the target is either the
    // previous content or the complete new content
    const int fd = fileno(file);
    if (fd >= 0)
    {
#if defined(_WIN32)
        _commit(fd);
#else
        fsync(fd);
#endif
    }
    fclose(file);

#if defined(_WIN32)
    // keep a read-only attribute from blocking the replace
    SetFileAttributesA(fs_path_ascii(path), FILE_ATTRIBUTE_NORMAL);
    if (!MoveFileExA(tmpName.c_str(), fs_path_ascii(path), MOVEFILE_REPLACE_EXISTING))
    {
        Warning(Assistant, "atomic save: replace failed for %s\n", fs_path_ascii(path));
        fs_del(tmpPath);
        return false;
    }
#else
    // preserve the original file mode across the replace
    struct stat st;
    if (stat(fs_path_ascii(path), &st) == 0)
    {
        chmod(tmpName.c_str(), st.st_mode & 07777);
    }

    if (rename(tmpName.c_str(), fs_path_ascii(path)) != 0)
    {
        Warning(Assistant, "atomic save: rename failed for %s\n", fs_path_ascii(path));
        fs_del(tmpPath);
        return false;
    }
#endif

    return true;
}

// --- INI writer -------------------------------------------------------------

static astr_t ini_escape_value(const astr_t &value)
{
    if (value.find('\\') == astr_t::npos && value.find('\n') == astr_t::npos &&
        value.find('\r') == astr_t::npos)
    {
        return value;
    }
    astr_t result;
    result.reserve(value.size() + 8);
    for (char c : value)
    {
        switch (c)
        {
            case '\\':
                result += "\\\\";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            default:
                result += c;
                break;
        }
    }
    return result;
}

static astr_t ini_unescape_value(const astr_t &value)
{
    if (value.find('\\') == astr_t::npos)
    {
        return value;
    }
    astr_t result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size(); i++)
    {
        if (value[i] == '\\' && i + 1 < value.size())
        {
            switch (value[++i])
            {
                case 'n':
                    result += '\n';
                    break;
                case 'r':
                    result += '\r';
                    break;
                default:
                    result += value[i];
                    break;
            }
        }
        else
        {
            result += value[i];
        }
    }
    return result;
}

astr_t IniSectionName(const char *base, int index)
{
    return astr_t(base) + str_from(index);
}

bool CIniFile::Save(const fs_path &path) const
{
    astr_t out;
    for (const auto &section : Sections)
    {
        out += "[";
        out += section.first;
        out += "]\n";
        for (const auto &key : section.second)
        {
            out += key.first;
            out += "=";
            out += ini_escape_value(key.second);
            out += "\n";
        }
        out += "\n";
    }

    return AtomicFileWrite(path, out.data(), out.size());
}

namespace {

struct IniAccumulator
{
    std::map<astr_t, CIniSection> *sections;
};

// inih line handler: accumulate every name=value pair into its section map.
// Always returns 1 (accept): section/key/value errors are impossible from a
// writer that never emits them, and a malformed file is rejected wholesale
// via ini_parse's return value.
int ini_accumulate(void *user, const char *section, const char *name, const char *value, int lineno)
{
    IniAccumulator *state = (IniAccumulator *)user;
    (void)lineno;
    if (name == nullptr)
    {
        // the "[section]" start notification (INI_CALL_HANDLER_ON_NEW_SECTION)
        return 1;
    }
    (*state->sections)[section][name] = (value != nullptr ? value : "");
    return 1;
}

} // namespace

bool CIniFile::Load(const fs_path &path)
{
    Sections.clear();

    // inih returns the first offending line on a parse error and -1 when the
    // file cannot be opened; both read as "no config" and the caller keeps
    // its current (default) state.
    IniAccumulator state;
    state.sections = &Sections;
    return ini_parse(fs_path_ascii(path), ini_accumulate, &state) == 0;
}

bool CIniFile::Has(const astr_t &section, const astr_t &key) const
{
    const auto it = Sections.find(section);
    if (it == Sections.end())
    {
        return false;
    }
    return it->second.find(key) != it->second.end();
}

astr_t CIniFile::Get(const astr_t &section, const astr_t &key, const astr_t &defaultValue) const
{
    const auto it = Sections.find(section);
    if (it == Sections.end())
    {
        return defaultValue;
    }
    const auto keyIt = it->second.find(key);
    return (keyIt == it->second.end()) ? defaultValue : ini_unescape_value(keyIt->second);
}

int CIniFile::GetInt(const astr_t &section, const astr_t &key, int defaultValue) const
{
    const auto it = Sections.find(section);
    if (it == Sections.end())
    {
        return defaultValue;
    }
    const auto keyIt = it->second.find(key);
    if (keyIt == it->second.end())
    {
        return defaultValue;
    }
    return (int)strtol(keyIt->second.c_str(), nullptr, 0);
}

bool CIniFile::GetBool(const astr_t &section, const astr_t &key, bool defaultValue) const
{
    const auto it = Sections.find(section);
    if (it == Sections.end())
    {
        return defaultValue;
    }
    const auto keyIt = it->second.find(key);
    if (keyIt == it->second.end())
    {
        return defaultValue;
    }
    const astr_t value = keyIt->second;
    if (value == "true" || value == "on")
    {
        return true;
    }
    return atoi(value.c_str()) != 0;
}

void CIniFile::Set(const astr_t &section, const astr_t &key, const astr_t &value)
{
    Sections[section][key] = value;
}

void CIniFile::SetInt(const astr_t &section, const astr_t &key, int value)
{
    Set(section, key, str_from(value));
}

void CIniFile::SetBool(const astr_t &section, const astr_t &key, bool value)
{
    Set(section, key, value ? "true" : "false");
}

} // namespace xuoassist
