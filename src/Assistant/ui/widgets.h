// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// ImGui widget kit for the assistant UI. The custom widgets the tab screens
// are built from:

#pragma once

#include <external/gfx/imgui/imgui.h>
#include <stdint.h>
#include <algorithm> // std::rotate (list drag & drop reorder)

#include <common/fs.h> // fs_path (IconAssetPath)

#include "../core/global.h"
#include "../model/commonitems/finditem.h"
#include "../model/commonitems/ignoreitem.h"

namespace xuoassist {

// --- shared links -------------------------------------------------------------
// the assistant scripting docs page: the About tab "Docs" button and the
// Scripts tab "Script examples" link both open this URL through
// ::Platform::OpenBrowser (opens the ScriptLibrary
extern const char *const DOCS_URL;

// opens a URL in the system browser (::Platform::OpenBrowser) - the shared
// launcher so tab TUs do not have to include src/Platform.h (its SDL event
// typedefs collide with xuocore/plugininterface.h when included late)
void OpenURL(const char *url);

// --- icon atlas (src/Assistant/assets/*.png, 32x32 cells) -----------------
enum XA_ICON
{
    XA_ICON_ADD = 0,
    XA_ICON_SAVE,
    XA_ICON_REMOVE,
    XA_ICON_TARGET,
    XA_ICON_TRAY,
    XA_ICON_HOTKEY,
    XA_ICON_MAP,
    XA_ICON_MACRO_DEFAULT,
    XA_ICON_MACRO_RECORD,
    XA_ICON_MACRO_START,
    XA_ICON_MACRO_STOP,
    XA_ICON_MACRO_PAUSE,
    XA_ICON_MACRO_RESUME,
    XA_ICON_MACRO_EDIT,
    XA_ICON_CORPSE,
    XA_ICON_COUNT
};

// Loads the atlas and registers it with the ImGui backend. Safe to call every
// frame (idempotent); needs a live ImGui context, so the first draw does it.
bool Icons_Init();
// Frees the CPU-side pixels only; must run AFTER ui_shutdown/DestroyContext.
// The GL texture is reclaimed by the renderer backend during its own shutdown
// (ImGui_ImplOpenGL3_Shutdown walks the still-registered textures: RefCount
// must still be 1, so the atlas must NOT be unregistered before that). The
// assistant window calls this right after ui_shutdown returns.
void Icons_Shutdown();
// Loads an assistant asset by file name (the embedded copy first, the
// assets/xuoassist disk lookup as fallback); the caller frees with
// stbi_image_free. Shared by the icon atlas and the other png-loading UI
// modules (the About logo).
unsigned char *IconAssetLoad(const char *file, int *w, int *h, int *comp);

// --- IconButton ------------------------------------------------------------
// Flat icon button: hover rgb(192,192,192), pressed rgb(160,160,160) with the
// 2px press shift. size 0 = one icon cell sized from the font height.
bool IconButton(
    XA_ICON icon,
    const char *tip,
    bool enabled = true,
    float size = 0.0f,
    const char *idSeed = "");

// --- color helpers ----------------------------------------------------------
// 15-bit UO color -> RGBA preview (approximation: 5-bit channels x8).
ImU32 UOColorToRGBA(uint16_t color);
// Renders an atlas icon at an explicit draw-list rectangle (map corpse
// marker); atlas lazy-init like IconButton.
void IconImage(XA_ICON icon, const ImVec2 &topLeft, const ImVec2 &size);
// Preview-only swatch for the 0x02B2-style hue fields (no RGB picker for
void HueSwatch(uint16_t uoColor, float size = 18.0f);
// Swatch + picker popup for true RGB fields (Display tab).
// commit (optional): set inside the picker popup scope when the edit session
// ends (ImGui::IsItemDeactivatedAfterEdit - EndPopup restores LastItemData, so
// the query is only valid here); lets callers throttle saves to the release.
bool ColorSwatch(const char *id, ImU32 *rgba, float size = 18.0f, bool *commit = nullptr);

// --- ComboGroup ------------------------------------------------------------
// Items starting with '#' are painted as lightGray group separator rows.
bool ComboGroup(const char *id, int *current, const char *const items[], int count);

// --- list tab icon button row ----------------------------------------------
enum
{
    XALB_NONE = 0,
    XALB_ADD = 1,
    XALB_SAVE = 2,
    XALB_REMOVE = 4,
    XALB_TARGET = 8 // the from-target button
};

// Draws the add/save/remove/from-target row; returns the pressed buttons as a
// XALB_* bitmask. idSeed must be unique
// per row instance (a tab can host several list panes).
int ListButtons(const char *idSeed, int excludeFlags = 0);

// --- nested sub-tab pages ----------------------------------------------------
// The nested tab bars (Display/Lists/Filters/Agents) sit inside the main tab's
// child: drawn directly under its top edge the bar stays pinned, and each
// sub-tab's content lives in a scrolling child that fills the height left
// under the bar - the same bar-outside-the-scroll structure as the main
// window tabs. Use instead of BeginTabItem/EndTabItem; childId must be unique
// per sub-tab (the child carries its scroll state).
bool BeginSubTab(const char *label, const char *childId);
void EndSubTab();

// --- hex line edits -----------------------------------------------------------
bool HexEditField(const char *id, char *value, size_t valueSize, float width = 0.0f);
bool HexEditField(const char *id, astr_t *value, float width = 0.0f);

// --- generic list core (implemented in widgets.cpp) -------------------------
struct XA_LIST_RENAME
{
    int row;
    char buf[128];
    ImGuiID List;    // owning list: a rename never renders/commits elsewhere
    uint32_t Generation; // container mutation count captured at Begin
};

// Frame-static scratch buffers/state shared by all list widgets.
char *ListRowLabel();                          // reused every frame
bool ListNamesEqual(const astr_t &a, const char *b); // case-insensitive
XA_LIST_RENAME *ListRenameBegin(ImGuiID list, int row, const char *name);
XA_LIST_RENAME *ListRenameActive();
void ListRenameEnd();
// Bumps a list's structural generation out of band (the same guard the rename
// state carries): call when the backing container of an already-drawn list id
// is swapped for another one without a count change - e.g. the selected list
// changed on a Find/Ignore tab, so a pending rename must not resume/commit
// against a foreign container of the same row count.
void ListGenerationBump(ImGuiID id);

struct XA_LIST_VIEW
{
    void *Ctx{ nullptr };
    int (*Count)(void *ctx){ nullptr };
    const char *(*Name)(void *ctx, int index){ nullptr }; // may use ListRowLabel()
    void (*SetName)(void *ctx, int index, const char *name){ nullptr };
    void (*Insert)(void *ctx, int index){ nullptr };      // new row at index
    void (*Remove)(void *ctx, int index){ nullptr };
    void (*Move)(void *ctx, int from, int to){ nullptr };
    bool *(*Check)(void *ctx, int index){ nullptr };      // optional checkbox column
    bool AllowRename{ true };
};

// Renders the list (table with clipper, drag reorder, Delete key). Returns
// true when the container changed so the caller can persist immediately.
bool ListCore(
    const char *id,
    const XA_LIST_VIEW &view,
    int *selected,
    bool checkColumn,
    float height);

// --- NameList (CExtendedListWidget) ----------------------------------------
// One selectable row per item. label/setLabel name the row's display field
// (e.g. &CTypeEntry::Name for the config lists, &CFilterSpeechEntry::Text for
// the speech filters).
// - double click: inline rename; empty or colliding names are uniquified with
//   the GetFreeName algorithm
// - Delete key: removes the selected row while a row is hovered and the
//   assistant is not capturing text (does not steal keys the game wants)
// - drag & drop: reorders rows (the caller saves)
// - checkColumn: renders the row's Enabled state as a leading checkbox
namespace detail {

// friends/enemies rows carry the checked state as an Enabled member; other
// row types have none (the second overload only matches then)
template <class U> auto RowEnabled(U &item, int) -> decltype(&item.Enabled) { return &item.Enabled; }
template <class U> bool *RowEnabled(U &, long) { return nullptr; }

} // namespace detail

// context handed to the XA_LIST_VIEW callbacks (captureless lambdas only:
template <class T> struct XA_NAMED_LIST_CTX
{
    vector<T> *Items;
    const char *(*Label)(const T &);
    void (*SetLabel)(T &, const char *);
};

template <class T>
bool NameList(
    const char *id,
    vector<T> &items,
    int *selected,
    const char *(*label)(const T &),
    void (*setLabel)(T &, const char *),
    bool checkColumn = false,
    float height = -1.0f)
{
    XA_NAMED_LIST_CTX<T> ctx = { &items, label, setLabel };
    XA_LIST_VIEW view;
    view.Ctx = &ctx;
    view.Count = [](void *c) -> int { return (int)((XA_NAMED_LIST_CTX<T> *)c)->Items->size(); };
    view.Name = [](void *c, int index) -> const char *
    {
        XA_NAMED_LIST_CTX<T> *x = (XA_NAMED_LIST_CTX<T> *)c;
        return x->Label((*x->Items)[index]);
    };
    view.SetName = [](void *c, int index, const char *name)
    {
        XA_NAMED_LIST_CTX<T> *x = (XA_NAMED_LIST_CTX<T> *)c;
        x->SetLabel((*x->Items)[index], name);
    };
    view.Insert = [](void *c, int index)
    {
        vector<T> *v = ((XA_NAMED_LIST_CTX<T> *)c)->Items;
        v->insert(v->begin() + index, T());
    };
    view.Remove = [](void *c, int index)
    {
        vector<T> *v = ((XA_NAMED_LIST_CTX<T> *)c)->Items;
        v->erase(v->begin() + index);
    };
    view.Move = [](void *c, int from, int to)
    {
        // rotate instead of erase+insert: the row type is move-only (owning
        // list), and this lands the row at the same index the old
        // erase(from)/insert(to>from ? to-1 : to) pair did
        vector<T> *v = ((XA_NAMED_LIST_CTX<T> *)c)->Items;
        if (to > from)
        {
            std::rotate(v->begin() + from, v->begin() + from + 1, v->begin() + to);
        }
        else
        {
            std::rotate(v->begin() + to, v->begin() + from, v->begin() + from + 1);
        }
    };
    view.Check = checkColumn
        ? [](void *c, int index) -> bool *
          { return detail::RowEnabled((*( (XA_NAMED_LIST_CTX<T> *)c )->Items)[index], 0); }
        : (bool *(*)(void *, int))nullptr;
    view.AllowRename = true;
    return ListCore(id, view, selected, checkColumn, height);
}

// --- HotkeyKeyboard (the on-screen keyboard from hotkeysform.ui) ------------
// Table-driven ~110 key caps with the four state colors (hotkeysform.cpp:
// 607-710): unset / set green rgb(183,255,183) / duplicate red rgb(255,183,183)
// / selected blue rgb(183,183,255); modifier caps light up gray rgb(200,200,200)
// while the sticky or physically-held modifiers match. Clicking a key cap
// assigns it to the selected bindings row through the engine (the same event a
// physical press would produce); Shift/Ctrl/Alt caps toggle sticky modifiers.
// selectedRow: the bindings-table row rendered blue (-1 = none).
void HotkeyKeyboard_Draw(const char *id, int selectedRow);
// The cap-click route into the owning form (ui/hotkeysform.cpp): forwards to
// g_Hotkeys.KeyboardAssign(keyId, the form's selected row).
void HotkeysForm_KeyboardAssign(int keyId);

// --- Find/Ignore content rows (CFindListItemListItem / CIgnoreListItem) -----
// Displays "graphic color comment" / "serial graphic color comment" rows
// Selection fills the editor fields below.
bool FindContentList(const char *id, vector<CFindItem> &items, int *selected, float height = -1.0f);
bool IgnoreContentList(const char *id, vector<CIgnoreItem> &items, int *selected, float height = -1.0f);

// --- StatusBar ------------------------------------------------------------
void StatusBar_Draw(uint32_t sessionStartTicks);

} // namespace xuoassist
