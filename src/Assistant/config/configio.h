// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Assistant config file I/O primitives: the crash-safe writer and the INI
// reader/writer used by every assistant config/save file
// (Config/<profile>/*.ini, GlobalConfig/*.ini).
//
// Schema rules:
//   - one .ini file per former config file, same stems (Options.ini,
//     Hotkeys.ini, Macros.ini, Map.ini, ...);
//   - repeated records use indexed sections ([Hotkey0]..[HotkeyN],
//     [Friend3], [Macro2Action1], ...) plus a [Meta] section holding
//     count = N, so readers iterate 0..N-1 deterministically;
//   - single-record files (Options, Map) use plain named sections;
//   - keys reuse the former attribute names verbatim (lowercase, no
// spaces; underscores where the name has them);
//   - values are written plain: numbers as decimal text, bools as
//     true/false, serials/graphics/colors through the CAssistant text
//     helpers, strings raw.
//
// Value limitations (by design):
//   - inih strips surrounding whitespace, so leading/trailing spaces and
//     tabs in values do not survive a round trip;
//   - a literal newline cannot appear raw; the writer escapes backslash,
//     '\n' and '\r' as \\, \n, \r and the reader reverses it;
//   - sections and keys are never escaped (the assistant never writes
//     ']', '=', or whitespace inside them);
//   - an empty value is explicit (``key=``) and reads back as "".

#pragma once

#include <map>
#include <string>

#include <common/fs.h> // fs_path

namespace xuoassist {

// Crash-safe file write: writes `data` to `<path>.tmp` in the same
// directory, flushes it to disk, preserves the original file mode when the
// target exists, then atomically renames the temp file over `path` - readers
// see either the old or the new content, never a partial file, even on a
// crash mid-write. Any failure leaves the original untouched (the stale temp
// is removed) and is logged. Load paths simply ignore a leftover
// `<path>.tmp`; the next successful save truncates it.
bool AtomicFileWrite(const fs_path &path, const void *data, size_t size);

// One INI section: a std::map keyed by the key name; the reader looks keys
// up by name, so the writer output order carries no meaning.
using CIniSection = std::map<astr_t, astr_t>;

// A parsed INI document (sections keyed by name) plus the writer.
// Load() returns false when the file is missing or malformed (including a
// line that does not parse) - callers treat that as "keep the defaults";
// inih never hands out partial documents with this configuration
// (INI_STOP_ON_FIRST_ERROR).
class CIniFile
{
public:
    std::map<astr_t, CIniSection> Sections;

    bool Load(const fs_path &path);
    bool Save(const fs_path &path) const;

    bool Has(const astr_t &section, const astr_t &key) const;
    astr_t Get(const astr_t &section, const astr_t &key, const astr_t &defaultValue = "") const;
    int GetInt(const astr_t &section, const astr_t &key, int defaultValue = 0) const;
    // Same accepted spellings as CAssistant::RawStringToBool
    // ("true"/"on"/non-zero number); writers emit "true"/"false".
    bool GetBool(const astr_t &section, const astr_t &key, bool defaultValue = false) const;

    void Set(const astr_t &section, const astr_t &key, const astr_t &value);
    void SetInt(const astr_t &section, const astr_t &key, int value);
    void SetBool(const astr_t &section, const astr_t &key, bool value);
};

// "[Hotkey" + 3 -> "[Hotkey3]"'s inner name; the shared indexer for the
// repeated-record section naming.
astr_t IniSectionName(const char *base, int index);

} // namespace xuoassist
