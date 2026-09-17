// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant map module, logic half (see assmap.h for the dedup map):
// settings record + GlobalConfig/Map.ini persistence (single [Map] section), the
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
#include "Party.h"                 // ::g_Party (party member map color)
#include "CrossUO.h"              // ::g_Game (season graphics)
#include "GameObjects/RenderStaticObject.h" // CRenderStaticObject::IsNoDrawTile

#include "../core/world.h"
#include "../config/assistantconfig.h" // g_AssistantConfig (friends/enemies classification)
#include "../managers/guildmanager.h"

namespace xuoassist {

CMapSettings g_MapSettings;

const char *g_FacetName[6] = { "Felucca", "Trammel", "Ilshenar", "Malas", "Tokuno", "TerMur" };

float MapSettings_Scale()
{
    float scale = g_MapSettings.Zoom * 0.25f;

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

CLIENT_MAP_CHARACTER_TYPE AssMap_GetCharacterType(const uint32_t &serial)
{
    if (g_World == nullptr)
    {
        return OMCT_OTHER;
    }
    if (serial == g_PlayerSerial)
    {
        return OMCT_PLAYER;
    }

    if (g_Party.Contains((int)serial))
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

// --- settings --------------------------------------------------------------

void CMapSettings::Copy(const CMapSettings &settings)
{
    ShowStatics = settings.ShowStatics;
    ApplyAltitude = settings.ApplyAltitude;
    AltitudeOnly = settings.AltitudeOnly;
    SmoothImage = settings.SmoothImage;
    DisplayCoordinates = settings.DisplayCoordinates;
    ShowOnLogin = settings.ShowOnLogin;
    ShowLastCorpse = settings.ShowLastCorpse;
    MarkDeadMobiles = settings.MarkDeadMobiles;
    RotateMap = settings.RotateMap;

    PlayerFontStyle = settings.PlayerFontStyle;
    PlayerColor = settings.PlayerColor;
    PlayerShowHits = settings.PlayerShowHits;
    PlayerShowMana = settings.PlayerShowMana;
    PlayerShowStam = settings.PlayerShowStam;
    PlayerShowName = settings.PlayerShowName;

    PartyEnabled = settings.PartyEnabled;
    PartyFontStyle = settings.PartyFontStyle;
    PartyColor = settings.PartyColor;
    PartyShowHits = settings.PartyShowHits;
    PartyShowName = settings.PartyShowName;

    GuildEnabled = settings.GuildEnabled;
    GuildFontStyle = settings.GuildFontStyle;
    GuildColor = settings.GuildColor;
    GuildShowHits = settings.GuildShowHits;
    GuildShowName = settings.GuildShowName;

    FriendsEnabled = settings.FriendsEnabled;
    FriendsFontStyle = settings.FriendsFontStyle;
    FriendsColor = settings.FriendsColor;
    FriendsShowHits = settings.FriendsShowHits;
    FriendsShowName = settings.FriendsShowName;

    EnemiesEnabled = settings.EnemiesEnabled;
    EnemiesFontStyle = settings.EnemiesFontStyle;
    EnemiesColor = settings.EnemiesColor;
    EnemiesShowHits = settings.EnemiesShowHits;
    EnemiesShowName = settings.EnemiesShowName;

    OtherEnabled = settings.OtherEnabled;
    OtherFontStyle = settings.OtherFontStyle;
    OtherColor = settings.OtherColor;
    OtherShowHits = settings.OtherShowHits;
    OtherShowName = settings.OtherShowName;
}

// --- radar sampling ----------------------------------------------------------
//
// Per-facet RADAR block fill: the body of CMapManager::GetRadarMapBlock
// (src/Managers/MapManager.cpp:272-318) parameterized by map - the client's
// own entry point is pinned to GetActualMap(), which cannot serve the map
// window's explicit facet selection. withStatics=false keeps the block
// land-only (the ShowStatics toggle).
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
// a content hash + a 150 ms floor; the throttle keeps a drag-pan at zoom
// 0.25 from resampling every frame.
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
            map, blockX, blockY, g_MapSettings.ShowStatics, s_Memo.Block);
        s_Memo.Valid = true;
    }

    const RADAR_MAP_CELLS &cell = s_Memo.Block.Cells[x & 7][y & 7];

    if (g_MapSettings.AltitudeOnly)
    {
        // grayscale ramp over the Z range
        const int z = (cell.Z + 127) * 32 / 256;
        const uint16_t c = uint16_t(((z & 0x1F) << 10) | ((z & 0x1F) << 5) | (z & 0x1F));
        return Color16ToRGBA32(0x8000 | c);
    }

    uint16_t color = RadarCellColor(cell);
    if (g_MapSettings.ApplyAltitude && cell.IsLand != 0 && cell.Graphic < 0x4000)
    {
        // the altitude blend uses a per-channel saturating add, so bright land
        // colors clamp to white instead of wrapping to dark. The ramp itself is
        // quantized to ((Z+127)/8)*32/256, so it
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
        // size change (window resize): queue the texture for destroy and register
        // a replacement (the atlas's own lifecycle sequence)
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
    const bool statics = g_MapSettings.ShowStatics;
    const bool altitude = g_MapSettings.ApplyAltitude;
    const bool altitudeOnly = g_MapSettings.AltitudeOnly;
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
