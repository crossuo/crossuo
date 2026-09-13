// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Assistant hotkey engine (Phase 3, master plan §2.5/§3.2/§5.3/§6/§7 item 1).
// Replaces the Qt app's QHotkeyEventFilter + HotkeysForm::HotkeyExistsAndUse
// with an SDL-event funnel living in the client's input path:
//
//   AssistantWindow_ProcessEvent (Alt+U, pop-out windows, ImGui gating)
//     -> CHotkeys::ProcessEvent  (capture field, then binding matching)
//       -> the game, unless consumed
//
// Key storage is the Phase-1c CHotkeyEntry table in g_AssistantConfig
// (Hotkeys.xml, version 1, SDL key codes + the HM_* modifier bitmask) - byte
// compatible with the Qt writer/reader. The display-name tables, the keypad
// bit semantics and the duplicate policy are ported from gui/hotkeybox.cpp.
//
// Qt semantics kept verbatim (hotkeysform.cpp:1436 HotkeyExistsAndUse):
//   - HT_TOGGLE_HOTKEYS fires even when the engine is off (it IS the switch);
//   - every other binding fires only when OnOffHotkeys() is set;
//   - a matched binding is consumed unless PassHotkeys() (then the key ALSO
//     reaches the game), with the Alt+F4 exception (never consumed);
//   - wheel/X-button/middle-click bindings behave like any other binding.
// Qt deltas (in-client port): key auto-repeat never fires a binding (the Qt
// capture filter skipped repeats; the keydown path always re-fired, an
// artifact of the client re-sending UOMSG_INPUT_KEYDOWN - here the game only
// sends non-repeat events to the engine, see AssistantWindow.cpp).

#ifndef XUOASSIST_CORE_HOTKEYS_H
#define XUOASSIST_CORE_HOTKEYS_H

#include "global.h"

union SDL_Event;

namespace xuoassist {

// --- common/keys.h (the Qt app's modifier vocabulary, storage compatible) --
enum HOTKEY_MODIFIERS
{
    HM_NONE = 0,
    HM_MOUSE_WHEEL = 0x01,
    HM_ALT = 0x02,
    HM_CTRL = 0x04,
    HM_SHIFT = 0x08,
    HM_KEYPAD = 0x10,
    HM_META = 0x20,
    HM_MOUSE_X = 0x40
};

// The pseudo-keycodes the Qt app assigned to mouse events (they are stored in
// Hotkeys.xml "key" for wheel/X bindings).
enum
{
    MOUSE_WHEEL_SCROLL_UP = 0x00000001,
    MOUSE_WHEEL_SCROLL_DOWN = 0x00000002,
    MOUSE_WHEEL_PRESS = 0x00000004,
    MOUSE_X_UP = 0x00000008,
    MOUSE_X_DOWN = 0x00000010
};

// common/keys.h eKeyId: identifies one on-screen keyboard cap / one keymap row.
enum XA_KEY_ID
{
    XKI_INVALID = 0,
    XKI_ESC,
    XKI_F1,
    XKI_F2,
    XKI_F3,
    XKI_F4,
    XKI_F5,
    XKI_F6,
    XKI_F7,
    XKI_F8,
    XKI_F9,
    XKI_F10,
    XKI_F11,
    XKI_F12,
    XKI_SLOCK,
    XKI_PAUSE,
    XKI_TILDA,
    XKI_1,
    XKI_2,
    XKI_3,
    XKI_4,
    XKI_5,
    XKI_6,
    XKI_7,
    XKI_8,
    XKI_9,
    XKI_0,
    XKI_SUB,
    XKI_RESULT,
    XKI_BACK,
    XKI_INS,
    XKI_HOME,
    XKI_PGUP,
    XKI_NUMLOCK,
    XKI_NUMDIV,
    XKI_NUMMUL,
    XKI_NUMSUB,
    XKI_TAB,
    XKI_Q,
    XKI_W,
    XKI_E,
    XKI_R,
    XKI_T,
    XKI_Y,
    XKI_U,
    XKI_I,
    XKI_O,
    XKI_P,
    XKI_BKTL,
    XKI_BKTR,
    XKI_ENTER,
    XKI_DEL,
    XKI_END,
    XKI_PGDN,
    XKI_NUM7,
    XKI_NUM8,
    XKI_NUM9,
    XKI_NUMADD,
    XKI_CAPS,
    XKI_A,
    XKI_S,
    XKI_D,
    XKI_F,
    XKI_G,
    XKI_H,
    XKI_J,
    XKI_K,
    XKI_L,
    XKI_SEMICOLON,
    XKI_QUOTE,
    XKI_SLASH,
    XKI_NUM4,
    XKI_NUM5,
    XKI_NUM6,
    XKI_Z,
    XKI_X,
    XKI_C,
    XKI_V,
    XKI_B,
    XKI_N,
    XKI_M,
    XKI_COMMA,
    XKI_DOT,
    XKI_DIV,
    XKI_UP,
    XKI_NUM1,
    XKI_NUM2,
    XKI_NUM3,
    XKI_NUMENTER,
    XKI_WINL,
    XKI_SPACE,
    XKI_WINR,
    XKI_APPS,
    XKI_LEFT,
    XKI_DOWN,
    XKI_RIGHT,
    XKI_NUM0,
    XKI_NUMDOT,
    XKI_MOUSE_WHEEL_UP,
    XKI_MOUSE_WHEEL_PRESS,
    XKI_MOUSE_WHEEL_DOWN,
    XKI_MOUSE_X1,
    XKI_MOUSE_X2,
    // the modifier caps have no keymap row: they toggle the sticky bitmask
    XKI_SHIFT_L,
    XKI_SHIFT_R,
    XKI_CTRL_L,
    XKI_CTRL_R,
    XKI_ALT,
    XKI_ALTGR,
    XKI_COUNT
};

// KeyMap / KeyInfo (hotkeybox.cpp:237): SDL keycode + the modifiers the
// on-screen cap assigns by default (keypad navigation block sets HM_KEYPAD,
// wheel/X set their mouse flags) + the display name.
struct XA_KEY_INFO
{
    uint Key;
    uchar Modifiers;
    const char *KeyName;
};

const XA_KEY_INFO &HotkeyKeyInfo(int id);
// Reverse lookup used by the "Hotkey" column text (GetKeycodeName).
const char *HotkeyKeycodeName(uint keycode);
// QHotkeyBox::GetHotkeyText (hotkeybox.cpp:792): "Alt + Ctrl + Shift + Pad +
// KeyName" / "Wheel Up|Down|Press" / "MouseX 1|2".
void HotkeyFormatText(uint key, uchar modifiers, char *out, size_t size);

// The engine. The binding table itself lives in g_AssistantConfig.m_Hotkeys.
class CHotkeys
{
public:
    // The single event funnel (called from AssistantWindow_ProcessEvent while
    // the assistant is initialized and the client is in game). Returns true
    // when the event is consumed and must not reach the game. Never fires
    // when ImGui owns the keyboard (AssistantWindow checks that first) or
    // when a binding is disabled.
    bool ProcessEvent(const SDL_Event &ev);

    // --- capture (was QHotkeyBox: click the field, press a key/wheel/X) ----
    bool IsCapturing() const { return m_Capturing; }
    void BeginCapture();                    // arm the field
    void CancelCapture() { m_Capturing = false; }
    uint CaptureKey() const { return m_CaptureKey; }
    uchar CaptureModifiers() const { return m_CaptureModifiers; }
    // Commits the captured (key, modifiers) into config row `row` with the Qt
    // duplicate policy (CheckExists): false when a duplicate popup is open
    // (the UI then drives ResolveDuplicate below).
    bool AssignCaptureToRow(int row);
    // The duplicate question (hotkeysform.cpp:184): 0 = replace the existing
    // binding(s), 1 = save the duplicate anyway, 2 = select the existing row.
    void ResolveDuplicate(int answer);
    int DuplicateRow() const { return m_DuplicateRow; }

    // --- on-screen keyboard caps (click = the same event as a physical key) -
    // Key caps assign (key, cap mods | sticky mods) to the selected row - the
    // exact payload a physical press would produce (on_KeyButtonClicked).
    void KeyboardAssign(int keyId, int row);
    void KeyboardToggleModifier(int keyId); // Shift/Ctrl/Alt caps (sticky)
    uchar StickyModifiers() const { return m_StickyModifiers; }
    bool StickyModifierSet(uchar flag) const { return (m_StickyModifiers & flag) != 0; }
    // UpdateKeyboard (hotkeysform.cpp:603): per-cap state for the 4 colors.
    // 0 = unset, 1 = set, 2 = duplicate, 3 = currently selected row's key.
    int KeyboardCapState(int keyId, int selectedRow) const;
    void ResetStickyModifiers() { m_StickyModifiers = 0; }

private:
    bool ProcessKeyDown(const SDL_Event &ev);
    bool ProcessKeyUp(const SDL_Event &ev);
    bool ProcessMouseButtonDown(const SDL_Event &ev);
    bool ProcessWheel(const SDL_Event &ev);
    // The matcher: walks the config table, fires the action. Returns
    // consumed (HotkeyExistsAndUse semantics).
    bool MatchAndUse(uint key, uchar modifiers);
    // CheckExists (hotkeybox.cpp:871): a duplicate exists outside `skipRow`.
    int FindDuplicate(uint key, uchar modifiers, int skipRow) const;
    // CreateKey + the CheckExists dialog outcomes, funneled: assigns
    // (key, modifiers) to `row` (or defers to the popup); saves on mutation.
    void CommitKeyToRow(int row, uint key, uchar modifiers);
    // Apply the pending assignment after ResolveDuplicate.
    void ApplyPending();

    bool m_Capturing{ false };
    uint m_CaptureKey{ 0 };
    uchar m_CaptureModifiers{ 0 };
    // pending duplicate resolution (row index in m_Hotkeys, -1 = none)
    int m_DuplicateRow{ -1 };
    uint m_PendingKey{ 0 };
    uchar m_PendingModifiers{ 0 };
    int m_PendingTargetRow{ -1 };
    uchar m_StickyModifiers{ 0 };
};

extern CHotkeys g_Hotkeys;

// The AssistantWindow event hooks (see AssistantWindow_ProcessEvent for the
// routing order: Alt+U -> pop-out windows -> ImGui capture -> capture field
// (Hotkeys_ProcessCapture) -> bindings (Hotkeys_ProcessGameEvent) -> game).
bool Hotkeys_IsCapturing();
// The armed capture field: consumes the next key/wheel/mouse event.
bool Hotkeys_ProcessCapture(const SDL_Event &ev);
// Binding matching + firing (HotkeyExistsAndUse semantics; "Pass keys to
// client" decides whether a matched key also reaches the game).
bool Hotkeys_ProcessGameEvent(const SDL_Event &ev);

} // namespace xuoassist

#endif // XUOASSIST_CORE_HOTKEYS_H
