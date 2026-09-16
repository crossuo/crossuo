// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Assistant ImGui widget kit implementation (master plan §6). See widgets.h
// for the per-widget mapping and behaviors.

#define LOGGER_MODULE Assistant

// Platform.h must come before widgets.h: with SDL already included,
// xuocore/plugininterface.h (pulled in by widgets.h -> core/global.h) picks
// the SDL event structs instead of its own KeyEvent/TextEvent, which would
// collide with Platform.h's SDL typedefs (same order as ui/tababout.cpp)
#if defined(XUO_ASSISTANT)
#include <SDL_wrapper.h>
#include <Platform.h> // ::Platform::OpenBrowser (OpenURL)
#endif

#include "widgets.h"

#if defined(XUO_ASSISTANT)

#include <external/gfx/imgui/imgui.h>
#include <external/gfx/imgui/imgui_internal.h> // RegisterUserTexture

#include <common/fs.h>
#include <common/logging/logging.h>
#include <xuocore/uodata.h> // ExeFilePath

#include "GameObjects/GameWorld.h" // client g_World (status bar census, like Phase 0)

#include "../core/hotkeys.h" // HotkeyKeyboard engine state (Phase 3)

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

// the assistant scripting docs page (widgets.h): About "Docs" + Scripts tab
// "Script examples"
const char *const DOCS_URL = "https://crossuo.com/book/scripting/scripting.html";

void OpenURL(const char *url)
{
    // the client's own launcher (Platform::OpenBrowser: ShellExecute /
    // xdg-open/open) is reused; SDL is 2.0.10 (no SDL_OpenURL)
    ::Platform::OpenBrowser(url);
}

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

fs_path IconAssetPath(const char *file)
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
        const fs_path path = IconAssetPath(s_IconFiles[icon].File);
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

// --- IconButton -----------------------------------------

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

    // IconButton look: hover rgb(192,192,192), pressed rgb(160,160,160)
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

// Draws an atlas icon at an explicit draw-list rectangle (no button chrome).
// Used by canvas-style widgets (the map corpse marker); initializes the atlas
// like IconButton does.
void IconImage(XA_ICON icon, const ImVec2 &topLeft, const ImVec2 &size)
{
    if ((s_Atlas == nullptr && !Icons_Init()) || ImGui::GetCurrentContext() == nullptr)
    {
        return;
    }
    ImGui::GetWindowDrawList()->AddImage(
        atlas_texture(), topLeft, ImVec2(topLeft.x + size.x, topLeft.y + size.y), s_UV0[icon],
        s_UV1[icon]);
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

bool ColorSwatch(const char *id, ImU32 *rgba, float size, bool *commit)
{
    ImVec4 col = ImGui::ColorConvertU32ToFloat4(*rgba);
    char label[48];
    snprintf(label, sizeof(label), "##swatch_%s", id);
    if (commit != nullptr)
    {
        *commit = false;
    }
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    bool changed = ImGui::Button(label, ImVec2(size, size));
    ImGui::PopStyleColor(3);
    if (ImGui::BeginPopupContextItem(label))
    {
        changed = ImGui::ColorPicker3(label, &col.x) || changed;
        if (commit != nullptr)
        {
            // the picker is group-wrapped: EndGroup forwards the deactivated
            // item, so this fires on the frame the drag/type-in commit ends
            *commit = ImGui::IsItemDeactivatedAfterEdit();
        }
        ImGui::EndPopup();
    }
    if (changed)
    {
        *rgba = ImGui::ColorConvertFloat4ToU32(col);
    }
    return changed;
}

// --- hex edit fields (the original le_* "0x..." line edits) ------------------------

// ImGuiInputTextFlags_CharsHexadecimal rejects the 'x' of the stored "0x..."
// prefix (typing would drop it and the saved text would diverge from the original
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

bool HexEditField(const char *id, astr_t *value, float width)
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
    // the from-target pipeline is live (g_TargetHandler +
    // RequestTarget; the reply arrives as AssistantUIEvents::OnFromTarget)
    if (IconButton(XA_ICON_TARGET, "Select from target", true, 0.0f, idSeed))
    {
        result |= XALB_TARGET;
    }
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

bool ListNamesEqual(const astr_t &a, const char *b)
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

void ListGenerationBump(ImGuiID id)
{
    list_generation_bump(id);
}

// The CExtendedListWidget::GetFreeName uniquifier: keep the name if no other
// row matches (case-insensitive), else append _1, _2, ... to the base name.
static astr_t get_free_name(const XA_LIST_VIEW &view, int skipRow, const astr_t &name)
{
    int count = view.Count(view.Ctx);
    astr_t current = name;
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
        current = name + "_" + str_from(offset++);
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
                        // Escape reverts (the original closed the editor without applying)
                        ListRenameEnd();
                        rename = nullptr;
                    }
                    else if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        // CExtendedListWidget::on_itemChanged: empty -> "name",
                        // collisions -> GetFreeName; then emit itemRenamed
                        astr_t finalName = rename->buf;
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
                // drop target on every row: dropping replaces the original move action
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

    // the global Delete key rule, gated on row hover
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

// --- HotkeyKeyboard (hotkeysform.ui) ------------------------------------------
//
// The on-screen keyboard, table-driven from the .ui geometry (rows, wide keys
// and the numpad block). Delta: the original grid had three tall keys (Enter, +,
// numpad Enter span two rows) - here every cap is one row high, which only
// affects cosmetics, not the assign semantics. "Prt" stays a dead button like
// in the original (pb_Prt was not in m_ButtonsMap). The wheel/X caps from the
// two "Mouse ..." groups sit in their own labeled rows.
//
// Draw path: no allocations (static table + ImGui style colors).

struct XA_CAP
{
    int KeyId;      // XA_KEY_ID (XKI_INVALID = dead cap like "Prt")
    bool Modifier;  // Shift/Ctrl/Alt caps toggle the sticky bitmask
    const char *Label;
    float Units;    // width in cells (~30px in the .ui)
};

static const XA_CAP s_RowF[] = {
    { XKI_ESC, false, "Esc", 1 },
    { XKI_F1, false, "F1", 1 }, { XKI_F2, false, "F2", 1 },
    { XKI_F3, false, "F3", 1 }, { XKI_F4, false, "F4", 1 },
    { XKI_F5, false, "F5", 1 }, { XKI_F6, false, "F6", 1 },
    { XKI_F7, false, "F7", 1 }, { XKI_F8, false, "F8", 1 },
    { XKI_F9, false, "F9", 1 }, { XKI_F10, false, "F10", 1 },
    { XKI_F11, false, "F11", 1 }, { XKI_F12, false, "F12", 1 },
    { XKI_INVALID, false, "Prt", 1 },
    { XKI_SLOCK, false, "SLc", 1 },
    { XKI_PAUSE, false, "Pause", 1 },
};
static const XA_CAP s_RowMouse[] = {
    { XKI_MOUSE_WHEEL_UP, false, "Up", 1.5f },
    { XKI_MOUSE_WHEEL_PRESS, false, "Press", 1.5f },
    { XKI_MOUSE_WHEEL_DOWN, false, "Down", 1.5f },
    { XKI_MOUSE_X1, false, "X1", 1.5f },
    { XKI_MOUSE_X2, false, "X2", 1.5f },
};
static const XA_CAP s_Row1[] = {
    { XKI_TILDA, false, "`", 0.8f },
    { XKI_1, false, "1", 1 }, { XKI_2, false, "2", 1 }, { XKI_3, false, "3", 1 },
    { XKI_4, false, "4", 1 }, { XKI_5, false, "5", 1 }, { XKI_6, false, "6", 1 },
    { XKI_7, false, "7", 1 }, { XKI_8, false, "8", 1 }, { XKI_9, false, "9", 1 },
    { XKI_0, false, "0", 1 },
    { XKI_SUB, false, "-", 1 },
    { XKI_RESULT, false, "=", 1 },
    { XKI_BACK, false, "Back", 2 },
    { XKI_INS, false, "Ins", 1 }, { XKI_HOME, false, "Home", 1 }, { XKI_PGUP, false, "PgUp", 1 },
    { XKI_NUMLOCK, false, "Num", 1 }, { XKI_NUMDIV, false, "/", 1 },
    { XKI_NUMMUL, false, "*", 1 }, { XKI_NUMSUB, false, "-", 1 },
};
static const XA_CAP s_Row2[] = {
    { XKI_TAB, false, "Tab", 1 },
    { XKI_Q, false, "q", 1 }, { XKI_W, false, "w", 1 }, { XKI_E, false, "e", 1 },
    { XKI_R, false, "r", 1 }, { XKI_T, false, "t", 1 }, { XKI_Y, false, "y", 1 },
    { XKI_U, false, "u", 1 }, { XKI_I, false, "i", 1 }, { XKI_O, false, "o", 1 },
    { XKI_P, false, "p", 1 },
    { XKI_BKTL, false, "[", 1 }, { XKI_BKTR, false, "]", 1 },
    { XKI_ENTER, false, "Enter", 2 },
    { XKI_DEL, false, "Del", 1 }, { XKI_END, false, "End", 1 }, { XKI_PGDN, false, "PgDn", 1 },
    { XKI_NUM7, false, "7", 1 }, { XKI_NUM8, false, "8", 1 }, { XKI_NUM9, false, "9", 1 },
    { XKI_NUMADD, false, "+", 1 },
};
static const XA_CAP s_Row3[] = {
    { XKI_CAPS, false, "Caps", 1.5f },
    { XKI_A, false, "a", 1 }, { XKI_S, false, "s", 1 }, { XKI_D, false, "d", 1 },
    { XKI_F, false, "f", 1 }, { XKI_G, false, "g", 1 }, { XKI_H, false, "h", 1 },
    { XKI_J, false, "j", 1 }, { XKI_K, false, "k", 1 }, { XKI_L, false, "l", 1 },
    { XKI_SEMICOLON, false, ";", 1 },
    { XKI_QUOTE, false, "'", 1 },
    { XKI_SLASH, false, "\\", 0.8f },
    { XKI_NUM4, false, "4", 1 }, { XKI_NUM5, false, "5", 1 }, { XKI_NUM6, false, "6", 1 },
};
static const XA_CAP s_Row4[] = {
    { XKI_SHIFT_L, true, "Shift", 2 },
    { XKI_Z, false, "z", 1 }, { XKI_X, false, "x", 1 }, { XKI_C, false, "c", 1 },
    { XKI_V, false, "v", 1 }, { XKI_B, false, "b", 1 }, { XKI_N, false, "n", 1 },
    { XKI_M, false, "m", 1 },
    { XKI_COMMA, false, ",", 1 }, { XKI_DOT, false, ".", 1 }, { XKI_DIV, false, "/", 1 },
    { XKI_SHIFT_R, true, "Shift", 1.5f },
    { XKI_UP, false, "^", 1 },
    { XKI_NUM1, false, "1", 1 }, { XKI_NUM2, false, "2", 1 }, { XKI_NUM3, false, "3", 1 },
    { XKI_NUMENTER, false, "Enter", 1 },
};
static const XA_CAP s_Row5[] = {
    { XKI_CTRL_L, true, "Ctrl", 1 },
    { XKI_WINL, false, "Win", 1 },
    { XKI_ALT, true, "Alt", 1 },
    { XKI_SPACE, false, "Space", 7 },
    { XKI_ALTGR, true, "AltGr", 1.5f },
    { XKI_WINR, false, "Win", 1 },
    { XKI_APPS, false, "Apps", 1.5f },
    { XKI_CTRL_R, true, "Ctrl", 1 },
    { XKI_LEFT, false, "<", 1 }, { XKI_DOWN, false, "v", 1 }, { XKI_RIGHT, false, ">", 1 },
    { XKI_NUM0, false, "0", 2 },
    { XKI_NUMDOT, false, ".", 1 },
};

void HotkeyKeyboard_Draw(const char *id, int selectedRow)
{
    (void)id;
    // the four key-cap state colors
    static const ImU32 COLOR_UNSET = IM_COL32(60, 60, 60, 255); // ImGui button default stand-in
    static const ImU32 COLOR_MODIFIER = IM_COL32(200, 200, 200, 255);
    static const ImU32 COLOR_SET = IM_COL32(183, 255, 183, 255);
    static const ImU32 COLOR_DUPLICATE = IM_COL32(255, 183, 183, 255);
    static const ImU32 COLOR_SELECTED = IM_COL32(183, 183, 255, 255);

    const float cell = ImGui::GetTextLineHeight() * 2.1f;
    const float height = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 size(cell, height);

    struct XA_ROW
    {
        const XA_CAP *Caps;
        int Count;
    };
    const XA_ROW rows[] = {
        { s_RowF, (int)(sizeof(s_RowF) / sizeof(s_RowF[0])) },
        { s_RowMouse, (int)(sizeof(s_RowMouse) / sizeof(s_RowMouse[0])) },
        { s_Row1, (int)(sizeof(s_Row1) / sizeof(s_Row1[0])) },
        { s_Row2, (int)(sizeof(s_Row2) / sizeof(s_Row2[0])) },
        { s_Row3, (int)(sizeof(s_Row3) / sizeof(s_Row3[0])) },
        { s_Row4, (int)(sizeof(s_Row4) / sizeof(s_Row4[0])) },
        { s_Row5, (int)(sizeof(s_Row5) / sizeof(s_Row5[0])) },
    };

    ImGui::PushID(id);
    for (const XA_ROW &row : rows)
    {
        for (int i = 0; i < row.Count; i++)
        {
            const XA_CAP &cap = row.Caps[i];
            const ImVec2 capSize(cap.Units * cell + (cap.Units - 1.0f) * spacing, height);
            ImU32 color = COLOR_UNSET;
            if (cap.Modifier)
            {
                uchar flag = 0;
                if (cap.KeyId == XKI_SHIFT_L || cap.KeyId == XKI_SHIFT_R)
                {
                    flag = HM_SHIFT;
                }
                else if (cap.KeyId == XKI_CTRL_L || cap.KeyId == XKI_CTRL_R)
                {
                    flag = HM_CTRL;
                }
                else if (cap.KeyId == XKI_ALT || cap.KeyId == XKI_ALTGR)
                {
                    flag = HM_ALT;
                }
                if (flag != 0 && g_Hotkeys.StickyModifierSet(flag))
                {
                    color = COLOR_MODIFIER;
                }
            }
            else if (cap.KeyId != XKI_INVALID)
            {
                switch (g_Hotkeys.KeyboardCapState(cap.KeyId, selectedRow))
                {
                    case 1:
                        color = COLOR_SET;
                        break;
                    case 2:
                        color = COLOR_DUPLICATE;
                        break;
                    case 3:
                        color = COLOR_SELECTED;
                        break;
                    default:
                        break;
                }
            }

            ImGui::PushStyleColor(ImGuiCol_Button, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(20, 20, 20, 255));
            if (ImGui::Button(cap.Label, capSize))
            {
                if (cap.Modifier)
                {
                    g_Hotkeys.KeyboardToggleModifier(cap.KeyId);
                }
                else if (cap.KeyId != XKI_INVALID)
                {
                    // the form routes the click to its selected row
                    HotkeysForm_KeyboardAssign(cap.KeyId);
                }
            }
            ImGui::PopStyleColor(4);
            if (i + 1 < row.Count)
            {
                ImGui::SameLine();
            }
        }
    }
    ImGui::PopID();
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
        snprintf(buf, 512, "%s %s %s", item.Graphic.c_str(), item.Color.c_str(), item.Comment.c_str());
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
            item.Serial.c_str(),
            item.Graphic.c_str(),
            item.Color.c_str(),
            item.Comment.c_str());
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
