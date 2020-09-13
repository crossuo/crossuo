// MIT License
// Copyright (C) August 2016 Hotride

#include <common/utils.h>
#include <common/str.h>
#include "AnimationManager.h"
#include "MouseManager.h"
#include "ConfigManager.h"
#include "ColorManager.h"
#include "CorpseManager.h"
#include <xuocore/uodata.h>
#include "../Application.h"
#include "../Target.h"
#include "../Constants.h"
#include "../Config.h"
#include "../CrossUO.h"
#include "../TargetGump.h"
#include "../SelectedObject.h"
#include "../GameWindow.h"
#include "../ScreenStages/GameScreen.h"
#include "../GameObjects/GameCharacter.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CAnimationManager g_AnimationManager;

void *LoadSpritePixels(int width, int height, uint16_t *pixels)
{
    auto spr = new CSprite();
    spr->LoadSprite16(width, height, pixels);
    return spr;
}

struct FRAME_OUTPUT_INFO
{
    int StartX = 0;
    int StartY = 0;
    int EndX = 0;
    int EndY = 0;
};

void CalculateFrameInformation(
    FRAME_OUTPUT_INFO &info, CGameObject *obj, bool mirror, uint8_t animIndex)
{
    const auto dir = g_AnimationManager.SelectAnim.Direction;
    const auto grp = g_AnimationManager.SelectAnim.Group;
    const auto dim = g_AnimationManager.GetAnimationDimensions(obj, animIndex, dir, grp);
    int y = -(dim.Height + dim.CenterY + 3);
    int x = -dim.CenterX;
    if (mirror)
    {
        x = -(dim.Width - dim.CenterX);
    }

    if (x < info.StartX)
    {
        info.StartX = x;
    }

    if (y < info.StartY)
    {
        info.StartY = y;
    }

    if (info.EndX < x + dim.Width)
    {
        info.EndX = x + dim.Width;
    }

    if (info.EndY < y + dim.Height)
    {
        info.EndY = y + dim.Height;
    }
}

const int CAnimationManager::m_UsedLayers[MAX_LAYER_DIRECTIONS][USED_LAYER_COUNT] = {
    {
        //dir 0
        OL_SHIRT, OL_PANTS, OL_SHOES,    OL_LEGS,   OL_TORSO,  OL_RING,   OL_TALISMAN, OL_BRACELET,
        OL_15,    OL_ARMS,  OL_GLOVES,   OL_SKIRT,  OL_TUNIC,  OL_ROBE,   OL_WAIST,    OL_NECKLACE,
        OL_HAIR,  OL_BEARD, OL_EARRINGS, OL_HELMET, OL_1_HAND, OL_2_HAND, OL_CLOAK,
    },
    {
        //dir 1
        OL_SHIRT, OL_PANTS, OL_SHOES,    OL_LEGS,   OL_TORSO,  OL_RING,   OL_TALISMAN, OL_BRACELET,
        OL_15,    OL_ARMS,  OL_GLOVES,   OL_SKIRT,  OL_TUNIC,  OL_ROBE,   OL_WAIST,    OL_NECKLACE,
        OL_HAIR,  OL_BEARD, OL_EARRINGS, OL_HELMET, OL_1_HAND, OL_2_HAND, OL_CLOAK,
    },
    {
        //dir 2
        OL_SHIRT, OL_PANTS, OL_SHOES,    OL_LEGS,   OL_TORSO,  OL_RING,  OL_TALISMAN, OL_BRACELET,
        OL_15,    OL_ARMS,  OL_GLOVES,   OL_SKIRT,  OL_TUNIC,  OL_ROBE,  OL_WAIST,    OL_NECKLACE,
        OL_HAIR,  OL_BEARD, OL_EARRINGS, OL_HELMET, OL_1_HAND, OL_CLOAK, OL_2_HAND,
    },
    {
        //dir 3
        OL_CLOAK,    OL_SHIRT, OL_PANTS, OL_SHOES,    OL_LEGS,   OL_TORSO,  OL_RING,   OL_TALISMAN,
        OL_BRACELET, OL_15,    OL_ARMS,  OL_GLOVES,   OL_SKIRT,  OL_TUNIC,  OL_ROBE,   OL_WAIST,
        OL_NECKLACE, OL_HAIR,  OL_BEARD, OL_EARRINGS, OL_HELMET, OL_1_HAND, OL_2_HAND,
    },
    {
        //dir 4
        OL_SHIRT, OL_PANTS, OL_SHOES,    OL_LEGS,   OL_TORSO,  OL_RING,  OL_TALISMAN, OL_BRACELET,
        OL_15,    OL_ARMS,  OL_GLOVES,   OL_SKIRT,  OL_TUNIC,  OL_ROBE,  OL_WAIST,    OL_NECKLACE,
        OL_HAIR,  OL_BEARD, OL_EARRINGS, OL_HELMET, OL_1_HAND, OL_CLOAK, OL_2_HAND,
    },
    {
        //dir 5
        OL_SHIRT, OL_PANTS, OL_SHOES,    OL_LEGS,   OL_TORSO,  OL_RING,   OL_TALISMAN, OL_BRACELET,
        OL_15,    OL_ARMS,  OL_GLOVES,   OL_SKIRT,  OL_TUNIC,  OL_ROBE,   OL_WAIST,    OL_NECKLACE,
        OL_HAIR,  OL_BEARD, OL_EARRINGS, OL_HELMET, OL_1_HAND, OL_2_HAND, OL_CLOAK,
    },
    {
        //dir 6
        OL_SHIRT, OL_PANTS, OL_SHOES,    OL_LEGS,   OL_TORSO,  OL_RING,   OL_TALISMAN, OL_BRACELET,
        OL_15,    OL_ARMS,  OL_GLOVES,   OL_SKIRT,  OL_TUNIC,  OL_ROBE,   OL_WAIST,    OL_NECKLACE,
        OL_HAIR,  OL_BEARD, OL_EARRINGS, OL_HELMET, OL_1_HAND, OL_2_HAND, OL_CLOAK,
    },
    {
        //dir 7
        OL_SHIRT, OL_PANTS, OL_SHOES,    OL_LEGS,   OL_TORSO,  OL_RING,   OL_TALISMAN, OL_BRACELET,
        OL_15,    OL_ARMS,  OL_GLOVES,   OL_SKIRT,  OL_TUNIC,  OL_ROBE,   OL_WAIST,    OL_NECKLACE,
        OL_HAIR,  OL_BEARD, OL_EARRINGS, OL_HELMET, OL_1_HAND, OL_2_HAND, OL_CLOAK,
    },
};

CAnimationManager::CAnimationManager()
{
    memset(m_AddressIdx, 0, sizeof(m_AddressIdx));
    memset(m_SizeIdx, 0, sizeof(m_SizeIdx));

    memset(m_CharacterLayerGraphic, 0, sizeof(m_CharacterLayerGraphic));
    memset(m_CharacterLayerAnimID, 0, sizeof(m_CharacterLayerAnimID));
}

CAnimationManager::~CAnimationManager()
{
    ClearUnusedTextures(g_Ticks + 100000);
}

void CAnimationManager::UpdateAnimationAddressTable()
{
    for (int i = 0; i < MAX_ANIMATIONS_DATA_INDEX_COUNT; i++)
    {
        CIndexAnimation &index = g_Index.m_Anim[i];

        for (int g = 0; g < MAX_ANIMATION_GROUPS_COUNT; g++)
        {
            CTextureAnimationGroup &group = index.m_Groups[g];

            for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
            {
                CTextureAnimationDirection &direction = group.m_Direction[d];
                bool replace = (direction.FileIndex >= 4);

                if (direction.FileIndex == 2)
                {
                    replace = (g_Config.ClientFlag >= CF_LBR);
                }
                else if (direction.FileIndex == 3)
                {
                    replace = (g_Config.ClientFlag >= CF_AOS);
                }
                //else if (direction.FileIndex == 4)
                //	replace = (g_LockedClientFeatures & LFF_AOS);
                //else if (direction.FileIndex == 5)
                //	replace = true; // (g_LockedClientFeatures & LFF_ML);

                if (replace)
                {
                    direction.Address = direction.PatchedAddress;
                    direction.Size = direction.PatchedSize;
                }
                else
                {
                    direction.Address = direction.BaseAddress;
                    direction.Size = direction.BaseSize;
                }
            }
        }
    }
}

static void load_animations(CAnimationManager *mgr)
{
    size_t maxAddress = mgr->m_AddressIdx[0] + mgr->m_SizeIdx[0];
    bool isValid = false; // CHECK
    for (int i = 0; i < MAX_ANIMATIONS_DATA_INDEX_COUNT; i++)
    {
        CIndexAnimation &index = g_Index.m_Anim[i];
        ANIMATION_GROUPS_TYPE groupType = AGT_UNKNOWN;
        size_t findID = 0;
        if (i >= 200)
        {
            if (i >= 400) //People
            {
                groupType = AGT_HUMAN;
                findID = (((i - 400) * 175) + 35000) * sizeof(AnimIdxBlock);
            }
            else //Low
            {
                groupType = AGT_ANIMAL;
                findID = (((i - 200) * 65) + 22000) * sizeof(AnimIdxBlock);
            }
        }
        else //Hight
        {
            groupType = AGT_MONSTER;
            findID = (i * 110) * sizeof(AnimIdxBlock);
        }

        if (findID >= mgr->m_SizeIdx[0])
        {
            break;
        }

        index.Graphic = (int)i;
        //if (index.Type != AGT_UNKNOWN)
        //	groupType = index.Type;
        int count = 0;
        switch (groupType)
        {
            case AGT_MONSTER:
            case AGT_SEA_MONSTER:
            {
                count = HAG_ANIMATION_COUNT;
                break;
            }
            case AGT_HUMAN:
            case AGT_EQUIPMENT:
            {
                count = PAG_ANIMATION_COUNT;
                break;
            }
            case AGT_ANIMAL:
            default:
            {
                count = LAG_ANIMATION_COUNT;
                break;
            }
        }

        index.Type = groupType;
        size_t address = mgr->m_AddressIdx[0] + findID;
        for (int j = 0; j < count; j++)
        {
            CTextureAnimationGroup &group = index.m_Groups[j];
            const int offset = (int)j * MAX_MOBILE_DIRECTIONS;
            for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
            {
                CTextureAnimationDirection &direction = group.m_Direction[d];
                const auto *aidx =
                    (AnimIdxBlock *)(address + ((offset + d) * sizeof(AnimIdxBlock)));
                if ((size_t)aidx >= maxAddress)
                {
                    break;
                }

                if ((aidx->Size != 0u) && aidx->Position != 0xFFFFFFFF && aidx->Size != 0xFFFFFFFF)
                {
                    direction.BaseAddress = aidx->Position;
                    direction.BaseSize = aidx->Size;
                    direction.Address = direction.BaseAddress;
                    direction.Size = direction.BaseSize;
                    isValid = true;
                }
            }
        }
        index.IsValidMUL = isValid;
    }
}

static void load_verdata(CAnimationManager *mgr)
{
    auto verdata = (uint32_t *)g_FileManager.m_VerdataMul.Start;
    if (verdata == nullptr)
        return;

    int dataCount = *verdata;
    for (int j = 0; j < dataCount; j++)
    {
        const auto *vh = (VERDATA_HEADER *)((size_t)verdata + 4 + (j * sizeof(VERDATA_HEADER)));
        if (vh->FileID == 0x06) //Anim
        {
            ANIMATION_GROUPS_TYPE groupType = AGT_HUMAN;
            uint32_t graphic = vh->BlockID;
            uint16_t id = 0xFFFF;
            uint32_t group = 0;
            uint32_t dir = 0;
            uint32_t offset = 0;
            int count = 0;

            TRACE(Data, "vh->ID = 0x%02X vh->BlockID = 0x%08X", vh->FileID, graphic);
            if (graphic < 35000)
            {
                if (graphic < 22000) //monsters
                {
                    count = 22;
                    groupType = AGT_MONSTER;
                    id = graphic / 110;
                    offset = graphic - (id * 110);
                }
                else //animals
                {
                    count = 13;
                    groupType = AGT_ANIMAL;
                    id = (graphic - 22000) / 65;
                    offset = graphic - ((id * 65) + 22000);
                    id += 200;
                }
            }
            else //humans
            {
                groupType = AGT_HUMAN;
                count = PAG_ANIMATION_COUNT;
                id = (graphic - 35000) / 175;
                offset = graphic - ((id * 175) + 35000);
                id += 400;
            }

            group = offset / MAX_MOBILE_DIRECTIONS;
            dir = offset % MAX_MOBILE_DIRECTIONS;
            if (id >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                Warning(Data, "Invalid animation patch 0x%04X (0x%08X)", id, graphic);
                continue;
            }
            if (group >= (uint32_t)count)
            {
                Warning(
                    Data,
                    "Invalid group index: %i in animation patch 0x%04X (0x%08X)",
                    group,
                    id,
                    graphic);
                continue;
            }

            CIndexAnimation &index = g_Index.m_Anim[id];
            CTextureAnimationDirection &direction = index.m_Groups[group].m_Direction[dir];
            direction.IsVerdata = true;
            direction.BaseAddress = (size_t)g_FileManager.m_VerdataMul.Start + vh->Position;
            direction.BaseSize = vh->Size;
            direction.Address = direction.BaseAddress;
            direction.Size = direction.BaseSize;
            index.Graphic = id;
            index.Type = groupType;
            index.IsValidMUL = true;
        }
    }
}

static void load_mobtype()
{
    static const astr_t typeNames[5] = { "monster", "sea_monster", "animal", "human", "equipment" };

    Wisp::CTextFileParser mobtypesParser(g_App.UOFilesPath("mobtypes.txt"), " \t", "#;//", "");
    while (!mobtypesParser.IsEOF())
    {
        const auto strings = mobtypesParser.ReadTokens();
        if (strings.size() >= 3)
        {
            const uint16_t index = str_to_int(strings[0]);
            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            const auto testType = str_lower(strings[1]);
            char *endP = nullptr;
            const auto number = strtoul(("0x" + strings[2]).c_str(), &endP, 16);
            for (int i = 0; i < 5; i++)
            {
                if (testType == typeNames[i])
                {
                    g_Index.m_Anim[index].Type = ANIMATION_GROUPS_TYPE(i);
                    g_Index.m_Anim[index].Flags = ANIMATION_FLAGS(AF_FOUND | number);
                    break;
                }
            }
        }
    }
}

static void load_animdef(CAnimationManager *mgr)
{
    Wisp::CTextFileParser animParser[] = {
        Wisp::CTextFileParser(g_App.UOFilesPath("Anim1.def"), " \t", "#;//", "{}"),
        Wisp::CTextFileParser(g_App.UOFilesPath("Anim2.def"), " \t", "#;//", "{}"),
    };
    for (int i = 0; i < countof(animParser); i++)
    {
        while (!animParser[i].IsEOF())
        {
            const auto strings = animParser[i].ReadTokens();
            if (strings.size() < 2)
            {
                continue;
            }
            const auto group = uint16_t(str_to_int(strings[0]));
            if (group == 0xffff)
            {
                continue;
            }
            const auto replaceGroup = uint8_t(str_to_int(strings[1]));
            mgr->m_GroupReplaces[i].push_back({ group, replaceGroup });
        }
    }
}

static void load_equipconv(CAnimationManager *mgr)
{
    Wisp::CTextFileParser equipConvParser(g_App.UOFilesPath("Equipconv.def"), " \t", "#;//", "");
    while (!equipConvParser.IsEOF())
    {
        const auto strings = equipConvParser.ReadTokens();
        if (strings.size() >= 5)
        {
            const auto body = (uint16_t)str_to_int(strings[0]);
            if (body >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            const auto graphic = (uint16_t)str_to_int(strings[1]);
            if (graphic >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            auto newGraphic = (uint16_t)str_to_int(strings[2]);
            if (newGraphic >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                newGraphic = graphic;
            }

            const auto gump_field = (uint32_t)str_to_int(strings[3]);
            if (gump_field >= MAX_GUMP_DATA_INDEX_COUNT)
            {
                continue;
            }

            auto gump = checked_cast<uint16_t>(gump_field);
            if (gump == 0)
            {
                gump = graphic; // +50000;
            }
            else if (gump == 0xFFFF)
            {
                gump = newGraphic; // +50000;
            }

            auto color = (uint16_t)str_to_int(strings[4]);
            auto bodyMapIter = mgr->m_EquipConv.find(body);
            if (bodyMapIter == mgr->m_EquipConv.end())
            {
                mgr->m_EquipConv.insert({ body, {} });
                bodyMapIter = mgr->m_EquipConv.find(body);
                if (bodyMapIter == mgr->m_EquipConv.end())
                {
                    continue; //?!?!??
                }
            }
            bodyMapIter->second.insert({ graphic, { newGraphic, gump, color } });
        }
    }
}

static void load_bodyconv(CAnimationManager *mgr)
{
    Wisp::CTextFileParser bodyconvParser(g_App.UOFilesPath("Bodyconv.def"), " \t", "#;//", "");
    while (!bodyconvParser.IsEOF())
    {
        auto strings = bodyconvParser.ReadTokens();
        if (strings.size() >= 2)
        {
            const uint16_t index = str_to_int(strings[0]);
            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            int anim[4] = { str_to_int(strings[1]), -1, -1, -1 };
            if (strings.size() >= 3)
            {
                anim[1] = str_to_int(strings[2]);
                if (strings.size() >= 4)
                {
                    anim[2] = str_to_int(strings[3]);
                    if (strings.size() >= 5)
                    {
                        anim[3] = str_to_int(strings[4]);
                    }
                }
            }

            int animFile = 1;
            uint16_t realAnimID = 0xffff;
            char mountedHeightOffset = 0;

            if (anim[0] != -1 && mgr->m_AddressIdx[2] != 0 && g_FileManager.IsMulFileOpen(2))
            {
                animFile = 2;
                realAnimID = anim[0];
                if (index == 192 || index == 793) // 0x00C0 || 0x0319
                {
                    mountedHeightOffset = -9;
                }
            }
            else if (anim[1] != -1 && mgr->m_AddressIdx[3] != 0 && g_FileManager.IsMulFileOpen(3))
            {
                animFile = 3;
                realAnimID = anim[1];
                if (index == 1401) // 0x0579
                {
                    mountedHeightOffset = 9;
                }
            }
            else if (anim[2] != -1 && mgr->m_AddressIdx[4] != 0 && g_FileManager.IsMulFileOpen(4))
            {
                animFile = 4;
                realAnimID = anim[2];
            }
            else if (anim[3] != -1 && mgr->m_AddressIdx[5] != 0 && g_FileManager.IsMulFileOpen(5))
            {
                animFile = 5;
                realAnimID = anim[3];
                switch (index)
                {
                    case 192: // 0x00C0
                    case 277: // 0x0115
                        mountedHeightOffset = 0;
                        break;
                    case 1069: // 0x042D
                        mountedHeightOffset = 3;
                        break;
                    default:
                        mountedHeightOffset = -9;
                        break;
                }
            }

            if (animFile > 1 && realAnimID != 0xffff)
            {
                CIndexAnimation &dataIndex = g_Index.m_Anim[index];
                auto realType = (g_Config.ClientVersion < CV_500A) ?
                                    uo_type_by_graphic(realAnimID) :
                                    dataIndex.Type;
                int count = 0;
                auto addressOffset =
                    uo_get_anim_offset(realAnimID, dataIndex.Flags, realType, count);
                if (addressOffset < mgr->m_SizeIdx[animFile])
                {
                    dataIndex.Type = realType;
                    dataIndex.GraphicConversion = realAnimID | 0x8000;
                    dataIndex.FileIndex = animFile;
                    if (dataIndex.MountedHeightOffset == 0)
                    {
                        dataIndex.MountedHeightOffset = mountedHeightOffset;
                    }

                    addressOffset += mgr->m_AddressIdx[animFile];
                    size_t maxAddress = mgr->m_AddressIdx[animFile] + mgr->m_SizeIdx[animFile];
                    int offset = 0;
                    for (int j = 0; j < count; j++)
                    {
                        auto &group = dataIndex.m_Groups[j];
                        //int offset = (int)j * MAX_MOBILE_DIRECTIONS;
                        for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
                        {
                            const auto *aidx =
                                (AnimIdxBlock *)(addressOffset + offset * sizeof(AnimIdxBlock));
                            if ((size_t)aidx >= maxAddress)
                            {
                                continue;
                            }

                            if (aidx->Size != 0 && aidx->Position != -1 && aidx->Size != -1)
                            {
                                auto &direction = group.m_Direction[d];
                                direction.PatchedAddress = aidx->Position;
                                direction.PatchedSize = aidx->Size;
                                direction.FileIndex = animFile;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void load_bodydef()
{
    Wisp::CTextFileParser newBodyParser({}, " \t,{}", "#;//", "");
    Wisp::CTextFileParser bodyParser(g_App.UOFilesPath("Body.def"), " \t", "#;//", "{}");
    while (!bodyParser.IsEOF())
    {
        const auto strings = bodyParser.ReadTokens();
        if (strings.size() >= 3)
        {
            const uint16_t index = str_to_int(strings[0]);
            const auto newBody = newBodyParser.GetTokens(strings[1]);
            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT || newBody.empty())
            {
                continue;
            }

            const uint16_t checkIndex = str_to_int(newBody[0]);
            if (checkIndex >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            CIndexAnimation &dataIndex = g_Index.m_Anim[index];
            CIndexAnimation &checkDataIndex = g_Index.m_Anim[checkIndex];
            int count = 0;
            int ignoreGroups[2] = { -1, -1 };

            switch (checkDataIndex.Type)
            {
                case AGT_MONSTER:
                case AGT_SEA_MONSTER:
                {
                    count = HAG_ANIMATION_COUNT;
                    ignoreGroups[0] = HAG_DIE_1;
                    ignoreGroups[1] = HAG_DIE_2;
                    break;
                }
                case AGT_HUMAN:
                case AGT_EQUIPMENT:
                {
                    count = PAG_ANIMATION_COUNT;
                    ignoreGroups[0] = PAG_DIE_1;
                    ignoreGroups[1] = PAG_DIE_2;
                    break;
                }
                case AGT_ANIMAL:
                {
                    count = LAG_ANIMATION_COUNT;
                    ignoreGroups[0] = LAG_DIE_1;
                    ignoreGroups[1] = LAG_DIE_2;
                    break;
                }
                default:
                    break;
            }

            for (int j = 0; j < count; j++)
            {
                if (j == ignoreGroups[0] || j == ignoreGroups[1])
                {
                    continue;
                }

                CTextureAnimationGroup &group = dataIndex.m_Groups[j];
                CTextureAnimationGroup &newGroup = checkDataIndex.m_Groups[j];
                for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
                {
                    CTextureAnimationDirection &direction = group.m_Direction[d];
                    CTextureAnimationDirection &newDirection = newGroup.m_Direction[d];
                    direction.BaseAddress = newDirection.BaseAddress;
                    direction.BaseSize = newDirection.BaseSize;
                    direction.Address = direction.BaseAddress;
                    direction.Size = direction.BaseSize;
                    if (direction.PatchedAddress == 0)
                    {
                        direction.PatchedAddress = newDirection.PatchedAddress;
                        direction.PatchedSize = newDirection.PatchedSize;
                        direction.FileIndex = newDirection.FileIndex;
                    }

                    if (direction.BaseAddress == 0)
                    {
                        direction.BaseAddress = direction.PatchedAddress;
                        direction.BaseSize = direction.PatchedSize;
                        direction.Address = direction.BaseAddress;
                        direction.Size = direction.BaseSize;
                    }
                }
            }

            dataIndex.Type = checkDataIndex.Type;
            dataIndex.Flags = checkDataIndex.Flags;
            dataIndex.Graphic = checkIndex;
            dataIndex.IsValidMUL = true;
            dataIndex.Color = str_to_int(strings[2]);
        }
    }
}

static void load_corpsedef()
{
    Wisp::CTextFileParser newBodyParser({}, " \t,{}", "#;//", "");
    Wisp::CTextFileParser corpseParser(g_App.UOFilesPath("Corpse.def"), " \t", "#;//", "{}");
    while (!corpseParser.IsEOF())
    {
        const auto strings = corpseParser.ReadTokens();
        if (strings.size() >= 3)
        {
            const uint16_t index = str_to_int(strings[0]);
            const auto newBody = newBodyParser.GetTokens(strings[1]);
            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT || newBody.empty())
            {
                continue;
            }

            const uint16_t checkIndex = str_to_int(newBody[0]);
            if (checkIndex >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            CIndexAnimation &dataIndex = g_Index.m_Anim[index];
            CIndexAnimation &checkDataIndex = g_Index.m_Anim[checkIndex];
            int ignoreGroups[2] = { -1, -1 };
            switch (checkDataIndex.Type)
            {
                case AGT_MONSTER:
                case AGT_SEA_MONSTER:
                {
                    ignoreGroups[0] = HAG_DIE_1;
                    ignoreGroups[1] = HAG_DIE_2;
                    break;
                }
                case AGT_HUMAN:
                case AGT_EQUIPMENT:
                {
                    ignoreGroups[0] = PAG_DIE_1;
                    ignoreGroups[1] = PAG_DIE_2;
                    break;
                }
                case AGT_ANIMAL:
                {
                    ignoreGroups[0] = LAG_DIE_1;
                    ignoreGroups[1] = LAG_DIE_2;
                    break;
                }
                default:
                    break;
            }

            if (ignoreGroups[0] == -1)
            {
                continue;
            }

            for (int j = 0; j < 2; j++)
            {
                CTextureAnimationGroup &group = dataIndex.m_Groups[ignoreGroups[j]];
                CTextureAnimationGroup &newGroup = checkDataIndex.m_Groups[ignoreGroups[j]];
                for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
                {
                    CTextureAnimationDirection &direction = group.m_Direction[d];
                    CTextureAnimationDirection &newDirection = newGroup.m_Direction[d];
                    direction.BaseAddress = newDirection.BaseAddress;
                    direction.BaseSize = newDirection.BaseSize;
                    direction.Address = direction.BaseAddress;
                    direction.Size = direction.BaseSize;
                    if (direction.PatchedAddress == 0)
                    {
                        direction.PatchedAddress = newDirection.PatchedAddress;
                        direction.PatchedSize = newDirection.PatchedSize;
                        direction.FileIndex = newDirection.FileIndex;
                    }

                    if (direction.BaseAddress == 0)
                    {
                        direction.BaseAddress = direction.PatchedAddress;
                        direction.BaseSize = direction.PatchedSize;
                        direction.Address = direction.BaseAddress;
                        direction.Size = direction.BaseSize;
                    }
                }
            }

            dataIndex.Type = checkDataIndex.Type;
            dataIndex.Flags = checkDataIndex.Flags;
            dataIndex.Graphic = checkIndex;
            dataIndex.IsValidMUL = true;
            dataIndex.Color = str_to_int(strings[2]);
        }
    }
}

void static load_data_patches(CAnimationManager *mgr)
{
    if (g_Config.ClientVersion >= CV_500A)
    {
        load_mobtype();
    }
    load_animations(mgr);
    load_verdata(mgr);
    load_animdef(mgr);
    if (g_Config.ClientVersion < CV_305D)
    {
        return;
    }
    load_equipconv(mgr);
    load_bodyconv(mgr);
    load_bodydef();
    load_corpsedef();
}

void CAnimationManager::Init()
{
    for (int i = 0; i < countof(g_FileManager.m_AnimIdx); i++)
    {
        m_AddressIdx[i] = (size_t)g_FileManager.m_AnimIdx[i].Start;
        m_SizeIdx[i] = (size_t)g_FileManager.m_AnimIdx[i].Size;
    }
    load_data_patches(this);
}

ANIMATION_GROUPS CAnimationManager::GetGroupIndex(uint16_t id) const
{
    if (id >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
    {
        Warning(Data, "GetGroupIndex: Invalid ID: 0x%04X", id);
        return AG_HIGHT;
    }

    switch (g_Index.m_Anim[id].Type)
    {
        case AGT_ANIMAL:
            return AG_LOW;
        case AGT_MONSTER:
        case AGT_SEA_MONSTER:
            return AG_HIGHT;
        case AGT_HUMAN:
        case AGT_EQUIPMENT:
            return AG_PEOPLE;
        case AGT_UNKNOWN:
            break;
    }

    return AG_HIGHT;
}

uint8_t CAnimationManager::GetDieGroupIndex(uint16_t id, bool second)
{
    DEBUG(Data, "gr: 0x%04X, %i", id, g_Index.m_Anim[id].Type);
    switch (g_Index.m_Anim[id].Type)
    {
        case AGT_ANIMAL:
            return (uint8_t)(second ? LAG_DIE_2 : LAG_DIE_1);
        case AGT_MONSTER:
        case AGT_SEA_MONSTER:
            return (uint8_t)(second ? HAG_DIE_2 : HAG_DIE_1);
        case AGT_HUMAN:
        case AGT_EQUIPMENT:
            return (uint8_t)(second ? PAG_DIE_2 : PAG_DIE_1);
        case AGT_UNKNOWN:
            break;
    }

    return 0;
}

void CAnimationManager::GetAnimDirection(uint8_t &dir, bool &mirror)
{
    switch (dir)
    {
        case 2:
        case 4:
        {
            mirror = (dir == 2);
            dir = 1;

            break;
        }
        case 1:
        case 5:
        {
            mirror = (dir == 1);
            dir = 2;

            break;
        }
        case 0:
        case 6:
        {
            mirror = (dir == 0);
            dir = 3;

            break;
        }
        case 3:
        {
            dir = 0;

            break;
        }
        case 7:
        {
            dir = 4;

            break;
        }
        default:
            break;
    }
}

void CAnimationManager::GetSittingAnimDirection(uint8_t &dir, bool &mirror, int &x, int &y)
{
    switch (dir)
    {
        case 0:
        {
            mirror = true;
            dir = 3;

            break;
        }
        case 2:
        {
            mirror = true;
            dir = 1;

            break;
        }
        case 4:
        {
            mirror = false;
            dir = 1;

            break;
        }
        case 6:
        {
            mirror = false;
            dir = 3;

            break;
        }
        default:
            break;
    }
}

void CAnimationManager::ClearUnusedTextures(uint32_t ticks)
{
    ticks -= CLEAR_ANIMATION_TEXTURES_DELAY;
    int count = 0;
    for (auto it = m_UsedAnimList.begin(); it != m_UsedAnimList.end();)
    {
        CTextureAnimationDirection *obj = *it;
        if (obj->LastAccessTime < ticks)
        {
            if (obj->m_Frames != nullptr)
            {
                delete[] obj->m_Frames;
                obj->m_Frames = nullptr;
            }
            obj->FrameCount = 0;
            obj->LastAccessTime = 0;
            it = m_UsedAnimList.erase(it);
            if (++count >= MAX_ANIMATIONS_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR)
            {
                break;
            }
        }
        else
        {
            it++;
        }
    }
    if (count)
    {
        Info(Data, "removed %d animation textures", count);
    }
}

bool CAnimationManager::TestPixels(
    CGameObject *obj, int x, int y, bool mirror, uint8_t &frameIndex, uint16_t id)
{
    if (obj == nullptr)
    {
        return false;
    }

    if (id == 0)
    {
        id = obj->GetMountAnimation();
    }

    if (id >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
    {
        return false;
    }

    assert(SelectAnim.Direction < MAX_MOBILE_DIRECTIONS && "Out-of-bound access");
    auto direction =
        g_AnimationManager.ExecuteAnimation(SelectAnim.Group, SelectAnim.Direction, id);
    const int fc = direction.FrameCount;
    if (fc > 0 && frameIndex >= fc)
    {
        if (obj->IsCorpse())
        {
            frameIndex = fc - 1;
        }
        else
        {
            frameIndex = 0;
        }
    }

    if (frameIndex >= direction.FrameCount)
    {
        return false;
    }

    auto &frame = direction.m_Frames[frameIndex];
    auto spr = (CSprite *)frame.UserData;

    if (!spr)
    {
        // FIXME: before we had CSprite in the struct, we need cleanup how to construct these objects
        spr = new CSprite();
    }

    assert(spr);

    y -= spr->Height + frame.CenterY;
    x = g_MouseManager.Position.X - x;
    if (mirror)
    {
        x += spr->Width - frame.CenterX;
    }
    else
    {
        x += frame.CenterX;
    }

    if (mirror)
    {
        x = spr->Width - x;
    }
    x = g_MouseManager.Position.X - x;
    return spr->Select(x, y);
}

void CAnimationManager::Draw(
    CGameObject *obj, int x, int y, bool mirror, uint8_t &frameIndex, int id)
{
    ScopedPerfMarker(__FUNCTION__);

    if (obj == nullptr)
    {
        return;
    }

    const bool isShadow = (id >= 0x10000);
    if (isShadow)
    {
        id -= 0x10000;
    }

    if (id == 0)
    {
        id = obj->GetMountAnimation();
    }

    if (id >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
    {
        return;
    }

    assert(SelectAnim.Direction < MAX_MOBILE_DIRECTIONS && "Out-of-bound access");
    auto direction =
        g_AnimationManager.ExecuteAnimation(SelectAnim.Group, SelectAnim.Direction, id);
    const int fc = direction.FrameCount;
    if (fc > 0 && frameIndex >= fc)
    {
        if (obj->IsCorpse())
        {
            frameIndex = fc - 1;
        }
        else
        {
            frameIndex = 0;
        }
    }

    if (frameIndex >= direction.FrameCount)
    {
        return;
    }

    CTextureAnimationFrame &frame = direction.m_Frames[frameIndex];
    auto spr = (CSprite *)frame.UserData;
    if (!spr) //spr->Texture == 0)
    {
        return;
    }

    if (mirror)
    {
        x -= spr->Width - frame.CenterX;
    }
    else
    {
        x -= frame.CenterX;
    }

    y -= spr->Height + frame.CenterY;
    auto sdmNoColor = true;
    if (isShadow)
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_SHADOW);
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        g_GL.DrawShadow(*spr->Texture, x, y, mirror);
        if (m_UseBlending)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else
        {
            glDisable(GL_BLEND);
        }
#else
        auto tex = spr->Texture;
        RenderAdd_DrawShadow(
            g_renderCmdList,
            DrawShadowCmd{ tex->Texture,
                           x,
                           y,
                           tex->Width,
                           tex->Height,
                           g_ShaderDrawMode,
                           SDM_SHADOW,
                           mirror,
                           m_UseBlending });
#endif
        sdmNoColor = false;
    }
    else
    {
        bool spectralColor = false;
        if (!g_GrayedPixels)
        {
            uint16_t color = Color;
            bool partialHue = false;
            if (color == 0u)
            {
                color = obj->Color;
                partialHue = obj->IsPartialHue();
                if ((color & 0x8000) != 0)
                {
                    partialHue = true;
                    color &= 0x7FFF;
                }

                if (color == 0u)
                {
                    if (direction.Address != direction.PatchedAddress)
                    {
                        color = g_Index.m_Anim[id].Color;
                    }
                    if ((color == 0u) && m_EquipConvItem != nullptr)
                    {
                        color = m_EquipConvItem->Color;
                    }
                    partialHue = false;
                }
            }

            if ((color & SPECTRAL_COLOR_FLAG) != 0)
            {
                spectralColor = true;
#ifndef NEW_RENDERER_ENABLED
                glEnable(GL_BLEND);
                if (color == SPECTRAL_COLOR_SPECIAL)
                {
                    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                    glUniform1iARB(g_ShaderDrawMode, SDM_SPECIAL_SPECTRAL);
                }
                else
                {
                    glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                    glUniform1iARB(g_ShaderDrawMode, SDM_SPECTRAL);
                }
#else
                auto uniformValue = SDM_SPECTRAL;
                auto blendSrc = BlendFactor::BlendFactor_Zero;
                auto blendDst = BlendFactor::BlendFactor_SrcColor;
                if (color == SPECTRAL_COLOR_SPECIAL)
                {
                    blendDst = BlendFactor::BlendFactor_OneMinusSrcColor;
                    uniformValue = SDM_SPECIAL_SPECTRAL;
                }

                RenderAdd_SetBlend(g_renderCmdList, BlendStateCmd{ blendSrc, blendDst });
                ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
                cmd.value.asInt1 = uniformValue;
                RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
                sdmNoColor = false;
            }
            else if (color != 0u)
            {
#ifndef NEW_RENDERER_ENABLED
                if (partialHue)
                {
                    glUniform1iARB(g_ShaderDrawMode, SDM_PARTIAL_HUE);
                }
                else
                {
                    glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);
                }
#else
                ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
                cmd.value.asInt1 = partialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
                RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
                g_ColorManager.SendColorsToShader(color);
                sdmNoColor = false;
            }
        }

        if (sdmNoColor)
        {
#ifndef NEW_RENDERER_ENABLED
            glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
            ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
            cmd.value.asInt1 = SDM_NO_COLOR;
            RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        }

        if (m_Transform)
        {
            if (obj->IsHuman())
            {
                short frameHeight = spr->Height;
                m_CharacterFrameStartY = y;
                m_CharacterFrameHeight = spr->Height;
                m_StartCharacterWaistY =
                    (int)(frameHeight * UPPER_BODY_RATIO) + m_CharacterFrameStartY;
                m_StartCharacterKneesY =
                    (int)(frameHeight * MID_BODY_RATIO) + m_CharacterFrameStartY;
                m_StartCharacterFeetY =
                    (int)(frameHeight * LOWER_BODY_RATIO) + m_CharacterFrameStartY;
            }

            float h3mod = UPPER_BODY_RATIO;
            float h6mod = MID_BODY_RATIO;
            float h9mod = LOWER_BODY_RATIO;
            if (!obj->NPC)
            {
                float itemsEndY = (float)(y + spr->Height);
                //Определяем соотношение верхней части текстуры, до перелома.
                if (y >= m_StartCharacterWaistY)
                {
                    h3mod = 0;
                }
                else if (itemsEndY <= m_StartCharacterWaistY)
                {
                    h3mod = 1.0f;
                }
                else
                {
                    float upperBodyDiff = (float)(m_StartCharacterWaistY - y);
                    h3mod = upperBodyDiff / spr->Height;
                    if (h3mod < 0)
                    {
                        h3mod = 0;
                    }
                }

                //Определяем соотношение средней части, где идет деформация с растягиванием по Х.
                if (m_StartCharacterWaistY >= itemsEndY || y >= m_StartCharacterKneesY)
                {
                    h6mod = 0;
                }
                else if (m_StartCharacterWaistY <= y && itemsEndY <= m_StartCharacterKneesY)
                {
                    h6mod = 1.0f;
                }
                else
                {
                    float midBodyDiff = 0.0f;
                    if (y >= m_StartCharacterWaistY)
                    {
                        midBodyDiff = (float)(m_StartCharacterKneesY - y);
                    }
                    else if (itemsEndY <= m_StartCharacterKneesY)
                    {
                        midBodyDiff = (float)(itemsEndY - m_StartCharacterWaistY);
                    }
                    else
                    {
                        midBodyDiff = (float)(m_StartCharacterKneesY - m_StartCharacterWaistY);
                    }

                    h6mod = h3mod + midBodyDiff / spr->Height;
                    if (h6mod < 0)
                    {
                        h6mod = 0;
                    }
                }

                //Определяем соотношение нижней части, она смещена на 8 Х.
                if (itemsEndY <= m_StartCharacterKneesY)
                {
                    h9mod = 0;
                }
                else if (y >= m_StartCharacterKneesY)
                {
                    h9mod = 1.0f;
                }
                else
                {
                    float lowerBodyDiff = itemsEndY - m_StartCharacterKneesY;
                    h9mod = h6mod + lowerBodyDiff / spr->Height;
                    if (h9mod < 0)
                    {
                        h9mod = 0;
                    }
                }
            }
#ifndef NEW_RENDERER_ENABLED
            g_GL.DrawSitting(*spr->Texture, x, y, mirror, h3mod, h6mod, h9mod);
#else
            auto cmd = DrawCharacterSittingCmd{ spr->Texture->Texture,
                                                x,
                                                y,
                                                spr->Texture->Width,
                                                spr->Texture->Height,
                                                h3mod,
                                                h6mod,
                                                h9mod,
                                                mirror };
            RenderAdd_DrawCharacterSitting(g_renderCmdList, cmd);
#endif
        }
        else
        {
#ifndef NEW_RENDERER_ENABLED
            g_GL.DrawMirrored(*spr->Texture, x, y, mirror);
#else
            auto textureCmd = DrawQuadCmd{ spr->Texture->Texture,
                                           x,
                                           y,
                                           spr->Texture->Width,
                                           spr->Texture->Height,
                                           1.f,
                                           1.f,
                                           g_ColorWhite,
                                           mirror };
            RenderAdd_DrawQuad(g_renderCmdList, textureCmd);
#endif
        }

        if (spectralColor)
        {
            if (m_UseBlending)
            {
#ifndef NEW_RENDERER_ENABLED
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#else
                RenderAdd_SetBlend(
                    g_renderCmdList,
                    BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                                   BlendFactor::BlendFactor_OneMinusSrcAlpha });
#endif
            }
            else
            {
#ifndef NEW_RENDERER_ENABLED
                glDisable(GL_BLEND);
#else
                RenderAdd_DisableBlend(g_renderCmdList);
#endif
            }
        }
    }
}

void CAnimationManager::FixSittingDirection(uint8_t &layerDirection, bool &mirror, int &x, int &y)
{
    const SITTING_INFO_DATA &data = SITTING_INFO[m_Sitting - 1];

    auto dir = SelectAnim.Direction;
    switch (dir)
    {
        case 7:
        case 0:
        {
            if (data.Direction1 == INVALID_DIRECTION)
            {
                if (dir == 7)
                {
                    dir = data.Direction4;
                }
                else
                {
                    dir = data.Direction2;
                }
            }
            else
            {
                dir = data.Direction1;
            }

            break;
        }
        case 1:
        case 2:
        {
            if (data.Direction2 == INVALID_DIRECTION)
            {
                if (dir == 1)
                {
                    dir = data.Direction1;
                }
                else
                {
                    dir = data.Direction3;
                }
            }
            else
            {
                dir = data.Direction2;
            }

            break;
        }
        case 3:
        case 4:
        {
            if (data.Direction3 == INVALID_DIRECTION)
            {
                if (dir == 3)
                {
                    dir = data.Direction2;
                }
                else
                {
                    dir = data.Direction4;
                }
            }
            else
            {
                dir = data.Direction3;
            }

            break;
        }
        case 5:
        case 6:
        {
            if (data.Direction4 == INVALID_DIRECTION)
            {
                if (dir == 5)
                {
                    dir = data.Direction3;
                }
                else
                {
                    dir = data.Direction1;
                }
            }
            else
            {
                dir = data.Direction4;
            }

            break;
        }
        default:
            break;
    }

    layerDirection = dir;
    GetSittingAnimDirection(dir, mirror, x, y);
    const int offsX = SITTING_OFFSET_X;
    if (mirror)
    {
        if (dir == 3)
        {
            y += 25 + data.MirrorOffsetY;
            x += offsX - 4;
        }
        else
        {
            y += data.OffsetY + 9;
        }
    }
    else
    {
        if (dir == 3)
        {
            y += 23 + data.MirrorOffsetY;
            x -= 3;
        }
        else
        {
            y += 10 + data.OffsetY;
            x -= offsX + 1;
        }
    }

    SelectAnim.Direction = dir;
}

void CAnimationManager::DrawCharacter(CGameCharacter *obj, int x, int y)
{
    m_EquipConvItem = nullptr;

    m_Transform = false;

    int drawX = x + obj->OffsetX;
    int drawY = y + obj->OffsetY - obj->OffsetZ - 3;

    uint16_t targetColor = 0;
    bool needHPLine = false;
    uint32_t serial = obj->Serial;
    bool drawShadow = !obj->IsDead();
    m_UseBlending = false;

    if (g_DrawAura)
    {
        uint32_t auraColor = g_ColorManager.GetPolygoneColor(
            16, g_ConfigManager.GetColorByNotoriety(obj->Notoriety));
#ifndef NEW_RENDERER_ENABLED
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        glColor4ub(ToColorR(auraColor), ToColorG(auraColor), ToColorB(auraColor), 0xFF);

        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_One,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_SetColor(
            g_renderCmdList,
            SetColorCmd{ { ToColorR(auraColor) / 255.f,
                           ToColorG(auraColor) / 255.f,
                           ToColorB(auraColor) / 255.f,
                           ToColorA(auraColor) / 255.f } });

        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = SDM_NO_COLOR;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
        g_AuraTexture.Draw(drawX - g_AuraTexture.Width / 2, drawY - g_AuraTexture.Height / 2);

#ifndef NEW_RENDERER_ENABLED
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_BLEND);
#else
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
        RenderAdd_DisableBlend(g_renderCmdList);
#endif
    }

    if (obj->Hidden())
    {
        drawShadow = false;
        Color = 0x038E;

        if (obj->IsPlayer() || !g_ConfigManager.UseHiddenModeOnlyForSelf)
        {
            switch (g_ConfigManager.HiddenCharactersRenderMode)
            {
                case HCRM_ALPHA_BLENDING:
                {
                    m_UseBlending = true;

#ifndef NEW_RENDERER_ENABLED
                    glColor4ub(0xFF, 0xFF, 0xFF, g_ConfigManager.HiddenAlpha);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#else
                    RenderAdd_SetColor(
                        g_renderCmdList,
                        SetColorCmd{ { 1.f, 1.f, 1.f, g_ConfigManager.HiddenAlpha / 255.f } });
                    RenderAdd_SetBlend(
                        g_renderCmdList,
                        BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                                       BlendFactor::BlendFactor_OneMinusSrcAlpha });
#endif

                    Color = 0x038C;

                    break;
                }
                case HCRM_SPECTRAL_COLOR:
                {
                    Color = 0x4001;
                    break;
                }
                case HCRM_SPECIAL_SPECTRAL_COLOR:
                {
                    Color = 0x4666;
                    break;
                }
                default:
                    break;
            }
        }
    }
    else if (g_StatusbarUnderMouse == serial)
    {
        Color = g_ConfigManager.GetColorByNotoriety(obj->Notoriety);
    }
    else
    {
        Color = 0;

        if (g_ConfigManager.GetApplyStateColorOnCharacters())
        {
            if (obj->IsPoisoned() || obj->SA_Poisoned)
            {
                Color = 0x0044;
            }
            else if (obj->IsParalyzed())
            {
                Color = 0x014C;
            }
            else if (obj->Notoriety != NT_INVULNERABLE && obj->YellowHits() && !obj->NPC)
            {
                Color = 0x0030;
            }
            else if (obj->pvpCaller)
            {
                Color = 0x080D;
            }
        }

        if (obj->IsDead())
        {
            Color = 0x0386;
        }
    }

    uint8_t *drawTextureColor = obj->m_DrawTextureColor;

    if (!m_UseBlending && drawTextureColor[3] != 0xFF)
    {
        m_UseBlending = true;
#ifndef NEW_RENDERER_ENABLED
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(
            drawTextureColor[0], drawTextureColor[1], drawTextureColor[2], drawTextureColor[3]);
#else
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_SetColor(
            g_renderCmdList,
            SetColorCmd{ { drawTextureColor[0] / 255.f,
                           drawTextureColor[1] / 255.f,
                           drawTextureColor[2] / 255.f,
                           drawTextureColor[3] / 255.f } });
#endif
    }

    bool isAttack = (serial == g_LastAttackObject);
    bool underMouseTarget = (g_SelectedObject.Object == obj && g_Target.IsTargeting());

    if (!obj->IsPlayer() && (isAttack || underMouseTarget || serial == g_LastTargetObject))
    {
        targetColor = g_ConfigManager.GetColorByNotoriety(obj->Notoriety);

        if (isAttack || serial == g_LastTargetObject)
        {
            needHPLine = (serial != g_NewTargetSystem.Serial);
        }

        if (isAttack || underMouseTarget)
        {
            Color = targetColor;
        }
    }

    SelectAnim.Direction = 0;
    obj->UpdateAnimationInfo_ProcessSteps(SelectAnim.Direction);

    bool mirror = false;
    uint8_t layerDir = SelectAnim.Direction;

    GetAnimDirection(SelectAnim.Direction, mirror);

    const uint16_t graphic = /*GetGraphicForAnimation*/ obj->Graphic;
    uint8_t animIndex = obj->AnimIndex;
    uint8_t animGroup = obj->GetAnimationGroup(graphic, true);
    SelectAnim.Group = animGroup;

    CGameItem *goi = obj->FindLayer(OL_MOUNT);

    int lightOffset = 20;

    if (obj->IsHuman() && goi != nullptr && !obj->IsDrivingBoat()) //Draw mount
    {
        m_Sitting = 0;
        lightOffset += 20;

        uint16_t mountID = goi->GetMountAnimation();
        int mountedHeightOffset = 0;

        if (mountID < MAX_ANIMATIONS_DATA_INDEX_COUNT)
        {
            mountedHeightOffset = g_Index.m_Anim[mountID].MountedHeightOffset;
        }

        if (drawShadow)
        {
            Draw(obj, drawX, drawY + 10 + mountedHeightOffset, mirror, animIndex, 0x10000);
            SelectAnim.Group = obj->GetAnimationGroup(mountID, false);
            Draw(goi, drawX, drawY, mirror, animIndex, mountID + 0x10000);
        }
        else
        {
            SelectAnim.Group = obj->GetAnimationGroup(mountID, false);
        }

        Draw(goi, drawX, drawY, mirror, animIndex, mountID);
        drawY += mountedHeightOffset;
    }
    else
    {
        m_Sitting = obj->IsSitting();

        if (m_Sitting != 0)
        {
            animGroup = PAG_STAND;
            animIndex = 0;

            obj->UpdateAnimationInfo_ProcessSteps(SelectAnim.Direction);

            FixSittingDirection(layerDir, mirror, drawX, drawY);

            if (SelectAnim.Direction == 3)
            {
                animGroup = 25;
            }
            else
            {
                m_Transform = true;
            }
        }
        else if (drawShadow)
        {
            Draw(obj, drawX, drawY, mirror, animIndex, 0x10000);
        }
    }

    SelectAnim.Group = animGroup;

    Draw(obj, drawX, drawY, mirror, animIndex); //Draw character

    if (obj->IsHuman()) //Draw layered objects
    {
        DrawEquippedLayers(false, obj, drawX, drawY, mirror, layerDir, animIndex, lightOffset);

        const SITTING_INFO_DATA &sittingData = SITTING_INFO[m_Sitting - 1];

        if ((m_Sitting != 0) && SelectAnim.Direction == 3 && sittingData.DrawBack &&
            obj->FindLayer(OL_CLOAK) == nullptr)
        {
            for (CRenderWorldObject *ro = obj->m_PrevXY; ro != nullptr; ro = ro->m_PrevXY)
            {
                if ((ro->Graphic & 0x3FFF) == sittingData.Graphic)
                {
                    //оффсеты для ножниц
                    int xOffset = mirror ? -20 : 0;
                    int yOffset = -70;

#ifndef NEW_RENDERER_ENABLED
                    g_GL.PushScissor(
#else
                    Render_PushScissor(
#endif
                        drawX + xOffset,
                        g_GameWindow.GetSize().Height - drawY + yOffset - 40,
                        20,
                        40);
                    bool selected = g_SelectedObject.Object == ro;
                    g_Game.DrawStaticArt(
                        sittingData.Graphic,
                        selected ? 0x0035 : ro->Color,
                        ro->RealDrawX,
                        ro->RealDrawY,
                        !selected);
#ifndef NEW_RENDERER_ENABLED
                    g_GL.PopScissor();
#else
                    Render_PopScissor();
#endif
                    break;
                }
            }
        }
    }

    if (m_UseBlending)
    {
        m_UseBlending = false;
#ifndef NEW_RENDERER_ENABLED
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_BLEND);
#else
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
        RenderAdd_DisableBlend(g_renderCmdList);
#endif
    }

    if (!g_ConfigManager.DisableNewTargetSystem && g_NewTargetSystem.Serial == obj->Serial)
    {
        uint16_t id = obj->GetMountAnimation();

        if (id < MAX_ANIMATIONS_DATA_INDEX_COUNT)
        {
            assert(SelectAnim.Direction < MAX_MOBILE_DIRECTIONS && "Out-of-bound access");
            CTextureAnimationDirection &direction =
                g_Index.m_Anim[id].m_Groups[SelectAnim.Group].m_Direction[SelectAnim.Direction];

            if (direction.Address != 0 && direction.m_Frames != nullptr)
            {
                CTextureAnimationFrame &frame = direction.m_Frames[0];
                auto spr = (CSprite *)frame.UserData;
                assert(spr);

                int frameWidth = spr->Width;
                int frameHeight = spr->Height;

                if (frameWidth >= 80)
                {
                    g_NewTargetSystem.GumpTop = 0x756D;
                    g_NewTargetSystem.GumpBottom = 0x756A;
                }
                else if (frameWidth >= 40)
                {
                    g_NewTargetSystem.GumpTop = 0x756E;
                    g_NewTargetSystem.GumpBottom = 0x756B;
                }
                else
                {
                    g_NewTargetSystem.GumpTop = 0x756F;
                    g_NewTargetSystem.GumpBottom = 0x756C;
                }

                switch (obj->Notoriety)
                {
                    case NT_INNOCENT:
                    {
                        g_NewTargetSystem.ColorGump = 0x7570;
                        break;
                    }
                    case NT_FRIENDLY:
                    {
                        g_NewTargetSystem.ColorGump = 0x7571;
                        break;
                    }
                    case NT_SOMEONE_GRAY:
                    case NT_CRIMINAL:
                    {
                        g_NewTargetSystem.ColorGump = 0x7572;
                        break;
                    }
                    case NT_ENEMY:
                    {
                        g_NewTargetSystem.ColorGump = 0x7573;
                        break;
                    }
                    case NT_MURDERER:
                    {
                        g_NewTargetSystem.ColorGump = 0x7576;
                        break;
                    }
                    case NT_INVULNERABLE:
                    {
                        g_NewTargetSystem.ColorGump = 0x7575;
                        break;
                    }
                    default:
                        break;
                }

                int per = obj->MaxHits;

                if (per > 0)
                {
                    per = (obj->Hits * 100) / per;

                    if (per > 100)
                    {
                        per = 100;
                    }

                    if (per < 1)
                    {
                        per = 0;
                    }
                    else
                    {
                        per = (34 * per) / 100;
                    }
                }

                g_NewTargetSystem.Hits = per;
                g_NewTargetSystem.X = drawX;
                g_NewTargetSystem.TopY = drawY - frameHeight - 8;
                g_NewTargetSystem.BottomY = drawY + 7;
                g_NewTargetSystem.TargetedCharacter = obj;
                if (obj->IsPoisoned() || obj->SA_Poisoned)
                {
                    g_NewTargetSystem.HealthColor = 63; //Character status line (green)
                }
                else if (obj->YellowHits())
                {
                    g_NewTargetSystem.HealthColor = 53; //Character status line (green)
                }
                else
                {
                    g_NewTargetSystem.HealthColor = 90; //Character status line (blue)
                }
            }
        }
    }

    if (needHPLine)
    {
        int per = obj->MaxHits;

        if (per > 0)
        {
            per = (obj->Hits * 100) / per;

            if (per > 100)
            {
                per = 100;
            }

            if (per < 1)
            {
                per = 0;
            }
            else
            {
                per = (34 * per) / 100;
            }
        }

        if (isAttack)
        {
            PrepareTargetAttackGump(g_AttackTargetGump, drawX, drawY, targetColor, per, *obj);
        }
        else
        {
            PrepareTargetAttackGump(g_TargetGump, drawX, drawY, targetColor, per, *obj);
        }
    }
}

void CAnimationManager::PrepareTargetAttackGump(
    CTargetGump &gump, int drawX, int drawY, uint16_t targetColor, int per, CGameCharacter &obj)
{
    gump.X = drawX - 20;
    gump.Y = drawY;
    gump.Color = targetColor;
    gump.Hits = per;
    gump.TargetedCharacter = &obj;
    if (obj.IsPoisoned() || obj.SA_Poisoned)
    {
        gump.HealthColor = 63; //Character status line (green)
    }
    else if (obj.YellowHits())
    {
        gump.HealthColor = 53; //Character status line (green)
    }
    else
    {
        gump.HealthColor = 90; //Character status line (blue)
    }
}

bool CAnimationManager::CharacterPixelsInXY(CGameCharacter *obj, int x, int y)
{
    y -= 3;
    m_Sitting = obj->IsSitting();
    SelectAnim.Direction = 0;
    obj->UpdateAnimationInfo_ProcessSteps(SelectAnim.Direction);

    bool mirror = false;
    uint8_t layerDir = SelectAnim.Direction;

    GetAnimDirection(SelectAnim.Direction, mirror);

    uint8_t animIndex = obj->AnimIndex;
    uint8_t animGroup = obj->GetAnimationGroup(0, true);

    CGameItem *goi = obj->FindLayer(OL_MOUNT);

    int drawX = x - obj->OffsetX;
    int drawY = y - obj->OffsetY - obj->OffsetZ;

    if (obj->IsHuman() && goi != nullptr) //Check mount
    {
        uint16_t mountID = goi->GetMountAnimation();
        SelectAnim.Group = obj->GetAnimationGroup(mountID, false);
        if (TestPixels(goi, drawX, drawY, mirror, animIndex, mountID))
        {
            return true;
        }

        if (mountID < MAX_ANIMATIONS_DATA_INDEX_COUNT)
        {
            drawY += g_Index.m_Anim[mountID].MountedHeightOffset;
        }
    }
    else if (m_Sitting != 0)
    {
        animGroup = PAG_STAND;
        animIndex = 0;
        obj->UpdateAnimationInfo_ProcessSteps(SelectAnim.Direction);
        FixSittingDirection(layerDir, mirror, drawX, drawY);
        if (SelectAnim.Direction == 3)
        {
            animGroup = 25;
        }
    }

    SelectAnim.Group = animGroup;

    return TestPixels(obj, drawX, drawY, mirror, animIndex) ||
           DrawEquippedLayers(true, obj, drawX, drawY, mirror, layerDir, animIndex, 0);
}

void CAnimationManager::DrawCorpse(CGameItem *obj, int x, int y)
{
    if (g_CorpseManager.InList(obj->Serial, 0))
    {
        return;
    }

    m_Sitting = 0;
    SelectAnim.Direction = (obj->Layer & 0x7F) & 7;
    bool mirror = false;

    GetAnimDirection(SelectAnim.Direction, mirror);

    if (obj->Hidden())
    {
        Color = 0x038E;
    }
    else
    {
        Color = 0;
    }

    uint8_t animIndex = obj->AnimIndex;
    SelectAnim.Group = GetDieGroupIndex(obj->GetMountAnimation(), obj->UsedLayer != 0u);

    Draw(obj, x, y, mirror, animIndex); //Draw animation

    DrawEquippedLayers(false, obj, x, y, mirror, SelectAnim.Direction, animIndex, 0);
}

bool CAnimationManager::CorpsePixelsInXY(CGameItem *obj, int x, int y)
{
    if (g_CorpseManager.InList(obj->Serial, 0))
    {
        return false;
    }

    m_Sitting = 0;
    SelectAnim.Direction = (obj->Layer & 0x7F) & 7;
    bool mirror = false;

    GetAnimDirection(SelectAnim.Direction, mirror);

    uint8_t animIndex = obj->AnimIndex;
    SelectAnim.Group = GetDieGroupIndex(obj->GetMountAnimation(), obj->UsedLayer != 0u);

    return TestPixels(obj, x, y, mirror, animIndex) ||
           DrawEquippedLayers(true, obj, x, y, mirror, SelectAnim.Direction, animIndex, 0);
}

bool CAnimationManager::AnimationExists(uint16_t graphic, uint8_t group)
{
    bool result = false;
    if (graphic < MAX_ANIMATIONS_DATA_INDEX_COUNT && group < MAX_ANIMATION_GROUPS_COUNT)
    {
        auto groupDir = g_Index.m_Anim[graphic].m_Groups[group].m_Direction[0];
        result = groupDir.Address != 0 || groupDir.IsUOP;
    }
    return result;
}

AnimationFrameInfo CAnimationManager::GetAnimationDimensions(
    uint8_t frameIndex, uint16_t id, uint8_t dir, uint8_t animGroup, bool isCorpse)
{
    AnimationFrameInfo result = {};
    if (id < MAX_ANIMATIONS_DATA_INDEX_COUNT)
    {
        CTextureAnimationGroup &group = g_Index.m_Anim[id].m_Groups[animGroup];
        if (dir < MAX_MOBILE_DIRECTIONS)
        {
            auto &direction = group.m_Direction[dir];
            int fc = direction.FrameCount;
            if (fc > 0)
            {
                if (frameIndex >= fc)
                {
                    if (isCorpse)
                    {
                        frameIndex = fc - 1;
                    }
                    else
                    {
                        frameIndex = 0;
                    }
                }

                if (direction.m_Frames != nullptr)
                {
                    CTextureAnimationFrame &frame = direction.m_Frames[frameIndex];
                    auto spr = (CSprite *)frame.UserData;
                    //assert(spr);
                    if (!spr)
                    {
                        // FIXME: before we had CSprite in the struct, we need cleanup how to construct these objects
                        spr = new CSprite();
                    }
                    result.Width = spr->Width;
                    result.Height = spr->Height;
                    result.CenterX = frame.CenterX;
                    result.CenterY = frame.CenterY;
                    return result;
                }
            }
        }
        CTextureAnimationDirection &direction = group.m_Direction[0];
        g_FileManager.LoadAnimationFrameInfo(result, direction, group, frameIndex, isCorpse);
    }
    return result;
}

AnimationFrameInfo CAnimationManager::GetAnimationDimensions(
    CGameObject *obj, uint8_t frameIndex, uint8_t defaultDirection, uint8_t defaultGroup)
{
    uint8_t dir = defaultDirection & 0x7F;
    uint8_t animGroup = defaultGroup;
    uint16_t id = obj->GetMountAnimation();
    bool mirror = false;

    if (obj->NPC)
    {
        CGameCharacter *gc = obj->GameCharacterPtr();
        gc->UpdateAnimationInfo_ProcessSteps(dir);
        animGroup = gc->GetAnimationGroup(0, true);
        GetAnimDirection(dir, mirror);
    }
    else if (obj->IsCorpse())
    {
        dir = ((CGameItem *)obj)->Layer & 7;
        animGroup = GetDieGroupIndex(id, ((CGameItem *)obj)->UsedLayer != 0u);
        GetAnimDirection(dir, mirror);
    }
    else if (((CGameItem *)obj)->Layer != OL_MOUNT)
    { //TGameItem
        id = ((CGameItem *)obj)->AnimID;
    }

    if (frameIndex == 0xFF)
    {
        frameIndex = (uint8_t)obj->AnimIndex;
    }

    AnimationFrameInfo dims =
        GetAnimationDimensions(frameIndex, id, dir, animGroup, obj->IsCorpse());

    if ((dims.Width == 0) && (dims.Height == 0) && (dims.CenterX == 0) && (dims.CenterY == 0))
    {
        dims.Width = 20;

        if (obj->NPC && obj->IsMounted())
        {
            dims.Height = 100;
        }
        else
        {
            dims.Height = 60;
        }
    }

    return dims;
}

DRAW_FRAME_INFORMATION
CAnimationManager::CollectFrameInformation(CGameObject *gameObject, bool checkLayers)
{
    m_Sitting = 0;
    SelectAnim.Direction = 0;

    DRAW_FRAME_INFORMATION dfInfo = {};

    std::vector<CGameItem *> &list = gameObject->m_DrawLayeredObjects;

    if (checkLayers)
    {
        list.clear();

        memset(&m_CharacterLayerGraphic[0], 0, sizeof(m_CharacterLayerGraphic));
        memset(&m_CharacterLayerAnimID[0], 0, sizeof(m_CharacterLayerAnimID));

        QFOR(item, gameObject->m_Items, CGameItem *)
        {
            if (item->Layer < OL_MOUNT)
            {
                m_CharacterLayerGraphic[item->Layer] = item->Graphic;
                m_CharacterLayerAnimID[item->Layer] = item->AnimID;
            }
        }
    }

    if (gameObject->NPC)
    {
        CGameCharacter *obj = (CGameCharacter *)gameObject;
        obj->UpdateAnimationInfo_ProcessSteps(SelectAnim.Direction);

        bool mirror = false;
        uint8_t layerDir = SelectAnim.Direction;

        GetAnimDirection(SelectAnim.Direction, mirror);

        uint8_t animIndex = obj->AnimIndex;
        uint8_t animGroup = obj->GetAnimationGroup(0, true);

        FRAME_OUTPUT_INFO info = {};

        CGameItem *goi = obj->FindLayer(OL_MOUNT);

        if (goi != nullptr) //Check mount
        {
            uint16_t mountID = goi->GetMountAnimation();
            SelectAnim.Group = obj->GetAnimationGroup(mountID, false);
            CalculateFrameInformation(info, goi, mirror, animIndex);
            switch (animGroup)
            {
                case PAG_FIDGET_1:
                case PAG_FIDGET_2:
                case PAG_FIDGET_3:
                {
                    animGroup = PAG_ONMOUNT_STAND;
                    animIndex = 0;
                    break;
                }
                default:
                    break;
            }
        }

        SelectAnim.Group = animGroup;

        CalculateFrameInformation(info, obj, mirror, animIndex);

        if (obj->IsHuman() && checkLayers) //Check layred objects
        {
            for (int l = 0; l < USED_LAYER_COUNT; l++)
            {
                assert(layerDir < MAX_LAYER_DIRECTIONS && "Out-of-bounds access");
                goi = obj->FindLayer(m_UsedLayers[layerDir][l]);

                if (goi == nullptr || (goi->AnimID == 0u))
                {
                    continue;
                }

                if (!IsCovered(goi->Layer, obj))
                {
                    list.push_back(goi);
                    CalculateFrameInformation(info, goi, mirror, animIndex);
                }
            }
        }

        dfInfo.OffsetX = abs(info.StartX);
        dfInfo.OffsetY = abs(info.StartY);
        dfInfo.Width = dfInfo.OffsetX + info.EndX;
        dfInfo.Height = dfInfo.OffsetY + info.EndY;
    }
    else if (gameObject->IsCorpse())
    {
        CGameItem *obj = (CGameItem *)gameObject;

        SelectAnim.Direction = (obj->Layer & 0x7F) & 7;
        bool mirror = false;

        GetAnimDirection(SelectAnim.Direction, mirror);

        uint8_t animIndex = obj->AnimIndex;
        SelectAnim.Group = GetDieGroupIndex(obj->GetMountAnimation(), obj->UsedLayer != 0u);

        FRAME_OUTPUT_INFO info = {};

        CalculateFrameInformation(info, obj, mirror, animIndex);

        if (checkLayers)
        {
            for (int l = 0; l < USED_LAYER_COUNT; l++)
            {
                assert(SelectAnim.Direction < MAX_LAYER_DIRECTIONS && "Out-of-bounds access");
                CGameItem *goi = obj->FindLayer(m_UsedLayers[SelectAnim.Direction][l]);

                if (goi != nullptr && (goi->AnimID != 0u))
                {
                    if (!IsCovered(goi->Layer, obj))
                    {
                        list.push_back(goi);
                        CalculateFrameInformation(info, goi, mirror, animIndex);
                    }
                }
            }
        }

        dfInfo.OffsetX = abs(info.StartX);
        dfInfo.OffsetY = abs(info.StartY);
        dfInfo.Width = dfInfo.OffsetX + info.EndX;
        dfInfo.Height = dfInfo.OffsetY + info.EndY;
    }

    return dfInfo;
}

bool CAnimationManager::DrawEquippedLayers(
    bool selection,
    CGameObject *obj,
    int drawX,
    int drawY,
    bool mirror,
    uint8_t layerDir,
    uint8_t animIndex,
    int lightOffset)
{
    bool result = false;

    std::vector<CGameItem *> &list = obj->m_DrawLayeredObjects;

    uint16_t bodyGraphic = obj->Graphic;

    if (obj->IsCorpse())
    {
        bodyGraphic = obj->Count;
    }

    EQUIP_CONV_BODY_MAP::iterator bodyMapIter = m_EquipConv.find(bodyGraphic);

    if (selection)
    {
        for (auto i = list.begin(); i != list.end() && !result; ++i)
        {
            uint16_t id = (*i)->AnimID;

            if (bodyMapIter != m_EquipConv.end())
            {
                EQUIP_CONV_DATA_MAP::iterator dataIter = bodyMapIter->second.find(id);

                if (dataIter != bodyMapIter->second.end())
                {
                    id = dataIter->second.Graphic;
                }
            }

            result = TestPixels(*i, drawX, drawY, mirror, animIndex, id);
        }
    }
    else
    {
        for (auto i = list.begin(); i != list.end(); ++i)
        {
            CGameItem *item = *i;

            uint16_t id = item->AnimID;

            if (bodyMapIter != m_EquipConv.end())
            {
                EQUIP_CONV_DATA_MAP::iterator dataIter = bodyMapIter->second.find(id);

                if (dataIter != bodyMapIter->second.end())
                {
                    m_EquipConvItem = &dataIter->second;
                    id = m_EquipConvItem->Graphic;
                }
            }

            Draw(item, drawX, drawY, mirror, animIndex, id);
            m_EquipConvItem = nullptr;

            if (item->IsLightSource() && g_GameScreen.UseLight)
            {
                g_GameScreen.AddLight(obj, item, drawX, drawY - lightOffset);
            }
        }
    }

    return result;
}

bool CAnimationManager::IsCovered(int layer, CGameObject *owner)
{
    bool result = false;

    switch (layer)
    {
        case OL_SHOES:
        {
            if (m_CharacterLayerGraphic[OL_LEGS] != 0 ||
                m_CharacterLayerGraphic[OL_PANTS] == 0x1411)
            {
                result = true;
            }
            else if (
                m_CharacterLayerAnimID[OL_PANTS] == 0x0513 ||
                m_CharacterLayerAnimID[OL_PANTS] == 0x0514 ||
                m_CharacterLayerAnimID[OL_ROBE] == 0x0504)
            {
                result = true;
            }

            break;
        }
        case OL_PANTS:
        {
            uint16_t robe = m_CharacterLayerAnimID[OL_ROBE];
            uint16_t pants = m_CharacterLayerAnimID[OL_PANTS];

            if (m_CharacterLayerGraphic[OL_LEGS] != 0 || robe == 0x0504)
            {
                result = true;
            }
            if (pants == 0x01EB || pants == 0x03E5 || pants == 0x03EB)
            {
                uint16_t skirt = m_CharacterLayerAnimID[OL_SKIRT];

                if (skirt != 0x01C7 && skirt != 0x01E4)
                {
                    result = true;
                }
                else if (robe != 0x0229 && (robe <= 0x04E7 || robe > 0x04EB))
                {
                    result = true;
                }
            }

            break;
        }
        case OL_TUNIC:
        {
            uint16_t robe = m_CharacterLayerGraphic[OL_ROBE];

            if (robe != 0)
            {
                result = true;
            }
            else if (m_CharacterLayerGraphic[OL_TUNIC] == 0x0238)
            {
                result = (robe != 0x9985 && robe != 0x9986);
            }

            break;
        }
        case OL_TORSO:
        {
            uint16_t robe = m_CharacterLayerGraphic[OL_ROBE];

            if (robe != 0 && robe != 0x9985 && robe != 0x9986)
            {
                result = true;
            }
            else
            {
                uint16_t tunic = m_CharacterLayerGraphic[OL_TUNIC];
                uint16_t torso = m_CharacterLayerGraphic[OL_TORSO];

                if (tunic != 0 && tunic != 0x1541 && tunic != 0x1542)
                {
                    result = true;
                }
                else if (torso == 0x782A || torso == 0x782B)
                {
                    result = true;
                }
            }

            break;
        }
        case OL_ARMS:
        {
            uint16_t robe = m_CharacterLayerGraphic[OL_ROBE];
            result = (robe != 0 && robe != 0x9985 && robe != 0x9986);

            break;
        }
        case OL_HELMET:
            if (g_ConfigManager.DrawHelmetsOnShroud)
            {
                break;
            }
        case OL_HAIR:
        {
            uint16_t robe = m_CharacterLayerGraphic[OL_ROBE];

            if (robe > 0x3173)
            {
                if ((robe == 0x4B9D || robe == 0x7816) /*&& wat?*/)
                {
                    result = true;
                }
            }
            else
            {
                if (robe <= 0x2687)
                {
                    if (robe < 0x2683)
                    {
                        result = (robe >= 0x204E && robe <= 0x204F);
                    }
                    else
                    {
                        result = true;
                    }
                }
                else if (robe == 0x2FB9 || robe == 0x3173)
                {
                    result = true;
                }
            }

            break;
        }
        case OL_SKIRT:
        {
            uint16_t skirt = m_CharacterLayerAnimID[OL_SKIRT];

            if (skirt == 0x01C7 || skirt == 0x01E4)
            {
                //uint16_t pants = m_CharacterLayerAnimID[OL_PANTS];

                //result = (!pants || pants == 0x0200);
            }

            break;
        }
        default:
            break;
    }

    return result;
}

uint8_t CAnimationManager::GetReplacedObjectAnimation(CGameCharacter *obj, uint16_t index)
{
    auto getReplaceGroup = [](const std::vector<std::pair<uint16_t, uint8_t>> &list,
                              uint16_t index,
                              uint16_t walkIndex) -> uint16_t {
        for (const auto &item : list)
        {
            if (item.first == index)
            {
                if (item.second == 0xff)
                {
                    return walkIndex;
                }
                return (uint16_t)item.second;
            }
        }
        return index;
    };

    const ANIMATION_GROUPS group = GetGroupIndex(obj->Graphic);
    if (group == AG_LOW)
    {
        return (uint8_t)(
            getReplaceGroup(m_GroupReplaces[0], index, LAG_WALK) % LAG_ANIMATION_COUNT);
    }
    if (group == AG_PEOPLE)
    {
        return (uint8_t)(
            getReplaceGroup(m_GroupReplaces[1], index, PAG_WALK_UNARMED) % PAG_ANIMATION_COUNT);
    }
    return (uint8_t)(index % HAG_ANIMATION_COUNT);
}

bool CAnimationManager::IsReplacedObjectAnimation(uint8_t anim, uint16_t v13) const
{
    if (anim >= countof(m_GroupReplaces))
        return false;

    for (const auto &item : m_GroupReplaces[anim])
    {
        if (item.first == v13)
        {
            return item.second != 0xff;
        }
    }
    return false;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_0(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    if (action <= 10)
    {
        const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
        ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
        if (ia.Flags & AF_FOUND)
        {
            type = ia.Type;
        }

        if (type == AGT_MONSTER)
        {
            switch (mode % 4)
            {
                case 1:
                    return 5;
                case 2:
                    return 6;
                case 3:
                    if (ia.Flags & AF_UNKNOWN_1)
                    {
                        return 12;
                    }
                case 0:
                    return 4;
                default:
                    break;
            }
        }
        else if (type == AGT_SEA_MONSTER)
        {
            if ((mode % 2) != 0)
            {
                return 6;
            }
            return 5;
        }
        else if (type != AGT_ANIMAL)
        {
            if (obj->IsMounted())
            {
                if (action > 0)
                {
                    if (action == 1)
                    {
                        return 27;
                    }
                    if (action == 2)
                    {
                        return 28;
                    }
                    return 26;
                }
                return 29;
            }

            switch (action)
            {
                case 1:
                    return 18;
                case 2:
                    return 19;
                case 6:
                    return 12;
                case 7:
                    if (obj->IsGargoyle() && obj->IsFlying() &&
                        g_AnimationManager.AnimationExists(obj->Graphic, 72))
                    {
                        return 72;
                    }
                    return 13;
                case 8:
                    return 14;
                case 3:
                    return 11;
                case 4:
                    return 9;
                case 5:
                    return 10;
                default:
                    if (obj->IsGargoyle() && obj->IsFlying() &&
                        g_AnimationManager.AnimationExists(obj->Graphic, 71))
                    {
                        return 71;
                    }
                    else if (g_AnimationManager.AnimationExists(obj->Graphic, 31))
                    {
                        return 31;
                    }
            }
        }

        if (ia.Flags & AF_USE_2_IF_HITTED_WHILE_RUNNING)
        {
            return 2;
        }

        if ((mode % 2) != 0)
        {
            return 6;
        }
        return 5;
    }
    return 0;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_1_2(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
    if (ia.Flags & AF_FOUND)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type <= AGT_ANIMAL || obj->IsMounted())
        {
            return 0xff;
        }
        return 30;
    }
    if ((mode % 2) != 0)
    {
        return 15;
    }
    return 16;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_3(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
    if (ia.Flags & AF_FOUND)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type == AGT_SEA_MONSTER)
        {
            return 8;
        }
        if (type == AGT_ANIMAL)
        {
            if ((mode % 2) != 0)
            {
                return 21;
            }
            return 22;
        }

        if ((mode % 2) != 0)
        {
            return 8;
        }
        return 12;
    }
    if ((mode % 2) != 0)
    {
        return 2;
    }
    return 3;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_4(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
    if (ia.Flags & AF_FOUND)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type > AGT_ANIMAL)
        {
            if (obj->IsGargoyle() && obj->IsFlying() &&
                g_AnimationManager.AnimationExists(obj->Graphic, 77))
            {
                return 77;
            }
            if (obj->IsMounted())
            {
                return 0xff;
            }
            return 20;
        }
        return 7;
    }
    return 10;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_5(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
    if (ia.Flags & AF_FOUND)
    {
        type = ia.Type;
    }

    if (type <= AGT_SEA_MONSTER)
    {
        if ((mode % 2) != 0)
        {
            return 18;
        }
        return 17;
    }

    if (type != AGT_ANIMAL)
    {
        if (obj->IsMounted())
        {
            return 0xff;
        }

        if ((mode % 2) != 0)
        {
            return 6;
        }
        return 5;
    }

    switch (mode % 3)
    {
        case 1:
            return 10;
        case 2:
            return 3;
        default:
            break;
    }
    return 9;
}

uint8_t CAnimationManager::GetObjectNewAnimationType_6_14(
    CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
    if (ia.Flags & AF_FOUND)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type != AGT_SEA_MONSTER)
        {
            if (type == AGT_ANIMAL)
            {
                return 3;
            }

            if (obj->IsMounted())
            {
                return 0xff;
            }
            return 34;
        }
        return 5;
    }
    return 11;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_7(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    if (obj->IsMounted())
    {
        return 0xff;
    }

    if (action > 0)
    {
        if (action == 1)
        {
            return 33;
        }
    }
    else
    {
        return 32;
    }
    return 0;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_8(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
    if (ia.Flags & AF_FOUND)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type != AGT_SEA_MONSTER)
        {
            if (type == AGT_ANIMAL)
            {
                return 9;
            }

            if (obj->IsMounted())
            {
                return 0xff;
            }
            return 33;
        }
        return 3;
    }
    return 11;
}

uint8_t CAnimationManager::GetObjectNewAnimationType_9_10(
    CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
    if (ia.Flags & AF_FOUND)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (obj->IsGargoyle())
        {
            if (obj->IsFlying())
            {
                if (action == 0)
                {
                    return 60;
                }
            }
            else
            {
                if (action == 0)
                {
                    return 61;
                }
            }
        }
        return 0xff;
    }
    return 20;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_11(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    const CIndexAnimation &ia = g_Index.m_Anim[obj->Graphic];
    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;
    if (ia.Flags & AF_FOUND)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type >= AGT_ANIMAL)
        {
            if (obj->IsMounted())
            {
                return 0xff;
            }

            switch (action)
            {
                case 1:
                case 2:
                    if (obj->IsGargoyle() && obj->IsFlying())
                    {
                        return 76;
                    }
                    return 17;
                default:
                    break;
            }
            if (obj->IsGargoyle() && obj->IsFlying())
            {
                return 75;
            }
            return 16;
        }
        return 5;
    }
    return 12;
}

uint8_t CAnimationManager::GetObjectNewAnimation(
    CGameCharacter *obj, uint16_t type, uint16_t action, uint8_t mode)
{
    if (obj->Graphic >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
    {
        return 0;
    }

    switch (type)
    {
        case 0:
            return GetObjectNewAnimationType_0(obj, action, mode);
        case 1:
        case 2:
            return GetObjectNewAnimationType_1_2(obj, action, mode);
        case 3:
            return GetObjectNewAnimationType_3(obj, action, mode);
        case 4:
            return GetObjectNewAnimationType_4(obj, action, mode);
        case 5:
            return GetObjectNewAnimationType_5(obj, action, mode);
        case 6:
        case 14:
            return GetObjectNewAnimationType_6_14(obj, action, mode);
        case 7:
            return GetObjectNewAnimationType_7(obj, action, mode);
        case 8:
            return GetObjectNewAnimationType_8(obj, action, mode);
        case 9:
        case 10:
            return GetObjectNewAnimationType_9_10(obj, action, mode);
        case 11:
            return GetObjectNewAnimationType_11(obj, action, mode);
        default:
            break;
    }
    return 0;
}

CTextureAnimationDirection &
CAnimationManager::ExecuteAnimation(uint8_t group, uint8_t direction, uint16_t graphic)
{
    SelectAnim.Group = group;
    SelectAnim.Direction = direction;
    SelectAnim.Graphic = graphic;

    CTextureAnimationGroup &grp = g_Index.m_Anim[graphic].m_Groups[group];
    CTextureAnimationDirection &dir = grp.m_Direction[direction];
    if (dir.FrameCount == 0)
    {
        if (g_FileManager.LoadAnimation(SelectAnim, LoadSpritePixels))
        {
            m_UsedAnimList.push_back(&dir);
        }
    }
    return dir;
}

static void LABEL_222(ANIMATION_FLAGS flags, uint16_t &v13)
{
    if (flags & AF_CALCULATE_OFFSET_LOW_GROUP_EXTENDED)
    {
        switch (v13)
        {
            case 0:
                v13 = 0;
                goto LABEL_243;
            case 1:
                v13 = 19;
                goto LABEL_243;
            case 5:
            case 6:
                if (flags & AF_IDLE_AT_8_FRAME)
                    v13 = 4;
                else
                    v13 = uint16_t(6 - (RandomInt(2) & 1));
                goto LABEL_243;
            case 8:
                v13 = 2;
                goto LABEL_243;
            case 9:
                v13 = 17;
                goto LABEL_243;
            case 10:
                v13 = 18;
                if (flags & AF_IDLE_AT_8_FRAME)
                    v13--;
                goto LABEL_243;
            case 12:
                v13 = 3;
                goto LABEL_243;
        }
        // LABEL_241
        v13 = 1;
    }
    else
    {
        if (flags & AF_CALCULATE_OFFSET_BY_LOW_GROUP)
        {
            switch (v13)
            {
                case 0:
                    // LABEL_232
                    v13 = 0;
                    break;
                case 2:
                    v13 = 8;
                    break;
                case 3:
                    v13 = 12;
                    break;
                case 4:
                case 6:
                case 7:
                case 8:
                case 9:
                case 12:
                case 13:
                case 14:
                    v13 = 5;
                    break;
                case 5:
                    v13 = 6;
                    break;
                case 10:
                case 21:
                    v13 = 7;
                    break;
                case 11:
                    //LABEL_238:
                    v13 = 3;
                    break;
                case 17:
                    v13 = 9;
                    break;
                case 18:
                    v13 = 10;
                    break;
                case 19:
                    v13 = 1;
                    break;
                default:
                    //LABEL_242:
                    v13 = 2;
                    break;
            }
        }
    }

LABEL_243:
    v13 = uint16_t(v13 & 0x7F);
    if (v13 > 34)
        v13 = 0;
    //return uint8_t(v13);
}

static void LABEL_190(ANIMATION_FLAGS flags, uint16_t &v13)
{
    if (flags & AF_UNKNOWN_80 && v13 == 4)
    {
        v13 = 5;
    }

    if (flags & AF_UNKNOWN_200)
    {
        if (v13 - 7 > 9)
        {
            if (v13 == 19)
            {
                //LABEL_196
                v13 = 0;
            }
            else if (v13 > 19)
            {
                v13 = 1;
            }
            LABEL_222(flags, v13);
            return;
        }
    }
    else
    {
        if (flags & AF_UNKNOWN_100)
        {
            switch (v13)
            {
                case 10:
                case 15:
                case 16:
                    v13 = 1;
                    LABEL_222(flags, v13);
                    return;
                case 11:
                    v13 = 17;
                    LABEL_222(flags, v13);
                    return;
            }
            LABEL_222(flags, v13);
            return;
        }

        if (flags & AF_UNKNOWN_1)
        {
            if (v13 == 21)
            {
                v13 = 10;
            }
            LABEL_222(flags, v13);
            return;
        }

        if (flags & AF_CALCULATE_OFFSET_BY_PEOPLE_GROUP)
        {
            //LABEL_222:
            LABEL_222(flags, v13);
            return;
        }

        switch (v13)
        {
            case 0:
                v13 = 0;
                break;
            case 2:
                v13 = 21;
                LABEL_222(flags, v13);
                return;
            case 3:
                v13 = 22;
                LABEL_222(flags, v13);
                return;
            case 4:
            case 9:
                v13 = 9;
                LABEL_222(flags, v13);
                return;
            case 5:
                v13 = 11;
                LABEL_222(flags, v13);
                return;
            case 6:
                v13 = 13;
                LABEL_222(flags, v13);
                return;
            case 7:
                v13 = 18;
                LABEL_222(flags, v13);
                return;
            case 8:
                v13 = 19;
                LABEL_222(flags, v13);
                return;
            case 10:
            case 21:
                v13 = 20;
                LABEL_222(flags, v13);
                return;
            case 11:
                v13 = 3;
                LABEL_222(flags, v13);
                return;
            case 12:
            case 14:
                v13 = 16;
                LABEL_222(flags, v13);
                return;
            case 13:
                //LABEL_202:
                v13 = 17;
                LABEL_222(flags, v13);
                return;
            case 15:
            case 16:
                v13 = 30;
                LABEL_222(flags, v13);
                return;
            case 17:
                v13 = 5;
                LABEL_222(flags, v13);
                return;
            case 18:
                v13 = 6;
                LABEL_222(flags, v13);
                return;
            case 19:
                //LABEL_201:
                v13 = 1;
                LABEL_222(flags, v13);
                return;
        }
    }
    v13 = 4;
    LABEL_222(flags, v13);
}

uint8_t CAnimationManager::CorrectAnimationGroupServer(
    ANIMATION_GROUPS_TYPE type, ANIMATION_FLAGS flags, uint16_t v13) const
{
    auto originalType = type;
    if (v13 == 12)
    {
        if (!(type == AGT_HUMAN || type == AGT_EQUIPMENT || flags & AF_UNKNOWN_1000))
        {
            if (type != AGT_MONSTER)
            {
                if (type == AGT_HUMAN || type == AGT_EQUIPMENT)
                {
                    v13 = 16;
                }
                else
                    v13 = 5;
            }
            else
                v13 = 4;
        }
    }

    if (type != AGT_MONSTER)
    {
        if (type != AGT_SEA_MONSTER)
        {
            if (type == AGT_ANIMAL)
            {
                if (IsReplacedObjectAnimation(0, v13))
                {
                    originalType = AGT_UNKNOWN;
                }
                if (v13 > 12)
                {
                    v13 = 0; // 2
                }
            }
            else
            {
                if (IsReplacedObjectAnimation(1, v13))
                {
                    // LABEL_190:
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                }
            }
        }
        else
        {
            if (IsReplacedObjectAnimation(3, v13))
            {
                originalType = AGT_UNKNOWN;
            }
            if (v13 > 8)
            {
                v13 = 2;
            }
        }
    }
    else
    {
        if (IsReplacedObjectAnimation(2, v13))
        {
            originalType = AGT_UNKNOWN;
        }
        if (v13 > 21)
        {
            v13 = 1;
        }
    }

    if (originalType == AGT_UNKNOWN)
    {
        LABEL_190(flags, v13);
        return uint8_t(v13);
    }

    if (originalType != 0)
    {
        if (originalType == AGT_ANIMAL && type == AGT_MONSTER)
        {
            switch (v13)
            {
                case 0:
                    v13 = 0;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 1:
                    v13 = 19;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 3:
                    v13 = 11;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 5:
                    v13 = 4;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 6:
                    v13 = 5;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 7:
                case 11:
                    v13 = 10;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 8:
                    v13 = 2;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 9:
                    v13 = 17;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 10:
                    v13 = 18;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 12:
                    v13 = 3;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
            }
            // LABEL_187
            v13 = 1;
        }
        LABEL_190(flags, v13);
        return uint8_t(v13);
    }

    switch (type)
    {
        case AGT_HUMAN:
        {
            switch (v13)
            {
                case 0:
                    v13 = 0;
                    goto LABEL_189;
                case 2:
                    v13 = 21;
                    goto LABEL_189;
                case 3:
                    v13 = 22;
                    goto LABEL_189;
                case 4:
                case 9:
                    v13 = 9;
                    goto LABEL_189;
                case 5:
                    //LABEL_163:
                    v13 = 11;
                    goto LABEL_189;
                case 6:
                    v13 = 13;
                    goto LABEL_189;
                case 7:
                    //LABEL_165:
                    v13 = 18;
                    goto LABEL_189;
                case 8:
                    //LABEL_172:
                    v13 = 19;
                    goto LABEL_189;
                case 10:
                case 21:
                    v13 = 20;
                    goto LABEL_189;
                case 12:
                case 14:
                    v13 = 16;
                    goto LABEL_189;
                case 13:
                    //LABEL_164:
                    v13 = 17;
                    goto LABEL_189;
                case 15:
                case 16:
                    v13 = 30;
                    goto LABEL_189;
                case 17:
                    v13 = 5;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 18:
                    v13 = 6;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 19:
                    v13 = 1;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
            }
            //LABEL_161:
            v13 = 4;
            goto LABEL_189;
        }
        case AGT_ANIMAL:
        {
            switch (v13)
            {
                case 0:
                    v13 = 0;
                    goto LABEL_189;
                case 2:
                    v13 = 8;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 3:
                    v13 = 12;
                    goto LABEL_189;
                case 4:
                case 6:
                case 7:
                case 8:
                case 9:
                case 12:
                case 13:
                case 14:
                    v13 = 5;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 5:
                    v13 = 6;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 10:
                case 21:
                    v13 = 7;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 11:
                    v13 = 3;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
                case 17:
                    //LABEL_170:
                    v13 = 9;
                    goto LABEL_189;
                case 18:
                    //LABEL_162:
                    v13 = 10;
                    goto LABEL_189;
                case 19:
                    v13 = 1;
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
            }
            v13 = 2;
            LABEL_190(flags, v13);
            return uint8_t(v13);
        }
        case AGT_SEA_MONSTER:
        {
            switch (v13)
            {
                case 0:
                    //LABEL_182:
                    v13 = 0;
                    goto LABEL_189;
                case 2:
                case 3:
                    //LABEL_178:
                    v13 = 8;
                    goto LABEL_189;
                case 4:
                case 6:
                case 7:
                case 8:
                case 9:
                case 12:
                case 13:
                case 14:
                    //LABEL_183:
                    v13 = 5;
                    goto LABEL_189;
                case 5:
                    //LABEL_184:
                    v13 = 6;
                    goto LABEL_189;
                case 10:
                case 21:
                    //LABEL_185:
                    v13 = 7;
                    goto LABEL_189;
                case 17:
                    //LABEL_186:
                    v13 = 3;
                    goto LABEL_189;
                case 18:
                    v13 = 4;
                    goto LABEL_189;
                case 19:
                    LABEL_190(flags, v13);
                    return uint8_t(v13);
            }
            v13 = 2;
            LABEL_190(flags, v13);
            return uint8_t(v13);
        }
        default:
        {
        LABEL_189:
            LABEL_190(flags, v13);
            return uint8_t(v13);
        }
    }
    // LABEL_188
    v13 = 2;
    LABEL_190(flags, v13);
    return uint8_t(v13);
}
