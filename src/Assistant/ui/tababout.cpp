// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The About tab: the tray.png logo, the four

#define LOGGER_MODULE Assistant

#include <common/logging/logging.h>

#include <SDL_wrapper.h>

#include <external/gfx/imgui/imgui.h>
#include <external/gfx/imgui/imgui_internal.h> // RegisterUserTexture

#include <Platform.h> // ::Platform::OpenBrowser (the client's URL launcher)

#include <string.h>

#include <external/gfx/stb_image.h>

#include "../core/global.h"
#include "widgets.h" // DOCS_URL

#if defined(XUO_ASSISTANT)

namespace xuoassist {

namespace {

// the About logo texture: the full-res tray.png (the atlas cell is a 32px
// downsample; tababout.ui showed the logo at 128x128). Same lifecycle as the
// icon atlas: registered with RefCount == 1 (the backend reclaims the GL
// name during ui_shutdown), CPU pixels freed by TabAbout_AfterImGuiShutdown.
ImTextureData *s_Logo = nullptr;

bool Logo_Init()
{
    if (s_Logo != nullptr)
    {
        return true;
    }
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return false;
    }

    int w = 0;
    int h = 0;
    int comp = 0;
    stbi_uc *data = IconAssetLoad("tray.png", &w, &h, &comp);
    if (data == nullptr)
    {
        Warning(Assistant, "about logo not found: tray.png");
        return false;
    }

    s_Logo = new ImTextureData();
    s_Logo->Create(ImTextureFormat_RGBA32, w, h);
    s_Logo->SetStatus(ImTextureStatus_WantCreate);
    s_Logo->UseColors = true;
    memcpy(s_Logo->GetPixels(), data, (size_t)s_Logo->GetSizeInBytes());
    stbi_image_free(data);
    ImGui::RegisterUserTexture(s_Logo);
    return true;
}

void OpenLink(const char *url)
{
    // the client's own launcher (Platform::OpenBrowser: ShellExecute /
    // xdg-open/open) is reused; SDL is 2.0.10 (no SDL_OpenURL)
    ::Platform::OpenBrowser(url);
}

bool LinkButton(const char *label)
{
    // the stacked 81x23 buttons of tababout.ui
    const bool pressed = ImGui::Button(label, ImVec2(81.0f * 0.75f, 23.0f * 0.75f));
    if (pressed)
    {
        if (strcmp(label, "Website") == 0)
        {
            OpenLink("https://crossuo.com/");
        }
        else if (strcmp(label, "GitHub") == 0)
        {
            OpenLink("https://github.com/crossuo/crossuo");
        }
        else if (strcmp(label, "Discord") == 0)
        {
            OpenLink("https://discord.gg/MPcMHF5");
        }
        else if (strcmp(label, "Docs") == 0)
        {
            OpenLink(DOCS_URL);
        }
    }
    return pressed;
}

} // namespace

void TabAbout_AfterImGuiShutdown()
{
    // the context is gone: free the CPU pixels (the GL name was reclaimed by
    // the backend teardown - the atlas ordering rule)
    delete s_Logo;
    s_Logo = nullptr;
}

void TabAbout_Draw()
{
    if (Logo_Init())
    {
        // the 141x141 logo button of tababout.ui (non-interactive here; the
        // original button had no handler either)
        const float logoSize = 128.0f;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImage(
            s_Logo->GetTexRef(), pos, ImVec2(pos.x + logoSize, pos.y + logoSize));
        ImGui::Dummy(ImVec2(logoSize, logoSize));
        ImGui::SameLine();

        // the stacked link buttons
        ImGui::BeginGroup();
        LinkButton("Website");
        LinkButton("GitHub");
        LinkButton("Discord");
        LinkButton("Docs");
        ImGui::EndGroup();
    }

    ImGui::Separator();

    // the static rich-text block (links disabled)
    ImGui::TextWrapped("Developed by CrossUO Team");
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
