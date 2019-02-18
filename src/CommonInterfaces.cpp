// MIT License
// Copyright (C) September 2016 Hotride

#include "api/commoninterfaces.h"
#include "CrossUO.h"
#include "IndexObject.h"
#include "Target.h"
#include <SDL_timer.h>
#include "Gumps/GumpSecureTrading.h"
#include "Managers/ConfigManager.h"
#include "Managers/ColorManager.h"
#include "Managers/GumpManager.h"
#include "Managers/ClilocManager.h"
#include "Managers/FontsManager.h"
#include "Managers/FileManager.h"
#include "Network/Packets.h"
#include "Walker/PathFinder.h"
#include "GameObjects/GamePlayer.h"

static IGameString g_GameString;

IGameString::IGameString()
    : m_Unicode(false)
    , m_DataA(nullptr)
    , m_DataW(nullptr)
{
}

IGameString::~IGameString()
{
    RELEASE_POINTER(m_DataA);
    RELEASE_POINTER(m_DataW);
}

IGameString &IGameString::operator()(const string &str)
{
    RELEASE_POINTER(m_DataA);

    m_Unicode = false;
    if (str.length() != 0u)
    {
        m_DataA = new char[str.length() + 1];
        memcpy(&m_DataA[0], &str[0], str.length());
        m_DataA[str.length()] = 0;
    }

    return *this;
}

IGameString &IGameString::operator()(const wstring &str)
{
    RELEASE_POINTER(m_DataW);

    m_Unicode = true;
    if (str.length() != 0u)
    {
        m_DataW = new wchar_t[str.length() + 1];
        memcpy(&m_DataW[0], &str[0], str.length() * 2);
        m_DataW[str.length()] = 0;
    }

    return *this;
}

void CDECL FUNCBODY_PushScissor(int x, int y, int width, int height)
{
    g_GL.PushScissor(x, y, width, height);
}

void CDECL FUNCBODY_PopScissor()
{
    g_GL.PopScissor();
}

void CDECL FUNCBODY_DrawLine(unsigned int color, int x, int y, int width, int height)
{
    glColor4ub(ToColorR(color), ToColorG(color), ToColorB(color), ToColorA(color));
    g_GL.DrawLine(x, y, width, height);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void CDECL FUNCBODY_DrawPolygone(unsigned int color, int x, int y, int width, int height)
{
    glColor4ub(ToColorR(color), ToColorG(color), ToColorB(color), ToColorA(color));
    g_GL.DrawPolygone(x, y, width, height);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void CDECL FUNCBODY_DrawCircle(unsigned int color, float x, float y, float radius, int gradientMode)
{
    glColor4ub(ToColorR(color), ToColorG(color), ToColorB(color), ToColorA(color));
    g_GL.DrawCircle(x, y, radius, gradientMode);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void CDECL FUNCBODY_DrawTextA(
    int x,
    int y,
    unsigned char font,
    const char *text,
    unsigned short color,
    int width,
    TEXT_ALIGN_TYPE align,
    unsigned short flags)
{
    g_FontManager.DrawA(font, text, color, x, y, width, align, flags);
}

void CDECL FUNCBODY_DrawTextW(
    int x,
    int y,
    unsigned char font,
    const wchar_t *text,
    unsigned short color,
    int width,
    TEXT_ALIGN_TYPE align,
    unsigned short flags)
{
    g_FontManager.DrawW(font, text, color, x, y, 30, width, align, flags);
}

void CDECL FUNCBODY_DrawArt(int x, int y, unsigned short graphic, unsigned short color)
{
    g_Game.DrawStaticArt(graphic, color, x, y);
}

void CDECL FUNCBODY_DrawArtAnimated(int x, int y, unsigned short graphic, unsigned short color)
{
    g_Game.DrawStaticArtAnimated(graphic, color, x, y);
}

void CDECL FUNCBODY_DrawResizepicGump(int x, int y, unsigned short graphic, int width, int height)
{
    g_Game.DrawResizepicGump(graphic, x, y, width, height);
}

void CDECL FUNCBODY_DrawGump(int x, int y, unsigned short graphic, unsigned short color)
{
    g_Game.DrawGump(graphic, color, x, y);
}

void CDECL FUNCBODY_DrawGumppic(
    int x, int y, unsigned short graphic, unsigned short color, int width, int height)
{
    g_Game.DrawGump(graphic, color, x, y, width, height);
}

uint64_t CDECL FUNCBODY_GetLandFlags(unsigned short graphic)
{
    return g_Game.GetLandFlags(graphic);
}

uint64_t CDECL FUNCBODY_GetStaticFlags(unsigned short graphic)
{
    return g_Game.GetStaticFlags(graphic);
}

int CDECL FUNCBODY_GetValueInt(VALUE_KEY_INT key, int value)
{
    return g_Game.ValueInt(key, value);
}

void CDECL FUNCBODY_SetValueInt(VALUE_KEY_INT key, int value)
{
    g_Game.ValueInt(key, value);
}

IGameString *CDECL FUNCBODY_GetValueString(VALUE_KEY_STRING key, const char *value)
{
    return &g_GameString(g_Game.ValueString(key, value));
}

void CDECL FUNCBODY_SetValueString(VALUE_KEY_STRING key, const char *value)
{
    g_Game.ValueString(key, value);
}

void CDECL FUNCBODY_SetTargetData(unsigned char *buf, int size)
{
    Wisp::CDataReader reader(buf, size);
    reader.Move(1);

    if (*buf == 0x6C)
    {
        g_Target.SetData(reader);
    }
    else
    {
        g_Target.SetMultiData(reader);
    }
}

void CDECL FUNCBODY_SendTargetObject(unsigned int serial)
{
    g_Target.Plugin_SendTargetObject(serial);
}

void CDECL FUNCBODY_SendTargetTile(unsigned short graphic, short x, short y, char z)
{
    g_Target.Plugin_SendTargetTile(graphic, x, y, z);
}

void CDECL FUNCBODY_SendTargetCancel()
{
    g_Target.Plugin_SendCancelTarget();
}

void UOMsg_Send(uint8_t *data, size_t size)
{
    auto owned = new uint8_t[size];
    memcpy(owned, data, size);
    PUSH_EVENT(UOMSG_SEND, data, size);
}

void CDECL FUNCBODY_SendCastSpell(int index)
{
    if (index >= 0)
    {
        g_LastSpellIndex = index;
        CPacketCastSpell packet(index);
        UOMsg_Send(packet.Data().data(), packet.Data().size());
    }
}

void CDECL FUNCBODY_SendUseSkill(int index)
{
    if (index >= 0)
    {
        g_LastSkillIndex = index;
        CPacketUseSkill packet(index);
        UOMsg_Send(packet.Data().data(), packet.Data().size());
    }
}

void CDECL FUNCBODY_SendAsciiSpeech(const char *text, unsigned short color)
{
    if (color == 0u)
    {
        color = g_ConfigManager.SpeechColor;
    }

    CPacketASCIISpeechRequest packet(text, ST_NORMAL, 3, color);
    UOMsg_Send(packet.Data().data(), packet.Data().size());
}

void CDECL FUNCBODY_SendUnicodeSpeech(const wchar_t *text, unsigned short color)
{
    if (color == 0u)
    {
        color = g_ConfigManager.SpeechColor;
    }

    CPacketUnicodeSpeechRequest packet(text, ST_NORMAL, 3, color, (uint8_t *)g_Language.c_str());
    UOMsg_Send(packet.Data().data(), packet.Data().size());
}

void CDECL FUNCBODY_SendRenameMount(uint32_t serial, const char *text)
{
    CPacketRenameRequest packet(serial, text);
    UOMsg_Send(packet.Data().data(), packet.Data().size());
}

void CDECL FUNCBODY_SendMenuResponse(unsigned int serial, unsigned int id, int code)
{
    //UOI_MENU_RESPONSE data = { serial, id, code };
    auto *data = new UOI_MENU_RESPONSE;
    data->Serial = serial;
    data->ID = id;
    data->Code = code;
    PUSH_EVENT(UOMSG_MENU_RESPONSE, data, nullptr);
}

void CDECL FUNCBODY_DisplayStatusbarGump(unsigned int serial, int x, int y)
{
    g_Game.DisplayStatusbarGump(serial, x, y);
}

void CDECL FUNCBODY_CloseStatusbarGump(unsigned int serial)
{
    g_GumpManager.CloseGump(serial, 0, GT_STATUSBAR);
}

void CDECL FUNCBODY_Logout()
{
    g_Game.LogOut();
}

void CDECL FUNCBODY_SecureTradingCheckState(unsigned int id1, bool state)
{
    CGumpSecureTrading *gump = (CGumpSecureTrading *)g_GumpManager.UpdateGump(id1, 0, GT_TRADE);

    if (gump != nullptr)
    {
        gump->StateMine = state;
        CPacketTradeResponse packet(gump, 2);
        UOMsg_Send(packet.Data().data(), packet.Data().size());
    }
}

void CDECL FUNCBODY_SecureTradingClose(unsigned int id1)
{
    CGumpSecureTrading *gump = (CGumpSecureTrading *)g_GumpManager.GetGump(id1, 0, GT_TRADE);

    if (gump != nullptr)
    {
        gump->RemoveMark = true;
        CPacketTradeResponse packet(gump, 1);
        UOMsg_Send(packet.Data().data(), packet.Data().size());
    }
}

IGameString *CDECL FUNCBODY_GetClilocA(unsigned int clilocID, const char *defaultText)
{
    return &g_GameString(g_ClilocManager.Cliloc(g_Language)->GetA(clilocID, false, defaultText));
}

IGameString *CDECL FUNCBODY_GetClilocW(unsigned int clilocID, const char *defaultText)
{
    return &g_GameString(g_ClilocManager.Cliloc(g_Language)->GetW(clilocID, false, defaultText));
}

IGameString *CDECL FUNCBODY_GetClilocArguments(unsigned int clilocID, const wchar_t *args)
{
    return &g_GameString(g_ClilocManager.ParseArgumentsToClilocString(clilocID, false, args));
}

int CDECL FUNCBODY_GetHuesCount()
{
    return g_ColorManager.GetHuesCount();
}

unsigned short CDECL FUNCBODY_GetColor32To16(unsigned int &color)
{
    return g_ColorManager.Color32To16(color);
}

unsigned int CDECL FUNCBODY_GetColor16To32(unsigned short &color)
{
    return g_ColorManager.Color16To32(color);
}

unsigned short CDECL FUNCBODY_GetColorToGray(unsigned short &color)
{
    return g_ColorManager.ConvertToGray(color);
}

unsigned int CDECL FUNCBODY_GetPolygoneColor(unsigned short c, unsigned short color)
{
    return g_ColorManager.GetPolygoneColor(c, color);
}

unsigned int CDECL FUNCBODY_GetColor(unsigned short &c, unsigned short color)
{
    return g_ColorManager.GetColor(c, color);
}

unsigned int CDECL FUNCBODY_GetPartialHueColor(unsigned short &c, unsigned short color)
{
    return g_ColorManager.GetPartialHueColor(c, color);
}

bool CDECL FUNCBODY_GetCanWalk(unsigned char &direction, int &x, int &y, char &z)
{
    return g_PathFinder.CanWalk(direction, x, y, z);
}

bool CDECL FUNCBODY_GetWalk(bool run, unsigned char direction)
{
    return PUSH_EVENT(UOMSG_WALK, run, direction) != 0u;
}

bool CDECL FUNCBODY_GetWalkTo(int x, int y, int z, int distance)
{
    if (g_Player == nullptr)
    {
        return false;
    }

    CPoint2Di startPoint(g_Player->GetX(), g_Player->GetY());
    if (!g_Player->m_Steps.empty())
    {
        CWalkData &wd = g_Player->m_Steps.back();

        startPoint.X = wd.X;
        startPoint.Y = wd.Y;
    }

    if (GetDistance(startPoint, CPoint2Di(x, y)) <= distance)
    {
        return true;
    }

    bool result = PUSH_EVENT(
        UOMSG_PATHFINDING,
        ((x << 16) & 0xFFFF0000) | (y & 0xFFFF),
        ((x << 16) & 0xFFFF0000) | (distance & 0xFFFF));

    if (result)
    {
        while (g_PathFinder.AutoWalking)
        {
            SDL_Delay(100);
        }

        if (g_Player == nullptr)
        {
            return false;
        }

        CPoint2Di p(g_Player->GetX(), g_Player->GetY());
        if (!g_Player->m_Steps.empty())
        {
            CWalkData &wd = g_Player->m_Steps.back();
            p.X = wd.X;
            p.Y = wd.Y;
        }
        result = (GetDistance(p, CPoint2Di(x, y)) <= distance);
    }

    return result;
}

void CDECL FUNCBODY_GetStopAutowalk()
{
    if (g_PathFinder.PathFindidngCanBeCancelled)
    {
        g_PathFinder.StopAutoWalk();
    }
}

bool CDECL FUNCBODY_GetAutowalking()
{
    return g_PathFinder.AutoWalking;
}

void CDECL FUNCBODY_GetFileInfo(unsigned int index, XUO_RAW_FILE_INFO &info)
{
    Wisp::CMappedFile *file = nullptr;
    unsigned int extra = 0;

    switch (index)
    {
        case OFI_TILEDATA_MUL:
        {
            file = &g_FileManager.m_TiledataMul;
            break;
        }
        case OFI_MULTI_IDX:
        {
            file = &g_FileManager.m_MultiIdx;
            break;
        }
        case OFI_MULTI_MUL:
        {
            file = &g_FileManager.m_MultiMul;
            break;
        }
        case OFI_MULTI_UOP:
        {
            file = &g_FileManager.m_MultiCollection;
            break;
        }
        case OFI_HUES_MUL:
        {
            file = &g_FileManager.m_HuesMul;
            break;
        }
        case OFI_VERDATA_MUL:
        {
            file = &g_FileManager.m_VerdataMul;
            break;
        }
        case OFI_RADARCOL_MUL:
        {
            file = &g_FileManager.m_RadarcolMul;
            break;
        }
        case OFI_MAP_0_MUL:
        case OFI_MAP_1_MUL:
        case OFI_MAP_2_MUL:
        case OFI_MAP_3_MUL:
        case OFI_MAP_4_MUL:
        case OFI_MAP_5_MUL:
        {
            file = &g_FileManager.m_MapMul[index - OFI_MAP_0_MUL];
            break;
        }
        case OFI_MAP_0_UOP:
        case OFI_MAP_1_UOP:
        case OFI_MAP_2_UOP:
        case OFI_MAP_3_UOP:
        case OFI_MAP_4_UOP:
        case OFI_MAP_5_UOP:
        {
            file = &g_FileManager.m_MapUOP[index - OFI_MAP_0_UOP];
            break;
        }
        case OFI_MAPX_0_UOP:
        case OFI_MAPX_1_UOP:
        case OFI_MAPX_2_UOP:
        case OFI_MAPX_3_UOP:
        case OFI_MAPX_4_UOP:
        case OFI_MAPX_5_UOP:
        {
            file = &g_FileManager.m_MapXUOP[index - OFI_MAPX_0_UOP];
            break;
        }
        case OFI_STAIDX_0_MUL:
        case OFI_STAIDX_1_MUL:
        case OFI_STAIDX_2_MUL:
        case OFI_STAIDX_3_MUL:
        case OFI_STAIDX_4_MUL:
        case OFI_STAIDX_5_MUL:
        {
            file = &g_FileManager.m_StaticIdx[index - OFI_STAIDX_0_MUL];
            break;
        }
        case OFI_STATICS_0_MUL:
        case OFI_STATICS_1_MUL:
        case OFI_STATICS_2_MUL:
        case OFI_STATICS_3_MUL:
        case OFI_STATICS_4_MUL:
        case OFI_STATICS_5_MUL:
        {
            file = &g_FileManager.m_StaticMul[index - OFI_STATICS_0_MUL];
            break;
        }
        case OFI_MAP_DIF_0_MUL:
        case OFI_MAP_DIF_1_MUL:
        case OFI_MAP_DIF_2_MUL:
        case OFI_MAP_DIF_3_MUL:
        case OFI_MAP_DIF_4_MUL:
        case OFI_MAP_DIF_5_MUL:
        {
            file = &g_FileManager.m_MapDif[index - OFI_MAP_DIF_0_MUL];
            break;
        }
        case OFI_MAP_DIFL_0_MUL:
        case OFI_MAP_DIFL_1_MUL:
        case OFI_MAP_DIFL_2_MUL:
        case OFI_MAP_DIFL_3_MUL:
        case OFI_MAP_DIFL_4_MUL:
        case OFI_MAP_DIFL_5_MUL:
        {
            file = &g_FileManager.m_MapDifl[index - OFI_MAP_DIFL_0_MUL];
            break;
        }
        case OFI_STA_DIF_0_MUL:
        case OFI_STA_DIF_1_MUL:
        case OFI_STA_DIF_2_MUL:
        case OFI_STA_DIF_3_MUL:
        case OFI_STA_DIF_4_MUL:
        case OFI_STA_DIF_5_MUL:
        {
            file = &g_FileManager.m_StaDif[index - OFI_STA_DIF_0_MUL];
            break;
        }
        case OFI_STA_DIFI_0_MUL:
        case OFI_STA_DIFI_1_MUL:
        case OFI_STA_DIFI_2_MUL:
        case OFI_STA_DIFI_3_MUL:
        case OFI_STA_DIFI_4_MUL:
        case OFI_STA_DIFI_5_MUL:
        {
            file = &g_FileManager.m_StaDifi[index - OFI_STA_DIFI_0_MUL];
            break;
        }
        case OFI_STA_DIFL_0_MUL:
        case OFI_STA_DIFL_1_MUL:
        case OFI_STA_DIFL_2_MUL:
        case OFI_STA_DIFL_3_MUL:
        case OFI_STA_DIFL_4_MUL:
        case OFI_STA_DIFL_5_MUL:
        {
            file = &g_FileManager.m_StaDifl[index - OFI_STA_DIFL_0_MUL];
            break;
        }
        case OFI_CLILOC_MUL:
        {
            QFOR(item, g_ClilocManager.m_Items, CCliloc *)
            {
                if (item->Loaded && item->m_File.Start != nullptr && item->Language == "enu")
                {
                    file = &item->m_File;
                    extra = 'ENU';
                    break;
                }
            }

            break;
        }
        default:
            break;
    }

    if (file != nullptr)
    {
        info.Address = intptr_t(file->Start);
        info.Size = file->Size;
        info.Extra = extra;
    }
    else
    {
        info.Address = 0;
        info.Size = 0;
        info.Extra = 0;
    }
}

void CDECL FUNCBODY_GetLandArtInfo(unsigned short index, XUO_RAW_ART_INFO &info)
{
    if (index < MAX_LAND_DATA_INDEX_COUNT)
    {
        CIndexObjectLand &landData = g_Game.m_LandDataIndex[index];

        if (landData.Address != 0)
        {
            info.Address = (uint64_t)landData.Address;
            info.Size = (uint64_t)landData.DataSize;

            if (landData.UopBlock != nullptr)
            {
                info.CompressedSize = (uint64_t)landData.UopBlock->CompressedSize;
            }
            else
            {
                info.CompressedSize = 0;
            }

            return;
        }
    }

    info.Address = 0;
    info.Size = 0;
    info.CompressedSize = 0;
}

void CDECL FUNCBODY_GetStaticArtInfo(unsigned short index, XUO_RAW_ART_INFO &info)
{
    if (index < MAX_STATIC_DATA_INDEX_COUNT)
    {
        CIndexObjectStatic &staticData = g_Game.m_StaticDataIndex[index];

        if (staticData.Address != 0)
        {
            info.Address = (uint64_t)staticData.Address;
            info.Size = (uint64_t)staticData.DataSize;

            if (staticData.UopBlock != nullptr)
            {
                info.CompressedSize = (uint64_t)staticData.UopBlock->CompressedSize;
            }
            else
            {
                info.CompressedSize = 0;
            }

            return;
        }
    }

    info.Address = 0;
    info.Size = 0;
    info.CompressedSize = 0;
}

void CDECL FUNCBODY_GetGumpArtInfo(unsigned short index, XUO_RAW_GUMP_INFO &info)
{
    if (index < MAX_GUMP_DATA_INDEX_COUNT)
    {
        CIndexGump &gumpData = g_Game.m_GumpDataIndex[index];

        if (gumpData.Address != 0)
        {
            info.Address = (uint64_t)gumpData.Address;
            info.Size = (uint64_t)gumpData.DataSize;
            info.Width = gumpData.Width;
            info.Height = gumpData.Height;

            if (gumpData.UopBlock != nullptr)
            {
                info.CompressedSize = (uint64_t)gumpData.UopBlock->CompressedSize;
            }
            else
            {
                info.CompressedSize = 0;
            }

            return;
        }
    }

    info.Address = 0;
    info.Size = 0;
    info.CompressedSize = 0;
    info.Width = 0;
    info.Height = 0;
}

IGLEngine g_Interface_GL = { 0,
                             sizeof(IGLEngine),
                             FUNCBODY_PushScissor,
                             FUNCBODY_PopScissor,
                             FUNCBODY_DrawLine,
                             FUNCBODY_DrawPolygone,
                             FUNCBODY_DrawCircle,
                             FUNCBODY_DrawTextA,
                             FUNCBODY_DrawTextW,
                             FUNCBODY_DrawArt,
                             FUNCBODY_DrawArtAnimated,
                             FUNCBODY_DrawResizepicGump,
                             FUNCBODY_DrawGump,
                             FUNCBODY_DrawGumppic };

IUltimaOnline g_Interface_UO = { 2,
                                 sizeof(IUltimaOnline),
                                 FUNCBODY_GetLandFlags,
                                 FUNCBODY_GetStaticFlags,
                                 FUNCBODY_GetValueInt,
                                 FUNCBODY_SetValueInt,
                                 FUNCBODY_GetValueString,
                                 FUNCBODY_SetValueString,
                                 FUNCBODY_SetTargetData,
                                 FUNCBODY_SendTargetObject,
                                 FUNCBODY_SendTargetTile,
                                 FUNCBODY_SendTargetCancel,
                                 FUNCBODY_SendCastSpell,
                                 FUNCBODY_SendUseSkill,
                                 FUNCBODY_SendAsciiSpeech,
                                 FUNCBODY_SendUnicodeSpeech,
                                 FUNCBODY_SendRenameMount,
                                 FUNCBODY_SendMenuResponse,
                                 FUNCBODY_DisplayStatusbarGump,
                                 FUNCBODY_CloseStatusbarGump,
                                 FUNCBODY_Logout,
                                 FUNCBODY_SecureTradingCheckState,
                                 FUNCBODY_SecureTradingClose };

IClilocManager g_Interface_ClilocManager = {
    0, sizeof(IClilocManager), FUNCBODY_GetClilocA, FUNCBODY_GetClilocW, FUNCBODY_GetClilocArguments
};

IColorManager g_Interface_ColorManager = { 0,
                                           sizeof(IColorManager),
                                           FUNCBODY_GetHuesCount,
                                           FUNCBODY_GetColor32To16,
                                           FUNCBODY_GetColor16To32,
                                           FUNCBODY_GetColorToGray,
                                           FUNCBODY_GetPolygoneColor,
                                           FUNCBODY_GetColor,
                                           FUNCBODY_GetPartialHueColor };

IPathFinder g_Interface_PathFinder = { 0,
                                       sizeof(IPathFinder),
                                       FUNCBODY_GetCanWalk,
                                       FUNCBODY_GetWalk,
                                       FUNCBODY_GetWalkTo,
                                       FUNCBODY_GetStopAutowalk,
                                       FUNCBODY_GetAutowalking };

IFileManager g_Interface_FileManager = { 0,
                                         sizeof(IFileManager),
                                         FUNCBODY_GetFileInfo,
                                         FUNCBODY_GetLandArtInfo,
                                         FUNCBODY_GetStaticArtInfo,
                                         FUNCBODY_GetGumpArtInfo };
