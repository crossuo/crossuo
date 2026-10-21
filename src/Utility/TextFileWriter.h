// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 CrossUO Contributors

#pragma once

#include <common/fs.h>
#include <common/str.h>
#include <cstdlib>
#include <vector>

//!Shared helpers for the text desktop file (split comma lists, parse serials).
inline std::vector<astr_t> TextSplit(const astr_t &line, char separator)
{
    std::vector<astr_t> out;
    astr_t current;
    for (char c : line)
    {
        if (c == separator)
        {
            if (!current.empty())
            {
                out.push_back(current);
                current.clear();
            }
        }
        else
        {
            current += c;
        }
    }
    if (!current.empty())
    {
        out.push_back(current);
    }
    return out;
}

//!TextFileParser leaves the closing quote on quoted values; strip it.
inline astr_t TextUnquote(astr_t text)
{
    while (!text.empty() && text.back() == '"')
    {
        text.pop_back();
    }
    return text;
}

inline uint32_t TextToUInt(const astr_t &text)
{
    return (uint32_t)strtoul(text.c_str(), nullptr, 10);
}

//!Flatten the tokens of a line (key excluded) into numeric-ish fields,
//!accepting both comma-packed and space-separated layouts.
inline std::vector<astr_t> TextFields(const std::vector<astr_t> &tokens)
{
    std::vector<astr_t> out;
    for (size_t i = 1; i < tokens.size(); i++)
    {
        for (auto &part : TextSplit(tokens[i], ','))
        {
            for (auto &sub : TextSplit(part, ' '))
            {
                if (!sub.empty())
                {
                    out.push_back(sub);
                }
            }
        }
    }
    return out;
}

//!Minimal key=value line writer shared by the desktop save file sections.
struct TextFileWriter
{
    FILE *m_File{ nullptr };

    TextFileWriter(const fs_path &path) { m_File = fs_open(path, FS_WRITE); }

    ~TextFileWriter() { Close(); }

    bool Opened() const { return (m_File != nullptr); };
    void Close()
    {
        if (m_File != nullptr)
        {
            fs_close(m_File);
            m_File = nullptr;
        }
    }

    void WriteRaw(const astr_t &line) const
    {
        if (m_File != nullptr)
        {
            fputs(astr_t(line + "\n").c_str(), m_File);
        }
    }

    void WriteString(const astr_t &key, const astr_t &value)
    {
        WriteRaw(key + "=" + value);
    }

    //!Quoted value: survives spaces and the comment characters on read back.
    void WriteStringQuoted(const astr_t &key, const astr_t &value)
    {
        WriteRaw(key + "=\"" + value + "\"");
    }

    void WriteInt(const astr_t &key, int value)
    {
        WriteRaw(key + "=" + str_from(value));
    }

    //!Unsigned values (serials) written as decimal; read back with strtoul.
    void WriteUInt(const astr_t &key, uint32_t value)
    {
        char buf[32] = { 0 };
        snprintf(buf, sizeof(buf), "%u", value);
        WriteRaw(key + "=" + buf);
    }

    void WriteBool(const astr_t &key, bool value)
    {
        WriteRaw(key + "=" + (value ? "yes" : "no"));
    }
};
