// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "xml.h"
#include <algorithm>
#include <cstdio>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/stat.h> // stat/chmod (atomic-save file mode preservation)
#include <unistd.h>   // fsync
#else
#include <windows.h> // MoveFileExA / SetFileAttributesA
#endif

#include <common/logging/logging.h>
#include <common/str.h>
#include <common/utils.h> // countof

namespace xuoassist {

using std::string;

static const int XML_AUTOFORMAT_INDENT = 4; // the original XML writer's default

// ParseElement recursion cap: assistant schemas are flat records (max 3
// levels); 64 is generous headroom and stops runaway/malformed nesting.
static const int XML_MAX_PARSE_DEPTH = 64;

// --- helpers ---------------------------------------------------------------

static string xml_escape_attribute(const string &value)
{
    string result;
    result.reserve(value.size());
    char buf[16];
    for (char c : value)
    {
        switch (c)
        {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\n':
                result += "&#10;";
                break;
            case '\r':
                result += "&#13;";
                break;
            case '\t':
                result += "&#9;";
                break;
            default:
                if ((unsigned char)c < 0x20)
                {
                    snprintf(buf, sizeof(buf), "&#%u;", (unsigned char)c);
                    result += buf;
                }
                else
                {
                    result += c;
                }
                break;
        }
    }
    return result;
}

static string xml_escape_text(const string &value)
{
    string result;
    result.reserve(value.size());
    char buf[16];
    for (char c : value)
    {
        switch (c)
        {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            default:
                if ((unsigned char)c < 0x20 && c != '\n' && c != '\r' && c != '\t')
                {
                    snprintf(buf, sizeof(buf), "&#%u;", (unsigned char)c);
                    result += buf;
                }
                else
                {
                    result += c;
                }
                break;
        }
    }
    return result;
}

static string xml_unescape(const string &value)
{
    if (value.find('&') == string::npos)
    {
        return value;
    }
    string result;
    result.reserve(value.size());
    for (size_t i = 0; i < value.size();)
    {
        if (value[i] != '&')
        {
            result += value[i++];
            continue;
        }
        size_t semi = value.find(';', i);
        if (semi == string::npos || (semi - i) > 10)
        {
            result += value[i++];
            continue;
        }
        const string entity = value.substr(i + 1, semi - i - 1);
        if (entity == "amp")
        {
            result += '&';
        }
        else if (entity == "lt")
        {
            result += '<';
        }
        else if (entity == "gt")
        {
            result += '>';
        }
        else if (entity == "quot")
        {
            result += '"';
        }
        else if (entity == "apos")
        {
            result += '\'';
        }
        else if (entity.size() > 1 && entity[0] == '#')
        {
            const char *numeric = entity.c_str() + 1;
            int base = 10;
            if (numeric[0] == 'x' || numeric[0] == 'X')
            {
                numeric++;
                base = 16;
            }
            const int code = (int)strtol(numeric, nullptr, base);
            if (code > 0 && code < 256)
            {
                result += (char)code;
            }
        }
        else
        {
            result += value.substr(i, semi - i + 1);
        }
        i = semi + 1;
    }
    return result;
}

// --- XMLElement ------------------------------------------------------------

bool XMLElement::HasAttribute(const string &name) const
{
    for (const auto &attr : Attributes)
    {
        if (attr.first == name)
        {
            return true;
        }
    }
    return false;
}

string XMLElement::Attribute(const string &name, const string &defaultValue) const
{
    for (const auto &attr : Attributes)
    {
        if (attr.first == name)
        {
            return attr.second;
        }
    }
    return defaultValue;
}

int XMLElement::AttributeInt(const string &name, int defaultValue) const
{
    for (const auto &attr : Attributes)
    {
        if (attr.first == name)
        {
            return atoi(attr.second.c_str());
        }
    }
    return defaultValue;
}

void XMLElement::SetAttribute(const string &name, const string &value)
{
    for (auto &attr : Attributes)
    {
        if (attr.first == name)
        {
            attr.second = value;
            return;
        }
    }
    Attributes.push_back(std::make_pair(name, value));
}

XMLElement *XMLElement::FindChild(const string &name)
{
    for (auto &child : Children)
    {
        if (child.Name == name)
        {
            return &child;
        }
    }
    return nullptr;
}

const XMLElement *XMLElement::FindChild(const string &name) const
{
    for (const auto &child : Children)
    {
        if (child.Name == name)
        {
            return &child;
        }
    }
    return nullptr;
}

XMLElement &XMLElement::AddChild(const string &name)
{
    Children.push_back(XMLElement(name));
    return Children.back();
}

// --- parsing ---------------------------------------------------------------

namespace {

// Whitespace-only character data is autoformat residue (the newlines/indent
// the XML writer emits between elements), not content: skipping it keeps
// parse->write round-trips of files written by the original app byte identical instead of
// re-emitting it as element text (which produced blank lines). Text with any
// non-whitespace byte is kept as-is (never trimmed).
bool xml_text_has_content(const string &text)
{
    for (char c : text)
    {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
        {
            return true;
        }
    }
    return false;
}

struct XMLParser
{
    const string &Text;
    size_t Pos{ 0 };
    int Depth{ 0 };
    bool Error{ false };

    explicit XMLParser(const string &text)
        : Text(text)
    {
    }

    void SkipWhitespace()
    {
        while (Pos < Text.size())
        {
            const char c = Text[Pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                Pos++;
            }
            else
            {
                break;
            }
        }
    }

    bool SkipMisc()
    {
        bool any = false;
        for (;;)
        {
            SkipWhitespace();
            if (Pos + 3 < Text.size() && Text.compare(Pos, 4, "<!--") == 0)
            {
                const size_t end = Text.find("-->", Pos + 4);
                Pos = (end == string::npos) ? Text.size() : end + 3;
                any = true;
            }
            else if (Pos + 1 < Text.size() && Text[Pos] == '<' && Text[Pos + 1] == '?')
            {
                const size_t end = Text.find("?>", Pos + 2);
                Pos = (end == string::npos) ? Text.size() : end + 2;
                any = true;
            }
            else if (Pos + 1 < Text.size() && Text[Pos] == '<' && Text[Pos + 1] == '!')
            {
                const size_t end = Text.find('>', Pos + 2);
                Pos = (end == string::npos) ? Text.size() : end + 1;
                any = true;
            }
            else
            {
                break;
            }
        }
        return any;
    }

    bool ParseElement(XMLElement &element)
    {
        if (Depth >= XML_MAX_PARSE_DEPTH)
        {
            Error = true;
            return false;
        }
        Depth++;
        SkipMisc();
        if (Pos >= Text.size() || Text[Pos] != '<')
        {
            Error = true;
            return false;
        }
        Pos++; // '<'
        element.Name.clear();
        while (Pos < Text.size())
        {
            const char c = Text[Pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' || c == '/' ||
                c == '=')
            {
                break;
            }
            element.Name += c;
            Pos++;
        }
        if (element.Name.empty())
        {
            Error = true;
            return false;
        }

        // attributes
        for (;;)
        {
            SkipWhitespace();
            if (Pos >= Text.size())
            {
                Error = true;
                return false;
            }
            if (Text[Pos] == '>' || Text[Pos] == '/')
            {
                break;
            }
            string attrName;
            while (Pos < Text.size())
            {
                const char c = Text[Pos];
                if (c == '=' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '>' ||
                    c == '/')
                {
                    break;
                }
                attrName += c;
                Pos++;
            }
            string attrValue;
            if (Pos < Text.size() && Text[Pos] == '=')
            {
                Pos++;
                SkipWhitespace();
                if (Pos >= Text.size() || (Text[Pos] != '"' && Text[Pos] != '\''))
                {
                    Error = true;
                    return false;
                }
                const char quote = Text[Pos++];
                const size_t end = Text.find(quote, Pos);
                if (end == string::npos)
                {
                    Error = true;
                    return false;
                }
                attrValue = xml_unescape(Text.substr(Pos, end - Pos));
                Pos = end + 1;
            }
            if (!attrName.empty())
            {
                element.Attributes.push_back(std::make_pair(attrName, attrValue));
            }
        }

        if (Text[Pos] == '/')
        {
            if (Pos + 1 >= Text.size() || Text[Pos + 1] != '>')
            {
                Error = true;
                return false;
            }
            Pos += 2;
            Depth--;
            return true;
        }
        Pos++; // '>'

        // content
        string text;
        for (;;)
        {
            if (Pos >= Text.size())
            {
                Error = true;
                return false;
            }
            if (Text[Pos] == '<')
            {
                if (Text.compare(Pos, 9, "<![CDATA[") == 0)
                {
                    const size_t end = Text.find("]]>", Pos + 9);
                    if (end == string::npos)
                    {
                        Error = true;
                        return false;
                    }
                    text += Text.substr(Pos + 9, end - Pos - 9);
                    Pos = end + 3;
                    continue;
                }
                if (Pos + 1 < Text.size() && Text[Pos + 1] == '/')
                {
                    // closing tag; its name must match the element being
                    // closed (a mismatched </b> closing <a> is a parse error,
                    // not a silent accept)
                    const size_t end = Text.find('>', Pos + 2);
                    if (end == string::npos)
                    {
                        Error = true;
                        return false;
                    }
                    string closeName = Text.substr(Pos + 2, end - Pos - 2);
                    while (!closeName.empty() &&
                           (closeName.back() == ' ' || closeName.back() == '\t' ||
                            closeName.back() == '\n' || closeName.back() == '\r'))
                    {
                        closeName.pop_back();
                    }
                    if (closeName != element.Name)
                    {
                        Error = true;
                        return false;
                    }
                    Pos = end + 1;
                    break;
                }
                if (!text.empty())
                {
                    if (xml_text_has_content(text))
                    {
                        element.Text = xml_unescape(text);
                    }
                    text.clear();
                }
                if (SkipMisc())
                {
                    continue;
                }
                element.Children.push_back(XMLElement());
                if (!ParseElement(element.Children.back()))
                {
                    return false;
                }
                continue;
            }
            text += Text[Pos++];
        }
        if (!text.empty() && xml_text_has_content(text))
        {
            element.Text = xml_unescape(text);
        }
        Depth--;
        return true;
    }
};

} // namespace

bool XMLDocument::Parse(const string &text)
{
    Root = XMLElement();
    XMLParser parser(text);
    parser.SkipMisc();
    if (parser.Pos >= text.size() || !parser.ParseElement(Root))
    {
        return false;
    }
    return true;
}

bool XMLDocument::Load(const fs_path &path)
{
    FILE *file = fs_open(path, FS_READ);
    if (file == nullptr)
    {
        return false;
    }
    string contents;
    char buffer[4096];
    size_t read = 0;
    while ((read = fread(buffer, 1, sizeof(buffer), file)) != 0)
    {
        contents.append(buffer, read);
    }
    fclose(file);
    return Parse(contents);
}

// --- writing ---------------------------------------------------------------

static void xml_write_element(const XMLElement &element, int depth, bool autoFormat, string &out)
{
    if (autoFormat)
    {
        out.append(depth * XML_AUTOFORMAT_INDENT, ' ');
    }
    out += '<';
    out += element.Name;
    for (const auto &attr : element.Attributes)
    {
        out += ' ';
        out += attr.first;
        out += "=\"";
        out += xml_escape_attribute(attr.second);
        out += '"';
    }

    if (element.Children.empty() && element.Text.empty())
    {
        out += "/>";
    }
    else
    {
        out += '>';
        if (!element.Text.empty())
        {
            out += xml_escape_text(element.Text);
        }
        if (!element.Children.empty())
        {
            for (const auto &child : element.Children)
            {
                if (autoFormat)
                {
                    out += '\n';
                }
                xml_write_element(child, depth + 1, autoFormat, out);
            }
        }
        if (autoFormat && !element.Children.empty())
        {
            out += '\n';
            out.append(depth * XML_AUTOFORMAT_INDENT, ' ');
        }
        out += "</";
        out += element.Name;
        out += '>';
    }
}

string XMLDocument::Write(bool autoFormat) const
{
    string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    if (!Root.Name.empty())
    {
        if (autoFormat)
        {
            out += '\n';
        }
        xml_write_element(Root, 0, autoFormat, out);
    }
    if (autoFormat)
    {
        out += '\n';
    }
    return out;
}

bool AtomicFileWrite(const fs_path &path, const void *data, size_t size)
{
    // <dir>/<name>.tmp - same directory so the rename stays on one filesystem
    const string tmpName = string(fs_path_ascii(path)) + ".tmp";
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
    // previous content or the complete new content (the plan's §8.1 row)
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

bool XMLDocument::Save(const fs_path &path, bool autoFormat) const
{
    // every assistant config write funnels through here (17 XML files incl.
    // Options.xml, Hotkeys.xml, Macros.xml, Map.xml) - the temp+rename path
    // keeps a crash mid-save from truncating a profile
    const string out = Write(autoFormat);
    return AtomicFileWrite(path, out.data(), out.size());
}

// --- self test -------------------------------------------------------------

static bool XMLSelfTestCompare(const XMLElement &a, const XMLElement &b, int depth)
{
    if (a.Name != b.Name || a.Text != b.Text ||
        a.Attributes.size() != b.Attributes.size() ||
        a.Children.size() != b.Children.size())
    {
        Warning(Assistant, "xml self-test: round-trip mismatch at depth %i on <%s>\n", depth, a.Name.c_str());
        return false;
    }
    for (size_t i = 0; i < a.Attributes.size(); i++)
    {
        if (a.Attributes[i] != b.Attributes[i])
        {
            Warning(Assistant, "xml self-test: attribute mismatch on <%s %s>\n", a.Name.c_str(), a.Attributes[i].first.c_str());
            return false;
        }
    }
    for (size_t i = 0; i < a.Children.size(); i++)
    {
        if (!XMLSelfTestCompare(a.Children[i], b.Children[i], depth + 1))
        {
            return false;
        }
    }
    return true;
}

bool XMLSelfTest()
{
    // One synthetic record per assistant schema family (flat <data> records
    // with typed attributes, one nested list file, entities + unicode text).
    static const char *schemas[] = {
        // Options.xml: <data version> + per-section records (main/hotkeys/...)
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<data version=\"0\">\n"
        "    <main flags=\"4194303\" messageslevel=\"1\" searchdistance=\"3\" fontcolor=\"0x02B2\""
        " usefontcolor=\"false\" titledata=\"UO - {name} (&amp; more) &lt;test&gt; &#39;q&#39;\"/>\n"
        "    <display type=\"1\" flags=\"6\" textcolor=\"0\" titledata=\"a&quot;b\"/>\n"
        "</data>\n",
        // List files: <data> + repeated records
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<data version=\"0\" size=\"2\" dressbag=\"0x00000000\">\n"
        "    <object name=\"vendor\" id=\"0x40001234\"/>\n"
        "    <object name=\"&#209;\xc3\xa9 ball\" id=\"0x00000001\"/>\n"
        "</data>\n",
        // Nested two-level file (FindList.xml): data -> findlist -> item
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<data version=\"0\" size=\"2\">\n"
        "    <findlist name=\"default\">\n"
        "        <item graphic=\"0x0EED\" color=\"0xFFFF\" comment=\"gold coins\"/>\n"
        "        <item graphic=\"0x0F7B\" color=\"0x0000\" comment=\"\"/>\n"
        "    </findlist>\n"
        "    <findlist name=\"empty\"/>\n"
        "</data>\n",
    };
    for (const char *schema : schemas)
    {
        XMLDocument first;
        if (!first.Parse(schema))
        {
            Warning(Assistant, "xml self-test: parse failure\n");
            return false;
        }
        XMLDocument second;
        if (!second.Parse(first.Write()))
        {
            Warning(Assistant, "xml self-test: reparse failure\n");
            return false;
        }
        if (!XMLSelfTestCompare(first.Root, second.Root, 0))
        {
            return false;
        }
    }
    Info(Assistant, "xml self-test passed (%i schemas)\n", (int)countof(schemas));
    return true;
}

} // namespace xuoassist
