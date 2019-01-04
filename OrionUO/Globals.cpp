// MIT License
// Copyright (C) August 2016 Hotride

bool g_AltPressed = false;
bool g_CtrlPressed = false;
bool g_ShiftPressed = false;
bool g_MovingFromMouse = false;
bool g_AutoMoving = false;
bool g_AbyssPacket03First = true;

int g_LandObjectsCount = 0;
int g_StaticsObjectsCount = 0;
int g_GameObjectsCount = 0;
int g_MultiObjectsCount = 0;
int g_RenderedObjectsCountInGameWindow = 0;

GLdouble g_GlobalScale = 1.0;

CGLTexture g_MapTexture[MAX_MAPS_COUNT];
CGLTexture g_AuraTexture;

bool g_LogoutAfterClick = false;
int g_FrameDelay[2] = { FRAME_DELAY_INACTIVE_WINDOW, FRAME_DELAY_ACTIVE_WINDOW };

uint32_t g_LastSendTime = 0;
uint32_t g_LastPacketTime = 0;
uint32_t g_TotalSendSize = 0;
uint32_t g_TotalRecvSize = 0;
uint32_t g_Ticks = 0;

GLuint ShaderColorTable = 0;
GLuint g_ShaderDrawMode = 0;

string g_Language = "ENU";

GAME_STATE g_GameState = GS_MAIN;

CGLTexture g_TextureGumpState[2];

Wisp::CSize g_MapSize[MAX_MAPS_COUNT] = {
    // Felucca      Trammel         Ilshenar        Malas           Tokuno          TerMur
    { 7168, 4096 }, { 7168, 4096 }, { 2304, 1600 }, { 2560, 2048 }, { 1448, 1448 }, { 1280, 4096 },
};
Wisp::CSize g_MapBlockSize[MAX_MAPS_COUNT];

int g_MultiIndexCount = 0;

CGLFrameBuffer g_LightBuffer;

bool g_GumpPressed = false;
class CRenderObject *g_GumpSelectedElement = nullptr;
class CRenderObject *g_GumpPressedElement = nullptr;
Wisp::CPoint2Di g_GumpMovingOffset;
Wisp::CPoint2Df g_GumpTranslate;
bool g_ShowGumpLocker = false;

bool g_GrayedPixels = false;

bool g_ConfigLoaded = false;

uint8_t g_LightLevel = 0;
uint8_t g_PersonalLightLevel = 0;

char g_SelectedCharName[30] = { 0 };

uint8_t g_CurrentMap = 0;

uint8_t g_ServerTimeHour = 0;
uint8_t g_ServerTimeMinute = 0;
uint8_t g_ServerTimeSecond = 0;

bool g_PacketLoginComplete = false;

uint32_t g_ClientFlag = 0;

bool g_SendLogoutNotification = false;
bool g_PopupEnabled = false;
bool g_ChatEnabled = false;
bool g_TooltipsEnabled = false;
bool g_PaperdollBooks = false;

uint8_t g_GameSeed[4] = { 0 };

uint16_t g_OutOfRangeColor = 0;
char g_MaxGroundZ = 0;
bool g_NoDrawRoof = false;
char g_FoliageIndex = 0;
bool g_UseCircleTrans = false;

bool g_JournalShowSystem = true;
bool g_JournalShowObjects = true;
bool g_JournalShowClient = true;

uint32_t g_PlayerSerial = 0;
uint32_t g_StatusbarUnderMouse = 0;

int g_LastSpellIndex = 1;
int g_LastSkillIndex = 1;
uint32_t g_LastUseObject = 0;
uint32_t g_LastTargetObject = 0;
uint32_t g_LastAttackObject = 0;

CHARACTER_SPEED_TYPE g_SpeedMode = CST_NORMAL;

uint32_t g_DeathScreenTimer = 0;

float g_AnimCharactersDelayValue = 80.0f; //0x50

Wisp::CPoint2Di g_RemoveRangeXY;

int g_GrayMenuCount = 0;

PROMPT_TYPE g_ConsolePrompt = PT_NONE;
uint8_t g_LastASCIIPrompt[11] = { 0 };
uint8_t g_LastUnicodePrompt[11] = { 0 };

uint32_t g_PartyHelperTarget = 0;
uint32_t g_PartyHelperTimer = 0;

float g_DrawColor = 1.0f;

SEASON_TYPE g_Season = ST_SUMMER;
SEASON_TYPE g_OldSeason = ST_SUMMER;
int g_OldSeasonMusic = 0;

uint32_t g_LockedClientFeatures = 0;

bool g_GeneratedMouseDown = false;

DEVELOPER_MODE g_DeveloperMode = DM_SHOW_FPS_ONLY;
DEVELOPER_MODE g_OptionsDeveloperMode = DM_SHOW_FPS_ONLY;

uint16_t g_ObjectHandlesBackgroundPixels[g_ObjectHandlesWidth * g_ObjectHandlesHeight] = { 0 };

uint32_t g_Ping = 0;

bool g_DrawAura = false;

uint16_t g_AbilityList[MAX_ABILITIES_COUNT] = {
    AT_ARMOR_IGNORE,     AT_BLEED_ATTACK,    AT_CONCUSSION_BLOW,    AT_CRUSHING_BLOW,
    AT_DISARM,           AT_DISMOUNT,        AT_DOUBLE_STRIKE,      AT_INFECTING,
    AT_MORTAL_STRIKE,    AT_MOVING_SHOT,     AT_PARALYZING_BLOW,    AT_SHADOW_STRIKE,
    AT_WHIRLWIND_ATTACK, AT_RIDING_SWIPE,    AT_FRENZIED_WHIRLWIND, AT_BLOCK,
    AT_DEFENSE_MASTERY,  AT_NERVE_STRIKE,    AT_TALON_STRIKE,       AT_FEINT,
    AT_DUAL_WIELD,       AT_DOUBLE_SHOT,     AT_ARMOR_PIERCE,       AT_BLADEWEAVE,
    AT_FORCE_ARROW,      AT_LIGHTNING_ARROW, AT_PSYCHIC_ATTACK,     AT_SERPENT_ARROW,
    AT_FORCE_OF_NATURE,  AT_INFUSED_THROW,   AT_MYSTIC_ARC,         AT_DISROBE
};

uint8_t g_Ability[2] = { AT_DISARM, AT_PARALYZING_BLOW };

bool g_DrawStatLockers = false;

uint32_t g_SelectedGameObjectHandle = 0;

bool g_ShowWarnings = true;

uint32_t g_ProcessStaticAnimationTimer = 0;
uint32_t g_ProcessRemoveRangedTimer = 0;
int g_MaxViewRange = MAX_VIEW_RANGE_OLD;
uint32_t g_OrionFeaturesFlags = OFF_ALL_FLAGS;
PING_INFO_DATA g_GameServerPingInfo = {};
string g_PingString = {};
uint32_t g_PingTimer = 0;

bool CanBeDraggedByOffset(const Wisp::CPoint2Di &point)
{
    if (g_Target.IsTargeting())
    {
        return (
            abs(point.X) >= DRAG_PIXEL_RANGE_WITH_TARGET ||
            abs(point.Y) >= DRAG_PIXEL_RANGE_WITH_TARGET);
    }

    return (abs(point.X) >= DRAG_ITEMS_PIXEL_RANGE || abs(point.Y) >= DRAG_ITEMS_PIXEL_RANGE);
}

void TileOffsetOnMonitorToXY(int &ofsX, int &ofsY, int &x, int &y)
{
    if (ofsX == 0)
    {
        x = y = ofsY / 2;
    }
    else if (ofsY == 0)
    {
        x = ofsX / 2;
        y = -x;
    }
    else //if (ofsX && ofsY)
    {
        int absX = abs(ofsX);
        int absY = abs(ofsY);
        x = ofsX;

        if (ofsY > ofsX)
        {
            if (ofsX < 0 && ofsY < 0)
            {
                y = absX - absY;
            }
            else if (ofsX > 0 && ofsY > 0)
            {
                y = absY - absX;
            }
        }
        else if (ofsX > ofsY)
        {
            if (ofsX < 0 && ofsY < 0)
            {
                y = -(absY - absX);
            }
            else if (ofsX > 0 && ofsY > 0)
            {
                y = -(absX - absY);
            }
        }

        if ((y == 0) && ofsY != ofsX)
        {
            if (ofsY < 0)
            {
                y = -(absX + absY);
            }
            else
            {
                y = absX + absY;
            }
        }

        y /= 2;
        x += y;
    }
}

string ToCamelCase(string text)
{
    bool lastSpace = true;

    for (char &c : text)
    {
        if (lastSpace && (c >= 'a' && c <= 'z'))
        {
            c = 'A' + (c - 'a');
        }

        lastSpace = (c == ' ');
    }

    return text;
}

int GetDistance(CGameObject *current, CGameObject *target)
{
    if (current != nullptr && target != nullptr)
    {
        int distx = abs(target->GetX() - current->GetX());
        int disty = abs(target->GetY() - current->GetY());

        if (disty > distx)
        {
            distx = disty;
        }

        return distx;
    }

    return 100500;
}

int GetDistance(CGameObject *current, const Wisp::CPoint2Di &target)
{
    if (current != nullptr)
    {
        int distx = abs(target.X - current->GetX());
        int disty = abs(target.Y - current->GetY());

        if (disty > distx)
        {
            distx = disty;
        }

        return distx;
    }

    return 100500;
}

int GetDistance(const Wisp::CPoint2Di &current, CGameObject *target)
{
    if (target != nullptr)
    {
        int distx = abs(target->GetX() - current.X);
        int disty = abs(target->GetY() - current.Y);

        if (disty > distx)
        {
            distx = disty;
        }

        return distx;
    }

    return 100500;
}

int GetRemoveDistance(const Wisp::CPoint2Di &current, CGameObject *target)
{
    if (target != nullptr)
    {
        Wisp::CPoint2Di targetPoint(target->GetX(), target->GetY());

        if (target->NPC && !((CGameCharacter *)target)->m_Steps.empty())
        {
            CWalkData &wd = ((CGameCharacter *)target)->m_Steps.back();

            targetPoint = Wisp::CPoint2Di(wd.X, wd.Y);
        }

        int distx = abs(targetPoint.X - current.X);
        int disty = abs(targetPoint.Y - current.Y);

        if (disty > distx)
        {
            distx = disty;
        }

        return distx;
    }

    return 100500;
}

bool CheckMultiDistance(const Wisp::CPoint2Di &current, CGameObject *target, int maxDistance)
{
    bool result = false;

    if (target != nullptr)
    {
        maxDistance += ((CGameItem *)target)->MultiDistanceBonus;

        result =
            ((abs(target->GetX() - current.X) <= maxDistance) &&
             (abs(target->GetY() - current.Y) <= maxDistance));
    }

    return result;
}

int GetDistance(const Wisp::CPoint2Di &current, const Wisp::CPoint2Di &target)
{
    int distx = abs(target.X - current.X);
    int disty = abs(target.Y - current.Y);

    if (disty > distx)
    {
        distx = disty;
    }

    return distx;
}

int GetTopObjDistance(CGameObject *current, CGameObject *target)
{
    if (current != nullptr && target != nullptr)
    {
        while (target != nullptr && target->Container != 0xFFFFFFFF)
        {
            target = g_World->FindWorldObject(target->Container);
        }

        if (target != nullptr)
        {
            int distx = abs(target->GetX() - current->GetX());
            int disty = abs(target->GetY() - current->GetY());

            if (disty > distx)
            {
                distx = disty;
            }

            return distx;
        }
    }

    return 100500;
}

const char *GetReagentName(uint16_t id)
{
    switch (id)
    {
        case 0x0F7A:
            return "Black pearl";
        case 0x0F7B:
            return "Bloodmoss";
        case 0x0F84:
            return "Garlic";
        case 0x0F85:
            return "Ginseng";
        case 0x0F86:
            return "Mandrake root";
        case 0x0F88:
            return "Nightshade";
        case 0x0F8C:
            return "Sulfurous ash";
        case 0x0F8D:
            return "Spiders silk";
        default:
            break;
    }

    return "";
}
