// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Tiny self-contained XML DOM (plan §7 Phase 1 item 6 / §4.3): replaces the
// original app's XML reader/writer for the assistant's flat config
// records (Options.xml, Types.xml, FindList.xml, ... keep the byte-compatible
// element/attribute layout of the original writers).
//
// Design:
//   - XMLElement: name + ordered attributes + child elements + optional text.
//   - XMLDocument: Parse()/Load() a document into Root, Write()/Save() it back.
//
// The writer's default formatting is kept byte-compatible
//   with the original profile files:
//   <?xml version="1.0" encoding="UTF-8"?>\n and 4-space autoformat indents,
//   <tag a="1"/> for empty elements, entities &amp;&lt;&gt;&quot; (and \n,\r,\t
//   as character references) inside attribute values.
//
// Limitations (documented by design; every assistant schema is a flat record,
// so these are safe):
//   - no namespaces / prefixes, no DTD/schema validation;
//   - mixed content is not modeled - character data collapses into the
//     element's Text (our schemas never mix text with child elements);
//   - comments and processing instructions are skipped on parse;
//   - attribute order is preserved, not sorted;
//   - the parser is not a general-purpose hardened XML parser: it is built for
//     the assistant's own profile files.

#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include <common/fs.h> // fs_path

namespace xuoassist {

class XMLElement
{
public:
    astr_t Name;
    std::vector<std::pair<astr_t, astr_t>> Attributes;
    std::vector<XMLElement> Children;
    astr_t Text;

    XMLElement() = default;
    explicit XMLElement(const astr_t &name)
        : Name(name)
    {
    }

    // --- attributes --------------------------------------------------------
    bool HasAttribute(const astr_t &name) const;
    // Returns "" (or the given default) when the attribute is absent, exactly
        // like the original code's hasAttribute(...) guards.
    astr_t Attribute(const astr_t &name, const astr_t &defaultValue = "") const;
    int AttributeInt(const astr_t &name, int defaultValue = 0) const;
    void SetAttribute(const astr_t &name, const astr_t &value);

    // --- children ----------------------------------------------------------
    XMLElement *FindChild(const astr_t &name);
    const XMLElement *FindChild(const astr_t &name) const;
    XMLElement &AddChild(const astr_t &name);
};

class XMLDocument
{
public:
    XMLElement Root;

    bool Parse(const astr_t &text);
    bool Load(const fs_path &path);

    // autoFormat enables the indented formatting above (the
    // assistant writers always used it).
    astr_t Write(bool autoFormat = true) const;
    bool Save(const fs_path &path, bool autoFormat = true) const;
};

// Crash-safe file write (master plan §8.1 "Config corruption mid-save"):
// writes `data` to `<path>.tmp` in the same directory, flushes it to disk,
// preserves the original file mode when the target exists, then atomically
// renames the temp file over `path` - readers see either the old or the new
// content, never a partial file, even on a crash mid-write. Any failure
// leaves the original untouched (the stale temp is removed) and is logged.
// Load paths simply ignore a leftover `<path>.tmp`; the next successful save
// truncates it.
bool AtomicFileWrite(const fs_path &path, const void *data, size_t size);

// Runtime round-trip self-test (verification item 3): parse -> write -> parse
// on a synthetic document per assistant schema. Returns false on mismatch and
// logs the failure. Invoked once from AssistantConfig::LoadGlobals (all
// builds) so the util is exercised on every start without any UI.
bool XMLSelfTest();

} // namespace xuoassist
