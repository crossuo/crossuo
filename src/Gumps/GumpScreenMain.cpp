// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpScreenMain.h"
#include "TextEngine/EntryText.h"
#include "revision.h"
#include "../Config.h"
#include "../CrossUO.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/ClilocManager.h"
#include "../ScreenStages/MainScreen.h"
#include "../GUI/GUIColoredPolygone.h"
#include "../GUI/GUIExternalTexture.h"
#include "../GUI/GUIHTMLText.h"
#include "../GUI/GUISlider.h"
#include "../Gumps/LoginTheme.h"
#include "../LauncherWindow.h"
#include "xuocore/enumlist.h"
#include "xuocore/uodata_color.h"
#include <set>

enum
{
    ID_MS_NONE,

    ID_MS_QUIT,
    ID_MS_ARROW_NEXT,
    ID_MS_ACCOUNT,
    ID_MS_PASSWORD,
    ID_MS_SAVEPASSWORD,
    ID_MS_AUTOLOGIN,
    ID_MS_UOACCOUNT,
    ID_MS_UOCOM,
    ID_MS_HELP,
    ID_MS_MOVIE,
    ID_MS_CREDITS,
    ID_MS_LAUNCHER,
    // data-driven login screen only (docs/LOGIN_THEME.md)
    ID_MS_MUSIC,
    ID_MS_MUSICVOLUME,

    ID_MS_COUNT,

    //1998 (1.25.32) title screen buttons; MENU1/2/3/5 are the original
    //client's sub-screen buttons (credits, movie, etc.) kept for looks only
    ID_1998_LOGIN,
    ID_1998_LOGON,
    ID_1998_BACK,
    ID_1998_MENU1,
    ID_1998_MENU2,
    ID_1998_MENU3,
    ID_1998_MENU5,
};

CGumpScreenMain::CGumpScreenMain()
    : CGump(GT_NONE, 0, 0, 0)
{
    NoMove = true;
    NoClose = true;
}

CGumpScreenMain::~CGumpScreenMain()
= default;

void CGumpScreenMain::PrepareContent()
{
    static uint32_t lastArrowTick = 0;
    static bool arrowLighted = false;

    //1.25.32 login: mirror the original's entry art state machine - the
    //filler-text box shows while the field is unfocused and empty, the
    //plain box once it holds text, and the bright variant while focused
    if (g_Config.ClientVersion < CV_200 && m_1998AccountArt != nullptr)
    {
        const bool accountFocused = g_EntryPointer == &m_1998AccountEntry->m_Entry;
        int accountArt = 0x411;
        if (accountFocused)
        {
            accountArt = 0x420;
        }
        else if (m_1998AccountEntry->m_Entry.GetTextA().length() != 0)
        {
            accountArt = 0x412;
        }
        if (m_1998AccountArt->Graphic != accountArt)
        {
            m_1998AccountArt->Graphic = accountArt;
            WantRedraw = true;
        }

        const bool passwordFocused = g_EntryPointer == m_PasswordFake;
        int passwordArt = 0x413;
        if (passwordFocused)
        {
            passwordArt = 0x422;
        }
        else if (m_PasswordFake->GetTextA().length() != 0)
        {
            passwordArt = 0x414;
        }
        if (m_1998PasswordArt->Graphic != passwordArt)
        {
            m_1998PasswordArt->Graphic = passwordArt;
            WantRedraw = true;
        }
    }

    // the themed arrow blinks between the theme's normal/selected art, and
    // only when the theme asked for it (`blink = yes`)
    if (m_Themed && m_Arrow != nullptr && m_ThemedArrowBlinks)
    {
        if (lastArrowTick < g_Ticks)
        {
            arrowLighted = !arrowLighted;
            m_Arrow->Graphic = arrowLighted ? m_ThemedArrowArt : m_Arrow->GraphicSelected;
            lastArrowTick = g_Ticks + 500;
            WantRedraw = true;
        }
    }
    else if (!m_Themed && lastArrowTick < g_Ticks && m_Arrow != nullptr)
    {
        arrowLighted = !arrowLighted;
        if (g_Config.ClientVersion < CV_70611)
        {
            if (arrowLighted)
            {
                m_Arrow->Graphic = 0x15A5;
            }
            else
            {
                m_Arrow->Graphic = 0x15A4;
            }
        }
        else
        {
            if (arrowLighted)
            {
                m_Arrow->Graphic = 0x5CB;
            }
            else
            {
                m_Arrow->Graphic = 0x5CD;
            }
        }

        lastArrowTick = g_Ticks + 500;

        WantRedraw = true;
    }
}

void CGumpScreenMain::UpdateContent()
{
    if (m_Items != nullptr)
    {
        return;
    }

    if (const LoginTheme *theme = LoginTheme_Get())
    {
        BuildThemed(*theme);
        return;
    }

    // The 1.25.32 (1998) login screens paint their own backdrop art over a
    // cleared framebuffer; the modern tiled background is not part of them.
    if (g_Config.ClientVersion >= CV_200)
    {
        Add(new CGUIGumppicTiled(0x0E14, 0, 0, 640, 480));

        Add(new CGUIGumppic(0x157C, 0, 0));
    }

    if (DebugRenderScreen())
    {
        // draw-order regression aids: a colored polygone (like the minimap
        // notoriety dots) followed by a textured item, everything drawn after
        // the polygone must not inherit its draw state
        Add(new CGUIColoredPolygone(0, 0, 0, 0, 40, 40, 0xFF0000FF));
        Add(new CGUIGumppic(0x157C, 320, 0));
        return;
    }

    // The 1.25.32 (1998) client's login screens, reproduced with the gump art
    // ids and pixel positions of the original client's MainMenu/AcctLogin.
    // Both screens live in this gump: the title backdrop below is added
    // before any page so it stays visible behind the account panel (the
    // original client did not clear the framebuffer between the two screens);
    // the pages are swapped from GUMP_BUTTON_EVENT_C.
    if (g_Config.ClientVersion < CV_200)
    {
        //fullscreen backdrop (640x480): the inside-of-the-box artwork the
        //original blits into its framebuffer before the title screen; gump
        //0x587 is the post-intro variant, 0x3E8 the intro-skipped one. The
        //framebuffer is never cleared afterwards, so this stays behind both
        //screens
        Add(new CGUIGumppic(0x3E8, 0, 0));

        Add(new CGUIPage(1)); //page 1: the MainMenu (title) plate + buttons

        //MainMenu (title) backdrop (422x240) - page 1 only: the original
        //hides the title gump when the account screen opens, leaving just
        //the baked backdrop behind the login panel
        Add(new CGUIGumppic(0x3F2, 106, 117));

        // The six original title buttons (art up/down/pressed).
        Add(new CGUIButton(ID_1998_MENU1, 0x3F6, 0x3F7, 0x3F8, 145, 199));
        Add(new CGUIButton(ID_1998_MENU2, 0x3FC, 0x3FD, 0x3FE, 257, 130));
        Add(new CGUIButton(ID_1998_MENU3, 0x3FF, 0x400, 0x401, 265, 155));
        Add(new CGUIButton(ID_1998_LOGIN, 0x3F3, 0x3F4, 0x3F5, 247, 187));
        Add(new CGUIButton(ID_1998_MENU5, 0x3F9, 0x3FA, 0x3FB, 415, 199));
        Add(new CGUIButton(ID_MS_QUIT, 0x402, 0x403, 0x404, 281, 284));

        Add(new CGUIPage(2)); //page 2: the AcctLogin (account login) panel

        //login panel (371x256)
        Add(new CGUIGumppic(0x410, 128, 108));

        //account name entry: the three 190x21 box variants are complete
        //pieces with an identical shape - 0x411 carries the "USER I.D."
        //filler text (unfocused + empty), 0x412 is the empty unfocused one
        //and 0x420 the focused one; PrepareContent swaps them by state
        m_1998AccountArt = (CGUIGumppic *)Add(new CGUIGumppic(0x411, 219, 138));
        CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_ACCOUNT, 0x0386, 0x0386, 0x0386, 224, 139, 190, false, 2, TS_LEFT, 0, 30));
        entry->CheckOnSerial = true;
        g_MainScreen.m_Account = &entry->m_Entry;
        m_1998AccountEntry = entry;

        //password entry: the entry itself is not masked, the fake entry keeps
        //the asterisks while the real text goes to g_MainScreen.m_Password;
        //0x413 carries the "PASSWORD" filler, 0x414 empty unfocused, 0x422 focused
        m_1998PasswordArt = (CGUIGumppic *)Add(new CGUIGumppic(0x413, 217, 173));
        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_PASSWORD, 0x0386, 0x0386, 0x0386, 224, 175, 190, false, 2, TS_LEFT, 0, 30));
        entry->CheckOnSerial = true;
        m_PasswordFake = &entry->m_Entry;

        //save password toggle
        g_MainScreen.m_SavePassword = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_SAVEPASSWORD, 0x41B, 0x41D, 0x41B, 261, 212));

        //logon button
        Add(new CGUIButton(ID_1998_LOGON, 0x418, 0x419, 0x41A, 371, 222));
        //back button
        Add(new CGUIButton(ID_1998_BACK, 0x415, 0x416, 0x417, 161, 222));

        Add(new CGUIPage(3)); //page 3: never shown, never selected

        // CMainScreen::Load()/Save() always dereference the auto login
        // checkbox, so keep one alive off-screen (the 1998 screens have none).
        g_MainScreen.m_AutoLogin = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_AUTOLOGIN, 0x00D2, 0x00D3, 0x00D2, 0, 0));

        // the title screen reset lives in CMainScreen::Init: a rebuild here
        // runs after button handlers set the page (ID_1998_LOGIN opens the
        // AcctLogin panel) and would yank the user back mid-flow
        return;
    }

    if (g_Config.ClientVersion >= CV_500A && g_Config.ClientVersion < CV_70611)
    {
        Add(new CGUIGumppic(0x2329, 0, 0));
    }

    if (g_Config.ClientVersion < CV_500A)
    {
        Add(new CGUIGumppic(0x058A, 286, 45));
    }

    CGUIText *text = (CGUIText *)Add(new CGUIText(0x0386, 253, 305));

    if (g_Config.ClientVersion < CV_70611)
    {
        Add(new CGUIGumppic(0x15A0, 0, 4));
        Add(new CGUIResizepic(0, 0x13BE, 128, 288, 451, 157));

        //quit button
        Add(new CGUIButton(ID_MS_QUIT, 0x1589, 0x158A, 0x158B, 555, 4));

        text->CreateTextureA(2, "Log in to Ultima Online"); // cliloc 3000038

        text = (CGUIText *)Add(new CGUIText(0x0386, 183, 345));
        text->CreateTextureA(2, "Account Name"); // cliloc 3000039

        text = (CGUIText *)Add(new CGUIText(0x0386, 183, 385));
        text->CreateTextureA(2, "Password"); // cliloc 3000040

        Add(new CGUIResizepic(ID_MS_ACCOUNT, 0x0BB8, 328, 343, 210, 30));
        Add(new CGUIResizepic(ID_MS_PASSWORD, 0x0BB8, 328, 383, 210, 30));
        CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_ACCOUNT, 0x034F, 0x03E3, 0x0021, 335, 343, 190, false, 5, TS_LEFT, 0, 32));
        entry->CheckOnSerial = true;
        g_MainScreen.m_Account = &entry->m_Entry;

        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_PASSWORD, 0x034F, 0x03E3, 0x0021, 335, 385, 190, false, 5, TS_LEFT, 0, 32));
        entry->CheckOnSerial = true;
        m_PasswordFake = &entry->m_Entry;

        m_Arrow =
            (CGUIButton *)Add(new CGUIButton(ID_MS_ARROW_NEXT, 0x15A4, 0x15A5, 0x15A6, 610, 445));
#if defined(XUO_LAUNCHER_EMBEDDED)
        Add(new CGUIButton(ID_MS_LAUNCHER, 0x08D1, 0x08D1, 0x08D1, 40, 430));
#endif
        g_MainScreen.m_SavePassword = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_SAVEPASSWORD, 0x00D2, 0x00D3, 0x00D2, 328, 417));
        g_MainScreen.m_SavePassword->SetTextParameters(
            9, "Save Password", 0x0386, STP_RIGHT_CENTER);

#if !defined(XUO_EMSC)
        g_MainScreen.m_AutoLogin = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_AUTOLOGIN, 0x00D2, 0x00D3, 0x00D2, 183, 417));
        g_MainScreen.m_AutoLogin->SetTextParameters(9, "Auto Login", 0x0386, STP_RIGHT_CENTER);
#endif

        text = (CGUIText *)Add(new CGUIText(0x034E, 286, 455));
        text->CreateTextureA(9, astr_t("UO Version " + g_Config.ClientVersionString + "."));

        text = (CGUIText *)Add(new CGUIText(0x034E, 286, 467));
        text->CreateTextureA(9, astr_t(CLIENT_VERSION));
    }
    else
    {
        Add(new CGUIGumppic(0x014e, 0, 0));

        //Accout button
        Add(new CGUIButton(ID_MS_UOACCOUNT, 0x5C4, 0x5C2, 0x5C3, 10, 25));
        //UO.com button
        Add(new CGUIButton(ID_MS_UOCOM, 0x5C7, 0x5C5, 0x5C6, 10, 135));
        //Quit Button
        Add(new CGUIButton(ID_MS_QUIT, 0x5CA, 0x5C8, 0x5C9, 20, 240));

        //Help Button
        Add(new CGUIButton(ID_MS_HELP, 0x5D3, 0x5D1, 0x5D2, 530, 35));
        //Movie button
        Add(new CGUIButton(ID_MS_MOVIE, 0x5D6, 0x5D4, 0x5D5, 530, 80));
        //Credit button
        Add(new CGUIButton(ID_MS_CREDITS, 0x5D0, 0x5CE, 0x5CF, 530, 125));

        //TEEN RATING
        Add(new CGUIGumppic(0x2335, 30, 380));

#if defined(XUO_LAUNCHER_EMBEDDED)
        Add(new CGUIButton(ID_MS_LAUNCHER, 0x08D1, 0x08D1, 0x08D1, 590, 430));
#endif

        Add(new CGUIResizepic(ID_MS_ACCOUNT, 0x0BB8, 220, 280, 210, 30));
        Add(new CGUIResizepic(ID_MS_PASSWORD, 0x0BB8, 220, 335, 210, 30));
        CGUITextEntry *entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_ACCOUNT, 0x034F, 0x03E3, 0x0021, 228, 283, 190, false, 5, TS_LEFT, 0, 32));
        entry->CheckOnSerial = true;
        g_MainScreen.m_Account = &entry->m_Entry;

        entry = (CGUITextEntry *)Add(new CGUITextEntry(
            ID_MS_PASSWORD, 0x034F, 0x03E3, 0x0021, 228, 338, 190, false, 5, TS_LEFT, 0, 32));
        entry->CheckOnSerial = true;
        m_PasswordFake = &entry->m_Entry;

        m_Arrow =
            (CGUIButton *)Add(new CGUIButton(ID_MS_ARROW_NEXT, 0x5CD, 0x5CB, 0x5CC, 280, 365));
        g_MainScreen.m_SavePassword = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_SAVEPASSWORD, 0x00D2, 0x00D3, 0x00D2, 328, 417));
        g_MainScreen.m_SavePassword->SetTextParameters(9, "Save Password", 0x802, STP_RIGHT_CENTER);

#if !defined(XUO_EMSC)
        g_MainScreen.m_AutoLogin = (CGUICheckbox *)Add(
            new CGUICheckbox(ID_MS_AUTOLOGIN, 0x00D2, 0x00D3, 0x00D2, 183, 417));
        g_MainScreen.m_AutoLogin->SetTextParameters(9, "Auto Login", 0x802, STP_RIGHT_CENTER);
#endif

        text = (CGUIText *)Add(new CGUIText(0x802, 235, 440));
        text->CreateTextureA(9, astr_t("UO Version " + g_Config.ClientVersionString + "."));
        text = (CGUIText *)Add(new CGUIText(0x7F2, 200, 460));
        text->CreateTextureA(9, astr_t(CLIENT_VERSION));
    }
}

void CGumpScreenMain::InitToolTip()
{
    if (!g_ConfigManager.UseToolTips || g_SelectedObject.Object == nullptr)
    {
        return;
    }

    uint32_t id = g_SelectedObject.Serial;

    // theme-provided tooltip text wins over the built-in one
    if (const astr_t *custom = ThemedTooltip((int)id))
    {
        g_ToolTip.Set(wstr_from(*custom));
        return;
    }

    switch (id)
    {
        case ID_MS_QUIT:
        {
            g_ToolTip.Set(L"Quit Ultima Online", 80);
            break;
        }
        case ID_MS_LAUNCHER:
        {
            g_ToolTip.Set(L"Open X:UO Launcher", 120);
            break;
        }
        case ID_MS_ARROW_NEXT:
        {
            g_ToolTip.Set(L"Next screen");
            break;
        }
        case ID_MS_ACCOUNT:
        {
            g_ToolTip.Set(L"Click here to enter your user name", 150);
            break;
        }
        case ID_MS_PASSWORD:
        {
            g_ToolTip.Set(L"Click here to enter your password", 150);
            break;
        }
        case ID_MS_SAVEPASSWORD:
        {
            g_ToolTip.Set(L"Save your password in config file", 150);
            break;
        }
        case ID_MS_AUTOLOGIN:
        {
            g_ToolTip.Set(L"Auto LogIn (without selection screens)", 150);
            break;
        }
        default:
            break;
    }
}

void CGumpScreenMain::GUMP_BUTTON_EVENT_C
{
    switch (serial)
    {
        case ID_MS_QUIT:
        {
            g_MainScreen.CreateSmoothAction(CMainScreen::ID_SMOOTH_MS_QUIT);
            break;
        }
        case ID_MS_ARROW_NEXT:
        case ID_1998_LOGON:
        {
            g_MainScreen.CreateSmoothAction(CMainScreen::ID_SMOOTH_MS_CONNECT);
            break;
        }
        case ID_1998_LOGIN:
        {
            Page = 2; //AcctLogin panel page
            WantRedraw = true;
            WantUpdateContent = true; //re-run RecalculateSize for the page-2 gump extent
            break;
        }
        case ID_1998_BACK:
        {
            Page = 1; //MainMenu title page
            WantRedraw = true;
            WantUpdateContent = true;
            break;
        }
#if defined(XUO_LAUNCHER_EMBEDDED)
        case ID_MS_LAUNCHER:
        {
            LauncherWindow_Open();
            break;
        }
#endif
        case ID_MS_MUSIC:
        {
            if (g_MainScreen.m_Music != nullptr)
            {
                // the login music toggle writes the same setting the options
                // gump uses; the sound manager follows the config setter
                g_ConfigManager.SetMusic(g_MainScreen.m_Music->Checked);
                if (g_MainScreen.m_MusicVolume != nullptr)
                {
                    g_MainScreen.m_MusicVolume->Visible = g_MainScreen.m_Music->Checked;
                }
            }
            break;
        }
        case ID_MS_UOACCOUNT:
        {
            Platform::OpenBrowser(g_Config.ShardAccount);
            break;
        }
        case ID_MS_UOCOM:
        {
            Platform::OpenBrowser(g_Config.ShardWeb);
            break;
        }
        case ID_MS_HELP:
        {
            Platform::OpenBrowser(g_Config.ShardHelp);
            break;
        }
        case ID_MS_MOVIE:
        {
            Platform::OpenBrowser(g_Config.ShardVideo);
            break;
        }
        case ID_MS_CREDITS:
        {
            Platform::OpenBrowser(g_Config.ShardCredits);
            break;
        }
        default:
            break;
    }
}

void CGumpScreenMain::GUMP_TEXT_ENTRY_EVENT_C
{
    if (serial == ID_MS_PASSWORD)
    {
        g_MainScreen.m_Password->SetPos(m_PasswordFake->Pos());
    }
}

// ---------------------------------------------------------------------------
// Data-driven login screen (see docs/LOGIN_THEME.md)
//
// The theme describes widgets; this maps each one onto the same CGUI classes
// the hardcoded layout uses, and onto the fixed ID_MS_* serials so the button
// dispatch below stays in one place. Required bindings (account, password,
// both checkboxes) get hidden defaults when a theme omits them because
// CMainScreen::Load()/Save() dereference them unconditionally.
// ---------------------------------------------------------------------------
namespace
{
int SerialForBind(LOGIN_THEME_BIND bind)
{
    switch (bind)
    {
        case LOGIN_THEME_BIND::Account:
            return ID_MS_ACCOUNT;
        case LOGIN_THEME_BIND::Password:
            return ID_MS_PASSWORD;
        case LOGIN_THEME_BIND::AutoLogin:
            return ID_MS_AUTOLOGIN;
        case LOGIN_THEME_BIND::SavePassword:
            return ID_MS_SAVEPASSWORD;
        case LOGIN_THEME_BIND::Music:
            return ID_MS_MUSIC;
        case LOGIN_THEME_BIND::MusicVolume:
            return ID_MS_MUSICVOLUME;
        default:
            return ID_MS_NONE;
    }
}

int SerialForAction(const astr_t &action)
{
    if (action == "connect")
        return ID_MS_ARROW_NEXT;
    if (action == "quit")
        return ID_MS_QUIT;
    if (action == "launcher")
        return ID_MS_LAUNCHER;
    if (action == "web:account")
        return ID_MS_UOACCOUNT;
    if (action == "web:web")
        return ID_MS_UOCOM;
    if (action == "web:help")
        return ID_MS_HELP;
    if (action == "web:video")
        return ID_MS_MOVIE;
    if (action == "web:credits")
        return ID_MS_CREDITS;
    // page:<n> switches pages without firing the button event
    return ID_MS_NONE;
}

SLIDER_TEXT_POSITION ParseTextPosition(const astr_t &value)
{
    if (value.empty() || value == "right")
        return STP_RIGHT;
    if (value == "left")
        return STP_LEFT;
    if (value == "right_center" || value == "right-center")
        return STP_RIGHT_CENTER;
    if (value == "left_center" || value == "left-center")
        return STP_LEFT_CENTER;
    if (value == "top" || value == "bottom")
        return value == "top" ? STP_TOP : STP_BOTTOM;
    return STP_RIGHT;
}

astr_t ExpandThemeText(const astr_t &text)
{
    astr_t out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '\\' && i + 1 < text.size())
        {
            const char next = text[i + 1];
            if (next == 'n')
            {
                out.push_back('\n');
                i++;
                continue;
            }
            if (next == '1')
            {
                out += g_Config.ClientVersionString;
                i++;
                continue;
            }
            if (next == '2')
            {
                out += astr_t("UO Version ") + g_Config.ClientVersionString + ".";
                i++;
                continue;
            }
        }
        out.push_back(text[i]);
    }
    return out;
}

} // namespace

void CGumpScreenMain::BuildThemed(const LoginTheme &theme)
{
    Info(Client, "login theme: building %d themed widgets", (int)theme.Widgets.size());
    if (theme.HasBind(LOGIN_THEME_BIND::Account) || theme.HasBind(LOGIN_THEME_BIND::Password))
    {
        m_Themed = true;
    }

    // widgets are laid out in file order, so a theme controls the draw order
    // (backgrounds first, entries over their art, buttons last)
    for (const auto &widget : theme.Widgets)
    {
        switch (widget.Type)
        {
            case LOGIN_THEME_WIDGET_TYPE::Image:
            {
                auto *image = const_cast<LoginThemeImage *>(theme.GetImage(widget.Image));
                if (image == nullptr)
                {
                    break;
                }
                CSprite *sprite = LoginTheme_GetImageSprite(*image);
                if (sprite == nullptr)
                {
                    break;
                }
                // a widget box overrides the image's own placement; without
                // one the image sits at the theme's anchor and keeps its size
                const int x = widget.Width || widget.Height ? widget.X : image->X;
                const int y = widget.Width || widget.Height ? widget.Y : image->Y;
                const int w = widget.Width ? widget.Width : sprite->Width;
                const int h = widget.Height ? widget.Height : sprite->Height;
                auto *external = new CGUIExternalTexture(sprite, false, x, y, w, h);
                // the screen is a fixed-size surface: images are click-through
                external->SelectOnly = false;
                external->DrawOnly = true;
                external->BoundingBoxCheck = true;
                external->DrawMode = image->Mode;
                Add(external);
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::Gump:
            {
                Add(new CGUIGumppic(widget.Graphic, widget.X, widget.Y));
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::GumpTiled:
            {
                Add(new CGUIGumppicTiled(
                    widget.Graphic, widget.X, widget.Y, widget.Width, widget.Height));
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::Resizepic:
            {
                Add(new CGUIResizepic(
                    SerialForBind(widget.Bind),
                    widget.Graphic,
                    widget.X,
                    widget.Y,
                    widget.Width,
                    widget.Height));
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::Text:
            {
                if (widget.Unicode && !g_FontManager.UnicodeFontExists((uint8_t)widget.Font))
                {
                    Warning(
                        Client,
                        "login theme: widget '%s' uses font %d, which the client has no data"
                        " for (bitmap fonts are 0-19; using font 0)",
                        widget.Id.c_str(),
                        widget.Font);
                }

                auto *text = new CGUIText(widget.Color, widget.X, widget.Y);
                Add(text);
                const auto &raw = widget.Text;
                if (raw.rfind("cliloc:", 0) == 0)
                {
                    const auto cliloc = g_ClilocManager.ParseArgumentsToClilocString(
                        (uint32_t)str_to_int(raw.substr(7)), true, L"");
                    text->CreateTextureW(widget.Font, cliloc, 30, widget.Width, TS_LEFT, 0);
                }
                else
                {
                    if (widget.Unicode)
                    {
                        auto t = wstr_from_utf8(ExpandThemeText(raw));
                        text->CreateTextureW(
                            widget.Font, t, widget.Color, widget.Width, TS_LEFT, 0);
                    }
                    else
                    {
                        text->CreateTextureA(
                            widget.Font, ExpandThemeText(raw), widget.Width, TS_LEFT, 0);
                    }
                }
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::TextBox:
            {
                const int serial = SerialForBind(widget.Bind);
                if (serial == ID_MS_NONE)
                {
                    break;
                }
                const uint16_t color = widget.Color ? widget.Color : 0x034F;
                const uint16_t selected = widget.ColorSelected ? widget.ColorSelected : color;
                const uint16_t focused = widget.ColorFocused ? widget.ColorFocused : selected;
                auto *entry = (CGUITextEntry *)Add(new CGUITextEntry(
                    serial,
                    color,
                    selected,
                    focused,
                    widget.X,
                    widget.Y,
                    widget.Width,
                    false,
                    (uint8_t)widget.Font,
                    TS_LEFT,
                    0,
                    widget.MaxLength));
                entry->CheckOnSerial = true;
                if (widget.Bind == LOGIN_THEME_BIND::Account)
                {
                    g_MainScreen.m_Account = &entry->m_Entry;
                }
                else
                {
                    // the fake entry keeps the asterisks; the real password
                    // goes to g_MainScreen.m_Password (see GUMP_TEXT_ENTRY_EVENT)
                    m_PasswordFake = &entry->m_Entry;
                }
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::Button:
            {
                const int serial = SerialForAction(widget.Action);
                const int targetPage =
                    widget.Action.rfind("page:", 0) == 0 ? str_to_int(widget.Action.substr(5)) : -1;
                if (serial == ID_MS_NONE && targetPage < 0)
                {
                    Warning(
                        Client,
                        "login theme: button '%s' has no action, dropped",
                        widget.Id.c_str());
                    break;
                }
                auto *button = (CGUIButton *)Add(new CGUIButton(
                    serial,
                    widget.Graphic,
                    widget.GraphicSelected ? widget.GraphicSelected : widget.Graphic,
                    widget.GraphicPressed ? widget.GraphicPressed : widget.Graphic,
                    widget.X,
                    widget.Y));
                button->ToPage = targetPage;
                if (widget.Action == "connect")
                {
                    m_Arrow = button;
                    m_ThemedArrowArt = widget.Graphic;
                    m_ThemedArrowBlinks = widget.Blink;
                }
                if (!widget.Tooltip.empty())
                {
                    m_Tooltips[serial] = widget.Tooltip;
                }
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::Checkbox:
            {
                const int serial = SerialForBind(widget.Bind);
                if (serial == ID_MS_NONE)
                {
                    break;
                }
                auto *checkbox = (CGUICheckbox *)Add(new CGUICheckbox(
                    serial,
                    widget.Graphic,
                    widget.GraphicChecked ? widget.GraphicChecked : widget.Graphic,
                    widget.GraphicDisabled ? widget.GraphicDisabled : widget.Graphic,
                    widget.X,
                    widget.Y));
                if (!widget.Text.empty())
                {
                    checkbox->SetTextParameters(
                        (uint8_t)widget.Font,
                        ExpandThemeText(widget.Text),
                        widget.Color,
                        ParseTextPosition(widget.TextPosition));
                }
                if (serial == ID_MS_AUTOLOGIN)
                {
                    g_MainScreen.m_AutoLogin = checkbox;
                }
                else if (serial == ID_MS_SAVEPASSWORD)
                {
                    g_MainScreen.m_SavePassword = checkbox;
                }
                else if (serial == ID_MS_MUSIC)
                {
                    g_MainScreen.m_Music = checkbox;
                    checkbox->Checked = g_ConfigManager.GetMusic();
                }
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::Slider:
            {
                if (widget.Bind != LOGIN_THEME_BIND::MusicVolume)
                {
                    break;
                }
                // the slider art is the client's own; a theme only places it
                auto *slider = (CGUISlider *)Add(new CGUISlider(
                    ID_MS_MUSICVOLUME,
                    0x00D2,
                    0x00D3,
                    0x00D2,
                    0,
                    false,
                    false,
                    widget.X,
                    widget.Y,
                    widget.Width,
                    widget.MinValue,
                    widget.MaxValue ? widget.MaxValue : 100,
                    g_ConfigManager.GetMusicVolume()));
                g_MainScreen.m_MusicVolume = slider;
                break;
            }
            case LOGIN_THEME_WIDGET_TYPE::HtmlLink:
            {
                // one-line HTML link; the clickable rects are resolved by the
                // gump's HTML text click path into OnDirectHTMLLink
                // html text only renders with a unifont face (0-19); a theme
                // that used a bitmap index falls back to 1, which LoadFonts
                // always provides
                uint8_t htmlFont = (uint8_t)widget.Font;
                if (!g_FontManager.UnicodeFontExists(htmlFont))
                {
                    Warning(
                        Client,
                        "login theme: html_link '%s' uses font %d, which is not a unicode font"
                        " (0-19); using font 1",
                        widget.Id.c_str(),
                        widget.Font);
                    htmlFont = 1;
                }
                // the HTML builder resolves <a href> to a clickable rect and
                // OnDirectHTMLLink opens it. The html renderer colors anchor
                // text ONLY from the <body link=> attribute (the classic html
                // rule: an anchor overrides any <font> with m_WebLinkColor,
                // blue by default) - `color` is a no-op for html_link widgets.
                // `link-color` sets it; without one the blue default applies.
                astr_t body;
                if (widget.LinkColor != 0)
                {
                    // uo_color16_to_32 returns the desktop pixel word
                    // 0x00BBGGRR; the html parser reads "#RRGGBB"
                    const uint32_t pixel = uo_color16_to_32(widget.LinkColor);
                    char buf[32] = {};
                    snprintf(
                        buf,
                        sizeof(buf),
                        "<body link=\"#%02X%02X%02X\">",
                        (pixel >> 0) & 0xFF,
                        (pixel >> 8) & 0xFF,
                        (pixel >> 16) & 0xFF);
                    body = buf;
                }
                // no closing tags after the link: the HTML reader starts a new
                // line for a line break it sees outside a link, which would
                // make a one-line link render two rows tall
                body += "<a href=\"" + widget.Url + "\">" + ExpandThemeText(widget.Text) + "</a>";

                auto *html = (CGUIHTMLText *)Add(new CGUIHTMLText(
                    0,
                    htmlFont,
                    widget.Color,
                    widget.X,
                    widget.Y,
                    widget.Width ? widget.Width : 150,
                    TS_LEFT,
                    0,
                    0xFFFFFFFF));
                html->Text = wstr_from(body);
                html->Create(false);
                // a link wider than its box is split over several lines, which
                // is rarely what a shard intends: say so once
                const int textWidth =
                    g_FontManager.GetWidthW(htmlFont, wstr_from(ExpandThemeText(widget.Text)));
                if (widget.Width > 0 && textWidth > widget.Width)
                {
                    Warning(
                        Client,
                        "login theme: html_link '%s' text is %dpx wide but its box is %dpx;"
                        " raise `width` to keep it on one line",
                        widget.Id.c_str(),
                        textWidth,
                        widget.Width);
                }
                break;
            }
            default:
                break;
        }
    }

    // theme art ids come from the shard's data: warn once per id when the
    // client cannot resolve it, so a shard owner sees a typo or a data file
    // that is not the one the theme was authored against
    std::set<uint16_t> missingArt;
    for (const auto &widget : theme.Widgets)
    {
        for (const uint16_t graphic : { widget.Graphic,
                                        widget.GraphicSelected,
                                        widget.GraphicPressed,
                                        widget.GraphicChecked,
                                        widget.GraphicDisabled })
        {
            if (graphic == 0 || !missingArt.insert(graphic).second)
            {
                continue;
            }
            if (g_Game.ExecuteGump(graphic) == nullptr)
            {
                Warning(
                    Client,
                    "login theme: graphic 0x%04X (widget '%s') is not in the data "
                    "files",
                    graphic,
                    widget.Id.c_str());
            }
        }
    }

    // the entry pointers and checkboxes CMainScreen::Load()/Save() use must
    // exist even for a minimal theme: keep off-screen defaults for the ones
    // the theme left out (same trick as the 1.25.32 pages)
    if (g_MainScreen.m_Account == nullptr)
    {
        auto *entry = (CGUITextEntry *)Add(
            new CGUITextEntry(ID_MS_ACCOUNT, 0x034F, 0x03E3, 0x0021, 0, 0, 190, false, 5));
        entry->CheckOnSerial = true;
        g_MainScreen.m_Account = &entry->m_Entry;
    }
    if (m_PasswordFake == nullptr)
    {
        auto *entry = (CGUITextEntry *)Add(
            new CGUITextEntry(ID_MS_PASSWORD, 0x034F, 0x03E3, 0x0021, 0, 0, 190, false, 5));
        entry->CheckOnSerial = true;
        m_PasswordFake = &entry->m_Entry;
    }
    if (g_MainScreen.m_AutoLogin == nullptr)
    {
        g_MainScreen.m_AutoLogin =
            (CGUICheckbox *)Add(new CGUICheckbox(ID_MS_AUTOLOGIN, 0x00D2, 0x00D3, 0x00D2, 0, 0));
    }
    if (g_MainScreen.m_SavePassword == nullptr)
    {
        g_MainScreen.m_SavePassword =
            (CGUICheckbox *)Add(new CGUICheckbox(ID_MS_SAVEPASSWORD, 0x00D2, 0x00D3, 0x00D2, 0, 0));
    }
}

const astr_t *CGumpScreenMain::ThemedTooltip(int serial) const
{
    auto it = m_Tooltips.find(serial);
    return it == m_Tooltips.end() ? nullptr : &it->second;
}
