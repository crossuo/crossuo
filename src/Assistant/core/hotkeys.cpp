// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The hotkey engine (see hotkeys.h). Name/modifier tables ported from the
// original app's gui/hotkeybox.cpp (g_Keymap, GetHotkeyText, eventFilter) and the
// matching semantics from assist/hotkeysform.cpp (HotkeyExistsAndUse).

#define LOGGER_MODULE Assistant

#include "hotkeys.h"

#if defined(XUO_ASSISTANT)

#include <stdio.h>
#include <string.h>

#include <common/logging/logging.h>
#include "SDL_wrapper.h"

#include "../config/assistantconfig.h"
#include "actions.h"
#include "assistantevents.h"
#include "../macros/macroexecutor.h"
#include "../model/gameobjects/gameworld.h"

namespace xuoassist {

// --- keymap (the old g_Keymap, same order as XA_KEY_ID) ---------------------
// { SDL keycode, default modifiers for the on-screen cap, display name }.
static const XA_KEY_INFO s_Keymap[XKI_COUNT] = {
    { 0, 0, "No Key" },
    { SDLK_ESCAPE, 0, "ESCAPE" },
    { SDLK_F1, 0, "F1" },
    { SDLK_F2, 0, "F2" },
    { SDLK_F3, 0, "F3" },
    { SDLK_F4, 0, "F4" },
    { SDLK_F5, 0, "F5" },
    { SDLK_F6, 0, "F6" },
    { SDLK_F7, 0, "F7" },
    { SDLK_F8, 0, "F8" },
    { SDLK_F9, 0, "F9" },
    { SDLK_F10, 0, "F10" },
    { SDLK_F11, 0, "F11" },
    { SDLK_F12, 0, "F12" },
    { SDLK_SCROLLLOCK, 0, "SCROLLLOCK" },
    { SDLK_PAUSE, 0, "PAUSE" },
    { SDLK_BACKQUOTE, 0, "`" },
    { SDLK_1, 0, "1" },
    { SDLK_2, 0, "2" },
    { SDLK_3, 0, "3" },
    { SDLK_4, 0, "4" },
    { SDLK_5, 0, "5" },
    { SDLK_6, 0, "6" },
    { SDLK_7, 0, "7" },
    { SDLK_8, 0, "8" },
    { SDLK_9, 0, "9" },
    { SDLK_0, 0, "0" },
    { SDLK_MINUS, 0, "MINUS" },
    { SDLK_EQUALS, 0, "EQUALS" },
    { SDLK_BACKSPACE, 0, "BACKSPACE" },
    { SDLK_INSERT, HM_KEYPAD, "INSERT" },
    { SDLK_HOME, HM_KEYPAD, "HOME" },
    { SDLK_PAGEUP, HM_KEYPAD, "PAGEUP" },
    { SDLK_NUMLOCKCLEAR, HM_KEYPAD, "NUMLOCK" },
    { SDLK_KP_DIVIDE, HM_KEYPAD, "DIVIDE" },
    { SDLK_KP_MULTIPLY, HM_KEYPAD, "MULTIPLY" },
    { SDLK_KP_MINUS, HM_KEYPAD, "MINUS" },
    { SDLK_TAB, 0, "TAB" },
    { SDLK_q, 0, "Q" },
    { SDLK_w, 0, "W" },
    { SDLK_e, 0, "E" },
    { SDLK_r, 0, "R" },
    { SDLK_t, 0, "T" },
    { SDLK_y, 0, "Y" },
    { SDLK_u, 0, "U" },
    { SDLK_i, 0, "I" },
    { SDLK_o, 0, "O" },
    { SDLK_p, 0, "P" },
    { SDLK_LEFTBRACKET, 0, "[" },
    { SDLK_RIGHTBRACKET, 0, "]" },
    { SDLK_RETURN, 0, "RETURN" },
    { SDLK_DELETE, HM_KEYPAD, "DELETE" },
    { SDLK_END, HM_KEYPAD, "END" },
    { SDLK_PAGEDOWN, HM_KEYPAD, "PAGEDOWN" },
    { SDLK_KP_7, HM_KEYPAD, "7" },
    { SDLK_KP_8, HM_KEYPAD, "8" },
    { SDLK_KP_9, HM_KEYPAD, "9" },
    { SDLK_KP_PLUS, HM_KEYPAD, "PLUS" },
    { SDLK_CAPSLOCK, 0, "CAPSLOCK" },
    { SDLK_a, 0, "A" },
    { SDLK_s, 0, "S" },
    { SDLK_d, 0, "D" },
    { SDLK_f, 0, "F" },
    { SDLK_g, 0, "G" },
    { SDLK_h, 0, "H" },
    { SDLK_j, 0, "J" },
    { SDLK_k, 0, "K" },
    { SDLK_l, 0, "L" },
    { SDLK_SEMICOLON, 0, ";" },
    { SDLK_QUOTE, 0, "'" },
    { SDLK_BACKSLASH, 0, "\\" },
    { SDLK_KP_4, HM_KEYPAD, "4" },
    { SDLK_KP_5, HM_KEYPAD, "5" },
    { SDLK_KP_6, HM_KEYPAD, "6" },
    { SDLK_z, 0, "Z" },
    { SDLK_x, 0, "X" },
    { SDLK_c, 0, "C" },
    { SDLK_v, 0, "V" },
    { SDLK_b, 0, "B" },
    { SDLK_n, 0, "N" },
    { SDLK_m, 0, "M" },
    { SDLK_COMMA, 0, "," },
    { SDLK_PERIOD, 0, "." },
    { SDLK_SLASH, 0, "/" },
    { SDLK_UP, HM_KEYPAD, "UP" },
    { SDLK_KP_1, HM_KEYPAD, "1" },
    { SDLK_KP_2, HM_KEYPAD, "2" },
    { SDLK_KP_3, HM_KEYPAD, "3" },
    { SDLK_KP_ENTER, HM_KEYPAD, "ENTER" },
    { SDLK_LGUI, HM_KEYPAD, "LGUI" },
    { SDLK_SPACE, 0, "SPACE" },
    { SDLK_RGUI, HM_KEYPAD, "RGUI" },
    { SDLK_MENU, HM_KEYPAD, "MENU" },
    { SDLK_LEFT, HM_KEYPAD, "LEFT" },
    { SDLK_DOWN, HM_KEYPAD, "DOWN" },
    { SDLK_RIGHT, HM_KEYPAD, "RIGHT" },
    { SDLK_KP_0, HM_KEYPAD, "0" },
    { SDLK_KP_PERIOD, HM_KEYPAD, "." },
    { MOUSE_WHEEL_SCROLL_UP, HM_MOUSE_WHEEL, "MOUSE SCROLL UP" },
    { MOUSE_WHEEL_PRESS, HM_MOUSE_WHEEL, "MOUSE WHELL" }, // (sic) original table
    { MOUSE_WHEEL_SCROLL_DOWN, HM_MOUSE_WHEEL, "MOUSE SCROLL UP" },
    { MOUSE_X_UP, HM_MOUSE_X, "MOUSE X UP" },
    { MOUSE_X_DOWN, HM_MOUSE_X, "MOUSE X DOWN" },
};

const XA_KEY_INFO &HotkeyKeyInfo(int id)
{
    if (id <= XKI_INVALID || id >= XKI_COUNT)
    {
        return s_Keymap[XKI_INVALID];
    }
    return s_Keymap[id];
}

const char *HotkeyKeycodeName(uint keycode)
{
    // GetKeycodeName: linear scan over the same table
    for (int i = 1; i < XKI_COUNT; i++)
    {
        if (s_Keymap[i].Key == keycode && s_Keymap[i].Modifiers == 0)
        {
            return s_Keymap[i].KeyName;
        }
    }
    for (int i = 1; i < XKI_COUNT; i++)
    {
        if (s_Keymap[i].Key == keycode)
        {
            return s_Keymap[i].KeyName;
        }
    }
    return s_Keymap[XKI_INVALID].KeyName;
}

void HotkeyFormatText(uint key, uchar modifiers, char *out, size_t size)
{
    // the old GetHotkeyText formatting
    if (size == 0)
    {
        return;
    }
    out[0] = '\0';
    if ((modifiers & HM_ALT) != 0)
    {
        snprintf(out, size, "Alt ");
    }
    if ((modifiers & HM_CTRL) != 0)
    {
        if (out[0] != '\0')
        {
            snprintf(out + strlen(out), size - strlen(out), "+ ");
        }
        snprintf(out + strlen(out), size - strlen(out), "Ctrl ");
    }
    if ((modifiers & HM_SHIFT) != 0)
    {
        if (out[0] != '\0')
        {
            snprintf(out + strlen(out), size - strlen(out), "+ ");
        }
        snprintf(out + strlen(out), size - strlen(out), "Shift ");
    }
    if ((modifiers & HM_KEYPAD) != 0)
    {
        if (out[0] != '\0')
        {
            snprintf(out + strlen(out), size - strlen(out), "+ ");
        }
        snprintf(out + strlen(out), size - strlen(out), "Pad ");
    }
    if (out[0] != '\0')
    {
        snprintf(out + strlen(out), size - strlen(out), "+ ");
    }

    if ((modifiers & HM_MOUSE_WHEEL) != 0)
    {
        if (key == MOUSE_WHEEL_SCROLL_UP)
        {
            snprintf(out + strlen(out), size - strlen(out), "Wheel Up");
        }
        else if (key == MOUSE_WHEEL_SCROLL_DOWN)
        {
            snprintf(out + strlen(out), size - strlen(out), "Wheel Down");
        }
        else if (key == MOUSE_WHEEL_PRESS)
        {
            snprintf(out + strlen(out), size - strlen(out), "Wheel Press");
        }
    }
    else if ((modifiers & HM_MOUSE_X) != 0)
    {
        if (key == MOUSE_X_UP)
        {
            snprintf(out + strlen(out), size - strlen(out), "MouseX 1");
        }
        else if (key == MOUSE_X_DOWN)
        {
            snprintf(out + strlen(out), size - strlen(out), "MouseX 2");
        }
    }
    else
    {
        snprintf(out + strlen(out), size - strlen(out), "%s", HotkeyKeycodeName(key));
    }
}

namespace {

// Platform::IsKeypad (the SDL path): the keypad identity of
// a key event - reproduces the Windows native-modifier bit 24 of the original app.
bool is_keypad_keycode(uint key)
{
    switch (key)
    {
        case SDLK_KP_0:
        case SDLK_KP_1:
        case SDLK_KP_2:
        case SDLK_KP_3:
        case SDLK_KP_4:
        case SDLK_KP_5:
        case SDLK_KP_6:
        case SDLK_KP_7:
        case SDLK_KP_8:
        case SDLK_KP_9:
        case SDLK_KP_COLON:
        case SDLK_KP_COMMA:
        case SDLK_KP_DECIMAL:
        case SDLK_KP_DIVIDE:
        case SDLK_KP_ENTER:
        case SDLK_KP_MINUS:
        case SDLK_KP_MULTIPLY:
        case SDLK_KP_PERIOD:
        case SDLK_KP_PLUS:
            return true;
        default:
            return false;
    }
}

// eventFilter parity: bare modifier presses are never a binding.
bool is_bare_modifier(uint key)
{
    switch (key)
    {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
        case SDLK_LALT:
        case SDLK_RALT:
        case SDLK_RCTRL:
        case SDLK_LCTRL:
            return true;
        default:
            return false;
    }
}

// AddModifiers parity: SDL key mod -> the HM_* bitmask.
uchar modifiers_from_sdl(ushort mod)
{
    uchar result = HM_NONE;
    if ((mod & KMOD_ALT) != 0)
    {
        result |= HM_ALT;
    }
    if ((mod & KMOD_CTRL) != 0)
    {
        result |= HM_CTRL;
    }
    if ((mod & KMOD_SHIFT) != 0)
    {
        result |= HM_SHIFT;
    }
    return result;
}

} // namespace

// --- the engine ---------------------------------------------------------------

CHotkeys g_Hotkeys;

bool Hotkeys_IsCapturing()
{
    return g_Hotkeys.IsCapturing();
}

bool Hotkeys_ProcessCapture(const SDL_Event &ev)
{
    return g_Hotkeys.IsCapturing() && g_Hotkeys.ProcessEvent(ev);
}

bool Hotkeys_ProcessGameEvent(const SDL_Event &ev)
{
    return !g_Hotkeys.IsCapturing() && g_Hotkeys.ProcessEvent(ev);
}

void CHotkeys::BeginCapture()
{
    m_Capturing = true;
    m_CaptureKey = 0;
    m_CaptureModifiers = 0;
}

bool CHotkeys::ProcessEvent(const SDL_Event &ev)
{
    switch (ev.type)
    {
        case SDL_KEYDOWN:
            return ProcessKeyDown(ev);
        case SDL_KEYUP:
            return ProcessKeyUp(ev);
        case SDL_MOUSEBUTTONDOWN:
            return ProcessMouseButtonDown(ev);
        case SDL_MOUSEWHEEL:
            return ProcessWheel(ev);
        default:
            return false;
    }
}

bool CHotkeys::ProcessKeyDown(const SDL_Event &ev)
{
    const uint key = (uint)ev.key.keysym.sym;
    uchar modifiers = modifiers_from_sdl(ev.key.keysym.mod);
    if (is_keypad_keycode(key))
    {
        modifiers |= HM_KEYPAD;
    }

    // capture first: the field eats the next non-repeat, non-modifier key
    if (m_Capturing)
    {
        if (ev.key.repeat || is_bare_modifier(key))
        {
            return true; // swallowed either way (the capture stays armed)
        }
        m_CaptureKey = key;
        m_CaptureModifiers = modifiers;
        return true;
    }

    // key auto-repeat never re-fires a binding (see hotkeys.h note)
    if (ev.key.repeat)
    {
        return false;
    }

    return MatchAndUse(key, modifiers);
}

bool CHotkeys::ProcessKeyUp(const SDL_Event &)
{
    // UOMSG_INPUT_KEYUP was a no-op in the original app
    return false;
}

bool CHotkeys::ProcessMouseButtonDown(const SDL_Event &ev)
{
    // eventFilter MouseButtonPress parity: middle = wheel press, X1/X2
    uint key = 0;
    uchar mouseFlag = HM_NONE;
    if (ev.button.button == SDL_BUTTON_MIDDLE)
    {
        key = MOUSE_WHEEL_PRESS;
        mouseFlag = HM_MOUSE_WHEEL;
    }
    else if (ev.button.button == SDL_BUTTON_X1)
    {
        key = MOUSE_X_UP;
        mouseFlag = HM_MOUSE_X;
    }
    else if (ev.button.button == SDL_BUTTON_X2)
    {
        key = MOUSE_X_DOWN;
        mouseFlag = HM_MOUSE_X;
    }
    else
    {
        return false;
    }

    const uchar modifiers = modifiers_from_sdl(SDL_GetModState()) | mouseFlag;
    if (m_Capturing)
    {
        m_CaptureKey = key;
        m_CaptureModifiers = modifiers;
        return true;
    }
    return MatchAndUse(key, modifiers);
}

bool CHotkeys::ProcessWheel(const SDL_Event &ev)
{
    // eventFilter Wheel parity: delta direction picks scroll up/down
    uint key = 0;
    if (ev.wheel.y > 0)
    {
        key = MOUSE_WHEEL_SCROLL_UP;
    }
    else if (ev.wheel.y < 0)
    {
        key = MOUSE_WHEEL_SCROLL_DOWN;
    }
    else
    {
        return false;
    }

    const uchar modifiers = modifiers_from_sdl(SDL_GetModState()) | HM_MOUSE_WHEEL;
    if (m_Capturing)
    {
        m_CaptureKey = key;
        m_CaptureModifiers = modifiers;
        return true;
    }
    return MatchAndUse(key, modifiers);
}

bool CHotkeys::MatchAndUse(uint key, uchar modifiers)
{
    // HotkeyExistsAndUse: the original loop bailed out when
    // not logged in (g_World == nullptr)
    if (g_World == nullptr)
    {
        return false;
    }
    // A binding "exists" when the
    // key is assigned (Key != 0, the enabled checkbox); the engine switch
    // gates everything but HT_TOGGLE_HOTKEYS.
    const vector<CHotkeyEntry> &hotkeys = g_AssistantConfig.m_Hotkeys;
    bool matched = false;
    for (const CHotkeyEntry &entry : hotkeys)
    {
        if (entry.Key != key || entry.Modifiers != modifiers)
        {
            continue;
        }

        matched = true;
        const uint type = entry.Type;
        const uint action = entry.Action;
        if (type == HT_TOGGLE_HOTKEYS)
        {
            const bool state = (action == 0) ? !g_AssistantConfig.OnOffHotkeys() : (action == 1);
            if (g_AssistantConfig.m_OnOffHotkeys != state)
            {
                g_AssistantConfig.m_OnOffHotkeys = state;
                g_AssistantConfig.SaveMainOptions(); // saves on mutation
            }
            g_AssistantEvents->OnClientPrint(
                state ? "Hotkeys now is enabled" : "Hotkeys now is disabled");
        }
        else if (g_AssistantConfig.OnOffHotkeys())
        {
            switch (type)
            {
                case HT_PLAY_MACRO:
                    g_MacroExecutor.Play(entry.ScriptOrMacroName, entry.RunningMode);
                    break;
                case HT_STOP_MACRO:
                    g_MacroExecutor.Stop();
                    break;
                case HT_SCRIPT:
                    // No script engine attached (external code stays inert);
                    // the binding is still recognized + consumed.
                    break;
                case HT_EXTERNAL_CODE:
                    Actions_RunExternalCode(entry.ExternalCode);
                    break;
                default:
                    Actions_Execute(entry);
                    break;
            }
        }
        break; // the original loop always stopped at the first (key, mod) match
    }

    if (!matched)
    {
        return false;
    }

    // Alt+F4 always falls through to the game
    if (key == SDLK_F4 && (modifiers & HM_ALT) != 0)
    {
        return false;
    }
    // "Pass keys to client": a matched binding is consumed unless checked
    return !g_AssistantConfig.PassHotkeys();
}

int CHotkeys::FindDuplicate(uint key, uchar modifiers, int skipRow) const
{
    const vector<CHotkeyEntry> &hotkeys = g_AssistantConfig.m_Hotkeys;
    for (int i = 0; i < (int)hotkeys.size(); i++)
    {
        if (i == skipRow)
        {
            continue;
        }
        if (hotkeys[i].Key == key && hotkeys[i].Modifiers == modifiers)
        {
            return i;
        }
    }
    return -1;
}

void CHotkeys::CommitKeyToRow(int row, uint key, uchar modifiers)
{
    vector<CHotkeyEntry> &hotkeys = g_AssistantConfig.m_Hotkeys;
    if (row < 0 || row >= (int)hotkeys.size())
    {
        return;
    }

    // CheckExists: the edited row must already exist
    // (a fresh "Blank hotkey" row assigns to itself; the original required a current
    // row and returned early otherwise)
    const int dup = FindDuplicate(key, modifiers, row);
    if (dup != -1)
    {
        // open the question popup: replace / save duplicate / select existing
        m_DuplicateRow = dup;
        m_PendingKey = key;
        m_PendingModifiers = modifiers;
        m_PendingTargetRow = row;
        return;
    }

    hotkeys[row].Key = key;
    hotkeys[row].Modifiers = modifiers;
    g_AssistantConfig.SaveHotkeys(); // saves on mutation
}

bool CHotkeys::AssignCaptureToRow(int row)
{
    if (m_DuplicateRow != -1)
    {
        return false; // the question popup is still open
    }
    if (m_CaptureKey == 0)
    {
        return true; // nothing captured yet
    }
    CommitKeyToRow(row, m_CaptureKey, m_CaptureModifiers);
    return m_DuplicateRow == -1;
}

void CHotkeys::ResolveDuplicate(int answer)
{
    if (m_DuplicateRow == -1)
    {
        return;
    }
    if (answer == 2)
    {
        // "Select Existing": leave everything as it was
        m_DuplicateRow = -1;
        m_PendingKey = 0;
        m_PendingModifiers = 0;
        m_PendingTargetRow = -1;
        return;
    }
    ApplyPending();
    if (answer == 0)
    {
        // "Replace": strip the key from every OTHER row that had it (the
        // pending target row just received it in ApplyPending)
        vector<CHotkeyEntry> &hotkeys = g_AssistantConfig.m_Hotkeys;
        for (int i = 0; i < (int)hotkeys.size(); i++)
        {
            CHotkeyEntry &entry = hotkeys[i];
            if (i != m_PendingTargetRow && entry.Key == m_PendingKey &&
                entry.Modifiers == m_PendingModifiers)
            {
                entry.Key = 0;
                entry.Modifiers = 0;
            }
        }
        g_AssistantConfig.SaveHotkeys();
    }
    m_DuplicateRow = -1;
    m_PendingKey = 0;
    m_PendingModifiers = 0;
    m_PendingTargetRow = -1;
}

void CHotkeys::ApplyPending()
{
    vector<CHotkeyEntry> &hotkeys = g_AssistantConfig.m_Hotkeys;
    if (m_PendingTargetRow < 0 || m_PendingTargetRow >= (int)hotkeys.size())
    {
        return;
    }
    hotkeys[m_PendingTargetRow].Key = m_PendingKey;
    hotkeys[m_PendingTargetRow].Modifiers = m_PendingModifiers;
    g_AssistantConfig.SaveHotkeys();
}

void CHotkeys::KeyboardAssign(int keyId, int row)
{
    // on_KeyButtonClicked: the cap's own default
    // modifiers plus the sticky/physically-held ones
    const XA_KEY_INFO &info = HotkeyKeyInfo(keyId);
    uchar modifiers = info.Modifiers;
    const uchar live = modifiers_from_sdl(SDL_GetModState());
    modifiers |= (uchar)(m_StickyModifiers | live);
    CommitKeyToRow(row, info.Key, modifiers);
}

void CHotkeys::KeyboardToggleModifier(int keyId)
{
    // on_ModifierKeyButtonClicked: AltGr behaves like Alt
    uchar flag = 0;
    switch (keyId)
    {
        case XKI_SHIFT_L:
        case XKI_SHIFT_R:
            flag = HM_SHIFT;
            break;
        case XKI_CTRL_L:
        case XKI_CTRL_R:
            flag = HM_CTRL;
            break;
        case XKI_ALT:
        case XKI_ALTGR:
            flag = HM_ALT;
            break;
        default:
            return;
    }
    m_StickyModifiers = (uchar)(m_StickyModifiers ^ flag);
}

int CHotkeys::KeyboardCapState(int keyId, int selectedRow) const
{
    // UpdateKeyboard: count the bindings that map to a
    // cap while the current modifier set is held; the selected row wins.
    const XA_KEY_INFO &info = HotkeyKeyInfo(keyId);
    if (info.Key == 0)
    {
        return 0;
    }
    const uchar addMods = HM_ALT | HM_CTRL | HM_SHIFT;
    const uchar currentMods = (uchar)(m_StickyModifiers | modifiers_from_sdl(SDL_GetModState()));
    const vector<CHotkeyEntry> &hotkeys = g_AssistantConfig.m_Hotkeys;
    int found = 0;
    int selected = 0;
    for (int i = 0; i < (int)hotkeys.size(); i++)
    {
        const CHotkeyEntry &entry = hotkeys[i];
        if (entry.Key == 0 || (entry.Modifiers & addMods) != currentMods)
        {
            continue;
        }
        if (entry.Key == info.Key &&
            (info.Modifiers == 0 || (entry.Modifiers & info.Modifiers) != 0))
        {
            found++;
            if (i == selectedRow)
            {
                selected = 1;
            }
        }
    }
    if (found == 0)
    {
        return 0;
    }
    if (found > 1)
    {
        // The original painted the selected row's cap last (UpdateKeyboard's second
        // currentButton pass, hotkeysform.cpp:866): the blue selection wins
        // over the multi-binding red.
        return selected ? 3 : 2; // one more hotkey is set here (red)
    }
    return selected ? 3 : 1;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
