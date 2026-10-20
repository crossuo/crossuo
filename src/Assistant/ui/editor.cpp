// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The editor implementation: a single-purpose JS code editor widget.
//
// Highlighting is a hand-written per-line tokenizer (no regex): keywords, the
// assistant API objects, member calls, sentinels, escape-aware strings and
// template literals, hex/decimal numbers, // and /* */ comments (block
// comments carry across lines through a per-line state computed on document
// changes). Runs are cached per line and invalidated on edits.
//
// Auto-completion is fed by the real script API surface
// (ScriptAPI_CompletionList, collected from the same tables the runtime
// registers): members after `Client.`-style scopes, the merged engine-object
// members for unknown identifiers, and global identifiers while typing.
//
// Widget state is keyed by the owning buffer pointer: every document owns a
// stable astr_t (ui/editors.cpp), so the pointer is the document identity and
// SyncBuffer/release work from any window scope.

#include "editor.h"
#include "../script/apiscript.h" // ScriptAPI_CompletionList (the completion source)

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <external/gfx/imgui/imgui.h>
#include <external/gfx/imgui/imgui_internal.h> // ImGuiItemFlags_NoFocus (the popup items)

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_map>

namespace xuoassist {

namespace {

int editor_clamp(int value, int lo, int hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

bool is_ident_char(unsigned char c)
{
    return isalnum(c) != 0 || c == '_';
}

enum TOKEN_COLOR
{
    TC_DEFAULT = 0,
    TC_METHOD,
    TC_KEYWORD,
    TC_API,
    TC_GLOBAL,
    TC_NUMBER,
    TC_STRING,
    TC_COMMENT
};

ImU32 token_color(int token)
{
    switch (token)
    {
        case TC_METHOD:
            return IM_COL32(0xAA, 0x00, 0xAA, 0xFF); // darkMagenta
        case TC_KEYWORD:
            return IM_COL32(0x6A, 0x9E, 0xD8, 0xFF); // soft blue
        case TC_API:
            return IM_COL32(0xE5, 0xC0, 0x7B, 0xFF); // warm gold: the API objects
        case TC_GLOBAL:
            return IM_COL32(0x80, 0x80, 0x00, 0xFF); // darkYellow: sentinel aliases
        case TC_NUMBER:
            return IM_COL32(0x6A, 0xB0, 0x6A, 0xFF); // green
        case TC_STRING:
            return IM_COL32(0xCC, 0x88, 0x66, 0xFF); // amber-brown
        case TC_COMMENT:
            return IM_COL32(0xA0, 0xA0, 0xA4, 0xFF); // gray
        default:
            return IM_COL32(220, 220, 220, 255);
    }
}

// The JS keyword set (the assistant scripts are plain JS).
bool is_keyword(const char *text, size_t length)
{
    static const char *const KEYWORDS[] = {
        "function", "return", "if",     "else",   "for",      "while",  "do",     "switch",
        "case",     "default", "break", "continue", "var",    "let",   "const",  "new",
        "this",     "typeof",  "instanceof", "void", "delete", "in",    "of",     "try",
        "catch",    "finally", "throw", "true",   "false",   "null",  "undefined",
    };
    for (const char *keyword : KEYWORDS)
    {
        if (strlen(keyword) == length && strncmp(keyword, text, length) == 0)
        {
            return true;
        }
    }
    return false;
}

// The API objects registered on the global object (Script_RegisterAPI).
bool is_api_object(const char *text, size_t length)
{
    static const char *const OBJECTS[] = { "Client", "Player", "TextWindow", "SelectedTile" };
    for (const char *object : OBJECTS)
    {
        if (strlen(object) == length && strncmp(object, text, length) == 0)
        {
            return true;
        }
    }
    return false;
}

// The sentinel serial aliases (plain strings the API pattern-matches) and the
// one built-in string global.
bool is_global_word(const char *text, size_t length)
{
    static const char *const GLOBALS[] = { "self", "lastcorpse", "lasttarget", "lasttile",
        "backpack", "ground", "lastattack", "lastcontainer", "laststatus", "lastobject",
        "ScriptName" };
    for (const char *global : GLOBALS)
    {
        if (strlen(global) == length && strncmp(global, text, length) == 0)
        {
            return true;
        }
    }
    return false;
}

struct TOKEN_RUN
{
    int start;
    int length;
    int color;
};

// The per-line tokenizer: single left-to-right scan, no backtracking, no
// allocations per token beyond the run vector. `inBlockComment` carries the
// /* */ state in and out (the editor computes it per line on edits).
void tokenize_line(const astr_t &line, vector<TOKEN_RUN> &runs, bool &inBlockComment)
{
    runs.clear();
    const size_t length = line.size();
    size_t i = 0;
    bool afterDot = false; // the next identifier is a member (`.Name`)
    while (i < length)
    {
        const char c = line[i];

        // inside a /* */ block: the run extends to the closer or the EOL
        if (inBlockComment)
        {
            const size_t end = line.find("*/", i);
            if (end == astr_t::npos)
            {
                runs.push_back({ (int)i, (int)(length - i), TC_COMMENT });
                return;
            }
            runs.push_back({ (int)i, (int)(end + 2 - i), TC_COMMENT });
            i = end + 2;
            inBlockComment = false;
            continue;
        }

        // // comment to EOL
        if (c == '/' && i + 1 < length && line[i + 1] == '/')
        {
            runs.push_back({ (int)i, (int)(length - i), TC_COMMENT });
            return;
        }

        // /* block comment start
        if (c == '/' && i + 1 < length && line[i + 1] == '*')
        {
            const size_t end = line.find("*/", i + 2);
            if (end == astr_t::npos)
            {
                runs.push_back({ (int)i, (int)(length - i), TC_COMMENT });
                inBlockComment = true;
                return;
            }
            runs.push_back({ (int)i, (int)(end + 2 - i), TC_COMMENT });
            i = end + 2;
            continue;
        }

        // strings: ' " and template literals, with backslash escapes
        if (c == '"' || c == '\'' || c == '`')
        {
            size_t end = i + 1;
            while (end < length && line[end] != c)
            {
                if (line[end] == '\\' && end + 1 < length)
                {
                    end++; // the escaped character cannot close the string
                }
                end++;
            }
            if (end < length)
            {
                end++; // the closing quote
            }
            runs.push_back({ (int)i, (int)(end - i), TC_STRING });
            i = end;
            continue;
        }

        // 0x hex numbers
        if (c == '0' && i + 1 < length && (line[i + 1] == 'x' || line[i + 1] == 'X'))
        {
            size_t end = i + 2;
            while (end < length && isxdigit((unsigned char)line[end]))
            {
                end++;
            }
            if (end > i + 2)
            {
                runs.push_back({ (int)i, (int)(end - i), TC_NUMBER });
                i = end;
                continue;
            }
        }

        // decimal numbers with optional fraction and exponent
        if (isdigit((unsigned char)c))
        {
            size_t end = i;
            while (end < length && (isdigit((unsigned char)line[end]) || line[end] == '.'))
            {
                end++;
            }
            if (end < length && (line[end] == 'e' || line[end] == 'E'))
            {
                size_t expEnd = end + 1;
                if (expEnd < length && (line[expEnd] == '+' || line[expEnd] == '-'))
                {
                    expEnd++;
                }
                const size_t digitsStart = expEnd;
                while (expEnd < length && isdigit((unsigned char)line[expEnd]))
                {
                    expEnd++;
                }
                if (expEnd > digitsStart)
                {
                    end = expEnd;
                }
            }
            runs.push_back({ (int)i, (int)(end - i), TC_NUMBER });
            i = end;
            continue;
        }

        // '.' marks a member access: the next identifier is a method/property
        if (c == '.')
        {
            afterDot = true;
            i++;
            continue;
        }

        // identifiers: member calls magenta, API objects gold, keywords blue,
        // sentinels dark yellow
        if (isalpha((unsigned char)c) || c == '_')
        {
            size_t end = i + 1;
            while (end < length && is_ident_char(line[end]))
            {
                end++;
            }
            const size_t wordLength = end - i;
            int color = TC_DEFAULT;
            if (afterDot)
            {
                color = TC_METHOD;
            }
            else if (is_api_object(line.c_str() + i, wordLength))
            {
                color = TC_API;
            }
            else if (is_global_word(line.c_str() + i, wordLength))
            {
                color = TC_GLOBAL;
            }
            else if (is_keyword(line.c_str() + i, wordLength))
            {
                color = TC_KEYWORD;
            }
            if (color != TC_DEFAULT)
            {
                runs.push_back({ (int)i, (int)wordLength, color });
            }
            afterDot = false;
            i = end;
            continue;
        }

        if (!isspace((unsigned char)c))
        {
            afterDot = false;
        }
        i++;
    }
}

// One completion candidate: what the popup shows and what acceptance inserts
// (functions insert Name() and the caret goes between the parens).
struct XA_COMPLETION_ITEM
{
    astr_t Label;
    astr_t Insert;
    int Args;
    bool Function;
};

// The members of `scope` (an API object name, or anything else = one of the
// engine objects: GameObject/JournalEntry/Gump/Menu/GumpHook/ClientMacro/File,
// whose members merge - the type of a script variable is not tracked).
void completion_scope_items(const astr_t &scope, vector<XA_COMPLETION_ITEM> &out)
{
    static const char *const GLOBAL_OBJECT_SCOPES[] = { "Client", "Player", "TextWindow",
        "SelectedTile" };
    bool known = false;
    for (const char *object : GLOBAL_OBJECT_SCOPES)
    {
        if (scope == object)
        {
            known = true;
            break;
        }
    }
    const vector<XA_API_COMPLETION> &api = ScriptAPI_CompletionList();
    for (const XA_API_COMPLETION &entry : api)
    {
        if (entry.Scope[0] == '\0')
        {
            continue; // globals are the no-scope list
        }
        if (known ? scope == entry.Scope : true)
        {
            XA_COMPLETION_ITEM item;
            item.Label = entry.Name;
            item.Insert = entry.Name;
            item.Args = entry.Args;
            item.Function = entry.IsFunction;
            if (entry.IsFunction)
            {
                item.Label += "()";
                item.Insert += "()";
            }
            out.push_back(item);
        }
        if (known)
        {
            continue; // exact scope: stop widening after the first pass
        }
    }
}

// The global identifiers: the API objects, the sentinels and ScriptName.
void completion_global_items(vector<XA_COMPLETION_ITEM> &out)
{
    const vector<XA_API_COMPLETION> &api = ScriptAPI_CompletionList();
    for (const XA_API_COMPLETION &entry : api)
    {
        if (entry.Scope[0] != '\0')
        {
            continue;
        }
        XA_COMPLETION_ITEM item;
        item.Label = entry.Name;
        item.Insert = entry.Name;
        item.Args = -1;
        item.Function = false;
        out.push_back(item);
    }
}

struct EDITOR_STATE
{
    astr_t *Buffer{ nullptr };
    vector<astr_t> LineBuffer;
    int CursorLine{ 0 };
    int CursorCol{ 0 };
    int AnchorLine{ -1 }; // selection anchor (-1 = none)
    int AnchorCol{ 0 };
    int TopLine{ 0 };
    bool Modified{ false };

    // --- completion popup ---
    bool CompletionOpen{ false };
    int CompletionIndex{ 0 };
    int CompletionWordCol{ 0 }; // the typed prefix starts here (cursor line)
    vector<XA_COMPLETION_ITEM> CompletionItems;

    // --- layout of the last draw (the popup anchors to the caret) ---
    ImVec2 DrawOrigin{ 0, 0 };
    ImGuiViewport *DrawViewport{ nullptr }; // the editor's OS window (pop-out)
    float DrawLineHeight{ 18.0f };
    float DrawCharWidth{ 8.0f };
    float DrawGutter{ 40.0f };
    int DrawFirstLine{ 0 };
    int DrawLastLine{ 0 };

    // --- mouse multi-click tracking (double = word, triple = line) ---
    double LastClickTime{ -1.0 };
    int LastClickLine{ -1 };
    int ClickCount{ 0 };
    int DragStartLine{ 0 };
    int DragStartCol{ 0 };

    // --- highlight invalidation: any edit bumps the epoch, which rebuilds
    // the block-comment line state and drops the run cache
    uint32_t EditEpoch{ 0 };
    uint32_t CommentEpoch{ 0xFFFFFFFF };
    vector<bool> LineStartsInComment;

    struct CACHE_ENTRY
    {
        size_t hash{ 0 };
        vector<TOKEN_RUN> runs;
    };
    std::unordered_map<int, CACHE_ENTRY> Cache;
    uint32_t LastUndoTick{ 0 };

    const vector<TOKEN_RUN> &runs_for(int lineIndex, const astr_t &line)
    {
        const size_t hash = std::hash<astr_t>{}(line);
        auto it = Cache.find(lineIndex);
        if (it != Cache.end() && it->second.hash == hash)
        {
            return it->second.runs;
        }
        CACHE_ENTRY &entry = Cache[lineIndex];
        entry.hash = hash;
        bool inComment = lineIndex < (int)LineStartsInComment.size() &&
                         LineStartsInComment[lineIndex];
        tokenize_line(line, entry.runs, inComment);
        // keep the cache bounded when scrolling long files
        if (Cache.size() > 4096)
        {
            Cache.clear();
        }
        return entry.runs;
    }

    // The block-comment state at the start of every line: recomputed whenever
    // the document changed (any edit bumps EditEpoch).
    void ensure_comment_state()
    {
        if (CommentEpoch == EditEpoch && (int)LineStartsInComment.size() == (int)LineBuffer.size())
        {
            return;
        }
        LineStartsInComment.assign(LineBuffer.size(), false);
        bool inComment = false;
        for (size_t i = 0; i < LineBuffer.size(); i++)
        {
            LineStartsInComment[i] = inComment;
            const astr_t &text = LineBuffer[i];
            for (size_t p = 0; p + 1 < text.size(); p++)
            {
                if (inComment)
                {
                    if (text[p] == '*' && text[p + 1] == '/')
                    {
                        inComment = false;
                        p++;
                    }
                }
                else if (text[p] == '/' && text[p + 1] == '*')
                {
                    inComment = true;
                    p++;
                }
                else if (text[p] == '/' && text[p + 1] == '/')
                {
                    break; // the rest of the line is a line comment
                }
                else if (text[p] == '"' || text[p] == '\'' || text[p] == '`')
                {
                    // a comment marker inside a string does not count
                    const char quote = text[p];
                    p++;
                    while (p < text.size() && text[p] != quote)
                    {
                        if (text[p] == '\\' && p + 1 < text.size())
                        {
                            p++;
                        }
                        p++;
                    }
                }
            }
        }
        CommentEpoch = EditEpoch;
        Cache.clear(); // runs depend on the carried state
    }

    void split_buffer(const astr_t &text)
    {
        // (re)split the owning astr_t buffer into lines
        LineBuffer.clear();
        size_t start = 0;
        while (true)
        {
            size_t end = text.find('\n', start);
            if (end == astr_t::npos)
            {
                LineBuffer.push_back(text.substr(start));
                break;
            }
            LineBuffer.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        if (LineBuffer.empty())
        {
            LineBuffer.push_back("");
        }
        EditEpoch++;
    }

    astr_t join_buffer() const
    {
        astr_t text;
        for (size_t i = 0; i < LineBuffer.size(); i++)
        {
            text += LineBuffer[i];
            if (i + 1 < LineBuffer.size())
            {
                text += '\n';
            }
        }
        return text;
    }

    // --- undo/redo (snapshots with a memory budget) ---
    struct UNDO_STATE
    {
        vector<astr_t> lines;
        int line;
        int col;
        size_t bytes; // approximate memory of this snapshot
    };
    // total snapshot memory budget: past it the OLDEST snapshot is dropped
    // (an unbounded undo stack on a large script could grow unbounded)
    static constexpr size_t UNDO_BUDGET_BYTES = 8 * 1024 * 1024;
    vector<UNDO_STATE> Undo;
    vector<UNDO_STATE> Redo;
    size_t UndoBytes{ 0 };
    size_t RedoBytes{ 0 };
    bool InUndo{ false };

    size_t state_bytes() const
    {
        size_t bytes = 0;
        for (const astr_t &line : LineBuffer)
        {
            bytes += line.size() + 1;
        }
        return bytes;
    }

    void snapshot()
    {
        if (InUndo)
        {
            return;
        }
        Redo.clear(); // a new edit invalidates the redo stack
        RedoBytes = 0;
        Undo.push_back({ LineBuffer, CursorLine, CursorCol, state_bytes() });
        UndoBytes += Undo.back().bytes;
        while (UndoBytes > UNDO_BUDGET_BYTES && Undo.size() > 1)
        {
            UndoBytes -= Undo.front().bytes;
            Undo.erase(Undo.begin());
        }
        EditEpoch++;
    }

    void undo()
    {
        if (Undo.empty())
        {
            return;
        }
        InUndo = true;
        Redo.push_back({ LineBuffer, CursorLine, CursorCol, state_bytes() });
        RedoBytes += Redo.back().bytes;
        UNDO_STATE &restore = Undo.back();
        LineBuffer = restore.lines;
        CursorLine = std::min(restore.line, (int)LineBuffer.size() - 1);
        CursorCol = std::min(restore.col, (int)LineBuffer.at(CursorLine).size());
        Modified = true;
        UndoBytes -= restore.bytes;
        Undo.pop_back();
        InUndo = false;
        CompletionOpen = false;
        EditEpoch++;
    }

    void redo()
    {
        if (Redo.empty())
        {
            return;
        }
        InUndo = true;
        Undo.push_back({ LineBuffer, CursorLine, CursorCol, state_bytes() });
        UndoBytes += Undo.back().bytes;
        UNDO_STATE &restore = Redo.back();
        LineBuffer = restore.lines;
        CursorLine = std::min(restore.line, (int)LineBuffer.size() - 1);
        CursorCol = std::min(restore.col, (int)LineBuffer.at(CursorLine).size());
        Modified = true;
        RedoBytes -= restore.bytes;
        Redo.pop_back();
        InUndo = false;
        CompletionOpen = false;
        EditEpoch++;
    }

    bool has_selection() const { return AnchorLine >= 0 && (AnchorLine != CursorLine || AnchorCol != CursorCol); }

    void selection(int &line0, int &col0, int &line1, int &col1) const
    {
        line0 = AnchorLine;
        col0 = AnchorCol;
        line1 = CursorLine;
        col1 = CursorCol;
        if (line0 > line1 || (line0 == line1 && col0 > col1))
        {
            std::swap(line0, line1);
            std::swap(col0, col1);
        }
    }

    astr_t selection_text() const
    {
        if (!has_selection())
        {
            return "";
        }
        int line0, col0, line1, col1;
        selection(line0, col0, line1, col1);
        if (line0 == line1)
        {
            return LineBuffer.at(line0).substr(col0, col1 - col0);
        }
        astr_t out = LineBuffer.at(line0).substr(col0);
        for (int line = line0 + 1; line < line1; line++)
        {
            out += '\n';
            out += LineBuffer.at(line);
        }
        out += '\n';
        out += LineBuffer.at(line1).substr(0, col1);
        return out;
    }

    void delete_selection()
    {
        if (!has_selection())
        {
            return;
        }
        snapshot();
        int line0, col0, line1, col1;
        selection(line0, col0, line1, col1);
        if (line0 == line1)
        {
            astr_t &line = LineBuffer.at(line0);
            line.erase(col0, col1 - col0);
        }
        else
        {
            astr_t &first = LineBuffer.at(line0);
            astr_t &last = LineBuffer.at(line1);
            first.erase(col0);
            first += last.substr(col1);
            LineBuffer.erase(LineBuffer.begin() + line0 + 1, LineBuffer.begin() + line1 + 1);
        }
        CursorLine = line0;
        CursorCol = col0;
        AnchorLine = -1;
        Modified = true;
        CompletionOpen = false;
    }

    void insert_text(const astr_t &text)
    {
        snapshot();
        astr_t &line = LineBuffer.at(CursorLine);
        line.insert((size_t)CursorCol, text);
        CursorCol += (int)text.size();
        Modified = true;
    }

    // Multi-line insertion (paste): splits on '\n' and splices lines.
    void insert_block(const astr_t &text)
    {
        if (text.find('\n') == astr_t::npos)
        {
            insert_text(text);
            return;
        }
        snapshot();
        vector<astr_t> parts;
        size_t start = 0;
        while (true)
        {
            size_t end = text.find('\n', start);
            if (end == astr_t::npos)
            {
                if (start < text.size())
                {
                    parts.push_back(text.substr(start));
                }
                break;
            }
            parts.push_back(text.substr(start, end - start));
            start = end + 1;
        }
        if (parts.empty())
        {
            return;
        }
        astr_t &line = LineBuffer.at(CursorLine);
        const astr_t tail = line.substr(CursorCol);
        line.erase(CursorCol);
        line += parts[0];
        for (size_t i = 1; i < parts.size(); i++)
        {
            LineBuffer.insert(LineBuffer.begin() + CursorLine + i, parts[i]);
        }
        LineBuffer.at(CursorLine + parts.size() - 1) += tail;
        CursorLine += (int)parts.size() - 1;
        CursorCol = (int)parts.back().size();
        AnchorLine = -1;
        Modified = true;
        CompletionOpen = false;
    }
};

// --- completion ----------------------------------------------------------------

void completion_close(EDITOR_STATE *state)
{
    state->CompletionOpen = false;
    state->CompletionItems.clear();
}

// Recomputes the candidate list at the cursor. Opens the popup when there is
// something to show: members after a known scope (right after the dot too),
// or global identifiers once two characters are typed. `force` (Ctrl+Space)
// opens with any prefix.
void completion_update(EDITOR_STATE *state, bool force)
{
    const astr_t &line = state->LineBuffer.at(state->CursorLine);
    // only complete at a word end
    if (state->CursorCol < (int)line.size() && is_ident_char(line[state->CursorCol]))
    {
        completion_close(state);
        return;
    }
    // the typed prefix
    int start = state->CursorCol;
    while (start > 0 && is_ident_char(line[start - 1]))
    {
        start--;
    }
    const astr_t prefix = line.substr(start, state->CursorCol - start);
    // the scope: the identifier before a dot right before the prefix
    astr_t scope;
    if (start > 0 && line[start - 1] == '.')
    {
        int end = start - 1;
        int begin = end;
        while (begin > 0 && is_ident_char(line[begin - 1]))
        {
            begin--;
        }
        if (end > begin)
        {
            scope = line.substr(begin, end - begin);
        }
    }
    if (scope.empty() && (prefix.size() < 2 || !isalpha((unsigned char)prefix[0])) && !force)
    {
        completion_close(state);
        return;
    }

    vector<XA_COMPLETION_ITEM> items;
    if (scope.empty())
    {
        completion_global_items(items);
    }
    else
    {
        completion_scope_items(scope, items);
    }
    // case-insensitive prefix filter
    vector<XA_COMPLETION_ITEM> filtered;
    astr_t lower;
    for (const XA_COMPLETION_ITEM &item : items)
    {
        bool match = item.Label.size() >= prefix.size();
        for (size_t i = 0; match && i < prefix.size(); i++)
        {
            match = tolower((unsigned char)item.Label[i]) == tolower((unsigned char)prefix[i]);
        }
        if (match)
        {
            filtered.push_back(item);
        }
    }
    if (filtered.empty())
    {
        completion_close(state);
        return;
    }
    std::sort(filtered.begin(), filtered.end(),
        [](const XA_COMPLETION_ITEM &a, const XA_COMPLETION_ITEM &b) {
            return str_lower(a.Label) < str_lower(b.Label);
        });
    state->CompletionItems = filtered;
    state->CompletionWordCol = start;
    state->CompletionIndex = 0;
    state->CompletionOpen = true;
}

void completion_accept(EDITOR_STATE *state);

bool completion_handle_keys(EDITOR_STATE *state)
{
    const int count = (int)state->CompletionItems.size();
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
    {
        state->CompletionIndex = (state->CompletionIndex + count - 1) % count;
        return true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
    {
        state->CompletionIndex = (state->CompletionIndex + 1) % count;
        return true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
    {
        state->CompletionIndex = std::max(0, state->CompletionIndex - 8);
        return true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
    {
        state->CompletionIndex = std::min(count - 1, state->CompletionIndex + 8);
        return true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        completion_close(state);
        return true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter) ||
        ImGui::IsKeyPressed(ImGuiKey_Tab))
    {
        completion_accept(state);
        return true;
    }
    return false;
}

void completion_accept(EDITOR_STATE *state)
{
    if (state->CompletionIndex < 0 ||
        state->CompletionIndex >= (int)state->CompletionItems.size())
    {
        completion_close(state);
        return;
    }
    const XA_COMPLETION_ITEM item = state->CompletionItems[state->CompletionIndex];
    state->snapshot();
    astr_t &line = state->LineBuffer.at(state->CursorLine);
    // replace the typed prefix with the completion
    if (state->CursorCol > state->CompletionWordCol)
    {
        line.erase(state->CompletionWordCol, state->CursorCol - state->CompletionWordCol);
    }
    state->CursorCol = state->CompletionWordCol;
    line.insert((size_t)state->CursorCol, item.Insert);
    state->CursorCol += (int)item.Insert.size();
    if (item.Function && item.Args > 0)
    {
        state->CursorCol--; // the caret goes between the parens
    }
    state->AnchorLine = -1;
    state->Modified = true;
    completion_close(state);
}

// The popup: a borderless window whose items never take focus (clicking one
// leaves the keyboard in the editor), native wheel scrolling, selection kept
// in view. Drawn after the editor child so it lands on top.
void completion_draw(EDITOR_STATE *state, const char *editorId)
{
    if (!state->CompletionOpen || state->CompletionItems.empty())
    {
        return;
    }
    // the caret must be on a visible line for the anchor to make sense
    if (state->CursorLine < state->DrawFirstLine || state->CursorLine >= state->DrawLastLine)
    {
        return;
    }
    const float rowH = ImGui::GetTextLineHeight() + 2.0f;
    const int count = (int)state->CompletionItems.size();
    const int visRows = std::min(count, 10);

    float width = 180.0f;
    for (const XA_COMPLETION_ITEM &item : state->CompletionItems)
    {
        width = std::max(width, ImGui::CalcTextSize(item.Label.c_str()).x + 44.0f);
    }
    width = std::min(width, 460.0f);
    const float height = visRows * rowH;

    // anchor + clamp inside the editor's OWN viewport: a popped-out assistant
    // is a separate OS window with its own coordinate space (io.DisplaySize is
    // the main window only - clamping against it misplaced the popup or pulled
    // it into the game window)
    ImGuiViewport *viewport =
        state->DrawViewport != nullptr ? state->DrawViewport : ImGui::GetMainViewport();
    const ImVec2 vpPos = viewport->Pos;
    const ImVec2 vpSize = viewport->Size;
    ImVec2 pos(state->DrawOrigin.x + state->DrawGutter + 4.0f + state->CursorCol * state->DrawCharWidth,
        state->DrawOrigin.y + (state->CursorLine - state->DrawFirstLine + 1) * state->DrawLineHeight +
            2.0f);
    pos.x = std::min(pos.x, vpPos.x + vpSize.x - width - 8.0f);
    pos.x = std::max(pos.x, vpPos.x + 4.0f);
    if (pos.y + height > vpPos.y + vpSize.y - 8.0f)
    {
        // flip above the caret line when it would overflow the viewport
        pos.y = std::max(vpPos.y + 4.0f,
            state->DrawOrigin.y + (state->CursorLine - state->DrawFirstLine) * state->DrawLineHeight -
                height - 2.0f);
    }
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::SetNextWindowViewport(viewport->ID);
    char title[64];
    snprintf(title, sizeof(title), "scripteditor_completion_%p", (void *)state);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(70, 72, 84, 200));
    // the selected row must read clearly against the popup (the default
    // Header color is barely darker than the popup background)
    ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(38, 79, 160, 200));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(52, 98, 190, 210));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(52, 98, 190, 230));
    ImGui::PushItemFlag(ImGuiItemFlags_NoFocus, true); // clicks stay in the editor
    if (ImGui::Begin(title, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav))
    {
        for (int i = 0; i < count; i++)
        {
            const XA_COMPLETION_ITEM &item = state->CompletionItems[i];
            ImGui::PushID(i);
            const bool selected = i == state->CompletionIndex;
            if (ImGui::Selectable(item.Label.c_str(), selected, ImGuiSelectableFlags_None,
                    ImVec2(width, rowH)))
            {
                state->CompletionIndex = i;
                completion_accept(state);
                ImGui::PopID();
                break;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
                ImGui::SetScrollHereY(0.4f);
            }
            // dimmed arity hint on the function rows
            if (item.Function)
            {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
                ImGui::TextDisabled("%d args", std::max(0, item.Args));
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
    ImGui::PopItemFlag();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    (void)editorId;
}


// The widget states, keyed by the owning buffer pointer (ui/editors.cpp gives
// every document a stable astr_t; the pointer is the document identity, so
// SyncBuffer and release work from any window scope).
std::unordered_map<astr_t *, EDITOR_STATE> g_EditorStates;
// the widget the text operations (Find/Replace/Comment) act on: the last
// editor drawn this frame
EDITOR_STATE *s_ActiveEditor = nullptr;

} // namespace

void TextEditor_SyncBuffer(astr_t *buffer)
{
    // an external replace inside the SAME document (Reload from disk, a
    // script's LoadScript): re-split now so the next draw shows the new text
    // instead of the stale line buffer
    auto it = g_EditorStates.find(buffer);
    if (it == g_EditorStates.end())
    {
        return;
    }
    EDITOR_STATE *state = &it->second;
    state->split_buffer(*buffer);
    state->CursorLine = 0;
    state->CursorCol = 0;
    state->AnchorLine = -1;
    state->Modified = false;
    completion_close(state);
}

void TextEditor_ReleaseState(astr_t *buffer)
{
    // the document is gone: drop its widget state (the map is keyed by the
    // buffer pointer, which is about to be freed - a later document allocated
    // at the same address must not inherit stale lines)
    g_EditorStates.erase(buffer);
    if (s_ActiveEditor != nullptr && s_ActiveEditor->Buffer == buffer)
    {
        s_ActiveEditor = nullptr;
    }
}

bool TextEditor_Draw(
    const char *id, astr_t *buffer, float height, int gotoLine, bool *gotoPending)
{
    auto it = g_EditorStates.find(buffer);
    if (it == g_EditorStates.end())
    {
        it = g_EditorStates.emplace(buffer, EDITOR_STATE{}).first;
        it->second.Buffer = buffer;
        it->second.split_buffer(*buffer);
    }
    EDITOR_STATE *state = &it->second;
    s_ActiveEditor = state;
    const vector<astr_t> &lines = state->LineBuffer;

    // the popup only makes sense while the editor owns the keyboard
    if (state->CompletionOpen &&
        !(ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
            ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
    {
        completion_close(state);
    }

    if (gotoPending != nullptr && *gotoPending)
    {
        *gotoPending = false;
        if (gotoLine >= 1 && gotoLine <= (int)lines.size())
        {
            state->CursorLine = gotoLine - 1;
            state->CursorCol = 0;
            state->TopLine = std::max(0, state->CursorLine - 5);
        }
    }

    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const float gutterWidth = ImGui::CalcTextSize("00000").x + ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 region = ImGui::GetContentRegionAvail();
    const ImVec2 size(height > 0 ? ImVec2(0, height) : region);

    // the editor is a bordered child region; scrolling is ours
    ImGui::BeginChild(id, size, ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_NoScrollbar);
    const float contentHeight = (float)lines.size() * lineHeight;
    ImGui::BeginChild("scroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove);
    const float scrollY = ImGui::GetScrollY();
    const float viewHeight = ImGui::GetWindowHeight();
    int firstLine = std::max(0, (int)(scrollY / lineHeight) - 1);
    int lastLine = std::min((int)lines.size(), firstLine + (int)(viewHeight / lineHeight) + 3);
    state->TopLine = firstLine;

    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float charWidth = ImGui::CalcTextSize("M").x;
    const float currentY = origin.y + (state->CursorLine - firstLine) * lineHeight;

    // layout snapshot for the completion popup (anchored to the caret)
    state->DrawOrigin = origin;
    state->DrawViewport = ImGui::GetWindowViewport();
    state->DrawLineHeight = lineHeight;
    state->DrawCharWidth = charWidth;
    state->DrawGutter = gutterWidth;
    state->DrawFirstLine = firstLine;
    state->DrawLastLine = lastLine;

    // highlight invalidation: block-comment state + run cache
    state->ensure_comment_state();

    // current-line highlight (CExtendedPlainTextEdit)
    if (state->CursorLine >= firstLine && state->CursorLine < lastLine)
    {
        draw->AddRectFilled(ImVec2(origin.x, currentY),
            ImVec2(origin.x + ImGui::GetWindowWidth(), currentY + lineHeight),
            IM_COL32(255, 255, 255, 12));
    }

    ImFont *font = ImGui::GetFont();

    for (int index = firstLine; index < lastLine; index++)
    {
        const astr_t &line = lines[index];
        const float y = origin.y + (index - firstLine) * lineHeight;
        const ImVec2 linePos(origin.x + (gutterWidth + 4), y);

        // gutter line number
        char number[16];
        snprintf(number, sizeof(number), "%d", index + 1);
        draw->AddText(ImVec2(origin.x + 4, y), IM_COL32(150, 150, 150, 200), number);

        // selection background on this line
        if (state->has_selection())
        {
            int line0, col0, line1, col1;
            state->selection(line0, col0, line1, col1);
            if (index >= line0 && index <= line1)
            {
                const int from = index == line0 ? col0 : 0;
                const int to = index == line1 ? col1 : (int)line.size();
                draw->AddRectFilled(
                    ImVec2(linePos.x + from * charWidth, y),
                    ImVec2(linePos.x + to * charWidth, y + lineHeight),
                    IM_COL32(38, 79, 160, 130));
            }
        }

        // the cursor caret
        if (index == state->CursorLine &&
            (int)(ImGui::GetTime() * 2.0f) % 2 == 0)
        {
            draw->AddRectFilled(ImVec2(linePos.x + state->CursorCol * charWidth, y),
                ImVec2(linePos.x + state->CursorCol * charWidth + 1.5f, y + lineHeight),
                IM_COL32(200, 200, 200, 220));
        }

        // the tokenized text (visible lines only; per-line cached)
        const vector<TOKEN_RUN> &runs = state->runs_for(index, line);
        size_t pos = 0;
        float x = linePos.x;
        for (const TOKEN_RUN &run : runs)
        {
            if (run.start > (int)pos)
            {
                const astr_t plain = line.substr(pos, run.start - pos);
                draw->AddText(font, ImGui::GetFontSize(), ImVec2(x, y),
                    IM_COL32(220, 220, 220, 255), plain.c_str(), plain.c_str() + plain.size());
                x += ImGui::CalcTextSize(plain.c_str()).x;
            }
            const astr_t piece = line.substr(run.start, run.length);
            draw->AddText(font, ImGui::GetFontSize(), ImVec2(x, y), token_color(run.color),
                piece.c_str(), piece.c_str() + piece.size());
            x += ImGui::CalcTextSize(piece.c_str()).x;
            pos = run.start + run.length;
        }
        if (pos < line.size())
        {
            const astr_t plain = line.substr(pos);
            draw->AddText(font, ImGui::GetFontSize(), ImVec2(x, y),
                IM_COL32(220, 220, 220, 255), plain.c_str(), plain.c_str() + plain.size());
        }
    }

    // hit target: the invisible button IS the scrollable content, placed over
    // the text (it used to sit after a full-height Dummy, i.e. entirely below
    // the text, which made every mouse interaction dead)
    ImGui::SetCursorScreenPos(origin);
    ImGui::InvisibleButton("area", ImVec2(ImGui::GetWindowWidth(), contentHeight),
        ImGuiButtonFlags_MouseButtonLeft);
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    if (hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
    }

    ImGuiIO &io = ImGui::GetIO();
    if (hovered && ImGui::IsMouseClicked(0))
    {
        const ImVec2 clicked = io.MousePos;
        const int clickedLine = editor_clamp(
            firstLine + (int)((clicked.y - origin.y) / lineHeight), 0, (int)lines.size() - 1);
        const int clickedCol =
            editor_clamp((int)((clicked.x - origin.x - gutterWidth - 4) / charWidth), 0,
                (int)lines[clickedLine].size());

        // multi-click tracking: double = select word, triple = select line
        if (ImGui::GetTime() - state->LastClickTime < 0.45 && state->LastClickLine == clickedLine)
        {
            state->ClickCount++;
        }
        else
        {
            state->ClickCount = 1;
        }
        state->LastClickTime = ImGui::GetTime();
        state->LastClickLine = clickedLine;
        state->DragStartLine = clickedLine;
        state->DragStartCol = clickedCol;

        if (state->ClickCount == 1)
        {
            if (io.KeyShift)
            {
                // extend: the anchor is where the caret was
                state->AnchorLine = state->CursorLine;
                state->AnchorCol = state->CursorCol;
                state->CursorLine = clickedLine;
                state->CursorCol = clickedCol;
            }
            else
            {
                state->CursorLine = clickedLine;
                state->CursorCol = clickedCol;
                state->AnchorLine = -1;
            }
            completion_close(state);
        }
        else if (state->ClickCount == 2)
        {
            // select the word under the click
            const astr_t &text = lines[clickedLine];
            int begin = clickedCol;
            int end = clickedCol;
            while (begin > 0 && is_ident_char(text[begin - 1]))
            {
                begin--;
            }
            while (end < (int)text.size() && is_ident_char(text[end]))
            {
                end++;
            }
            if (end > begin)
            {
                state->AnchorLine = clickedLine;
                state->AnchorCol = begin;
                state->CursorLine = clickedLine;
                state->CursorCol = end;
            }
            else
            {
                state->AnchorLine = -1;
            }
        }
        else
        {
            // triple click (or more): select the whole line
            state->AnchorLine = clickedLine;
            state->AnchorCol = 0;
            state->CursorLine = clickedLine;
            state->CursorCol = (int)lines[clickedLine].size();
        }
    }
    if (active && ImGui::IsMouseDragging(0))
    {
        // the anchor stays at the drag start (fixed: it used to follow the
        // caret every frame, scrambling selections on fast drags)
        if (state->AnchorLine < 0 && state->ClickCount == 1)
        {
            state->AnchorLine = state->DragStartLine;
            state->AnchorCol = state->DragStartCol;
        }
        const ImVec2 drag = io.MousePos;
        state->CursorLine = editor_clamp(
            firstLine + (int)((drag.y - origin.y) / lineHeight), 0, (int)lines.size() - 1);
        state->CursorCol =
            editor_clamp((int)((drag.x - origin.x - gutterWidth - 4) / charWidth), 0,
                (int)lines[state->CursorLine].size());
    }

    bool enterPressed = false;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        // key handling only while this editor child is focused
        if (ImGui::IsWindowFocused())
        {
            auto clampCursor = [&]() {
                state->CursorLine = editor_clamp(state->CursorLine, 0, (int)lines.size() - 1);
                state->CursorCol = editor_clamp(
                    state->CursorCol, 0, (int)lines[state->CursorLine].size());
            };
            // keyboard selection: with Shift held the caret movement extends a
            // selection (the anchor is where the caret was when Shift first
            // mattered); without it every move collapses the selection
            auto moveSelect = [&]() {
                if (io.KeyShift)
                {
                    if (state->AnchorLine < 0)
                    {
                        state->AnchorLine = state->CursorLine;
                        state->AnchorCol = state->CursorCol;
                    }
                    return true;
                }
                state->AnchorLine = -1;
                return false;
            };

            // the completion popup owns Up/Down/PageUp/PageDown/Enter/Tab/Escape
            // (Shift means the user is selecting - the popup yields)
            if (state->CompletionOpen && io.KeyShift)
            {
                completion_close(state);
            }
            const bool completionAte =
                state->CompletionOpen ? completion_handle_keys(state) : false;

            // Ctrl+Space: force the popup with whatever prefix is at the caret
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space))
            {
                completion_update(state, true);
            }

            // clipboard + selection + history (the character loop below skips
            // everything while Ctrl is held)
            if (io.KeyCtrl)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_C))
                {
                    const astr_t selected = state->selection_text();
                    if (!selected.empty())
                    {
                        ImGui::SetClipboardText(selected.c_str());
                    }
                }
                if (ImGui::IsKeyPressed(ImGuiKey_X))
                {
                    const astr_t selected = state->selection_text();
                    if (!selected.empty())
                    {
                        ImGui::SetClipboardText(selected.c_str());
                        state->delete_selection();
                    }
                }
                if (ImGui::IsKeyPressed(ImGuiKey_V))
                {
                    const char *clipboard = ImGui::GetClipboardText();
                    if (clipboard != nullptr && clipboard[0] != '\0')
                    {
                        state->insert_block(clipboard);
                    }
                }
                if (ImGui::IsKeyPressed(ImGuiKey_A))
                {
                    state->AnchorLine = 0;
                    state->AnchorCol = 0;
                    state->CursorLine = (int)lines.size() - 1;
                    state->CursorCol = (int)lines[state->CursorLine].size();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Y) ||
                    (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)))
                {
                    state->redo();
                }
            }

            if (!completionAte && !state->CompletionOpen)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
                {
                    const bool select = moveSelect();
                    state->CursorLine = std::max(0, state->CursorLine - 1);
                    clampCursor();
                    if (!select)
                    {
                        state->AnchorLine = -1;
                    }
                }
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                {
                    const bool select = moveSelect();
                    state->CursorLine = std::min((int)lines.size() - 1, state->CursorLine + 1);
                    clampCursor();
                    if (!select)
                    {
                        state->AnchorLine = -1;
                    }
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            {
                completion_close(state);
                const bool select = moveSelect();
                if (state->CursorCol > 0)
                {
                    state->CursorCol--;
                }
                else if (state->CursorLine > 0)
                {
                    state->CursorLine--;
                    state->CursorCol = (int)lines[state->CursorLine].size();
                }
                if (!select)
                {
                    state->AnchorLine = -1;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            {
                completion_close(state);
                const bool select = moveSelect();
                if (state->CursorCol < (int)lines[state->CursorLine].size())
                {
                    state->CursorCol++;
                }
                else if (state->CursorLine < (int)lines.size() - 1)
                {
                    state->CursorLine++;
                    state->CursorCol = 0;
                }
                if (!select)
                {
                    state->AnchorLine = -1;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Home) || ImGui::IsKeyPressed(ImGuiKey_End))
            {
                completion_close(state);
                const bool select = moveSelect();
                state->CursorCol = ImGui::IsKeyPressed(ImGuiKey_Home)
                    ? 0
                    : (int)lines[state->CursorLine].size();
                if (!select)
                {
                    state->AnchorLine = -1;
                }
            }
            if (!completionAte && !state->CompletionOpen)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
                {
                    const bool select = moveSelect();
                    state->CursorLine = std::max(0, state->CursorLine - 20);
                    if (!select)
                    {
                        state->AnchorLine = -1;
                    }
                }
                if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
                {
                    const bool select = moveSelect();
                    state->CursorLine = std::min((int)lines.size() - 1, state->CursorLine + 20);
                    if (!select)
                    {
                        state->AnchorLine = -1;
                    }
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
            {
                if (state->has_selection())
                {
                    state->delete_selection();
                }
                else if (state->CursorCol > 0)
                {
                    state->snapshot();
                    astr_t &line = state->LineBuffer.at(state->CursorLine);
                    line.erase(state->CursorCol - 1, 1);
                    state->CursorCol--;
                    state->Modified = true;
                }
                else if (state->CursorLine > 0)
                {
                    state->snapshot();
                    const astr_t tail = lines[state->CursorLine];
                    state->CursorLine--;
                    state->CursorCol = (int)lines[state->CursorLine].size();
                    state->LineBuffer.at(state->CursorLine) += tail;
                    state->LineBuffer.erase(state->LineBuffer.begin() + state->CursorLine + 1);
                    state->Modified = true;
                }
                if (state->CompletionOpen)
                {
                    completion_update(state, false);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete))
            {
                if (state->has_selection())
                {
                    state->delete_selection();
                }
                else if (state->CursorCol < (int)lines[state->CursorLine].size())
                {
                    state->snapshot();
                    astr_t &line = state->LineBuffer.at(state->CursorLine);
                    line.erase(state->CursorCol, 1);
                    state->Modified = true;
                }
                else if (state->CursorLine < (int)lines.size() - 1)
                {
                    state->snapshot();
                    astr_t &line = state->LineBuffer.at(state->CursorLine);
                    line += lines[state->CursorLine + 1];
                    state->LineBuffer.erase(state->LineBuffer.begin() + state->CursorLine + 1);
                    state->Modified = true;
                }
                if (state->CompletionOpen)
                {
                    completion_update(state, false);
                }
            }
            if (!completionAte && !state->CompletionOpen &&
                (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                    ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
            {
                if (state->has_selection())
                {
                    state->delete_selection(); // Enter replaces the selection
                }
                state->snapshot();
                astr_t &line = state->LineBuffer.at(state->CursorLine);
                const astr_t tail = line.substr(state->CursorCol);
                const astr_t head = line.substr(0, state->CursorCol);
                line.erase(state->CursorCol);
                // auto-indent: the new line keeps the current indent, plus one
                // level when the line opens a block (unless the moved-down tail
                // closes it right away)
                astr_t indent;
                for (char c : head)
                {
                    if (c == ' ' || c == '\t')
                    {
                        indent += c;
                    }
                    else
                    {
                        break;
                    }
                }
                char lastNonWs = 0;
                for (auto rev = head.rbegin(); rev != head.rend(); ++rev)
                {
                    if (!isspace((unsigned char)*rev))
                    {
                        lastNonWs = *rev;
                        break;
                    }
                }
                size_t tailFirst = tail.find_first_not_of(" \t");
                const bool opensBlock = lastNonWs == '{';
                const bool closesRightAway =
                    tailFirst != astr_t::npos && tail[tailFirst] == '}';
                if (opensBlock && !closesRightAway)
                {
                    indent += "    ";
                }
                state->LineBuffer.insert(
                    state->LineBuffer.begin() + state->CursorLine + 1, indent + tail);
                state->CursorLine++;
                state->CursorCol = (int)indent.size();
                state->Modified = true;
                enterPressed = true;
            }
            if (!completionAte && !state->CompletionOpen && ImGui::IsKeyPressed(ImGuiKey_Tab))
            {
                state->delete_selection(); // Tab replaces the selection
                state->insert_text("    ");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && !io.KeyShift)
            {
                state->undo();
            }
            bool typedIdentifier = false;
            if (io.InputQueueCharacters.Size > 0 && !io.KeyCtrl && !io.KeySuper)
            {
                if (state->has_selection())
                {
                    state->delete_selection(); // typing replaces the selection
                }
                for (int i = 0; i < io.InputQueueCharacters.Size; i++)
                {
                    const ImWchar c = io.InputQueueCharacters[i];
                    if (c >= 32)
                    {
                        astr_t text;
                        // encode UTF-8 (the buffer is bytes; ImGui gives codepoints)
                        if (c < 0x80)
                        {
                            text += (char)c;
                        }
                        else if (c < 0x800)
                        {
                            text += (char)(0xC0 | (c >> 6));
                            text += (char)(0x80 | (c & 0x3F));
                        }
                        else
                        {
                            text += (char)(0xE0 | (c >> 12));
                            text += (char)(0x80 | ((c >> 6) & 0x3F));
                        }
                        state->insert_text(text);
                        if (c == '.' || is_ident_char((char)c))
                        {
                            typedIdentifier = true;
                        }
                    }
                }
                io.InputQueueCharacters.resize(0);
                if (state->CompletionOpen || typedIdentifier)
                {
                    completion_update(state, false);
                }
            }

            // keep the cursor visible
            if (state->CursorLine < firstLine + 1)
            {
                ImGui::SetScrollY(std::max(0.0f, (state->CursorLine - 1) * lineHeight));
            }
            if (state->CursorLine > lastLine - 2)
            {
                ImGui::SetScrollY(std::max(0.0f, (state->CursorLine + 1) * lineHeight - viewHeight));
            }
        }
    }

    ImGui::EndChild();
    ImGui::EndChild();

    // the completion popup (a floating window drawn over the editor; its
    // items never take focus, so the keyboard stays here)
    completion_draw(state, id);

    if (state->Modified)
    {
        state->Modified = false;
        *buffer = state->join_buffer();
        return true;
    }
    (void)enterPressed;
    return false;
}

// --- text operations ----------------------------------------------------------

namespace {

int find_in_state(EDITOR_STATE *state, const astr_t &needle, bool down, bool caseSensitive)
{
    // case-sensitive search (the dialog default); down = forward
    const vector<astr_t> &lines = state->LineBuffer;
    int startLine = state->CursorLine;
    int startCol = state->CursorCol + (down ? 1 : 0);

    auto match = [&](const astr_t &text, size_t pos) {
        if (pos + needle.size() > text.size())
        {
            return false;
        }
        if (caseSensitive)
        {
            return text.compare(pos, needle.size(), needle) == 0;
        }
        for (size_t i = 0; i < needle.size(); i++)
        {
            if (tolower((unsigned char)text[pos + i]) != tolower((unsigned char)needle[i]))
            {
                return false;
            }
        }
        return true;
    };

    if (down)
    {
        for (int line = startLine; line < (int)lines.size(); line++)
        {
            const size_t from = line == startLine ? std::max(0, startCol) : 0;
            const astr_t &text = lines[line];
            for (size_t pos = from; pos + needle.size() <= text.size(); pos++)
            {
                if (match(text, pos))
                {
                    state->CursorLine = line;
                    state->CursorCol = (int)pos + (int)needle.size();
                    state->AnchorLine = line;
                    state->AnchorCol = (int)pos;
                    return true;
                }
            }
        }
    }
    else
    {
        for (int line = startLine; line >= 0; line--)
        {
            const astr_t &text = lines[line];
            int from = line == startLine ? std::min((int)text.size(), startCol) : (int)text.size();
            for (int pos = from - (int)needle.size(); pos >= 0; pos--)
            {
                if (match(text, (size_t)pos))
                {
                    state->CursorLine = line;
                    state->CursorCol = pos;
                    state->AnchorLine = line;
                    state->AnchorCol = pos + (int)needle.size();
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

bool TextEditor_Find(const char *text, bool down, bool caseSensitive)
{
    if (text == nullptr || strlen(text) == 0 || s_ActiveEditor == nullptr)
    {
        return false;
    }
    return find_in_state(s_ActiveEditor, text, down, caseSensitive);
}

bool TextEditor_Replace(const char *findText, const char *replaceText, bool caseSensitive)
{
    if (s_ActiveEditor == nullptr || findText == nullptr || replaceText == nullptr ||
        strlen(findText) == 0)
    {
        return false;
    }
    EDITOR_STATE *state = s_ActiveEditor;
    // a selection equal to the needle is the "match at the cursor": replace it
    if (state->has_selection())
    {
        int line0, col0, line1, col1;
        state->selection(line0, col0, line1, col1);
        const astr_t selected = state->LineBuffer.at(line0).substr(col0, col1 - col0);
        const astr_t needle(findText);
        bool equal = selected.size() == needle.size();
        for (size_t i = 0; equal && i < needle.size(); i++)
        {
            equal = caseSensitive ? selected[i] == needle[i]
                                  : tolower((unsigned char)selected[i]) ==
                        tolower((unsigned char)needle[i]);
        }
        if (equal)
        {
            state->delete_selection();
            state->insert_text(replaceText);
        }
    }
    return TextEditor_Find(findText, true, caseSensitive);
}

bool TextEditor_ReplaceAll(const char *findText, const char *replaceText, bool caseSensitive)
{
    if (s_ActiveEditor == nullptr || findText == nullptr || replaceText == nullptr ||
        strlen(findText) == 0)
    {
        return false;
    }
    EDITOR_STATE *state = s_ActiveEditor;
    int count = 0;
    state->CursorLine = 0;
    state->CursorCol = 0;
    state->AnchorLine = -1;
    while (find_in_state(state, findText, true, caseSensitive))
    {
        int line0, col0, line1, col1;
        state->selection(line0, col0, line1, col1);
        astr_t &line = state->LineBuffer.at(line0);
        line.replace(col0, col1 - col0, replaceText);
        state->CursorCol = col0 + (int)strlen(replaceText);
        state->AnchorLine = -1;
        state->Modified = true;
        count++;
        if (count > 100000)
        {
            break; // safety against an empty-replacement infinite loop
        }
    }
    state->EditEpoch++; // the lines changed without snapshots
    return count > 0;
}

void TextEditor_InsertAtCursor(const astr_t &text)
{
    if (s_ActiveEditor != nullptr && !text.empty())
    {
        s_ActiveEditor->insert_text(text);
    }
}

int TextEditor_CurrentLine()
{
    return s_ActiveEditor != nullptr ? s_ActiveEditor->CursorLine + 1 : 1;
}

bool TextEditor_CommentSelection()
{
    if (s_ActiveEditor == nullptr)
    {
        return false;
    }
    EDITOR_STATE *state = s_ActiveEditor;
    int line0, col0, line1, col1;
    if (state->has_selection())
    {
        state->selection(line0, col0, line1, col1);
    }
    else
    {
        line0 = line1 = state->CursorLine;
    }
    state->snapshot();
    for (int line = line0; line <= line1; line++)
    {
        astr_t &text = state->LineBuffer.at(line);
        const size_t first = text.find_first_not_of(" \t");
        if (first != astr_t::npos && text.compare(first, 2, "//") == 0)
        {
            text.erase(first, 2); // uncomment
        }
        else
        {
            text.insert(text.begin(), (char)'/');
            text.insert(text.begin(), (char)'/'); // comment out
        }
    }
    state->Modified = true;
    state->AnchorLine = -1;
    return true;
}

} // namespace xuoassist
