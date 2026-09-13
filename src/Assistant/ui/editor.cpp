// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The editor implementation. Palette mapping (the original CSyntaxHighlighter rules):
//   darkMagenta 0xAA00AA  - the `.Method` names
//   darkBlue   0x000080B  - keywords (Client/Player/if/for/...)
//   darkYellow 0x808000   - the sentinel globals (self/lasttarget/...)
//   blue       0x0000FF   - decimal + 0x hex numbers
//   darkGreen  0x008000   - "strings" / 'strings'
//   gray       0xA0A0A4   - // comments

#include "editor.h"
#include "../script/scripthandle.h" // ScriptEditor_Generation (buffer token)

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace xuoassist {

namespace {

int editor_clamp(int value, int lo, int hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

enum TOKEN_COLOR
{
    TC_DEFAULT = 0,
    TC_METHOD,
    TC_KEYWORD,
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
            return IM_COL32(0x00, 0x00, 0x80, 0xFF); // darkBlue (bold kept visually)
        case TC_GLOBAL:
            return IM_COL32(0x80, 0x80, 0x00, 0xFF); // darkYellow
        case TC_NUMBER:
            return IM_COL32(0x00, 0x00, 0xFF, 0xFF); // blue
        case TC_STRING:
            return IM_COL32(0x00, 0x80, 0x00, 0xFF); // darkGreen
        case TC_COMMENT:
            return IM_COL32(0xA0, 0xA0, 0xA4, 0xFF); // gray
        default:
            return 0;
    }
}

bool is_keyword(const char *text, size_t length)
{
    static const char *const KEYWORDS[] = { "Client", "if", "else", "for", "var", "TextWindow",
        "while", "Player", "function", "return", "break", "switch", "case", "continue",
        "SelectedTile", "true", "false", "null", "undefined" };
    for (const char *keyword : KEYWORDS)
    {
        if (strlen(keyword) == length && strncmp(keyword, text, length) == 0)
        {
            return true;
        }
    }
    return false;
}

bool is_global_word(const char *text, size_t length)
{
    static const char *const GLOBALS[] = { "self", "lastcorpse", "lasttarget", "lasttile",
        "backpack", "ground", "lastattack", "lastcontainer", "nullptr", "laststatus",
        "lastobject", "ScriptName" };
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

// The per-line tokenizer (the original regex rules, applied left to right).
void tokenize_line(const string &line, vector<TOKEN_RUN> &runs)
{
    runs.clear();
    const size_t length = line.size();
    size_t i = 0;
    while (i < length)
    {
        const char c = line[i];

        // // comment to EOL
        if (c == '/' && i + 1 < length && line[i + 1] == '/')
        {
            runs.push_back({ (int)i, (int)(length - i), TC_COMMENT });
            break;
        }

        // strings (minimal match: no spanning quotes - the original patterns were
        // non-greedy ".*")
        if (c == '"' || c == '\'')
        {
            size_t end = i + 1;
            while (end < length && line[end] != c)
            {
                end++;
            }
            if (end < length)
            {
                end++;
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

        // decimal numbers
        if (isdigit((unsigned char)c))
        {
            size_t end = i;
            while (end < length && isdigit((unsigned char)line[end]))
            {
                end++;
            }
            runs.push_back({ (int)i, (int)(end - i), TC_NUMBER });
            i = end;
            continue;
        }

        // identifiers: `.Method` magenta, keywords/global words otherwise
        if (isalpha((unsigned char)c) || c == '_' || c == '.')
        {
            const bool dot = c == '.';
            size_t end = i + 1;
            while (end < length &&
                   (isalnum((unsigned char)line[end]) || line[end] == '_' || line[end] == '.'))
            {
                end++;
            }
            size_t wordStart = dot ? i + 1 : i;
            if (dot && end == wordStart)
            {
                i = end;
                continue;
            }
            const size_t wordLength = end - wordStart;
            if (dot)
            {
                runs.push_back({ (int)wordStart, (int)wordLength, TC_METHOD });
            }
            else if (is_global_word(line.c_str() + i, wordLength))
            {
                runs.push_back({ (int)i, (int)wordLength, TC_GLOBAL });
            }
            else if (is_keyword(line.c_str() + i, wordLength))
            {
                runs.push_back({ (int)i, (int)wordLength, TC_KEYWORD });
            }
            i = end;
            continue;
        }

        i++;
    }
}

struct EDITOR_STATE
{
    string *Buffer{ nullptr };
    uint BufferGeneration{ 0 }; // the shared-buffer token (stale-resplit fix)
    vector<string> LineBuffer;
    int CursorLine{ 0 };
    int CursorCol{ 0 };
    int AnchorLine{ -1 }; // selection anchor (-1 = none)
    int AnchorCol{ 0 };
    int TopLine{ 0 };
    bool Modified{ false };

    struct CACHE_ENTRY
    {
        size_t hash{ 0 };
        vector<TOKEN_RUN> runs;
    };
    std::unordered_map<int, CACHE_ENTRY> Cache;
    uint LastUndoTick{ 0 };

    const vector<TOKEN_RUN> &runs_for(int lineIndex, const string &line)
    {
        const size_t hash = std::hash<string>{}(line);
        auto it = Cache.find(lineIndex);
        if (it != Cache.end() && it->second.hash == hash)
        {
            return it->second.runs;
        }
        CACHE_ENTRY &entry = Cache[lineIndex];
        entry.hash = hash;
        tokenize_line(line, entry.runs);
        // keep the cache bounded when scrolling long files
        if (Cache.size() > 4096)
        {
            Cache.clear();
        }
        return entry.runs;
    }

    void split_buffer(const string &text)
    {
        // (re)split the owning string buffer into lines
        LineBuffer.clear();
        size_t start = 0;
        while (true)
        {
            size_t end = text.find('\n', start);
            if (end == string::npos)
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
    }

    string join_buffer() const
    {
        string text;
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

    // --- undo (simple snapshots; the original undo stack was the text edit's) ---
    struct UNDO_STATE
    {
        vector<string> lines;
        int line;
        int col;
        size_t bytes; // approximate memory of this snapshot
    };
    // total snapshot memory budget: past it the OLDEST snapshot is dropped
    // (an unbounded undo stack on a large script could grow unbounded)
    static constexpr size_t UNDO_BUDGET_BYTES = 8 * 1024 * 1024;
    vector<UNDO_STATE> Undo;
    size_t UndoBytes{ 0 };
    bool InUndo{ false };

    void snapshot()
    {
        if (InUndo)
        {
            return;
        }
        size_t bytes = 0;
        for (const string &line : LineBuffer)
        {
            bytes += line.size() + 1;
        }
        Undo.push_back({ LineBuffer, CursorLine, CursorCol, bytes });
        UndoBytes += bytes;
        while (UndoBytes > UNDO_BUDGET_BYTES && Undo.size() > 1)
        {
            UndoBytes -= Undo.front().bytes;
            Undo.erase(Undo.begin());
        }
    }

    void undo()
    {
        if (Undo.empty())
        {
            return;
        }
        InUndo = true;
        UNDO_STATE &state = Undo.back();
        LineBuffer = state.lines;
        CursorLine = std::min(state.line, (int)Buffer->size() - 1);
        CursorCol = std::min(state.col, (int)LineBuffer.at(CursorLine).size());
        Modified = true;
        UndoBytes -= state.bytes;
        Undo.pop_back();
        InUndo = false;
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
            string &line = LineBuffer.at(line0);
            line.erase(col0, col1 - col0);
        }
        else
        {
            string &first = LineBuffer.at(line0);
            string &last = LineBuffer.at(line1);
            first.erase(col0);
            first += last.substr(col1);
            Buffer->erase(Buffer->begin() + line0 + 1, Buffer->begin() + line1 + 1);
        }
        CursorLine = line0;
        CursorCol = col0;
        AnchorLine = -1;
        Modified = true;
    }

    void insert_text(const string &text)
    {
        snapshot();
        string &line = LineBuffer.at(CursorLine);
        line.insert((size_t)CursorCol, text);
        CursorCol += (int)text.size();
        Modified = true;
    }
};

EDITOR_STATE *editor_state(ImGuiID id, string *buffer)
{
    static std::unordered_map<ImGuiID, EDITOR_STATE> states;
    EDITOR_STATE &state = states[id];
    // Re-split on the buffer POINTER changing (a different owning string) or
    // on the shared-buffer GENERATION moving (Open/New/Reload/LoadScript
    // replace the contents of the same string object - the pointer never
    // changes, so without the token the widget kept drawing stale lines and
    // an edit rejoined the old content over the new file).
    const uint generation = ScriptEditor_Generation();
    if (state.Buffer != buffer || state.BufferGeneration != generation)
    {
        state.Buffer = buffer;
        state.BufferGeneration = generation;
        state.split_buffer(*buffer);
        state.CursorLine = 0;
        state.CursorCol = 0;
    }
    return &state;
}

EDITOR_STATE *s_ActiveEditor = nullptr;

} // namespace

bool TextEditor_Draw(
    const char *id, string *buffer, float height, int gotoLine, bool *gotoPending)
{
    ImGuiID imId = ImGui::GetID(id);
    EDITOR_STATE *state = editor_state(imId, buffer);
    s_ActiveEditor = state;
    const vector<string> &lines = state->LineBuffer;

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

    // the editor is a bordered child region; scrolling is ours (the original
    // NoWrap + vertical scrollbar behavior)
    ImGui::BeginChild(id, size, ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_NoScrollbar);
    const float contentHeight = (float)lines.size() * lineHeight;
    ImGui::SetCursorPosY(-(float)state->TopLine * lineHeight); // no-op safety
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
        const string &line = lines[index];
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
                    IM_COL32(80, 120, 200, 60));
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
                const string plain = line.substr(pos, run.start - pos);
                draw->AddText(font, ImGui::GetFontSize(), ImVec2(x, y), IM_COL32(220, 220, 220, 255),
                    plain.c_str(), plain.c_str() + plain.size());
                x += ImGui::CalcTextSize(plain.c_str()).x;
            }
            const string piece = line.substr(run.start, run.length);
            draw->AddText(font, ImGui::GetFontSize(), ImVec2(x, y), token_color(run.color),
                piece.c_str(), piece.c_str() + piece.size());
            x += ImGui::CalcTextSize(piece.c_str()).x;
            pos = run.start + run.length;
        }
        if (pos < line.size())
        {
            const string plain = line.substr(pos);
            draw->AddText(font, ImGui::GetFontSize(), ImVec2(x, y), IM_COL32(220, 220, 220, 255),
                plain.c_str(), plain.c_str() + plain.size());
        }
    }

    // hit target + keyboard focus
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(gutterWidth + 4 + (float)lines[std::min(state->CursorLine, (int)lines.size() - 1)].size() * charWidth,
        (float)lines.size() * lineHeight));
    ImGui::InvisibleButton("area", ImVec2(ImGui::GetWindowWidth(), contentHeight),
        ImGuiButtonFlags_MouseButtonLeft);
    const bool active = ImGui::IsItemActive();
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
    {
        const ImVec2 clicked = ImGui::GetIO().MousePos;
        state->CursorLine = firstLine + (int)((clicked.y - origin.y) / lineHeight);
        state->CursorLine = editor_clamp(state->CursorLine, 0, (int)lines.size() - 1);
        state->CursorCol = editor_clamp((int)((clicked.x - origin.x - gutterWidth - 4) / charWidth), 0,
            (int)lines[state->CursorLine].size());
        if (!ImGui::GetIO().KeyShift)
        {
            state->AnchorLine = -1;
        }
    }
    if (active && ImGui::IsMouseDragging(0))
    {
        state->AnchorLine = state->CursorLine; // extend-from-click-start baseline
        const ImVec2 drag = ImGui::GetIO().MousePos;
        int line = firstLine + (int)((drag.y - origin.y) / lineHeight);
        state->CursorLine = editor_clamp(line, 0, (int)lines.size() - 1);
        state->CursorCol = editor_clamp((int)((drag.x - origin.x - gutterWidth - 4) / charWidth), 0,
            (int)lines[state->CursorLine].size());
        if (state->AnchorLine < 0)
        {
            state->AnchorLine = state->CursorLine;
        }
    }

    bool enterPressed = false;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        ImGuiIO &io = ImGui::GetIO();
        // key handling only while this editor child is focused
        if (ImGui::IsWindowFocused())
        {
            auto clampCursor = [&]() {
                state->CursorLine = editor_clamp(state->CursorLine, 0, (int)lines.size() - 1);
                state->CursorCol = editor_clamp(
                    state->CursorCol, 0, (int)lines[state->CursorLine].size());
            };

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
            {
                state->CursorLine = std::max(0, state->CursorLine - 1);
                clampCursor();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            {
                state->CursorLine = std::min((int)lines.size() - 1, state->CursorLine + 1);
                clampCursor();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            {
                if (state->CursorCol > 0)
                {
                    state->CursorCol--;
                }
                else if (state->CursorLine > 0)
                {
                    state->CursorLine--;
                    state->CursorCol = (int)lines[state->CursorLine].size();
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            {
                if (state->CursorCol < (int)lines[state->CursorLine].size())
                {
                    state->CursorCol++;
                }
                else if (state->CursorLine < (int)lines.size() - 1)
                {
                    state->CursorLine++;
                    state->CursorCol = 0;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Home))
            {
                state->CursorCol = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_End))
            {
                state->CursorCol = (int)lines[state->CursorLine].size();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            {
                state->CursorLine = std::max(0, state->CursorLine - 20);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            {
                state->CursorLine = std::min((int)lines.size() - 1, state->CursorLine + 20);
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
                    string &line = state->LineBuffer.at(state->CursorLine);
                    line.erase(state->CursorCol - 1, 1);
                    state->CursorCol--;
                    state->Modified = true;
                }
                else if (state->CursorLine > 0)
                {
                    state->snapshot();
                    const string tail = lines[state->CursorLine];
                    state->CursorLine--;
                    state->CursorCol = (int)lines[state->CursorLine].size();
                    state->LineBuffer.at(state->CursorLine) += tail;
                    state->LineBuffer.erase(state->LineBuffer.begin() + state->CursorLine + 1);
                    state->Modified = true;
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
                    string &line = state->LineBuffer.at(state->CursorLine);
                    line.erase(state->CursorCol, 1);
                    state->Modified = true;
                }
                else if (state->CursorLine < (int)lines.size() - 1)
                {
                    state->snapshot();
                    string &line = state->LineBuffer.at(state->CursorLine);
                    line += lines[state->CursorLine + 1];
                    state->LineBuffer.erase(state->LineBuffer.begin() + state->CursorLine + 1);
                    state->Modified = true;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                state->snapshot();
                string &line = state->LineBuffer.at(state->CursorLine);
                const string tail = line.substr(state->CursorCol);
                line.erase(state->CursorCol);
                state->LineBuffer.insert(state->LineBuffer.begin() + state->CursorLine + 1, tail);
                state->CursorLine++;
                state->CursorCol = 0;
                state->Modified = true;
                enterPressed = true;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Tab))
            {
                state->insert_text("    ");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl)
            {
                state->undo();
            }
            if (io.InputQueueCharacters.Size > 0 && !io.KeyCtrl && !io.KeySuper)
            {
                for (int i = 0; i < io.InputQueueCharacters.Size; i++)
                {
                    const ImWchar c = io.InputQueueCharacters[i];
                    if (c >= 32)
                    {
                        string text;
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
                            text += (char)(0x80 | (c & 0x3F));
                        }
                        state->insert_text(text);
                    }
                }
                io.InputQueueCharacters.resize(0);
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

    if (state->Modified)
    {
        state->Modified = false;
        *buffer = state->join_buffer();
        return true;
    }
    (void)enterPressed;
    return false;
}

// --- text operations (textoperationdialog round-trips) -----------------------

namespace {

int find_in_state(EDITOR_STATE *state, const string &needle, bool down, bool caseSensitive)
{
    // case-sensitive search like the original dialog default; down = forward
    const vector<string> &lines = state->LineBuffer;
    int startLine = state->CursorLine;
    int startCol = state->CursorCol + (down ? 1 : 0);

    auto match = [&](const string &text, size_t pos) {
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
            const string &text = lines[line];
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
            const string &text = lines[line];
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
        const string selected = state->LineBuffer.at(line0).substr(col0, col1 - col0);
        const string needle(findText);
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
        string &line = state->LineBuffer.at(line0);
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
    return count > 0;
}

void TextEditor_InsertAtCursor(const string &text)
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
        string &text = state->LineBuffer.at(line);
        const size_t first = text.find_first_not_of(" \t");
        if (first != string::npos && text.compare(first, 2, "//") == 0)
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
