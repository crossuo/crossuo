#include "imgui.h"
#include "imgui_internal.h"

void ImGui::SetRichTextElementCallbacks(ImRichTextElementCalcSizeCallback calcSizeCb, ImRichTextElementDrawCallback drawCb)
{
    ImGuiContext& g = *GImGui;
    g.RichTextCalcSizeCallback = calcSizeCb;
    g.RichTextDrawCallback = drawCb;
}

void ImGui::EnableRichText()
{
    ImGuiContext& g = *GImGui;
    g.UseRichText = g.RichTextCalcSizeCallback && g.RichTextDrawCallback;
}

void ImGui::DisableRichText()
{
    ImGuiContext& g = *GImGui;
    g.UseRichText = false;
}

static const char* FindTextSegmentEnd(const char* text, const char* text_end)
{
    const char* text_display_end = text;
    if (!text_end)
        text_end = (const char*)-1;

    while (text_display_end < text_end && *text_display_end != '\0' && text_display_end[0] != '{')
        text_display_end++;
    return text_display_end;
}

static const char* FindRichTextElementEnd(const char* text, const char* text_end)
{
    const char* text_display_end = text;
    if (!text_end)
        text_end = (const char*)-1;

    while (text_display_end < text_end && *text_display_end != '\0' && text_display_end[0] != '}')
        text_display_end++;
    return text_display_end;
}

void ImGui::RenderRichTextClipped(const ImVec2& pos_min, const ImVec2& pos_max, const char* text, const char* text_end, const ImVec2* text_size_if_known, const ImVec2& align/*, const ImRect* clip_rect*/)
{
    //RenderTextClipped(pos_min, pos_max, text, text_end, text_size_if_known, align, clip_rect);
    ImGuiContext& g = *GImGui;
    ImFont* font = g.Font;
    const float font_size = g.FontSize;
    ImGuiWindow* window = g.CurrentViewport->Window;

    ImVec2 pos = pos_min;
    const char *cur_beg = text;
    const char *cur_end = text_end;
    do {
        if (cur_beg[0] != '{')
        {
            cur_end = FindTextSegmentEnd(cur_beg, cur_end);
            if (cur_end != cur_beg)
            {
                RenderTextClippedEx(window->DrawList, pos, pos_max, cur_beg, cur_end, text_size_if_known, align, NULL);//clip_rect);
                ImVec2 segment_size = font->CalcTextSizeA(font_size, FLT_MAX, pos_max.x, cur_beg, cur_end, NULL);
                pos.x += segment_size.x;
            }
        }
        else
        {
            cur_end = FindRichTextElementEnd(cur_beg, cur_end);
            IM_ASSERT(*cur_end == '}' && "Malformed rich text tag.");
            ++cur_end; // skip last '}'
            if (cur_end != cur_beg)
            {
                ImVec2 oldPos = window->DC.CursorPos;
                window->DC.CursorPos = pos;
                ImVec2 r = g.RichTextDrawCallback(pos, window->DrawList, cur_beg + 1, cur_end - 1);
                pos.x += r.x;
                pos.y += r.y;
                window->DC.CursorPos = oldPos;
            }
        }
        cur_beg = cur_end;
        cur_end = text_end;
    } while (cur_beg < text_end);


    if (g.LogEnabled)
        LogRenderedText(&pos_min, text, text_end);  // No rich text in logging
}

ImVec2 ImGui::CalcRichTextSize(const char* text, const char* text_display_end, bool hide_text_after_double_hash, float wrap_width)
{
    // CalcTextSize
    ImGuiContext& g = *GImGui;
    ImFont* font = g.Font;
    const float font_size = g.FontSize;
    ImVec2 text_size = ImVec2(0.0f, font_size);
    if (text == text_display_end)
        return text_size;

    ImVec2 elem_size = ImVec2(0.0f, 0.0f);
    const char *cur_beg = text;
    const char *cur_end = text_display_end;
    do {
        if (cur_beg[0] != '{')
        {
            cur_end = FindTextSegmentEnd(cur_beg, cur_end);
            if (cur_end != cur_beg)
            {
                ImVec2 segment_size = font->CalcTextSizeA(font_size, FLT_MAX, wrap_width, cur_beg, cur_end, NULL);
                text_size.x += segment_size.x;
                text_size.y = ImMax(text_size.y, segment_size.y);
            }
        }
        else
        {
            cur_end = FindRichTextElementEnd(cur_beg, cur_end);
            IM_ASSERT(*cur_end == '}' && "Malformed rich text tag.");
            ++cur_end; // skip last '}'
            if (cur_end != cur_beg)
            {
                elem_size.x += g.RichTextCalcSizeCallback(cur_beg + 1, cur_end - 1).x;
            }
        }
        cur_beg = cur_end;
        cur_end = text_display_end;
    } while (cur_beg < text_display_end);

    // Cancel out character spacing for the last character of a line (it is baked into glyph->AdvanceX field)
    const float font_scale = font_size / font->FontSize;
    const float character_spacing_x = 1.0f * font_scale;
    if (text_size.x > 0.0f)
        text_size.x -= character_spacing_x;
    text_size.x = (float)(int)(text_size.x + 0.95f) + elem_size.x;
    text_size.y += elem_size.y;

    return text_size;
}
