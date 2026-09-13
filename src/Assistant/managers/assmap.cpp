// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant map module, logic half (see assmap.h for the dedup map):
// settings record + GlobalConfig/Map.xml persistence (the original format, exact
// attribute set), the zoom->scale rule, the character classification and the
// client-API radar sampler with its single-texture cache.

#define LOGGER_MODULE Assistant

#include "assmap.h"

#if defined(XUO_ASSISTANT)

#include <common/logging/logging.h>

#include <external/gfx/imgui/imgui.h>
#include <external/gfx/imgui/imgui_internal.h> // RegisterUserTexture

#include <xuocore/mulstruct.h>  // RADAR_MAP_BLOCK / MAP_BLOCK / STATICS_BLOCK
#include <xuocore/uodata.h>     // g_MapSize / g_MapBlockSize

#include "Managers/MapManager.h"    // ::g_MapManager (radar blocks via GetIndex)
#include "Managers/ColorManager.h" // ::g_ColorManager (radarcol + hue)
#include "CrossUO.h"              // ::g_Game (season graphics)
#include "GameObjects/RenderStaticObject.h" // CRenderStaticObject::IsNoDrawTile

#include "../config/xml.h"
#include "../config/assistantconfig.h" // GetGlobalConfigPath
#include "../core/assist.h"            // RawStringToBool/BoolToText
#include "../core/textconv.h"          // TextToSerial/SerialToText
#include "../model/gameobjects/gamecharacter.h"
#include "../model/gameobjects/gameworld.h"
#include "../managers/partymanager.h"
#include "../managers/guildmanager.h"

namespace xuoassist {

CMapSettings g_MapSettings;

const char *g_FacetName[6] = { "Felucca", "Trammel", "Ilshenar", "Malas", "Tokuno", "TerMur" };

float MapSettings_Scale()
{
    float scale = g_MapSettings.GetZoom() * 0.25f;

    if (scale < 0.25f)
    {
        scale = 0.25f;
    }
    else if (scale > 2.0f)
    {
        scale = 4.0f;
    }

    return scale;
}

CLIENT_MAP_CHARACTER_TYPE AssMap_GetCharacterType(const uint &serial)
{
    if (g_World == nullptr)
    {
        return OMCT_OTHER;
    }
    if (serial == g_PlayerSerial)
    {
        return OMCT_PLAYER;
    }

    if (g_PartyManager.Contains(serial))
    {
        return OMCT_PARTY_MEMBER;
    }
    if (g_GuildManager.Contains(serial))
    {
        return OMCT_GUILD_MEMBER;
    }
    if (g_AssistantConfig.InFriendList(serial))
    {
        return OMCT_FRIEND;
    }
    if (g_AssistantConfig.InEnemyList(serial))
    {
        return OMCT_ENEMY;
    }

    return OMCT_OTHER;
}

// --- settings (the original CMapSettings) ------------------------------------

void CMapSettings::Copy(const CMapSettings &settings)
{
    m_ShowStatics = settings.GetShowStatics();
    m_ApplyAltitude = settings.GetApplyAltitude();
    m_AltitudeOnly = settings.GetAltitudeOnly();
    m_SmoothImage = settings.GetSmoothImage();
    m_DisplayCoordinates = settings.GetDisplayCoordinates();
    m_ShowOnLogin = settings.GetShowOnLogin();
    m_ShowLastCorpse = settings.GetShowLastCorpse();
    m_MarkDeadMobiles = settings.GetMarkDeadMobiles();
    m_RotateMap = settings.GetRotateMap();

    m_PlayerFontStyle = settings.GetPlayerFontStyle();
    m_PlayerColor = settings.GetPlayerColor();
    m_PlayerShowHits = settings.GetPlayerShowHits();
    m_PlayerShowMana = settings.GetPlayerShowMana();
    m_PlayerShowStam = settings.GetPlayerShowStam();
    m_PlayerShowName = settings.GetPlayerShowName();

    m_PartyEnabled = settings.GetPartyEnabled();
    m_PartyFontStyle = settings.GetPartyFontStyle();
    m_PartyColor = settings.GetPartyColor();
    m_PartyShowHits = settings.GetPartyShowHits();
    m_PartyShowName = settings.GetPartyShowName();

    m_GuildEnabled = settings.GetGuildEnabled();
    m_GuildFontStyle = settings.GetGuildFontStyle();
    m_GuildColor = settings.GetGuildColor();
    m_GuildShowHits = settings.GetGuildShowHits();
    m_GuildShowName = settings.GetGuildShowName();

    m_FriendsEnabled = settings.GetFriendsEnabled();
    m_FriendsFontStyle = settings.GetFriendsFontStyle();
    m_FriendsColor = settings.GetFriendsColor();
    m_FriendsShowHits = settings.GetFriendsShowHits();
    m_FriendsShowName = settings.GetFriendsShowName();

    m_EnemiesEnabled = settings.GetEnemiesEnabled();
    m_EnemiesFontStyle = settings.GetEnemiesFontStyle();
    m_EnemiesColor = settings.GetEnemiesColor();
    m_EnemiesShowHits = settings.GetEnemiesShowHits();
    m_EnemiesShowName = settings.GetEnemiesShowName();

    m_OtherEnabled = settings.GetOtherEnabled();
    m_OtherFontStyle = settings.GetOtherFontStyle();
    m_OtherColor = settings.GetOtherColor();
    m_OtherShowHits = settings.GetOtherShowHits();
    m_OtherShowName = settings.GetOtherShowName();
}

// attribute helpers over the shared xml module (the original code inlined
// hasAttribute guards per attribute; Attribute/AttributeInt already default)
static void load_bool(const XMLElement &e, const char *name, bool *value)
{
    if (e.HasAttribute(name))
    {
        *value = CAssistant::RawStringToBool(e.Attribute(name));
    }
}

static void load_int(const XMLElement &e, const char *name, int *value)
{
    if (e.HasAttribute(name))
    {
        *value = e.AttributeInt(name);
    }
}

// Packed RGBA text form (the original code round-tripped the ARGB uint through
// CAssistant::SerialToText/TextToSerial - same helpers here)
static void load_color(const XMLElement &e, const char *name, uint *value)
{
    if (e.HasAttribute(name))
    {
        *value = TextToSerial(e.Attribute(name));
    }
}

static void set_bool(XMLElement &e, const char *name, bool value)
{
    e.SetAttribute(name, CAssistant::BoolToText(value));
}

static void set_int(XMLElement &e, const char *name, int value)
{
    char buf[32] = { 0 };
    snprintf(buf, sizeof(buf), "%d", value);
    e.SetAttribute(name, buf);
}

static void set_color(XMLElement &e, const char *name, uint value)
{
    e.SetAttribute(name, CAssistant::SerialToText(value));
}

void CMapSettings::Load()
{
    const fs_path path = fs_path_join(CAssistantConfig::GetGlobalConfigPath(), "Map.xml");
    XMLDocument doc;
    if (!doc.Load(path))
    {
        return; // absent file = defaults (the original code silently kept defaults too)
    }

    const XMLElement &e = doc.Root;

    load_int(e, "window_x", &m_WindowX);
    load_int(e, "window_y", &m_WindowY);
    load_int(e, "window_width", &m_WindowWidth);
    load_int(e, "window_height", &m_WindowHeight);

    load_bool(e, "show_statics", &m_ShowStatics);
    load_bool(e, "apply_altitude", &m_ApplyAltitude);
    load_bool(e, "altitude_only", &m_AltitudeOnly);
    load_bool(e, "smooth_image", &m_SmoothImage);
    load_bool(e, "display_coordinates", &m_DisplayCoordinates);
    load_bool(e, "show_on_login", &m_ShowOnLogin);
    load_bool(e, "show_last_corpse", &m_ShowLastCorpse);
    load_bool(e, "mark_dead_mobiles", &m_MarkDeadMobiles);
    load_int(e, "opacity", &m_Opacity);
    load_bool(e, "rotate_map", &m_RotateMap);

    load_int(e, "player_font_style", &m_PlayerFontStyle);
    load_color(e, "player_color", &m_PlayerColor);
    load_bool(e, "player_show_hits", &m_PlayerShowHits);
    load_bool(e, "player_show_mana", &m_PlayerShowMana);
    load_bool(e, "player_show_stam", &m_PlayerShowStam);
    load_bool(e, "player_show_name", &m_PlayerShowName);

    load_bool(e, "party_enabled", &m_PartyEnabled);
    load_int(e, "party_font_style", &m_PartyFontStyle);
    load_color(e, "party_color", &m_PartyColor);
    load_bool(e, "party_show_hits", &m_PartyShowHits);
    load_bool(e, "party_show_name", &m_PartyShowName);

    load_bool(e, "guild_enabled", &m_GuildEnabled);
    load_int(e, "guild_font_style", &m_GuildFontStyle);
    load_color(e, "guild_color", &m_GuildColor);
    load_bool(e, "guild_show_hits", &m_GuildShowHits);
    load_bool(e, "guild_show_name", &m_GuildShowName);

    load_bool(e, "friends_enabled", &m_FriendsEnabled);
    load_int(e, "friends_font_style", &m_FriendsFontStyle);
    load_color(e, "friends_color", &m_FriendsColor);
    load_bool(e, "friends_show_hits", &m_FriendsShowHits);
    load_bool(e, "friends_show_name", &m_FriendsShowName);

    load_bool(e, "enemies_enabled", &m_EnemiesEnabled);
    load_int(e, "enemies_font_style", &m_EnemiesFontStyle);
    load_color(e, "enemies_color", &m_EnemiesColor);
    load_bool(e, "enemies_show_hits", &m_EnemiesShowHits);
    load_bool(e, "enemies_show_name", &m_EnemiesShowName);

    load_bool(e, "other_enabled", &m_OtherEnabled);
    load_int(e, "other_font_style", &m_OtherFontStyle);
    load_color(e, "other_color", &m_OtherColor);
    load_bool(e, "other_show_hits", &m_OtherShowHits);
    // Format quirk: this attribute is spelled
    // "other_sho_name" in BOTH write/read directions - kept for byte compatibility.
    load_bool(e, "other_sho_name", &m_OtherShowName);
}

void CMapSettings::Save()
{
    XMLDocument doc;
    XMLElement &e = doc.Root;
    e.Name = "data";

    set_int(e, "window_x", m_WindowX);
    set_int(e, "window_y", m_WindowY);
    set_int(e, "window_width", m_WindowWidth);
    set_int(e, "window_height", m_WindowHeight);

    set_bool(e, "show_statics", m_ShowStatics);
    set_bool(e, "apply_altitude", m_ApplyAltitude);
    set_bool(e, "altitude_only", m_AltitudeOnly);
    set_bool(e, "smooth_image", m_SmoothImage);
    set_bool(e, "display_coordinates", m_DisplayCoordinates);
    set_bool(e, "show_on_login", m_ShowOnLogin);
    set_bool(e, "show_last_corpse", m_ShowLastCorpse);
    set_bool(e, "mark_dead_mobiles", m_MarkDeadMobiles);
    set_int(e, "opacity", m_Opacity);
    set_bool(e, "rotate_map", m_RotateMap);

    set_int(e, "player_font_style", m_PlayerFontStyle);
    set_color(e, "player_color", m_PlayerColor);
    set_bool(e, "player_show_hits", m_PlayerShowHits);
    set_bool(e, "player_show_mana", m_PlayerShowMana);
    set_bool(e, "player_show_stam", m_PlayerShowStam);
    set_bool(e, "player_show_name", m_PlayerShowName);

    set_bool(e, "party_enabled", m_PartyEnabled);
    set_int(e, "party_font_style", m_PartyFontStyle);
    set_color(e, "party_color", m_PartyColor);
    set_bool(e, "party_show_hits", m_PartyShowHits);
    set_bool(e, "party_show_name", m_PartyShowName);

    set_bool(e, "guild_enabled", m_GuildEnabled);
    set_int(e, "guild_font_style", m_GuildFontStyle);
    set_color(e, "guild_color", m_GuildColor);
    set_bool(e, "guild_show_hits", m_GuildShowHits);
    set_bool(e, "guild_show_name", m_GuildShowName);

    set_bool(e, "friends_enabled", m_FriendsEnabled);
    set_int(e, "friends_font_style", m_FriendsFontStyle);
    set_color(e, "friends_color", m_FriendsColor);
    set_bool(e, "friends_show_hits", m_FriendsShowHits);
    set_bool(e, "friends_show_name", m_FriendsShowName);

    set_bool(e, "enemies_enabled", m_EnemiesEnabled);
    set_int(e, "enemies_font_style", m_EnemiesFontStyle);
    set_color(e, "enemies_color", m_EnemiesColor);
    set_bool(e, "enemies_show_hits", m_EnemiesShowHits);
    set_bool(e, "enemies_show_name", m_EnemiesShowName);

    set_bool(e, "other_enabled", m_OtherEnabled);
    set_int(e, "other_font_style", m_OtherFontStyle);
    set_color(e, "other_color", m_OtherColor);
    set_bool(e, "other_show_hits", m_OtherShowHits);
    set_bool(e, "other_sho_name", m_OtherShowName); // format quirk, see Load

    doc.Save(fs_path_join(CAssistantConfig::GetGlobalConfigPath(), "Map.xml"));
}

// --- radar sampling ----------------------------------------------------------
//
// Per-facet RADAR block fill: the body of CMapManager::GetRadarMapBlock
// (src/Managers/MapManager.cpp:272-318) parameterized by map - the client's
// own entry point is pinned to GetActualMap(), which cannot serve the map
// window's explicit facet selection. withStatics=false keeps the block
// land-only (the original ShowStatics toggle).
static void GetRadarMapBlockForMap(
    int map, int blockX, int blockY, bool withStatics, RADAR_MAP_BLOCK &mb)
{
    CIndexMap *indexMap = ::g_MapManager.GetIndex(map, blockX, blockY);
    if (indexMap == nullptr || indexMap->MapAddress == 0)
    {
        return;
    }

    MAP_BLOCK *pmb = (MAP_BLOCK *)indexMap->MapAddress;
    for (int x = 0; x < 8; x++)
    {
        for (int y = 0; y < 8; y++)
        {
            MAP_CELLS &inCell = pmb->Cells[(y * 8) + x];
            RADAR_MAP_CELLS &outCell = mb.Cells[x][y];
            outCell.Graphic = inCell.TileID;
            outCell.Hue = 0u;
            outCell.Z = inCell.Z;
            outCell.IsLand = 1;
        }
    }

    STATICS_BLOCK *sb =
        withStatics ? (STATICS_BLOCK *)indexMap->StaticAddress : nullptr;
    if (sb != nullptr)
    {
        int count = indexMap->StaticCount;

        for (int c = 0; c < count; c++)
        {
            if ((sb->Color != 0u) && sb->Color != 0xFFFF &&
                !CRenderStaticObject::IsNoDrawTile(sb->Color))
            {
                RADAR_MAP_CELLS &outCell = mb.Cells[sb->X][sb->Y];
                if (outCell.Z <= sb->Z)
                {
                    outCell.Graphic = sb->Color;
                    outCell.Hue = sb->Hue;
                    outCell.Z = sb->Z;
                    outCell.IsLand = 0;
                }
            }
            sb++;
        }
    }
}

// The radar color of one cell (GumpMinimap.cpp:186-193 pipeline, minus the
// isometric line table the minimap draws): season graphic -> radarcol -> hue.
// Returns 0x8000 | color16 (the client's 16bpp alpha-bit convention).
static uint16_t RadarCellColor(const RADAR_MAP_CELLS &cell)
{
    uint32_t color = cell.Graphic;

    if (cell.IsLand == 0)
    {
        color = ::g_Game.GetSeasonGraphic(color) + 0x4000;
    }
    else
    {
        color = ::g_Game.GetLandSeasonGraphic(color);
    }

    uint32_t result = 0x8000 | ::g_ColorManager.GetRadarColorData(color);
    if (cell.Hue != 0u)
    {
        result = 0x8000 | ::g_ColorManager.GetColor16(uint16_t(result & 0xFFFF), cell.Hue);
    }

    return uint16_t(result);
}

// 16bpp 555/alpha-bit color -> opaque RGBA32 (the stb atlas format)
static uint32_t Color16ToRGBA32(uint16_t color)
{
    const int r = ((color >> 10) & 0x1F) * 8;
    const int g = ((color >> 5) & 0x1F) * 8;
    const int b = (color & 0x1F) * 8;
    return 0xFF000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | uint32_t(r);
}

// --- texture cache -----------------------------------------------------------

namespace {

// One RGBA texture the size of the map window content. Rebuilds are gated by
// a content hash + a 150 ms floor (the original redraw timer ran at 100 ms; the
// throttle keeps a drag-pan at zoom 0.25 from resampling every frame).
constexpr uint32_t REBUILD_INTERVAL = 150;

ImTextureData *s_Tex = nullptr;
int s_TexWidth = 0;
int s_TexHeight = 0;

// resize retirements: textures queued with WantDestroyNextFrame; once the
// backend reports Destroyed they are unregistered and freed (they cannot be
// deleted early: they still sit in the context's UserTextures list)
vector<ImTextureData *> s_Retired;

uint64_t s_Hash = 0;
uint32_t s_LastBuildTick = 0;
bool s_Invalidated = false;

// block memo for the current build pass (one index lookup per 8x8 cells)
struct
{
    int Map = -1;
    int BlockX = -1;
    int BlockY = -1;
    RADAR_MAP_BLOCK Block = {};
    bool Valid = false;
} s_Memo;

uint64_t fnv1a(uint64_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t i = 0; i < size; i++)
    {
        hash ^= bytes[i];
        hash *= 0x100000001b3ull;
    }
    return hash;
}

// The radar color of world tile (x, y) with the altitude modifiers from the
// original sampler applied; returns a packed RGBA32 pixel.
uint32_t SampleTile(int map, int x, int y)
{
    const int blockX = x >> 3;
    const int blockY = y >> 3;
    if (!s_Memo.Valid || s_Memo.Map != map || s_Memo.BlockX != blockX || s_Memo.BlockY != blockY)
    {
        s_Memo.Map = map;
        s_Memo.BlockX = blockX;
        s_Memo.BlockY = blockY;
        s_Memo.Block = RADAR_MAP_BLOCK{};
        GetRadarMapBlockForMap(
            map, blockX, blockY, g_MapSettings.GetShowStatics(), s_Memo.Block);
        s_Memo.Valid = true;
    }

    const RADAR_MAP_CELLS &cell = s_Memo.Block.Cells[x & 7][y & 7];

    if (g_MapSettings.GetAltitudeOnly())
    {
        // grayscale ramp over the Z range
        const int z = (cell.Z + 127) * 32 / 256;
        const uint16_t c = uint16_t(((z & 0x1F) << 10) | ((z & 0x1F) << 5) | (z & 0x1F));
        return Color16ToRGBA32(0x8000 | c);
    }

    uint16_t color = RadarCellColor(cell);
    if (g_MapSettings.GetApplyAltitude() && cell.IsLand != 0 && cell.Graphic < 0x4000)
    {
        // map.cpp:411-416 blended with a per-channel saturating add here,
        // while the original masked the sum with & 0x1F, letting bright land colors
        // wrap over (visual delta: this clamps to white where the original wrapped to
        // dark). The ramp itself is quantized to ((Z+127)/8)*32/256, so it
        // only spans 0..3 of the 5-bit channel (unlike the AltitudeOnly ramp
        // above, which maps the full Z range with Z=-128 -> 0 exactly); both
        // forms truncate toward zero, so negative Z never wraps under the
        // & 0x1F masks below.
        const int z = ((cell.Z + 127) / 8) * 32 / 256;
        const uint16_t ramp = uint16_t(((z & 0x1F) << 10) | ((z & 0x1F) << 5) | (z & 0x1F));
        uint32_t mixed = 0;
        for (int ch = 0; ch < 3; ch++)
        {
            const int shift = ch * 5;
            int v = int(((color >> shift) & 0x1F) + ((ramp >> shift) & 0x1F));
            if (v > 0x1F)
            {
                v = 0x1F;
            }
            mixed |= (uint32_t)(v & 0x1F) << shift;
        }
        color = uint16_t(0x8000 | mixed);
    }

    return Color16ToRGBA32(color);
}

// Rebuilds s_Tex's pixels for the view (the transform derivation lives in
// ui/mapwindow.cpp; here is only the screen->world gather mapping).
void RebuildPixels(const XA_MAP_VIEW &view)
{
    const int width = s_TexWidth;
    const int height = s_TexHeight;
    const float scale = view.Scale > 0.0f ? view.Scale : 1.0f;
    const int mapWidth = g_MapSize[view.Map].Width;
    const int mapHeight = g_MapSize[view.Map].Height;
    const int halfW = width / 2;
    const int halfH = height / 2;
    uint32_t *pixels = (uint32_t *)s_Tex->GetPixels();

    for (int sy = 0; sy < height; sy++)
    {
        uint32_t *row = pixels + (sy * width);
        for (int sx = 0; sx < width; sx++)
        {
            int wx;
            int wy;
            if (view.Rotated)
            {
                // inverse of the overlay rotation (mapwindow.cpp): the 45deg
                // isometric basis x-y / x+y
                const int dx = int((sx - halfW) * scale);
                const int dy = int((sy - halfH) * scale);
                wx = view.CX + ((dx + dy) / 2);
                wy = view.CY + ((dy - dx) / 2);
            }
            else
            {
                wx = view.CX + int((sx - halfW) * scale);
                wy = view.CY + int((sy - halfH) * scale);
            }

            if (wx < 0 || wy < 0 || wx >= mapWidth || wy >= mapHeight)
            {
                row[sx] = 0xFF000000u; // outside the facet (the original drew black)
                continue;
            }

            row[sx] = SampleTile(view.Map, wx, wy);
        }
    }
}

} // namespace

ImTextureRef AssMap_RadarTexture(int width, int height, const XA_MAP_VIEW &view)
{
    if (ImGui::GetCurrentContext() == nullptr)
    {
        return ImTextureRef();
    }

    // reap the resize retirements the backend finished destroying
    for (size_t i = 0; i < s_Retired.size();)
    {
        ImTextureData *tex = s_Retired[i];
        if (tex->Status == ImTextureStatus_Destroyed)
        {
            ImGui::UnregisterUserTexture(tex);
            delete tex;
            s_Retired.erase(s_Retired.begin() + i);
        }
        else
        {
            i++;
        }
    }

    width = width < 32 ? 32 : (width > 1024 ? 1024 : width);
    height = height < 32 ? 32 : (height > 1024 ? 1024 : height);

    if (s_Tex != nullptr && (s_TexWidth != width || s_TexHeight != height))
    {
        // size change (window resize): the canonical sequence of the atlas
        // itself (imgui_draw.cpp ImFontAtlasTextureAdd) - queue the old one
        // for destroy and register a replacement
        s_Tex->WantDestroyNextFrame = true;
        s_Retired.push_back(s_Tex);
        s_Tex = nullptr;
    }

    if (s_Tex == nullptr)
    {
        s_Tex = new ImTextureData();
        s_Tex->Create(ImTextureFormat_RGBA32, width, height);
        s_Tex->SetStatus(ImTextureStatus_WantCreate);
        s_Tex->UseColors = true;
        memset(s_Tex->GetPixels(), 0, (size_t)s_Tex->GetSizeInBytes());
        ImGui::RegisterUserTexture(s_Tex);
        s_TexWidth = width;
        s_TexHeight = height;
        s_Hash = 0;
    }

    // the content hash: view geometry + everything the sampler reads
    uint64_t hash = 0x9E3779B97F4A7C15ull;
    hash = fnv1a(hash, &view.Map, sizeof(view.Map));
    hash = fnv1a(hash, &view.CX, sizeof(view.CX));
    hash = fnv1a(hash, &view.CY, sizeof(view.CY));
    hash = fnv1a(hash, &view.Scale, sizeof(view.Scale));
    hash = fnv1a(hash, &view.Rotated, sizeof(view.Rotated));
    const bool statics = g_MapSettings.GetShowStatics();
    const bool altitude = g_MapSettings.GetApplyAltitude();
    const bool altitudeOnly = g_MapSettings.GetAltitudeOnly();
    hash = fnv1a(hash, &statics, sizeof(statics));
    hash = fnv1a(hash, &altitude, sizeof(altitude));
    hash = fnv1a(hash, &altitudeOnly, sizeof(altitudeOnly));

    if (hash != s_Hash || s_Invalidated)
    {
        s_Invalidated = false;
        if ((g_Ticks - s_LastBuildTick) >= REBUILD_INTERVAL)
        {
            s_LastBuildTick = g_Ticks;
            s_Hash = hash;
            s_Memo.Valid = false;
            RebuildPixels(view);
            if (s_Tex->Status == ImTextureStatus_WantCreate)
            {
                // first frame of this texture: the create upload carries the
                // pixels - do NOT downgrade the status (the backend asserts
                // if a texture is never created)
                return s_Tex->GetTexRef();
            }
            // one full-texture upload (ImTextureData::Updates, ImGui 1.92)
            s_Tex->Updates.clear();
            ImTextureRect full;
            full.x = 0;
            full.y = 0;
            full.w = (unsigned short)width;
            full.h = (unsigned short)height;
            s_Tex->Updates.push_back(full);
            s_Tex->SetStatus(ImTextureStatus_WantUpdates);
        }
    }

    return s_Tex->GetTexRef();
}

void AssMap_Invalidate()
{
    s_Invalidated = true;
}

void AssMap_Shutdown()
{
    // keep the texture registered (RefCount == 1) so ui_shutdown's backend
    // teardown reclaims the GL name exactly like the icon atlas does
    s_Memo.Valid = false;
    s_Hash = 0;
}

void AssMap_AfterImGuiShutdown()
{
    // the ImGui context is gone: the ImTextureData objects are plain CPU
    // memory now (their GL names were reclaimed by the backend teardown
    // inside ui_shutdown, which walks every registered texture with
    // RefCount == 1)
    delete s_Tex;
    s_Tex = nullptr;
    for (ImTextureData *tex : s_Retired)
    {
        delete tex;
    }
    s_Retired.clear();
    s_TexWidth = 0;
    s_TexHeight = 0;
    s_Hash = 0;
    s_Invalidated = false;
    s_Memo.Valid = false;
}

} // namespace xuoassist

#endif // XUO_ASSISTANT
