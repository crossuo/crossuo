// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Assistant ImGui widget kit implementation (master plan §6). See widgets.h
// for the per-widget Qt mapping and behaviors.

#define LOGGER_MODULE Assistant

#include "widgets.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>
#include <external/gfx/imgui/imgui_internal.h> // RegisterUserTexture

#include <common/fs.h>
#include <common/logging/logging.h>
#include <xuocore/uodata.h> // ExeFilePath

#include "GameObjects/GameWorld.h" // client g_World (status bar census, like Phase 0)

#include <ctype.h>
#include <stdio.h>
#include <string.h>

// Vendored single-header PNG decoder (public domain). Assistant-local per the
// master plan §6 "Icon atlas": stb_image is not part of external/.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace xuoassist {

// ---------------------------------------------------------------------------
// icon atlas
// ---------------------------------------------------------------------------

struct XA_ICON_FILE
{
    XA_ICON Icon;
    const char *File;
};

static const XA_ICON_FILE s_IconFiles[XA_ICON_COUNT] = {
    { XA_ICON_ADD, "button_add.png" },
    { XA_ICON_SAVE, "button_save.png" },
    { XA_ICON_REMOVE, "button_remove.png" },
    { XA_ICON_TARGET, "button_from_target.png" },
    { XA_ICON_TRAY, "tray.png" },
    { XA_ICON_HOTKEY, "hotkey.png" },
    { XA_ICON_MAP, "map_icon.png" },
    { XA_ICON_MACRO_DEFAULT, "macro_default.png" },
    { XA_ICON_MACRO_RECORD, "macro_record.png" },
    { XA_ICON_MACRO_START, "macro_start.png" },
    { XA_ICON_MACRO_STOP, "macro_stop.png" },
    { XA_ICON_MACRO_PAUSE, "macro_pause.png" },
    { XA_ICON_MACRO_RESUME, "macro_resume.png" },
    { XA_ICON_MACRO_EDIT, "macro_edit.png" },
    { XA_ICON_CORPSE, "corpse.png" },
};

static ImTextureData *s_Atlas = nullptr;
static ImVec2 s_UV0[XA_ICON_COUNT];
static ImVec2 s_UV1[XA_ICON_COUNT];

static fs_path icon_asset_path(const char *file)
{
    // dev builds run from the binary dir (assets copied there by CMake);
    // installs keep them next to the exe
    const fs_path relative = fs_path_join("assets", "xuoassist");
    if (fs_path_exists(fs_path_join(relative, file)))
    {
        return fs_path_join(relative, file);
    }
    return fs_path_join(ExeFilePath("assets/xuoassist"), file);
}

bool Icons_Init()
{
    if (s_Atlas != nullptr)
    {
        return true;
    }
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return false;
    }

    // the assets are 128x128: a real 2x2 box downsample (four source pixels
    // averaged per atlas cell) keeps the icons free of point-sample aliasing
    const int cell = 32;
    const int srcStep = 4; // 128 / 32
    const int cols = 8;
    const int rows = (XA_ICON_COUNT + cols - 1) / cols;
    const int width = cols * cell;
    const int height = rows * cell;

    ImTextureData *tex = new ImTextureData();
    tex->Create(ImTextureFormat_RGBA32, width, height);
    tex->SetStatus(ImTextureStatus_WantCreate);
    tex->UseColors = true;
    memset(tex->GetPixels(), 0, (size_t)tex->GetSizeInBytes());
    ImGui::RegisterUserTexture(tex);

    for (int icon = 0; icon < XA_ICON_COUNT; icon++)
    {
        const int cx = (icon % cols) * cell;
        const int cy = (icon / cols) * cell;

        int w = 0, h = 0, comp = 0;
        const fs_path path = icon_asset_path(s_IconFiles[icon].File);
        stbi_uc *data = stbi_load(fs_path_ascii(path), &w, &h, &comp, 4);
        if (data == nullptr)
        {
            Warning(Assistant, "icon asset not found: %s", fs_path_ascii(path));
        }
        else
        {
            const int copyW = w < cell * srcStep ? w : cell * srcStep;
            const int copyH = h < cell * srcStep ? h : cell * srcStep;
            for (int y = 0; y < cell; y++)
            {
                const int sy = y * srcStep < copyH ? y * srcStep : copyH - 1;
                for (int x = 0; x < cell; x++)
                {
                    const int sx = x * srcStep < copyW ? x * srcStep : copyW - 1;
                    // 2x2 box filter: average the four source pixels
                    uint sum[4] = { 0, 0, 0, 0 };
                    for (int by = 0; by < 2; by++)
                    {
                        const int py = sy + by < copyH ? sy + by : copyH - 1;
                        for (int bx = 0; bx < 2; bx++)
                        {
                            const int px = sx + bx < copyW ? sx + bx : copyW - 1;
                            const stbi_uc *src = data + ((py * w) + px) * 4;
                            for (int c = 0; c < 4; c++)
                            {
                                sum[c] += src[c];
                            }
                        }
                    }
                    stbi_uc *dst = (stbi_uc *)tex->GetPixelsAt(cx + x, cy + y);
                    dst[0] = (stbi_uc)(sum[0] / 4);
                    dst[1] = (stbi_uc)(sum[1] / 4);
                    dst[2] = (stbi_uc)(sum[2] / 4);
                    dst[3] = (stbi_uc)(sum[3] / 4);
                }
            }
            stbi_image_free(data);
        }

        const float fw = (float)width;
        const float fh = (float)height;
        s_UV0[icon] = ImVec2(cx / fw, cy / fh);
        s_UV1[icon] = ImVec2((cx + cell) / fw, (cy + cell) / fh);
    }

    s_Atlas = tex;
    Info(Assistant, "icon atlas ready (%dx%d)", width, height);
    return true;
}

void Icons_Shutdown()
{
    if (s_Atlas == nullptr)
    {
        return;
    }
    // ImGui 1.92.9b texture contract: the backend releases the GL name during
    // ImGui_ImplOpenGL3_Shutdown -> DestroyDeviceObjects, which walks
    // PlatformIO.Textures and destroys every texture whose RefCount is still
    // 1 (imgui_impl_opengl3.cpp:1023-1031). The atlas must therefore stay
    // registered (RefCount == 1) until ui_shutdown/DestroyContext is done:
    // unregistering + deleting the ImTextureData earlier leaves a dangling
    // pointer in PlatformIO.Textures (UAF) and, with RefCount already 0, the
    // backend skips glDeleteTextures (GL leak in the shared game context,
    // cumulative per logout/login). This frees the CPU-side pixels only and
    // must be called AFTER the ImGui context is gone (AssistantWindow).
    delete s_Atlas;
    s_Atlas = nullptr;
}

static ImTextureRef atlas_texture()
{
    return s_Atlas != nullptr ? s_Atlas->GetTexRef() : ImTextureRef();
}

// --- IconButton (QImagedPushButton) -----------------------------------------

bool IconButton(XA_ICON icon, const char *tip, bool enabled, float size, const char *idSeed)
{
    if ((s_Atlas == nullptr && !Icons_Init()) || ImGui::GetCurrentContext() == nullptr)
    {
        return false;
    }
    if (size <= 0.0f)
    {
        size = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    }

    const float shift = size >= 20.0f ? 2.0f : 1.0f; // the stylesheet's 2px press shift
    char id[96];
    snprintf(id, sizeof(id), "##iconbtn_%d_%s", (int)icon, idSeed != nullptr ? idSeed : "");

    if (!enabled)
    {
        ImGui::BeginDisabled();
    }
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
    if (!enabled)
    {
        ImGui::EndDisabled();
    }

    const bool hovered = ImGui::IsItemHovered() && enabled;
    const bool active = ImGui::IsItemActive() && enabled;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetItemRectMin();
    const ImVec2 end = ImGui::GetItemRectMax();

    // QImagedPushButton stylesheet: hover rgb(192,192,192), pressed rgb(160,160,160)
    ImU32 bg = 0;
    if (active)
    {
        bg = IM_COL32(160, 160, 160, 255);
    }
    else if (hovered)
    {
        bg = IM_COL32(192, 192, 192, 255);
    }
    if (bg != 0)
    {
        dl->AddRectFilled(pos, end, bg);
    }

    const float ofs = active ? shift : 0.0f;
    const ImU32 tint = enabled ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 255, 255, 110);
    dl->AddImage(
        atlas_texture(),
        ImVec2(pos.x + ofs, pos.y + ofs),
        ImVec2(end.x - shift + ofs, end.y - shift + ofs),
        s_UV0[icon],
        s_UV1[icon],
        tint);

    if (tip != nullptr && tip[0] != '\0' && hovered)
    {
        ImGui::SetTooltip("%s", tip);
    }
    return pressed;
}

// --- color helpers ----------------------------------------------------------

ImU32 UOColorToRGBA(ushort color)
{
    // 15-bit (5-5-5) UO color, channels scaled x8 like the common conversions
    const uint r = ((color >> 10) & 0x1F) * 8;
    const uint g = ((color >> 5) & 0x1F) * 8;
    const uint b = (color & 0x1F) * 8;
    return IM_COL32(r, g, b, 255);
}

void HueSwatch(ushort uoColor, float size)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), UOColorToRGBA(uoColor));
    dl->AddRect(pos, ImVec2(pos.x + size, pos.y + size), IM_COL32(0, 0, 0, 200));
    ImGui::Dummy(ImVec2(size, size));
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("0x%04X", uoColor);
    }
}

bool ColorSwatch(const char *id, ImU32 *rgba, float size)
{
    ImVec4 col = ImGui::ColorConvertU32ToFloat4(*rgba);
    char label[48];
    snprintf(label, sizeof(label), "##swatch_%s", id);
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    bool changed = ImGui::Button(label, ImVec2(size, size));
    ImGui::PopStyleColor(3);
    if (ImGui::BeginPopupContextItem(label))
    {
        changed = ImGui::ColorPicker3(label, &col.x) || changed;
        ImGui::EndPopup();
    }
    if (changed)
    {
        *rgba = ImGui::ColorConvertFloat4ToU32(col);
    }
    return changed;
}

// --- hex edit fields (the Qt le_* "0x..." line edits) ------------------------

// ImGuiInputTextFlags_CharsHexadecimal rejects the 'x' of the stored "0x..."
// prefix (typing would drop it and the saved text would diverge from the Qt
// format), so the stored value keeps its prefix and the edit buffer carries
// the bare digits only: they are seeded while the field is idle and the value
// is re-prefixed on commit. While the field is active ImGui gives its own
// in-widget state priority over the user buffer, so the re-seed never
// clobbers an ongoing edit.
static bool hex_edit_field_impl(const char *id, char buf[], size_t bufSize, const char *value, float width)
{
    const char *src = value != nullptr ? value : "";
    if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X'))
    {
        src += 2; // edit start: strip the prefix into the edit buffer
    }
    snprintf(buf, bufSize, "%s", src);

    if (width > 0.0f)
    {
        ImGui::SetNextItemWidth(width);
    }
    ImGui::InputText(
        id, buf, (int)bufSize, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_AutoSelectAll);
    if (!ImGui::IsItemDeactivatedAfterEdit())
    {
        return false;
    }
    // commit: re-prefix (an emptied field stays empty so the "is set" checks
    // keep working); the callers parse the text afterwards as before
    if (buf[0] != '\0')
    {
        char prefixed[40] = { 0 };
        snprintf(prefixed, sizeof(prefixed), "0x%s", buf);
        snprintf(buf, bufSize, "%s", prefixed);
    }
    return true;
}

bool HexEditField(const char *id, char *value, size_t valueSize, float width)
{
    char buf[24] = { 0 };
    const bool changed = hex_edit_field_impl(id, buf, sizeof(buf), value, width);
    if (changed)
    {
        snprintf(value, valueSize, "%s", buf);
    }
    return changed;
}

bool HexEditField(const char *id, string *value, float width)
{
    char buf[24] = { 0 };
    const bool changed = hex_edit_field_impl(id, buf, sizeof(buf), value->c_str(), width);
    if (changed)
    {
        *value = buf;
    }
    return changed;
}

// --- ComboGroup (CComboDelegate) ---------------------------------------------

bool ComboGroup(const char *id, int *current, const char *const items[], int count)
{
    const char *preview = "";
    if (*current >= 0 && *current < count)
    {
        preview = items[*current];
        if (preview[0] == '#')
        {
            preview++; // group markers are internal, never shown in the box
        }
    }
    bool changed = false;
    if (!ImGui::BeginCombo(id, preview))
    {
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        const bool group = items[i][0] == '#';
        if (group)
        {
            // CComboDelegate: lightGray background, darkGray text; the row is
            // a separator and cannot be selected (Selectable(false) semantics)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.29f, 0.29f, 0.29f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.777f, 0.777f, 0.777f, 0.5f));
            ImGui::Selectable(items[i], false, ImGuiSelectableFlags_Disabled);
            ImGui::PopStyleColor(2);
            continue;
        }
        if (ImGui::Selectable(items[i], *current == i))
        {
            *current = i;
            changed = true;
        }
        if (*current == i)
        {
            ImGui::SetItemDefaultFocus();
        }
    }
    ImGui::EndCombo();
    return changed;
}

// --- ListButtons -------------------------------------------------------------

int ListButtons(const char *idSeed)
{
    int result = XALB_NONE;
    if (IconButton(XA_ICON_ADD, "Create", true, 0.0f, idSeed))
    {
        result |= XALB_ADD;
    }
    ImGui::SameLine();
    if (IconButton(XA_ICON_SAVE, "Save", true, 0.0f, idSeed))
    {
        result |= XALB_SAVE;
    }
    ImGui::SameLine();
    if (IconButton(XA_ICON_REMOVE, "Remove", true, 0.0f, idSeed))
    {
        result |= XALB_REMOVE;
    }
    ImGui::SameLine();
    // phase 3: the target pipeline exists, the capture flow does not yet
    ImGui::BeginDisabled();
    IconButton(XA_ICON_TARGET, "Select from target (available with the hotkey engine, phase 3)", false, 0.0f, idSeed);
    ImGui::EndDisabled();
    return result;
}

// --- shared list internals ----------------------------------------------------

static char s_RowLabel[512];
static XA_LIST_RENAME s_Rename; // one row edits at a time (fixed buffer)
static bool s_RenameActive = false;

// Per-list structural generations: the rename state stores the generation of
// its owning list at Begin; add/remove/move bumps it so a rename that somehow
// outlived its container (tab switch + reselect) is cancelled instead of
// committing its stale buffer into a shifted or foreign row.
struct XA_LIST_GENERATION
{
    ImGuiID Id;      // 0 = free slot
    int Count;
    uint Generation;
};
static const int XA_MAX_TRACKED_LISTS = 16;
static XA_LIST_GENERATION s_ListGenerations[XA_MAX_TRACKED_LISTS] = {};

static XA_LIST_GENERATION *list_generation_slot(ImGuiID id)
{
    for (int i = 0; i < XA_MAX_TRACKED_LISTS; i++)
    {
        if (s_ListGenerations[i].Id == id)
        {
            return &s_ListGenerations[i];
        }
    }
    for (int i = 0; i < XA_MAX_TRACKED_LISTS; i++)
    {
        if (s_ListGenerations[i].Id == 0)
        {
            s_ListGenerations[i].Id = id;
            return &s_ListGenerations[i];
        }
    }
    return nullptr; // registry full: renames stay guarded by the range check
}

// Called once per list draw: bumps the generation when the row count changed
// since the last frame (an add/remove happened anywhere, including the tab
// level) and returns the current generation.
static uint list_generation_track(ImGuiID id, int count)
{
    XA_LIST_GENERATION *slot = list_generation_slot(id);
    if (slot == nullptr)
    {
        return 0;
    }
    if (slot->Count != count)
    {
        slot->Count = count;
        slot->Generation++;
    }
    return slot->Generation;
}

static void list_generation_bump(ImGuiID id)
{
    XA_LIST_GENERATION *slot = list_generation_slot(id);
    if (slot != nullptr)
    {
        slot->Generation++;
    }
}

char *ListRowLabel()
{
    return s_RowLabel;
}

bool ListNamesEqual(const string &a, const char *b)
{
    // GetFreeName compares stored names lowercased (case-insensitive)
    const size_t len = strlen(b);
    if (a.size() != len)
    {
        return false;
    }
    for (size_t i = 0; i < len; i++)
    {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
        {
            return false;
        }
    }
    return true;
}

XA_LIST_RENAME *ListRenameBegin(ImGuiID list, int row, const char *name)
{
    XA_LIST_GENERATION *slot = list_generation_slot(list);
    s_Rename.row = row;
    snprintf(s_Rename.buf, sizeof(s_Rename.buf), "%s", name != nullptr ? name : "");
    s_Rename.List = list;
    s_Rename.Generation = slot != nullptr ? slot->Generation : 0;
    s_RenameActive = true;
    return &s_Rename;
}

XA_LIST_RENAME *ListRenameActive()
{
    return s_RenameActive ? &s_Rename : nullptr;
}

void ListRenameEnd()
{
    s_RenameActive = false;
}

// The CExtendedListWidget::GetFreeName uniquifier: keep the name if no other
// row matches (case-insensitive), else append _1, _2, ... to the base name.
static string get_free_name(const XA_LIST_VIEW &view, int skipRow, const string &name)
{
    int count = view.Count(view.Ctx);
    string current = name;
    int offset = 1;
    while (true)
    {
        bool free = true;
        for (int i = count - 1; i >= 0; i--)
        {
            if (i != skipRow && ListNamesEqual(view.Name(view.Ctx, i), current.c_str()))
            {
                free = false;
                break;
            }
        }
        if (free)
        {
            return current;
        }
        current = name + "_" + std::to_string(offset++);
    }
}

// Row payload shared through the drag & drop clipboard
struct XA_ROW_PAYLOAD
{
    ImGuiID List;
    int Index;
};

static const char *ROW_PAYLOAD_TYPE = "XUOASSIST_ROW";

bool ListCore(const char *id, const XA_LIST_VIEW &view, int *selected, bool checkColumn, float height)
{
    bool changed = false;
    const ImGuiID listId = ImGui::GetID(id);
    const int count = view.Count(view.Ctx);
    if (*selected >= count)
    {
        *selected = count > 0 ? count - 1 : -1;
    }

    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_NoBordersInBody |
                                  ImGuiTableFlags_PadOuterX;
    if (!ImGui::BeginTable(id, checkColumn ? 2 : 1, flags, ImVec2(0.0f, height)))
    {
        return false;
    }

    bool rowHovered = false;
    // the inline rename is scoped to its owning list: a different list id, a
    // container mutation (add/remove/move) or an out-of-range row cancels it
    // instead of rendering/committing it into the wrong container
    const uint generation = list_generation_track(listId, count);
    XA_LIST_RENAME *rename = ListRenameActive();
    if (rename != nullptr)
    {
        if (rename->List != listId || rename->Generation != generation ||
            rename->row < 0 || rename->row >= count)
        {
            ListRenameEnd();
            rename = nullptr;
        }
    }

    ImGuiListClipper clipper;
    clipper.Begin(count);
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            ImGui::PushID(row);
            ImGui::TableNextRow();
            int col = 0;
            if (checkColumn)
            {
                ImGui::TableSetColumnIndex(col++);
                bool *checked = view.Check != nullptr ? view.Check(view.Ctx, row) : nullptr;
                if (checked != nullptr && ImGui::Checkbox("##enabled", checked))
                {
                    changed = true;
                }
            }
            ImGui::TableSetColumnIndex(col);
            const char *name = view.Name(view.Ctx, row);
            const bool renaming = rename != nullptr && rename->row == row;

            if (renaming)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                ImGui::InputText("##rename", rename->buf, sizeof(rename->buf), ImGuiInputTextFlags_AutoSelectAll);
                ImGui::PopStyleVar();
                if (ImGui::IsItemDeactivated())
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    {
                        // Escape reverts (Qt closed the editor without applying)
                        ListRenameEnd();
                        rename = nullptr;
                    }
                    else if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        // CExtendedListWidget::on_itemChanged: empty -> "name",
                        // collisions -> GetFreeName; then emit itemRenamed
                        string finalName = rename->buf;
                        if (finalName.empty())
                        {
                            finalName = "name";
                        }
                        finalName = get_free_name(view, row, finalName);
                        view.SetName(view.Ctx, row, finalName.c_str());
                        *selected = row;
                        changed = true;
                        ListRenameEnd();
                        rename = nullptr;
                    }
                    else
                    {
                        // focus lost without an edit: abandon the rename
                        ListRenameEnd();
                        rename = nullptr;
                    }
                }
            }
            else
            {
                if (ImGui::Selectable(name, *selected == row, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    *selected = row;
                }
                if (ImGui::IsItemHovered())
                {
                    rowHovered = true;
                    if (ImGui::IsMouseDoubleClicked(0) && view.AllowRename && !ImGui::GetIO().WantTextInput)
                    {
                        rename = ListRenameBegin(listId, row, name);
                    }
                }
                if (ImGui::BeginDragDropSource())
                {
                    const XA_ROW_PAYLOAD payload = { listId, row };
                    ImGui::SetDragDropPayload(ROW_PAYLOAD_TYPE, &payload, sizeof(payload));
                    ImGui::TextUnformatted(name);
                    ImGui::EndDragDropSource();
                }
                // drop target on every row: dropping replaces Qt's MoveAction
                // reorder (insert at the hovered row)
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(ROW_PAYLOAD_TYPE))
                    {
                        XA_ROW_PAYLOAD src;
                        memcpy(&src, payload->Data, sizeof(src));
                        // reorder only within the same list
                        if (src.List == listId && src.Index != row)
                        {
                            view.Move(view.Ctx, src.Index, row);
                            list_generation_bump(listId);
                            if (*selected == src.Index)
                            {
                                *selected = row;
                            }
                            changed = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            ImGui::PopID();
        }
    }

    ImGui::EndTable();

    // the global Delete key rule (assistform.cpp:203-230), gated on row hover
    // and on the assistant not capturing text/keys
    if (*selected >= 0 && rowHovered && !ImGui::GetIO().WantCaptureKeyboard &&
        !ImGui::GetIO().WantTextInput && ImGui::IsKeyChordPressed(ImGuiKey_Delete))
    {
        view.Remove(view.Ctx, *selected);
        list_generation_bump(listId);
        *selected = -1;
        changed = true;
    }

    return changed;
}

// --- Find/Ignore content rows -----------------------------------------------

static XA_LIST_VIEW content_view_common(void *ctx)
{
    XA_LIST_VIEW view;
    view.Ctx = ctx;
    view.AllowRename = false;
    return view;
}

bool FindContentList(const char *id, vector<CFindItem> &items, int *selected, float height)
{
    XA_LIST_VIEW view = content_view_common(&items);
    view.Count = [](void *ctx) -> int { return (int)((vector<CFindItem> *)ctx)->size(); };
    view.Name = [](void *ctx, int index) -> const char *
    {
        const CFindItem &item = (*(vector<CFindItem> *)ctx)[index];
        char *buf = ListRowLabel();
        snprintf(buf, 512, "%s %s %s", item.GetGraphic().c_str(), item.GetColor().c_str(), item.GetComment().c_str());
        return buf;
    };
    view.Insert = [](void *ctx, int index)
    {
        vector<CFindItem> *v = (vector<CFindItem> *)ctx;
        v->insert(v->begin() + index, CFindItem());
    };
    view.Remove = [](void *ctx, int index)
    {
        vector<CFindItem> *v = (vector<CFindItem> *)ctx;
        v->erase(v->begin() + index);
    };
    view.Move = [](void *ctx, int from, int to)
    {
        vector<CFindItem> *v = (vector<CFindItem> *)ctx;
        const CFindItem item = (*v)[from];
        v->erase(v->begin() + from);
        v->insert(v->begin() + (to > from ? to - 1 : to), item);
    };
    return ListCore(id, view, selected, false, height);
}

bool IgnoreContentList(const char *id, vector<CIgnoreItem> &items, int *selected, float height)
{
    XA_LIST_VIEW view = content_view_common(&items);
    view.Count = [](void *ctx) -> int { return (int)((vector<CIgnoreItem> *)ctx)->size(); };
    view.Name = [](void *ctx, int index) -> const char *
    {
        const CIgnoreItem &item = (*(vector<CIgnoreItem> *)ctx)[index];
        char *buf = ListRowLabel();
        snprintf(
            buf,
            512,
            "%s %s %s %s",
            item.GetSerial().c_str(),
            item.GetGraphic().c_str(),
            item.GetColor().c_str(),
            item.GetComment().c_str());
        return buf;
    };
    view.Insert = [](void *ctx, int index)
    {
        vector<CIgnoreItem> *v = (vector<CIgnoreItem> *)ctx;
        v->insert(v->begin() + index, CIgnoreItem());
    };
    view.Remove = [](void *ctx, int index)
    {
        vector<CIgnoreItem> *v = (vector<CIgnoreItem> *)ctx;
        v->erase(v->begin() + index);
    };
    view.Move = [](void *ctx, int from, int to)
    {
        vector<CIgnoreItem> *v = (vector<CIgnoreItem> *)ctx;
        const CIgnoreItem item = (*v)[from];
        v->erase(v->begin() + from);
        v->insert(v->begin() + (to > from ? to - 1 : to), item);
    };
    return ListCore(id, view, selected, false, height);
}

// --- StatusBar ---------------------------------------------------------------

void StatusBar_Draw(uint32_t sessionStartTicks)
{
    const uint32_t elapsed = g_Ticks - sessionStartTicks;
    const int hours = int(elapsed / (60u * 60u * 1000u));
    const int minutes = int((elapsed / (60u * 1000u)) % 60u);
    // trivial world census: every object the client currently tracks
    const int items = g_World != nullptr ? int(g_World->m_Map.size()) : 0;
    ImGui::TextDisabled(
        "Running time: %dh %dm | Items: %d. Developed by CrossUO Team.", hours, minutes, items);
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
