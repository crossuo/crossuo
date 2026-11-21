// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

// Data-driven login screen (CGumpScreenMain).
//
// A theme is an INI file (or url / embedded resource / <data dir>/login.theme)
// describing the widgets of the main login screen: background art, gump art,
// texts, entries, buttons, checkboxes and shard images. When a theme loads,
// CGumpScreenMain::UpdateContent builds it instead of the hardcoded layout.
//
// See docs/LOGIN_THEME.md for the format.

#pragma once

#include <stdint.h>
#include <common/str.h>
#include <common/fs.h>
#include <vector>
#include <map>

#include "GUI/GUIExternalTexture.h" // EXTERNAL_TEXTURE_DRAW_MODE

struct CSprite;

struct LoginThemeImage
{
    astr_t File; // as written in the theme (resolved lazily)
    EXTERNAL_TEXTURE_DRAW_MODE Mode = EXTERNAL_TEXTURE_DRAW_MODE::Native;
    int X = 0;
    int Y = 0;

    // cached decode/upload; decoded on first use, released by
    // LoginTheme_ReleaseImages()
    CSprite *Sprite = nullptr;
    bool Tried = false;
};

enum class LOGIN_THEME_WIDGET_TYPE
{
    None,
    Image,
    Gump,
    GumpTiled,
    Resizepic,
    Text,
    TextBox,
    Button,
    Checkbox,
    Slider,
    HtmlLink,
};

enum class LOGIN_THEME_BIND
{
    None,
    Account,
    Password,
    AutoLogin,
    SavePassword,
    Music,
    MusicVolume,
};

struct LoginThemeWidget
{
    LOGIN_THEME_WIDGET_TYPE Type = LOGIN_THEME_WIDGET_TYPE::None;
    LOGIN_THEME_BIND Bind = LOGIN_THEME_BIND::None;
    astr_t Id;

    // `action = <name>` (buttons): connect, quit, web:<key>, launcher, page:N
    astr_t Action;

    // geometry (all 640x480 screen pixels)
    int X = 0;
    int Y = 0;
    int Width = 0;
    int Height = 0;

    // gump art (16-bit gump ids)
    uint16_t Graphic = 0;
    uint16_t GraphicSelected = 0;
    uint16_t GraphicPressed = 0;
    uint16_t GraphicChecked = 0;
    uint16_t GraphicDisabled = 0;

    // image widget
    astr_t Image;

    // text
    astr_t Text;
    int Font = 0;
    bool Unicode = false;
    uint16_t Color = 0;
    // html_link only: the anchor color. `color` is a no-op for html_link
    // (the engine's html renderer overrides anchor text with the body's
    // link color, blue by default) - use this to set it explicitly
    uint16_t LinkColor = 0;
    uint16_t ColorSelected = 0;
    uint16_t ColorFocused = 0;
    int MaxLength = 0;
    bool Masked = false;
    bool Blink = false;
    astr_t TextPosition; // checkboxes: left/right (+ _center variants)
    astr_t Tooltip;

    // slider
    int MinValue = 0;
    int MaxValue = 0;

    // html_link
    astr_t Url;
};

struct LoginTheme
{
    using ImageMap = std::map<astr_t, LoginThemeImage>;

    astr_t Source;  // description of where the theme came from, for logs
    astr_t BaseDir; // local theme file directory, for relative image paths
    ImageMap Images;
    std::vector<LoginThemeWidget> Widgets;

    const LoginThemeImage *GetImage(const astr_t &name) const;
    const LoginThemeWidget *Find(const astr_t &id) const;
    bool HasBind(LOGIN_THEME_BIND bind) const;
};

// Resolves and loads the theme once (CLI --login-theme, then the login-theme
// config key, then the embedded resource, then <data dir>/login.theme).
// Returns nullptr when no theme is available or every source failed to parse,
// in which case the caller must build the hardcoded layout.
const LoginTheme *LoginTheme_Get();

// Returns the cached sprite for a theme image, decoding and uploading it on
// first use. Requires a live GL context. Returns nullptr when the image could
// not be resolved or decoded (logged once).
CSprite *LoginTheme_GetImageSprite(LoginThemeImage &image);

// Frees every decoded theme sprite and removes the sprites it registered in
// the gump art cache (they are ownerless allocations, so the caller owns that
// bookkeeping).
void LoginTheme_ReleaseImages();
