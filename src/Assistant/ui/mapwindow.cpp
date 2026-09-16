// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant Map window (see mapwindow.h for the transform derivation and
// the input contract). Overlay model: the original CMapDrawObject; the
// populated types are exactly the ones the original draw loops built (grep: party
// out-of-mirror members, mirror NPCs classified by AssMap_GetCharacterType,
// the guild list, the last-corpse marker) plus the tracker point the original app
// forwarded to external UOAM (deviation, now drawn here).

#define LOGGER_MODULE Assistant

#include "mapwindow.h"

#if defined(XUO_ASSISTANT)

#include <common/logging/logging.h>

#include <xuocore/enumlist.h> // MAX_MAPS_COUNT
#include <xuocore/uodata.h>   // g_MapSize (per-facet dimensions)

#include "../core/global.h"
#include "../core/assist.h"
#include "../core/assistantevents.h"
#include "../config/assistantconfig.h"
#include "../managers/assmap.h"
#include "../managers/guildmanager.h"
#include "../core/world.h"
#include "Party.h" // ::g_Party (shared party data)
#include "widgets.h"

#include <math.h>

namespace xuoassist {

namespace {

constexpr float MAP_WINDOW_DEFAULT_WIDTH = 415.0f;  // map.ui
constexpr float MAP_WINDOW_DEFAULT_HEIGHT = 415.0f;

// the original m_CurrentWorldCoordinates initial value
int s_CenterX = 507;
int s_CenterY = 840;
bool s_CenterValid = false; // becomes true on the first pan/follow update

// the drag-pan state (the original m_MousePressed/m_MousePressPoint/m_PressedWorld..)
bool s_Panning = false;
ImVec2 s_PanStartMouse = ImVec2(0.0f, 0.0f);
int s_PanStartCenterX = 0;
int s_PanStartCenterY = 0;

// the tracker point (was the external-UOAM notify)
bool s_TrackActive = false;
int s_TrackX = 0;
int s_TrackY = 0;
uint32_t s_TrackTick = 0;

// overlay working set: reused every frame (no steady-state allocation - the
// vector only ever grows to the peak observed count)
vector<XA_MAP_DRAW_OBJECT> s_Overlays;

} // namespace

void MapWindow_OnMapUpdate() {}

void MapWindow_OnMapRedraw() {}

void MapWindow_OnTrack(bool state, int x, int y)
{
    s_TrackActive = state;
    s_TrackX = x;
    s_TrackY = y;
    s_TrackTick = g_Ticks;
}

void MapWindow_Open()
{
    g_AssistantMapOpen = true;
}

// The window geometry attributes (Map.xml window_x/y/width/height) stay in
// the format and are honored for the first placement below; ongoing geometry
// lives in the ImGui window ini like every other assistant window (the
// original ~Map wrote them back on close - documented delta).
void MapWindow_Shutdown()
{
    g_AssistantMapOpen = false;
    s_Panning = false;
    s_CenterValid = false;
    s_TrackActive = false;
    s_Overlays.clear();
    AssMap_Shutdown(); // drop the radar block memo + hash (idempotent)
    MapSettings_Shutdown();
}

// --- overlay collection (the original DrawMode0) -----------------------------

namespace {

struct MAP_DRAW_STYLE
{
    float Width;
    float Height;
    uint32_t Color;   // settings color record (0xAARRGGBB)
    bool Enabled;
    bool DrawName;
    bool DrawHits;
    bool DrawMana;
    bool DrawStam;
};

ImU32 StyleColor(const MAP_DRAW_STYLE &style)
{
    // 0xAARRGGBB -> IM_COL32(R,G,B,A)
    return IM_COL32(
        (style.Color >> 16) & 0xFF,
        (style.Color >> 8) & 0xFF,
        style.Color & 0xFF,
        (style.Color >> 24) & 0xFF);
}

void fill_styles(MAP_DRAW_STYLE table[6])
{
    table[OMCT_PLAYER] = { 6.0f, 6.0f, g_MapSettings.PlayerColor, true,
                           g_MapSettings.PlayerShowName != 0,
                           g_MapSettings.PlayerShowHits != 0,
                           g_MapSettings.PlayerShowMana != 0,
                           g_MapSettings.PlayerShowStam != 0 };
    table[OMCT_PARTY_MEMBER] = { 4.0f, 4.0f, g_MapSettings.PartyColor,
                                 g_MapSettings.PartyEnabled != 0,
                                 g_MapSettings.PartyShowName != 0,
                                 g_MapSettings.PartyShowHits != 0, false, false };
    table[OMCT_GUILD_MEMBER] = { 4.0f, 4.0f, g_MapSettings.GuildColor,
                                 g_MapSettings.GuildEnabled != 0,
                                 g_MapSettings.GuildShowName != 0,
                                 g_MapSettings.GuildShowHits != 0, false, false };
    table[OMCT_FRIEND] = { 4.0f, 4.0f, g_MapSettings.FriendsColor,
                           g_MapSettings.FriendsEnabled != 0,
                           g_MapSettings.FriendsShowName != 0,
                           g_MapSettings.FriendsShowHits != 0, false, false };
    table[OMCT_ENEMY] = { 4.0f, 4.0f, g_MapSettings.EnemiesColor,
                          g_MapSettings.EnemiesEnabled != 0,
                          g_MapSettings.EnemiesShowName != 0,
                          g_MapSettings.EnemiesShowHits != 0, false, false };
    table[OMCT_OTHER] = { 4.0f, 4.0f, g_MapSettings.OtherColor,
                          g_MapSettings.OtherEnabled != 0,
                          g_MapSettings.OtherShowName != 0,
                          g_MapSettings.OtherShowHits != 0, false, false };
}

} // namespace

bool g_AssistantMapOpen = false;

void MapWindow_Draw(bool *open)
{
    if (open != nullptr && !*open)
    {
        return;
    }

    // per-window opacity (the original setWindowOpacity): alpha =
    // (100 - opacity*25)/100 with the opacity steps 0/25/50 - pushed around
    // this window only, never the global style
    const float alpha = 1.0f - (g_MapSettings.Opacity * 0.25f);
    ImGui::PushStyleVar(
        ImGuiStyleVar_Alpha, alpha < 0.25f ? 0.25f : (alpha > 1.0f ? 1.0f : alpha));

    ImGui::SetNextWindowSize(
        ImVec2(MAP_WINDOW_DEFAULT_WIDTH, MAP_WINDOW_DEFAULT_HEIGHT), ImGuiCond_FirstUseEver);
    if ((g_MapSettings.WindowX != 0) && (g_MapSettings.WindowY != 0))
    {
        ImGui::SetNextWindowPos(
            ImVec2((float)g_MapSettings.WindowX, (float)g_MapSettings.WindowY),
            ImGuiCond_Once);
    }

    if (!ImGui::Begin("Map", open))
    {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const ImVec2 canvasTopLeft = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const int width = canvasSize.x < 32.0f ? 32 : (int)canvasSize.x;
    const int height = canvasSize.y < 32.0f ? 32 : (int)canvasSize.y;
    const int halfW = width / 2;
    const int halfH = height / 2;
    const float scale = MapSettings_Scale();

    // the selected facet: 0xFFFFFFFF follows the client
    int map = (int)g_CurrentMap;
    const uint32_t facet = g_MapSettings.Facet;
    if (facet != 0xFFFFFFFF && facet < MAX_MAPS_COUNT)
    {
        map = (int)facet;
    }

    // follow: player-locked center, current facet only
    CPlayer *player = g_Player;
    const bool following =
        (g_World != nullptr && player != nullptr && g_MapSettings.FollowPlayer &&
         facet == 0xFFFFFFFF);
    if (following)
    {
        s_CenterX = player->GetX();
        s_CenterY = player->GetY();
        s_CenterValid = true;
    }
    else if (!s_CenterValid)
    {
        s_CenterX = 507;
        s_CenterY = 840;
    }

    // the world rect the canvas covers (the overlay cull)
    const int minX = s_CenterX - int(halfW * scale) - 1;
    const int minY = s_CenterY - int(halfH * scale) - 1;
    const int maxX = s_CenterX + int(halfW * scale) + 1;
    const int maxY = s_CenterY + int(halfH * scale) + 1;

    // --- canvas input -----------------------------------------------------
    // the invisible button is the canvas: left drag pans (unless following),
    // wheel zooms, right click opens the menu (the original forwarded all of these
    // through CMapImageWidget; WantCaptureMouse keeps the game out of it)
    ImGui::InvisibleButton("##map_canvas", canvasSize, ImGuiButtonFlags_MouseButtonLeft);
    const bool canvasHovered = ImGui::IsItemHovered();
    const bool canvasActive = ImGui::IsItemActive();

    if (canvasActive && !following)
    {
        if (!s_Panning)
        {
            s_Panning = true;
            s_PanStartMouse = ImGui::GetIO().MouseClickedPos[0];
            s_PanStartCenterX = s_CenterX;
            s_PanStartCenterY = s_CenterY;
        }

        // screen displacement -> world displacement through the inverse of
        // the view transform (mapwindow.h); the center moves opposite so the
        // world point under the cursor follows it
        const float dx = (ImGui::GetIO().MousePos.x - s_PanStartMouse.x) * scale;
        const float dy = (ImGui::GetIO().MousePos.y - s_PanStartMouse.y) * scale;
        int wx;
        int wy;
        if (g_MapSettings.RotateMap)
        {
            wx = s_PanStartCenterX - int((dx + dy) / 2 + 0.5f);
            wy = s_PanStartCenterY - int((dy - dx) / 2 + 0.5f);
        }
        else
        {
            wx = s_PanStartCenterX - int(dx + 0.5f);
            wy = s_PanStartCenterY - int(dy + 0.5f);
        }

        // clamp onto the facet
        if (map >= 0 && map < MAX_MAPS_COUNT)
        {
            const int mapWidth = g_MapSize[map].Width;
            const int mapHeight = g_MapSize[map].Height;
            if (wx > mapWidth)
            {
                wx = mapWidth;
            }
            if (wx < 0)
            {
                wx = 0;
            }
            if (wy > mapHeight)
            {
                wy = mapHeight;
            }
            if (wy < 0)
            {
                wy = 0;
            }
        }
        s_CenterX = wx;
        s_CenterY = wy;
        s_CenterValid = true;
    }
    else
    {
        s_Panning = false;
    }

    // wheel zoom (the original mapWheelEvent: index 1..8 with
    // the x2.0 <-> x4.0 jump, up = out, down = in); the two direction
    // comparisons below already cover the whole non-zero wheel range, so no
    // (!= 0.0f) equality test is needed here
    if (canvasHovered)
    {
        const int zoom = g_MapSettings.Zoom;
        if (ImGui::GetIO().MouseWheel > 0.0f && zoom > 1)
        {
            g_MapSettings.Zoom = zoom == 16 ? 8 : zoom - 1;
        }
        else if (ImGui::GetIO().MouseWheel < 0.0f && zoom <= 8)
        {
            g_MapSettings.Zoom = zoom == 8 ? 16 : zoom + 1;
        }
    }

    // --- radar ---------------------------------------------------------------
    XA_MAP_VIEW view;
    view.Map = map;
    view.CX = s_CenterX;
    view.CY = s_CenterY;
    view.Scale = scale;
    view.Rotated = g_MapSettings.RotateMap != 0;

    ImDrawList *dl = ImGui::GetWindowDrawList();
    const ImTextureRef tex = AssMap_RadarTexture(width, height, view);
    if (tex.GetTexID() != ImTextureID_Invalid)
    {
        dl->AddImage(tex, canvasTopLeft, ImVec2(canvasTopLeft.x + width, canvasTopLeft.y + height));
    }
    else
    {
        dl->AddRectFilled(
            canvasTopLeft,
            ImVec2(canvasTopLeft.x + width, canvasTopLeft.y + height),
            IM_COL32(0, 0, 0, 255));
    }

    // --- overlays -----------------------------------------------------------
    s_Overlays.clear();
    MAP_DRAW_STYLE table[6];
    fill_styles(table);

    if (g_World != nullptr)
    {
        // party members the mirror does not know: from the packet-fed member
        // info
        if (g_MapSettings.PartyEnabled && g_Party.Leader != 0u)
        {
            for (int i = 0; i < 10; i++)
            {
                const uint32_t serial = g_Party.Member[i].Serial;
                if (serial == 0u || g_World->FindWorldCharacter(serial) != nullptr)
                {
                    continue;
                }

                const CPartyObject &info = g_Party.Member[i];
                if (info.MapIndex == (uint8_t)map && info.X != 0 && info.Y != 0 && info.X >= minX &&
                    info.X < maxX && info.Y >= minY && info.Y < maxY)
                {
                    XA_MAP_DRAW_OBJECT item = { OMCT_PARTY_MEMBER, info.X, info.Y, info.Name,
                                                nullptr };
                    s_Overlays.push_back(item);
                }
            }
        }

        // the mirror NPCs. MapIndex guard added: the original
        // bounds check could not reject a foreign-facet character.
        for (CGameObject *obj = (CGameObject *)g_World->m_Items; obj != nullptr; obj = (CGameObject *)obj->m_Next)
        {
            if (!obj->NPC || obj->MapIndex != map)
            {
                continue;
            }
            if (obj->GetX() < minX || obj->GetX() >= maxX || obj->GetY() < minY ||
                obj->GetY() >= maxY)
            {
                continue;
            }

            const uint32_t serial = obj->Serial;
            const CLIENT_MAP_CHARACTER_TYPE type = AssMap_GetCharacterType(serial);
            if (type >= OMCT_CORPSE || !table[type].Enabled)
            {
                continue;
            }

            const astr_t &objName = obj->GetName();
            if (type == OMCT_GUILD_MEMBER)
            {
                // refresh the cached guild name; the guild list draws the marker
                g_GuildManager.SetMemberName(serial, objName);
                continue;
            }

            // copy only when the party-name substitution actually happens
            astr_t substituted;
            const astr_t *name = &objName;
            if (objName.empty() && g_Party.Leader != 0u)
            {
                for (int i = 0; i < 10; i++)
                {
                    if (g_Party.Member[i].Serial == serial)
                    {
                        substituted = g_Party.Member[i].Name;
                        name = &substituted;
                        break;
                    }
                }
            }

            XA_MAP_DRAW_OBJECT item = {
                type, obj->GetX(), obj->GetY(), *name, (CGameCharacter *)obj
            };
            s_Overlays.push_back(item);
        }

        // the guild list markers (the original pushed them with the
        // party type - kept, that is the style they rendered with; iterated
        // over the manager's own store - GetMembers() returns by value and
        // the draw path must not allocate per frame)
        if (table[OMCT_GUILD_MEMBER].Enabled)
        {
            for (const auto &it : g_GuildManager.GetMembersMap())
            {
                const GUILD_MEMBER_INFO &member = it.second;
                if (member.Map == (uint32_t)map && member.X != 0 && member.Y != 0 && member.X >= minX &&
                    member.X < maxX && member.Y >= minY && member.Y < maxY)
                {
                    XA_MAP_DRAW_OBJECT item = { OMCT_PARTY_MEMBER, member.X, member.Y, member.Name,
                                                nullptr };
                    s_Overlays.push_back(item);
                }
            }
        }

        // the last corpse marker (the 5 minute window)
        if (g_MapSettings.ShowLastCorpse &&
            g_LastPlayerDeadTimer != 0u && g_Ticks < g_LastPlayerDeadTimer + 300000u &&
            g_LastPlayerCorpsePosition.X >= minX && g_LastPlayerCorpsePosition.X < maxX &&
            g_LastPlayerCorpsePosition.Y >= minY && g_LastPlayerCorpsePosition.Y < maxY)
        {
            XA_MAP_DRAW_OBJECT item = { OMCT_CORPSE,
                                        g_LastPlayerCorpsePosition.X,
                                        g_LastPlayerCorpsePosition.Y,
                                        "Corpse",
                                        nullptr };
            s_Overlays.push_back(item);
        }

        // the tracker point (was the UOAM notify; drawn here for 10 s)
        if (s_TrackActive && g_AssistantConfig.Tracker() && g_Ticks - s_TrackTick < 10000u &&
            s_TrackX >= minX && s_TrackX < maxX && s_TrackY >= minY && s_TrackY < maxY)
        {
            XA_MAP_DRAW_OBJECT item = { OMCT_INVALID, s_TrackX, s_TrackY, "Track", nullptr };
            s_Overlays.push_back(item);
        }

        // --- overlay pass -------------------------------------------------
        for (const XA_MAP_DRAW_OBJECT &item : s_Overlays)
        {
            float x;
            float y;
            if (view.Rotated)
            {
                const float dx = float(item.X - s_CenterX);
                const float dy = float(item.Y - s_CenterY);
                x = canvasTopLeft.x + halfW + (dx - dy) / scale;
                y = canvasTopLeft.y + halfH + (dx + dy) / scale;
            }
            else
            {
                x = canvasTopLeft.x + halfW + (item.X - s_CenterX) / scale;
                y = canvasTopLeft.y + halfH + (item.Y - s_CenterY) / scale;
            }

            if (item.Type == OMCT_CORPSE)
            {
                // the corpse.png icon + white label; the
                // atlas cell is 32px downsampled from the 128px asset
                const float iconSize = 15.0f; // the original scaled the icon to 15x15
                IconImage(
                    XA_ICON_CORPSE,
                    ImVec2(x - iconSize / 2, y - iconSize / 2),
                    ImVec2(iconSize, iconSize));
                const ImVec2 textSize = ImGui::CalcTextSize(item.Name.c_str());
                dl->AddText(
                    ImVec2(x - textSize.x / 2, y - iconSize / 2 - textSize.y - 1.0f),
                    IM_COL32_WHITE,
                    item.Name.c_str());
                continue;
            }

            if (item.Type == OMCT_INVALID)
            {
                // the tracker crosshair
                dl->AddCircle(ImVec2(x, y), 5.0f, IM_COL32_WHITE, 12, 1.5f);
                dl->AddLine(ImVec2(x - 8.0f, y), ImVec2(x + 8.0f, y), IM_COL32_WHITE, 1.0f);
                dl->AddLine(ImVec2(x, y - 8.0f), ImVec2(x, y + 8.0f), IM_COL32_WHITE, 1.0f);
                continue;
            }

            const MAP_DRAW_STYLE &style = table[item.Type];
            const ImU32 color = StyleColor(style);
            const bool dead =
                (item.Ptr != nullptr && item.Ptr->IsDead() && g_MapSettings.MarkDeadMobiles);

            const float dotW = dead ? 6.0f : style.Width;
            const float dotH = dead ? 6.0f : style.Height;
            const float offX = dotW / 2;
            const float offY = dotH / 2;

            if (style.DrawName)
            {
                const ImVec2 textSize = ImGui::CalcTextSize(item.Name.c_str());
                dl->AddText(
                    ImVec2(x - textSize.x / 2, y - offY - textSize.y - 1.0f),
                    color,
                    item.Name.c_str());
            }

            if (dead)
            {
                // the original dead X
                dl->AddLine(ImVec2(x - offX, y - offY), ImVec2(x + offX, y + offY), color, 1.0f);
                dl->AddLine(ImVec2(x - offX, y + offY), ImVec2(x + offX, y - offY), color, 1.0f);
                continue;
            }

            dl->AddCircleFilled(ImVec2(x, y), dotW / 2, color, 16);

            if (item.Ptr != nullptr && style.DrawHits)
            {
                // the status bars (the original DrawLine): a 20x2 dark
                // strip at the dot bottom, the colored fill scaled by the %
                const ImU32 bg = IM_COL32(0, 0, 0, 127);
                dl->AddRectFilled(ImVec2(x - 10.0f, y), ImVec2(x + 10.0f, y + 2.0f), bg);

                auto draw_bar = [&](short value, short maxValue, float yOffset)
                {
                    float pct = maxValue != 0 ? (value * 100) / float(maxValue) : 0.0f;
                    if (pct > 100.0f)
                    {
                        pct = 100.0f;
                    }
                    const float end = (pct * 18.0f) / 100.0f - 9.0f;
                    if (end > -9.0f)
                    {
                        dl->AddLine(
                            ImVec2(x - 9.0f, y + yOffset), ImVec2(x + end, y + yOffset), color, 1.5f);
                    }
                };

                draw_bar(item.Ptr->Hits, item.Ptr->MaxHits, 1.0f);
                if (style.DrawMana)
                {
                    draw_bar(item.Ptr->Mana, item.Ptr->MaxMana, offY + 3.0f);
                }
                if (style.DrawStam)
                {
                    draw_bar(item.Ptr->Stam, item.Ptr->MaxStam, offY + 5.0f);
                }
            }
        }
    }

    // the coordinate line
    if (g_MapSettings.DisplayCoordinates)
    {
        const char *name =
            (map >= 0 && map < 6) ? g_FacetName[map] : g_FacetName[0];
        char text[64] = { 0 };
        snprintf(text, sizeof(text), "%s, x=%d y=%d", name, s_CenterX, s_CenterY);
        dl->AddText(ImVec2(canvasTopLeft.x + 2.0f, canvasTopLeft.y + 2.0f), IM_COL32_WHITE, text);
    }

    // --- context menu (the original on_miw_Image_customContextMenuRequested,
    //     mirrored here) ------------------------------------------------------
    if (ImGui::BeginPopupContextItem("##map_context"))
    {
        if (ImGui::BeginMenu("Facet"))
        {
            if (ImGui::MenuItem("Current", nullptr, facet == 0xFFFFFFFF))
            {
                MapWindow_OnAction(OMA_FACET, 0xFFFFFFFF);
            }
            for (int i = 0; i < MAX_MAPS_COUNT; i++)
            {
                // every facet is always offered: g_MapSize is a compile-time
                // client table (the original availability check stayed commented
                // out), so selecting an uninstalled one just
                // renders black
                if (g_MapSize[i].Width == 0)
                {
                    continue;
                }
                if (ImGui::MenuItem(g_FacetName[i], nullptr, facet == (uint32_t)i))
                {
                    MapWindow_OnAction(OMA_FACET, (uint32_t)i);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Follow (only for Current facet)"))
        {
            if (ImGui::MenuItem("Player", nullptr, g_MapSettings.FollowPlayer != 0))
            {
                MapWindow_OnAction(OMA_FOLLOW_CHARACTER, g_PlayerSerial);
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Zoom"))
        {
            static const struct
            {
                const char *Label;
                int Index;
            } zooms[] = { { "x0.25", 1 }, { "x0.5", 2 },  { "x0.75", 3 }, { "x1.0", 4 },
                          { "x1.25", 5 }, { "x1.5", 6 },  { "x1.75", 7 }, { "x2.0", 8 },
                          { "x4.0", 16 } };
            for (const auto &z : zooms)
            {
                if (ImGui::MenuItem(z.Label, nullptr, g_MapSettings.Zoom == z.Index))
                {
                    MapWindow_OnAction(OMA_ZOOM, (uint32_t)z.Index);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Rotate map", nullptr, g_MapSettings.RotateMap != 0))
        {
            MapWindow_OnAction(OMA_ROTATE_MAP, 0);
        }
        if (ImGui::MenuItem("Show coordinates", nullptr, g_MapSettings.DisplayCoordinates != 0))
        {
            MapWindow_OnAction(OMA_DISPLAY_COORDINATES, 0);
        }
        if (ImGui::MenuItem("Show altitude map", nullptr, g_MapSettings.AltitudeOnly != 0))
        {
            MapWindow_OnAction(OMA_ALTITUDE_ONLY, 0);
        }
        if (!g_MapSettings.AltitudeOnly)
        {
            if (ImGui::MenuItem("Show statics", nullptr, g_MapSettings.ShowStatics != 0))
            {
                MapWindow_OnAction(OMA_SHOW_STATICS, 0);
            }
            if (ImGui::MenuItem("Apply altitude", nullptr, g_MapSettings.ApplyAltitude != 0))
            {
                MapWindow_OnAction(OMA_APPLY_ALTITUDE, 0);
            }
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Window opacity"))
        {
            static const struct
            {
                const char *Label;
                int Value;
            } opacities[] = { { "Opaque", 0 }, { "25%", 1 }, { "50%", 2 } };
            for (const auto &o : opacities)
            {
                if (ImGui::MenuItem(o.Label, nullptr, g_MapSettings.Opacity == o.Value))
                {
                    MapWindow_OnAction(OMA_TRANSPARENT_WINDOW, (uint32_t)o.Value);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Open display settings"))
        {
            MapWindow_OnAction(OMA_OPEN_SETTINGS, 0);
        }

        ImGui::EndPopup();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// The original Map::OnActionSelect: the context-menu -> settings
// transitions. Kept as a free function so the menu reads like the original.
void MapWindow_OnAction(uint32_t type, uint32_t extra)
{
    bool wantRedraw = true;
    bool wantUpdateImage = false;

    switch (type)
    {
        case OMA_FACET:
        {
            if (g_MapSettings.Facet != extra)
            {
                g_MapSettings.Facet = extra;
                wantUpdateImage = true;
            }
            break;
        }
        case OMA_FOLLOW_CHARACTER:
        {
            g_MapSettings.FollowPlayer = !g_MapSettings.FollowPlayer;
            break;
        }
        case OMA_ZOOM:
        {
            g_MapSettings.Zoom = (int)extra;
            break;
        }
        case OMA_SHOW_STATICS:
        {
            g_MapSettings.ShowStatics = !g_MapSettings.ShowStatics;
            wantUpdateImage = true;
            break;
        }
        case OMA_APPLY_ALTITUDE:
        {
            g_MapSettings.ApplyAltitude = !g_MapSettings.ApplyAltitude;
            wantUpdateImage = true;
            break;
        }
        case OMA_ALTITUDE_ONLY:
        {
            g_MapSettings.AltitudeOnly = !g_MapSettings.AltitudeOnly;
            wantUpdateImage = true;
            break;
        }
        case OMA_TRANSPARENT_WINDOW:
        {
            g_MapSettings.Opacity = (int)extra;
            wantRedraw = false;
            break;
        }
        case OMA_DISPLAY_COORDINATES:
        {
            g_MapSettings.DisplayCoordinates = !g_MapSettings.DisplayCoordinates;
            break;
        }
        case OMA_OPEN_SETTINGS:
        {
            MapSettings_Open();
            return;
        }
        case OMA_ROTATE_MAP:
        {
            g_MapSettings.RotateMap = !g_MapSettings.RotateMap;
            break;
        }
        default:
        {
            wantRedraw = false;
            break;
        }
    }

    if (wantRedraw && wantUpdateImage)
    {
        AssMap_Invalidate();
    }
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
