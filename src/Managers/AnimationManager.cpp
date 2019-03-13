// MIT License
// Copyright (C) August 2016 Hotride

#include "AnimationManager.h"
#include "MouseManager.h"
#include "ConfigManager.h"
#include "ColorManager.h"
#include "CorpseManager.h"
#include "FileManager.h"
#include "../Application.h"
#include "../Target.h"
#include "../Constants.h"
#include "../Config.h"
#include "../CrossUO.h"
#include "../TargetGump.h"
#include "../SelectedObject.h"
#include "../PartyObject.h"
#include "../GameWindow.h"
#include "../ScreenStages/GameScreen.h"
#include "../GameObjects/GameCharacter.h"

CAnimationManager g_AnimationManager;

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
    DEBUG_TRACE_FUNCTION;
    memset(m_AddressIdx, 0, sizeof(m_AddressIdx));
    memset(m_SizeIdx, 0, sizeof(m_SizeIdx));

    memset(m_CharacterLayerGraphic, 0, sizeof(m_CharacterLayerGraphic));
    memset(m_CharacterLayerAnimID, 0, sizeof(m_CharacterLayerAnimID));
}

CAnimationManager::~CAnimationManager()
{
    DEBUG_TRACE_FUNCTION;
    ClearUnusedTextures(g_Ticks + 100000);
}

void CAnimationManager::UpdateAnimationAddressTable()
{
    for (int i = 0; i < MAX_ANIMATIONS_DATA_INDEX_COUNT; i++)
    {
        CIndexAnimation &index = m_DataIndex[i];

        for (int g = 0; g < ANIMATION_GROUPS_COUNT; g++)
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

void CAnimationManager::Load(uint32_t *verdata)
{
    DEBUG_TRACE_FUNCTION;
    size_t maxAddress = m_AddressIdx[0] + m_SizeIdx[0];

    for (int i = 0; i < MAX_ANIMATIONS_DATA_INDEX_COUNT; i++)
    {
        CIndexAnimation &index = m_DataIndex[i];

        ANIMATION_GROUPS_TYPE groupType = AGT_UNKNOWN;
        size_t findID = 0;

        if (i >= 200)
        {
            if (i >= 400) //People
            {
                groupType = AGT_HUMAN;
                findID = (((i - 400) * 175) + 35000) * sizeof(ANIM_IDX_BLOCK);
            }
            else //Low
            {
                groupType = AGT_ANIMAL;
                findID = (((i - 200) * 65) + 22000) * sizeof(ANIM_IDX_BLOCK);
            }
        }
        else //Hight
        {
            groupType = AGT_MONSTER;
            findID = (i * 110) * sizeof(ANIM_IDX_BLOCK);
        }

        if (findID >= m_SizeIdx[0])
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

        size_t address = m_AddressIdx[0] + findID;

        for (int j = 0; j < count; j++)
        {
            CTextureAnimationGroup &group = index.m_Groups[j];
            int offset = (int)j * MAX_MOBILE_DIRECTIONS;

            for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
            {
                CTextureAnimationDirection &direction = group.m_Direction[d];

                ANIM_IDX_BLOCK *aidx =
                    (ANIM_IDX_BLOCK *)(address + ((offset + d) * sizeof(ANIM_IDX_BLOCK)));

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
                }
            }
        }
    }

    if (verdata != nullptr)
    {
        int dataCount = *verdata;

        for (int j = 0; j < dataCount; j++)
        {
            VERDATA_HEADER *vh =
                (VERDATA_HEADER *)((size_t)verdata + 4 + (j * sizeof(VERDATA_HEADER)));

            if (vh->FileID == 0x06) //Anim
            {
                ANIMATION_GROUPS_TYPE groupType = AGT_HUMAN;
                uint32_t graphic = vh->BlockID;
                uint16_t id = 0xFFFF;
                uint32_t group = 0;
                uint32_t dir = 0;
                uint32_t offset = 0;
                int count = 0;

                DEBUG(Data, "vh->ID = 0x%02X vh->BlockID = 0x%08X", vh->FileID, graphic);
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

                CIndexAnimation &index = m_DataIndex[id];

                CTextureAnimationDirection &direction = index.m_Groups[group].m_Direction[dir];

                direction.IsVerdata = true;
                direction.BaseAddress = (size_t)g_FileManager.m_VerdataMul.Start + vh->Position;
                direction.BaseSize = vh->Size;
                direction.Address = direction.BaseAddress;
                direction.Size = direction.BaseSize;

                index.Graphic = id;
                index.Type = groupType;
            }
        }
    }
}

void CAnimationManager::InitIndexReplaces(uint32_t *verdata)
{
    DEBUG_TRACE_FUNCTION;
    if (g_Config.ClientVersion >= CV_500A)
    {
        static const string typeNames[5] = {
            "monster", "sea_monster", "animal", "human", "equipment"
        };

        Wisp::CTextFileParser mobtypesParser(g_App.UOFilesPath("mobtypes.txt"), " \t", "#;//", "");

        while (!mobtypesParser.IsEOF())
        {
            vector<string> strings = mobtypesParser.ReadTokens();

            if (strings.size() >= 3)
            {
                uint16_t index = atoi(strings[0].c_str());

                if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
                {
                    continue;
                }

                string testType = ToLowerA(strings[1]);

                for (int i = 0; i < 5; i++)
                {
                    if (testType == typeNames[i])
                    {
                        m_DataIndex[index].Type = (ANIMATION_GROUPS_TYPE)i;

                        char *endP = nullptr;
                        m_DataIndex[index].Flags =
                            0x80000000 | strtoul(("0x" + strings[2]).c_str(), &endP, 16);

                        break;
                    }
                }
            }
        }
    }

    Load(verdata);

    //std::pair<uint16_t, char> m_GroupReplaces[2];

    Wisp::CTextFileParser animParser[2] = {
        Wisp::CTextFileParser(g_App.UOFilesPath("Anim1.def"), " \t", "#;//", "{}"),
        Wisp::CTextFileParser(g_App.UOFilesPath("Anim2.def"), " \t", "#;//", "{}"),
    };

    for (int i = 0; i < 2; i++)
    {
        while (!animParser[i].IsEOF())
        {
            vector<string> strings = animParser[i].ReadTokens();

            if (strings.size() < 2)
            {
                continue;
            }

            uint16_t group = (uint16_t)atoi(strings[0].c_str());
            int replaceGroup = atoi(strings[1].c_str());

            m_GroupReplaces[i].push_back(
                std::pair<uint16_t, uint8_t>(group, (uint8_t)replaceGroup));
        }
    }

    if (g_Config.ClientVersion < CV_305D)
    { //CV_204C
        return;
    }

    Wisp::CTextFileParser newBodyParser({}, " \t,{}", "#;//", "");
    Wisp::CTextFileParser bodyParser(g_App.UOFilesPath("Body.def"), " \t", "#;//", "{}");
    Wisp::CTextFileParser bodyconvParser(g_App.UOFilesPath("Bodyconv.def"), " \t", "#;//", "");
    Wisp::CTextFileParser corpseParser(g_App.UOFilesPath("Corpse.def"), " \t", "#;//", "{}");

    Wisp::CTextFileParser equipConvParser(g_App.UOFilesPath("Equipconv.def"), " \t", "#;//", "");

    while (!equipConvParser.IsEOF())
    {
        vector<string> strings = equipConvParser.ReadTokens();

        if (strings.size() >= 5)
        {
            uint16_t body = (uint16_t)atoi(strings[0].c_str());

            if (body >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            uint16_t graphic = (uint16_t)atoi(strings[1].c_str());

            if (graphic >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            uint16_t newGraphic = (uint16_t)atoi(strings[2].c_str());

            if (newGraphic >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                newGraphic = graphic;
            }

            uint16_t gump = (uint16_t)atoi(strings[3].c_str());

            if (gump >= MAX_GUMP_DATA_INDEX_COUNT)
            {
                continue;
            }
            if (gump == 0)
            {
                gump = graphic; // +50000;
            }
            else if (gump == 0xFFFF)
            {
                gump = newGraphic; // +50000;
            }

            uint16_t color = (uint16_t)atoi(strings[4].c_str());

            EQUIP_CONV_BODY_MAP::iterator bodyMapIter = m_EquipConv.find(body);

            if (bodyMapIter == m_EquipConv.end())
            {
                m_EquipConv.insert(EQUIP_CONV_BODY_MAP::value_type(body, EQUIP_CONV_DATA_MAP()));

                bodyMapIter = m_EquipConv.find(body);

                if (bodyMapIter == m_EquipConv.end())
                {
                    continue; //?!?!??
                }
            }

            bodyMapIter->second.insert(
                EQUIP_CONV_DATA_MAP::value_type(graphic, CEquipConvData(newGraphic, gump, color)));
        }
    }

    while (!bodyconvParser.IsEOF())
    {
        vector<string> strings = bodyconvParser.ReadTokens();

        if (strings.size() >= 2)
        {
            uint16_t index = atoi(strings[0].c_str());

            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            int anim[4] = { atoi(strings[1].c_str()), -1, -1, -1 };

            if (strings.size() >= 3)
            {
                anim[1] = atoi(strings[2].c_str());

                if (strings.size() >= 4)
                {
                    anim[2] = atoi(strings[3].c_str());

                    if (strings.size() >= 5)
                    {
                        anim[3] = atoi(strings[4].c_str());
                    }
                }
            }

            int startAnimID = -1;
            int animFile = 1;
            uint16_t realAnimID = 0;
            char mountedHeightOffset = 0;
            ANIMATION_GROUPS_TYPE groupType = AGT_UNKNOWN;

            if (anim[0] != -1 && m_AddressIdx[2] != 0 && g_FileManager.IsMulFileOpen(2))
            {
                animFile = 2;

                realAnimID = anim[0];

                if (realAnimID == 68)
                {
                    realAnimID = 122;
                }

                if (realAnimID >= 200) //Low
                {
                    startAnimID = ((realAnimID - 200) * 65) + 22000;
                    groupType = AGT_ANIMAL;
                    mountedHeightOffset = +8;
                }
                else //Hight
                {
                    startAnimID = realAnimID * 110;
                    groupType = AGT_MONSTER;
                }
            }
            else if (anim[1] != -1 && m_AddressIdx[3] != 0 && g_FileManager.IsMulFileOpen(3))
            {
                animFile = 3;
                realAnimID = anim[1];

                if (realAnimID < 630)
                {
                    startAnimID = realAnimID * 110;
                    groupType = AGT_MONSTER;
                }
                else
                {
                    startAnimID = 69300 + ((realAnimID - 630) * 175);
                    groupType = AGT_HUMAN;
                }
            }
            else if (anim[2] != -1 && m_AddressIdx[4] != 0 && g_FileManager.IsMulFileOpen(4))
            {
                animFile = 4;
                realAnimID = anim[2];

                if (realAnimID >= 200)
                {
                    if (realAnimID >= 400) //People
                    {
                        startAnimID = ((realAnimID - 400) * 175) + 35000;
                        groupType = AGT_HUMAN;
                    }
                    else //Low
                    {
                        startAnimID = ((realAnimID - 200) * 65) + 22000;
                        groupType = AGT_ANIMAL;
                        mountedHeightOffset = +8;
                    }
                }
                else //Hight
                {
                    startAnimID = realAnimID * 110;
                    groupType = AGT_MONSTER;
                }
            }
            else if (anim[3] != -1 && m_AddressIdx[5] != 0 && g_FileManager.IsMulFileOpen(5))
            {
                animFile = 5;
                realAnimID = anim[3];

                if (realAnimID == 34)
                {
                    startAnimID = ((realAnimID - 200) * 65) + 22000;
                }
                else if (realAnimID >= 200)
                {
                    if (realAnimID >= 400) //People
                    {
                        startAnimID = ((realAnimID - 400) * 175) + 35000;
                        groupType = AGT_HUMAN;
                    }
                    else //Low
                    {
                        startAnimID = ((realAnimID - 200) * 65) + 22000;
                        groupType = AGT_ANIMAL;
                        mountedHeightOffset = +8;
                    }
                }
                else //Hight
                {
                    startAnimID = realAnimID * 110;
                    groupType = AGT_MONSTER;
                }
            }

            if (animFile != 1 && startAnimID != -1)
            {
                startAnimID = startAnimID * sizeof(ANIM_IDX_BLOCK);

                if ((uint32_t)startAnimID < m_SizeIdx[animFile])
                {
                    CIndexAnimation &dataIndex = m_DataIndex[index];
                    dataIndex.MountedHeightOffset = mountedHeightOffset;

                    if (g_Config.ClientVersion < CV_500A || groupType == AGT_UNKNOWN)
                    {
                        if (realAnimID >= 200)
                        {
                            if (realAnimID >= 400)
                            { //People
                                dataIndex.Type = AGT_HUMAN;
                            }
                            else
                            { //Low
                                dataIndex.Type = AGT_ANIMAL;
                                mountedHeightOffset = +8;
                            }
                        }
                        else
                        {
                            dataIndex.Type = AGT_MONSTER;
                        }
                    }
                    else if (groupType != AGT_UNKNOWN)
                    {
                        dataIndex.Type = groupType;
                    }

                    int count = 0;

                    switch (dataIndex.Type)
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

                    size_t address = m_AddressIdx[animFile] + startAnimID;
                    size_t maxAddress = m_AddressIdx[animFile] + m_SizeIdx[animFile];

                    for (int j = 0; j < count; j++)
                    {
                        CTextureAnimationGroup &group = dataIndex.m_Groups[j];
                        int offset = (int)j * MAX_MOBILE_DIRECTIONS;

                        for (int d = 0; d < MAX_MOBILE_DIRECTIONS; d++)
                        {
                            CTextureAnimationDirection &direction = group.m_Direction[d];

                            ANIM_IDX_BLOCK *aidx =
                                (ANIM_IDX_BLOCK
                                     *)(address + ((offset + d) * sizeof(ANIM_IDX_BLOCK)));

                            if ((size_t)aidx >= maxAddress)
                            {
                                break;
                            }

                            if ((aidx->Size != 0u) && aidx->Position != 0xFFFFFFFF &&
                                aidx->Size != 0xFFFFFFFF)
                            {
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

    while (!bodyParser.IsEOF())
    {
        vector<string> strings = bodyParser.ReadTokens();

        if (strings.size() >= 3)
        {
            uint16_t index = atoi(strings[0].c_str());

            vector<string> newBody = newBodyParser.GetTokens(strings[1].c_str());

            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT || newBody.empty())
            {
                continue;
            }

            uint16_t checkIndex = atoi(newBody[0].c_str());

            if (checkIndex >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            CIndexAnimation &dataIndex = m_DataIndex[index];
            CIndexAnimation &checkDataIndex = m_DataIndex[checkIndex];

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

                    if (direction.PatchedAddress == 0u)
                    {
                        direction.PatchedAddress = newDirection.PatchedAddress;
                        direction.PatchedSize = newDirection.PatchedSize;
                        direction.FileIndex = newDirection.FileIndex;
                    }

                    if (direction.BaseAddress == 0u)
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
            dataIndex.Color = atoi(strings[2].c_str());
        }
    }

    while (!corpseParser.IsEOF())
    {
        vector<string> strings = corpseParser.ReadTokens();

        if (strings.size() >= 3)
        {
            uint16_t index = atoi(strings[0].c_str());

            vector<string> newBody = newBodyParser.GetTokens(strings[1].c_str());

            if (index >= MAX_ANIMATIONS_DATA_INDEX_COUNT || newBody.empty())
            {
                continue;
            }

            uint16_t checkIndex = atoi(newBody[0].c_str());

            if (checkIndex >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
            {
                continue;
            }

            CIndexAnimation &dataIndex = m_DataIndex[index];
            CIndexAnimation &checkDataIndex = m_DataIndex[checkIndex];

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

                    if (direction.PatchedAddress == 0u)
                    {
                        direction.PatchedAddress = newDirection.PatchedAddress;
                        direction.PatchedSize = newDirection.PatchedSize;
                        direction.FileIndex = newDirection.FileIndex;
                    }

                    if (direction.BaseAddress == 0u)
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
            dataIndex.Color = atoi(strings[2].c_str());
        }
    }
}

ANIMATION_GROUPS CAnimationManager::GetGroupIndex(uint16_t id)
{
    DEBUG_TRACE_FUNCTION;

    if (id >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
    {
        Warning(Data, "GetGroupIndex: Invalid ID: 0x%04X", id);
        return AG_HIGHT;
    }

    switch (m_DataIndex[id].Type)
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
    DEBUG_TRACE_FUNCTION;
    DEBUG(Data, "gr: 0x%04X, %i", id, m_DataIndex[id].Type);
    switch (m_DataIndex[id].Type)
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
        Info(Data, "CAnimationManager::ClearUnusedTextures::removed %i", count);
    }
}

bool CAnimationManager::LoadDirectionGroup(CTextureAnimationDirection &direction)
{
    DEBUG_TRACE_FUNCTION;

    if (direction.IsUOP)
    {
        return UopTryReadAnimDims(direction);
    }
    if (direction.Address == 0)
    {
        return false;
    }

    auto ptr = (uint8_t *)direction.Address;
    if (!direction.IsVerdata)
    {
        ptr = g_FileManager.MulReadAnimationData(direction);
    }
    SetData(ptr, direction.Size);
    ReadFramesPixelData(direction);
    m_UsedAnimList.push_back(&direction);
    return true;
}

bool CAnimationManager::TestPixels(
    CGameObject *obj, int x, int y, bool mirror, uint8_t &frameIndex, uint16_t id)
{
    DEBUG_TRACE_FUNCTION;
    if (obj == nullptr)
    {
        return false;
    }

    if (id == 0u)
    {
        id = obj->GetMountAnimation();
    }

    if (id >= MAX_ANIMATIONS_DATA_INDEX_COUNT)
    {
        return false;
    }

    assert(Direction < MAX_MOBILE_DIRECTIONS && "Out-of-bound access");
    CTextureAnimationDirection &direction =
        m_DataIndex[id].m_Groups[AnimGroup].m_Direction[Direction];
    AnimID = id;
    if (direction.FrameCount == 0 && !LoadDirectionGroup(direction))
    {
        return false;
    }

    int fc = direction.FrameCount;

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

    if (frameIndex < direction.FrameCount)
    {
        CTextureAnimationFrame &frame = direction.m_Frames[frameIndex];

        y -= frame.Height + frame.CenterY;

        x = g_MouseManager.Position.X - x;

        if (mirror)
        {
            x += frame.Width - frame.CenterX;
        }
        else
        {
            x += frame.CenterX;
        }

        if (mirror)
        {
            x = frame.Width - x;
        }

        x = g_MouseManager.Position.X - x;

        return frame.Select(x, y);
    }

    return false;
}

void CAnimationManager::Draw(
    CGameObject *obj, int x, int y, bool mirror, uint8_t &frameIndex, int id)
{
    DEBUG_TRACE_FUNCTION;
    //if (obj == nullptr)
    //	return;

    bool isShadow = (id >= 0x10000);

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

    assert(Direction < MAX_MOBILE_DIRECTIONS && "Out-of-bound access");
    CTextureAnimationDirection &direction =
        m_DataIndex[id].m_Groups[AnimGroup].m_Direction[Direction];
    AnimID = id;
    if (direction.FrameCount == 0 && !LoadDirectionGroup(direction))
    {
        return;
    }

    int fc = direction.FrameCount;

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

    if (frameIndex < direction.FrameCount)
    {
        CTextureAnimationFrame &frame = direction.m_Frames[frameIndex];

        if (frame.Texture == 0)
        {
            return;
        }

        if (mirror)
        {
            x -= frame.Width - frame.CenterX;
        }
        else
        {
            x -= frame.CenterX;
        }

        y -= frame.Height + frame.CenterY;

        if (isShadow)
        {
            glUniform1iARB(g_ShaderDrawMode, SDM_SHADOW);

            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);

            g_GL_DrawShadow(frame, x, y, mirror);

            if (m_UseBlending)
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
            {
                glDisable(GL_BLEND);
            }
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
                            color = m_DataIndex[id].Color;
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
                }
                else if (color != 0u)
                {
                    if (partialHue)
                    {
                        glUniform1iARB(g_ShaderDrawMode, SDM_PARTIAL_HUE);
                    }
                    else
                    {
                        glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);
                    }

                    g_ColorManager.SendColorsToShader(color);
                }
                else
                {
                    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
                }
            }
            else
            {
                glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
            }

            if (m_Transform)
            {
                if (obj->IsHuman())
                {
                    short frameHeight = frame.Height;
                    m_CharacterFrameStartY = y;
                    m_CharacterFrameHeight = frame.Height;
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
                    float itemsEndY = (float)(y + frame.Height);

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
                        h3mod = upperBodyDiff / frame.Height;
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

                        h6mod = h3mod + midBodyDiff / frame.Height;
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
                        h9mod = h6mod + lowerBodyDiff / frame.Height;
                        if (h9mod < 0)
                        {
                            h9mod = 0;
                        }
                    }
                }

                g_GL_DrawSitting(frame, x, y, mirror, h3mod, h6mod, h9mod);
            }
            else
            {
                g_GL_DrawMirrored(frame, x, y, mirror);
            }

            if (spectralColor)
            {
                if (m_UseBlending)
                {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
                else
                {
                    glDisable(GL_BLEND);
                }
            }
        }
    }
}

void CAnimationManager::FixSittingDirection(uint8_t &layerDirection, bool &mirror, int &x, int &y)
{
    DEBUG_TRACE_FUNCTION;
    const SITTING_INFO_DATA &data = SITTING_INFO[m_Sitting - 1];

    switch (Direction)
    {
        case 7:
        case 0:
        {
            if (data.Direction1 == -1)
            {
                if (Direction == 7)
                {
                    Direction = data.Direction4;
                }
                else
                {
                    Direction = data.Direction2;
                }
            }
            else
            {
                Direction = data.Direction1;
            }

            break;
        }
        case 1:
        case 2:
        {
            if (data.Direction2 == -1)
            {
                if (Direction == 1)
                {
                    Direction = data.Direction1;
                }
                else
                {
                    Direction = data.Direction3;
                }
            }
            else
            {
                Direction = data.Direction2;
            }

            break;
        }
        case 3:
        case 4:
        {
            if (data.Direction3 == -1)
            {
                if (Direction == 3)
                {
                    Direction = data.Direction2;
                }
                else
                {
                    Direction = data.Direction4;
                }
            }
            else
            {
                Direction = data.Direction3;
            }

            break;
        }
        case 5:
        case 6:
        {
            if (data.Direction4 == -1)
            {
                if (Direction == 5)
                {
                    Direction = data.Direction3;
                }
                else
                {
                    Direction = data.Direction1;
                }
            }
            else
            {
                Direction = data.Direction4;
            }

            break;
        }
        default:
            break;
    }

    layerDirection = Direction;
    GetSittingAnimDirection(Direction, mirror, x, y);

    int offsX = SITTING_OFFSET_X;

    if (mirror)
    {
        if (Direction == 3)
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
        if (Direction == 3)
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
}

void CAnimationManager::DrawCharacter(CGameCharacter *obj, int x, int y)
{
    m_EquipConvItem = nullptr;
    DEBUG_TRACE_FUNCTION;
    m_Transform = false;

    int drawX = x + obj->OffsetX;
    int drawY = y + obj->OffsetY - obj->OffsetZ - 3;

    uint16_t targetColor = 0;
    bool needHPLine = false;
    uint32_t serial = obj->Serial;
    bool drawShadow = !obj->Dead();
    m_UseBlending = false;

    if (g_DrawAura)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        uint32_t auraColor = g_ColorManager.GetPolygoneColor(
            16, g_ConfigManager.GetColorByNotoriety(obj->Notoriety));
        glColor4ub(ToColorR(auraColor), ToColorG(auraColor), ToColorB(auraColor), 0xFF);

        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
        g_AuraTexture.Draw(drawX - g_AuraTexture.Width / 2, drawY - g_AuraTexture.Height / 2);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_BLEND);
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

                    glColor4ub(0xFF, 0xFF, 0xFF, g_ConfigManager.HiddenAlpha);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
            if (obj->Poisoned() || obj->SA_Poisoned)
            {
                Color = 0x0044;
            }
            else if (obj->Frozen())
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

        if (obj->Dead())
        {
            Color = 0x0386;
        }
    }

    uint8_t *drawTextureColor = obj->m_DrawTextureColor;

    if (!m_UseBlending && drawTextureColor[3] != 0xFF)
    {
        m_UseBlending = true;
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(
            drawTextureColor[0], drawTextureColor[1], drawTextureColor[2], drawTextureColor[3]);
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

    Direction = 0;
    obj->UpdateAnimationInfo(Direction);

    bool mirror = false;
    uint8_t layerDir = Direction;

    GetAnimDirection(Direction, mirror);

    uint8_t animIndex = obj->AnimIndex;
    uint8_t animGroup = obj->GetAnimationGroup();
    AnimGroup = animGroup;

    CGameItem *goi = obj->FindLayer(OL_MOUNT);

    int lightOffset = 20;

    if (obj->IsHuman() && goi != nullptr) //Draw mount
    {
        m_Sitting = 0;
        lightOffset += 20;

        uint16_t mountID = goi->GetMountAnimation();
        int mountedHeightOffset = 0;

        if (mountID < MAX_ANIMATIONS_DATA_INDEX_COUNT)
        {
            mountedHeightOffset = m_DataIndex[mountID].MountedHeightOffset;
        }

        if (drawShadow)
        {
            Draw(obj, drawX, drawY + 10 + mountedHeightOffset, mirror, animIndex, 0x10000);
            AnimGroup = obj->GetAnimationGroup(mountID);

            Draw(goi, drawX, drawY, mirror, animIndex, mountID + 0x10000);
        }
        else
        {
            AnimGroup = obj->GetAnimationGroup(mountID);
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

            obj->UpdateAnimationInfo(Direction);

            FixSittingDirection(layerDir, mirror, drawX, drawY);

            if (Direction == 3)
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

    AnimGroup = animGroup;

    Draw(obj, drawX, drawY, mirror, animIndex); //Draw character

    if (obj->IsHuman()) //Draw layered objects
    {
        DrawEquippedLayers(false, obj, drawX, drawY, mirror, layerDir, animIndex, lightOffset);

        const SITTING_INFO_DATA &sittingData = SITTING_INFO[m_Sitting - 1];

        if ((m_Sitting != 0) && Direction == 3 && sittingData.DrawBack &&
            obj->FindLayer(OL_CLOAK) == nullptr)
        {
            for (CRenderWorldObject *ro = obj->m_PrevXY; ro != nullptr; ro = ro->m_PrevXY)
            {
                if ((ro->Graphic & 0x3FFF) == sittingData.Graphic)
                {
                    //оффсеты для ножниц
                    int xOffset = mirror ? -20 : 0;
                    int yOffset = -70;

                    g_GL.PushScissor(
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
                    g_GL.PopScissor();

                    break;
                }
            }
        }
    }

    if (m_UseBlending)
    {
        m_UseBlending = false;
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_BLEND);
    }

    if (!g_ConfigManager.DisableNewTargetSystem && g_NewTargetSystem.Serial == obj->Serial)
    {
        uint16_t id = obj->GetMountAnimation();

        if (id < MAX_ANIMATIONS_DATA_INDEX_COUNT)
        {
            assert(Direction < MAX_MOBILE_DIRECTIONS && "Out-of-bound access");
            CTextureAnimationDirection &direction =
                m_DataIndex[id].m_Groups[AnimGroup].m_Direction[Direction];

            if (direction.Address != 0 && direction.m_Frames != nullptr)
            {
                CTextureAnimationFrame &frame = direction.m_Frames[0];

                int frameWidth = frame.Width;
                int frameHeight = frame.Height;

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
                if (obj->Poisoned() || obj->SA_Poisoned)
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
    if (obj.Poisoned() || obj.SA_Poisoned)
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
    DEBUG_TRACE_FUNCTION;
    y -= 3;
    m_Sitting = obj->IsSitting();
    Direction = 0;
    obj->UpdateAnimationInfo(Direction);

    bool mirror = false;
    uint8_t layerDir = Direction;

    GetAnimDirection(Direction, mirror);

    uint8_t animIndex = obj->AnimIndex;
    uint8_t animGroup = obj->GetAnimationGroup();

    CGameItem *goi = obj->FindLayer(OL_MOUNT);

    int drawX = x - obj->OffsetX;
    int drawY = y - obj->OffsetY - obj->OffsetZ;

    if (obj->IsHuman() && goi != nullptr) //Check mount
    {
        uint16_t mountID = goi->GetMountAnimation();

        AnimGroup = obj->GetAnimationGroup(mountID);

        if (TestPixels(goi, drawX, drawY, mirror, animIndex, mountID))
        {
            return true;
        }

        if (mountID < MAX_ANIMATIONS_DATA_INDEX_COUNT)
        {
            drawY += m_DataIndex[mountID].MountedHeightOffset;
        }
    }
    else if (m_Sitting != 0)
    {
        animGroup = PAG_STAND;
        animIndex = 0;

        obj->UpdateAnimationInfo(Direction);

        FixSittingDirection(layerDir, mirror, drawX, drawY);

        if (Direction == 3)
        {
            animGroup = 25;
        }
    }

    AnimGroup = animGroup;

    return TestPixels(obj, drawX, drawY, mirror, animIndex) ||
           DrawEquippedLayers(true, obj, drawX, drawY, mirror, layerDir, animIndex, 0);
}

void CAnimationManager::DrawCorpse(CGameItem *obj, int x, int y)
{
    DEBUG_TRACE_FUNCTION;

    if (g_CorpseManager.InList(obj->Serial, 0))
    {
        return;
    }

    m_Sitting = 0;
    Direction = (obj->Layer & 0x7F) & 7;
    bool mirror = false;

    GetAnimDirection(Direction, mirror);

    if (obj->Hidden())
    {
        Color = 0x038E;
    }
    else
    {
        Color = 0;
    }

    uint8_t animIndex = obj->AnimIndex;
    AnimGroup = GetDieGroupIndex(obj->GetMountAnimation(), obj->UsedLayer != 0u);

    Draw(obj, x, y, mirror, animIndex); //Draw animation

    DrawEquippedLayers(false, obj, x, y, mirror, Direction, animIndex, 0);
}

bool CAnimationManager::CorpsePixelsInXY(CGameItem *obj, int x, int y)
{
    DEBUG_TRACE_FUNCTION;

    if (g_CorpseManager.InList(obj->Serial, 0))
    {
        return false;
    }

    m_Sitting = 0;
    Direction = (obj->Layer & 0x7F) & 7;
    bool mirror = false;

    GetAnimDirection(Direction, mirror);

    uint8_t animIndex = obj->AnimIndex;
    AnimGroup = GetDieGroupIndex(obj->GetMountAnimation(), obj->UsedLayer != 0u);

    return TestPixels(obj, x, y, mirror, animIndex) ||
           DrawEquippedLayers(true, obj, x, y, mirror, Direction, animIndex, 0);
}

bool CAnimationManager::AnimationExists(uint16_t graphic, uint8_t group)
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;
    if (graphic < MAX_ANIMATIONS_DATA_INDEX_COUNT && group < ANIMATION_GROUPS_COUNT)
    {
        auto groupDir = m_DataIndex[graphic].m_Groups[group].m_Direction[0];
        result = groupDir.Address != 0 || groupDir.IsUOP;
    }
    return result;
}

ANIMATION_DIMENSIONS CAnimationManager::GetAnimationDimensions(
    uint8_t frameIndex, uint16_t id, uint8_t dir, uint8_t animGroup, bool isCorpse)
{
    ANIMATION_DIMENSIONS result = {};
    if (id < MAX_ANIMATIONS_DATA_INDEX_COUNT)
    {
        if (dir < MAX_MOBILE_DIRECTIONS)
        {
            auto &direction = m_DataIndex[id].m_Groups[animGroup].m_Direction[dir];
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
                    result.Width = frame.Width;
                    result.Height = frame.Height;
                    result.CenterX = frame.CenterX;
                    result.CenterY = frame.CenterY;
                    return result;
                }
            }
        }

        CTextureAnimationDirection &direction = m_DataIndex[id].m_Groups[animGroup].m_Direction[0];
        uint8_t *ptr = (uint8_t *)direction.Address;
        if (ptr != nullptr)
        {
            if (!direction.IsVerdata)
            {
                ptr = g_FileManager.MulReadAnimationData(direction);
            }
            SetData(ptr, direction.Size);
            ReadFrameDimensionData(result, frameIndex, isCorpse);
        }
        else if (direction.IsUOP) // try reading uop anim frame
        {
            std::vector<uint8_t> scratchBuffer;
            if (!UopDecompressBlock(scratchBuffer, direction.FileIndex))
            {
                return result;
            }

            auto header = UopReadAnimationHeader();
            // read only first frame to get image dimensions
            UopAnimationFrame frame;
            frame.DataStart = Ptr;
            frame.GroupId = ReadInt16LE();
            frame.FrameId = ReadInt16LE();
            frame.Unk1 = ReadUInt32LE();
            frame.Unk2 = ReadUInt32LE();
            frame.PixelDataOffset = ReadUInt32LE();

            int16_t centerX, centerY, width, height;
            uint16_t *palette = nullptr;
            UopReadFrame(centerX, centerY, width, height, palette, frame);
            result.CenterX = centerX;
            result.CenterY = centerY;
            result.Width = width;
            result.Height = height;
        }
    }
    return result;
}

ANIMATION_DIMENSIONS CAnimationManager::GetAnimationDimensions(
    CGameObject *obj, uint8_t frameIndex, uint8_t defaultDirection, uint8_t defaultGroup)
{
    DEBUG_TRACE_FUNCTION;

    uint8_t dir = defaultDirection & 0x7F;
    uint8_t animGroup = defaultGroup;
    uint16_t id = obj->GetMountAnimation();
    bool mirror = false;

    if (obj->NPC)
    {
        CGameCharacter *gc = obj->GameCharacterPtr();
        gc->UpdateAnimationInfo(dir);
        animGroup = gc->GetAnimationGroup();
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

    ANIMATION_DIMENSIONS dims =
        GetAnimationDimensions(frameIndex, id, dir, animGroup, obj->IsCorpse());

    if ((dims.Width == 0) && (dims.Height == 0) && (dims.CenterX == 0) && (dims.CenterY == 0))
    {
        dims.Width = 20;

        if (obj->NPC && obj->FindLayer(OL_MOUNT) != nullptr)
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

// FIXME
bool CAnimationManager::UopDecompressBlock(std::vector<uint8_t> &scratchBuffer, int fileId)
{
    auto &block = m_DataIndex[AnimID].m_Groups[AnimGroup].m_UOPAnimData;
    if (block.Hash == 0)
    {
        return false;
    }

    scratchBuffer.reserve(block.DecompressedSize);
    if (CFileManager::UopDecompressBlock(block, scratchBuffer.data(), fileId))
    {
        SetData(scratchBuffer.data(), block.DecompressedSize);
        return true;
    }

    return false;
}

bool CAnimationManager::UopTryReadAnimDims(CTextureAnimationDirection &direction)
{
    std::vector<uint8_t> scratchBuffer;
    if (!UopDecompressBlock(scratchBuffer, direction.FileIndex))
    {
        return false;
    }

    auto framesData = UopReadFrameData();
    direction.FrameCount = checked_cast<uint8_t>(framesData.size() / 5);
    int dirFrameStartIdx = direction.FrameCount * Direction;
    if (direction.m_Frames == nullptr)
    {
        direction.m_Frames = new CTextureAnimationFrame[direction.FrameCount];
    }

    for (int i = 0; i < direction.FrameCount; i++)
    {
        CTextureAnimationFrame &frame = direction.m_Frames[i];
        if (frame.Texture != 0)
        {
            continue;
        }

        UopAnimationFrame frameData = framesData[i + dirFrameStartIdx];
        if (frameData.DataStart == nullptr)
        {
            continue;
        }

        int16_t centerX, centerY, width, height;
        uint16_t *palette;
        UopReadFrame(centerX, centerY, width, height, palette, frameData);
        frame.CenterX = centerX;
        frame.CenterY = centerY;
        if (width == 0 || height == 0)
        {
            continue;
        }

        const int textureSize = width * height;
        vector<uint16_t> pixels(textureSize, 0);
        if (pixels.size() != textureSize)
        {
            Info(
                Data,
                "Allocation pixels memory for TryReadUOPAnimDims failed (want size: %i)",
                textureSize);
            continue;
        }

        uint32_t header = ReadUInt32LE();
        while (header != 0x7FFF7FFF && !IsEOF())
        {
            const uint16_t runLength = (header & 0x0FFF);
            int x = (header >> 22) & 0x03FF;
            if ((x & 0x0200) != 0)
            {
                x |= 0xFFFFFE00;
            }

            int y = (header >> 12) & 0x03FF;
            if ((y & 0x0200) != 0)
            {
                y |= 0xFFFFFE00;
            }

            x += centerX;
            y += centerY + height;
            int block = (y * width) + x;
            for (int k = 0; k < runLength; k++)
            {
                const uint8_t paletteIndex = ReadUInt8();
                uint16_t val = palette[paletteIndex];
                if (val != 0u)
                {
                    val |= 0x8000;
                }
                pixels[block++] = val;
            }
            header = ReadUInt32LE();
        }
        g_GL_BindTexture16(frame, width, height, &pixels[0]);
    }
    m_UsedAnimList.push_back(&direction);

    return true;
}

void CAnimationManager::CalculateFrameInformation(
    FRAME_OUTPUT_INFO &info, CGameObject *obj, bool mirror, uint8_t animIndex)
{
    DEBUG_TRACE_FUNCTION;
    ANIMATION_DIMENSIONS dim = GetAnimationDimensions(obj, animIndex, Direction, AnimGroup);

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

DRAW_FRAME_INFORMATION
CAnimationManager::CollectFrameInformation(CGameObject *gameObject, bool checkLayers)
{
    DEBUG_TRACE_FUNCTION;
    m_Sitting = 0;
    Direction = 0;

    DRAW_FRAME_INFORMATION dfInfo = {};

    vector<CGameItem *> &list = gameObject->m_DrawLayeredObjects;

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
        obj->UpdateAnimationInfo(Direction);

        bool mirror = false;
        uint8_t layerDir = Direction;

        GetAnimDirection(Direction, mirror);

        uint8_t animIndex = obj->AnimIndex;
        uint8_t animGroup = obj->GetAnimationGroup();

        FRAME_OUTPUT_INFO info = {};

        CGameItem *goi = obj->FindLayer(OL_MOUNT);

        if (goi != nullptr) //Check mount
        {
            uint16_t mountID = goi->GetMountAnimation();

            AnimGroup = obj->GetAnimationGroup(mountID);

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

        AnimGroup = animGroup;

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

        Direction = (obj->Layer & 0x7F) & 7;
        bool mirror = false;

        GetAnimDirection(Direction, mirror);

        uint8_t animIndex = obj->AnimIndex;
        AnimGroup = GetDieGroupIndex(obj->GetMountAnimation(), obj->UsedLayer != 0u);

        FRAME_OUTPUT_INFO info = {};

        CalculateFrameInformation(info, obj, mirror, animIndex);

        if (checkLayers)
        {
            for (int l = 0; l < USED_LAYER_COUNT; l++)
            {
                assert(Direction < MAX_LAYER_DIRECTIONS && "Out-of-bounds access");
                CGameItem *goi = obj->FindLayer(m_UsedLayers[Direction][l]);

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
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    vector<CGameItem *> &list = obj->m_DrawLayeredObjects;

    uint16_t bodyGraphic = obj->Graphic;

    if (obj->IsCorpse())
    {
        bodyGraphic = obj->Count;
    }

    EQUIP_CONV_BODY_MAP::iterator bodyMapIter = m_EquipConv.find(bodyGraphic);

    if (selection)
    {
        for (vector<CGameItem *>::iterator i = list.begin(); i != list.end() && !result; ++i)
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
        for (vector<CGameItem *>::iterator i = list.begin(); i != list.end(); ++i)
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
    DEBUG_TRACE_FUNCTION;
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

UopAnimationHeader CAnimationManager::UopReadAnimationHeader()
{
    // FIXME
    UopAnimationHeader hdr;
    hdr.Format = ReadUInt32LE();
    hdr.Version = ReadUInt32LE();
    hdr.DecompressedSize = ReadUInt32LE();
    hdr.AnimationId = ReadUInt32LE();
    hdr.Unk1 = ReadUInt32LE();
    hdr.Unk2 = ReadUInt32LE();
    hdr.Unk3 = ReadInt16LE();
    hdr.Unk4 = ReadInt16LE();
    hdr.HeaderSize = ReadUInt32LE();
    hdr.FrameCount = ReadUInt32LE();
    hdr.Offset = ReadUInt32LE();
    Ptr = Start + hdr.Offset;
    return hdr;
}

vector<UopAnimationFrame> CAnimationManager::UopReadFrameData()
{
    vector<UopAnimationFrame> data;

    auto hdr = UopReadAnimationHeader();
    for (int i = 0; i < hdr.FrameCount; i++)
    {
        // FIXME
        UopAnimationFrame frame;
        frame.DataStart = Ptr;
        frame.GroupId = ReadInt16LE();
        frame.FrameId = ReadInt16LE();
        frame.Unk1 = ReadUInt32LE();
        frame.Unk2 = ReadUInt32LE();
        frame.PixelDataOffset = ReadUInt32LE();

        size_t vsize = data.size();
        if (vsize + 1 != frame.FrameId)
        {
            while (vsize + 1 != frame.FrameId)
            {
                data.push_back({});
                vsize++;
            }
        }
        data.push_back(frame);
    }
    size_t vectorSize = data.size();
    if (vectorSize < 50)
    {
        while (vectorSize != 50)
        {
            data.push_back({});
            vectorSize++;
        }
    }
    return data;
}

// FIXME: put directly in UopAnimationFrame?
void CAnimationManager::UopReadFrame(
    int16_t &centerX,
    int16_t &centerY,
    int16_t &width,
    int16_t &height,
    uint16_t *&palette,
    UopAnimationFrame &frame)
{
    Ptr = frame.DataStart + frame.PixelDataOffset;
    // FIXME
    palette = reinterpret_cast<uint16_t *>(Ptr);
    Move(512); //Palette
    centerX = ReadInt16LE();
    centerY = ReadInt16LE();
    width = ReadInt16LE();
    height = ReadInt16LE();
}

uint8_t CAnimationManager::GetReplacedObjectAnimation(CGameCharacter *obj, uint16_t index)
{
    auto getReplaceGroup = [](const vector<std::pair<uint16_t, uint8_t>> &list,
                              uint16_t index,
                              uint16_t walkIndex) -> uint16_t {
        for (const std::pair<uint16_t, uint8_t> &item : list)
        {
            if (item.first == index)
            {
                if (item.second == 0xFF)
                {
                    return walkIndex;
                }

                return (uint16_t)item.second;
            }
        }

        return index;
    };

    ANIMATION_GROUPS group = GetGroupIndex(obj->Graphic);

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

uint8_t
CAnimationManager::GetObjectNewAnimationType_0(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    if (action <= 10)
    {
        CIndexAnimation &ia = m_DataIndex[obj->Graphic];

        ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

        if ((ia.Flags & 0x80000000) != 0u)
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
                    if ((ia.Flags & 1) != 0u)
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
            if (obj->FindLayer(OL_MOUNT) != nullptr)
            {
                if (action != 0u)
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
                default:
                    return 31;
                case 1:
                    return 18;
                case 2:
                    return 19;
                case 6:
                    return 12;
                case 7:
                    return 13;
                case 8:
                    return 14;
                case 3:
                    return 11;
                case 4:
                    return 9;
                case 5:
                    return 10;
            }

            return 0;
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
    CIndexAnimation &ia = m_DataIndex[obj->Graphic];

    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

    if ((ia.Flags & 0x80000000) != 0u)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type <= AGT_ANIMAL || obj->FindLayer(OL_MOUNT) != nullptr)
        {
            return 0xFF;
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
    CIndexAnimation &ia = m_DataIndex[obj->Graphic];

    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

    if ((ia.Flags & 0x80000000) != 0u)
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
    CIndexAnimation &ia = m_DataIndex[obj->Graphic];

    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

    if ((ia.Flags & 0x80000000) != 0u)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type > AGT_ANIMAL)
        {
            if (obj->FindLayer(OL_MOUNT) != nullptr)
            {
                return 0xFF;
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
    CIndexAnimation &ia = m_DataIndex[obj->Graphic];

    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

    if ((ia.Flags & 0x80000000) != 0u)
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
        if (obj->FindLayer(OL_MOUNT) != nullptr)
        {
            return 0xFF;
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
    CIndexAnimation &ia = m_DataIndex[obj->Graphic];

    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

    if ((ia.Flags & 0x80000000) != 0u)
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

            if (obj->FindLayer(OL_MOUNT) != nullptr)
            {
                return 0xFF;
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
    if (obj->FindLayer(OL_MOUNT) != nullptr)
    {
        return 0xFF;
    }

    if (action != 0u)
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
    CIndexAnimation &ia = m_DataIndex[obj->Graphic];

    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

    if ((ia.Flags & 0x80000000) != 0u)
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

            if (obj->FindLayer(OL_MOUNT) != nullptr)
            {
                return 0xFF;
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
    CIndexAnimation &ia = m_DataIndex[obj->Graphic];

    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

    if ((ia.Flags & 0x80000000) != 0u)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        return 0xFF;
    }

    return 20;
}

uint8_t
CAnimationManager::GetObjectNewAnimationType_11(CGameCharacter *obj, uint16_t action, uint8_t mode)
{
    CIndexAnimation &ia = m_DataIndex[obj->Graphic];

    ANIMATION_GROUPS_TYPE type = AGT_MONSTER;

    if ((ia.Flags & 0x80000000) != 0u)
    {
        type = ia.Type;
    }

    if (type != AGT_MONSTER)
    {
        if (type >= AGT_ANIMAL)
        {
            if (obj->FindLayer(OL_MOUNT) != nullptr)
            {
                return 0xFF;
            }

            switch (action)
            {
                case 1:
                case 2:
                    return 17;
                default:
                    break;
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

void CAnimationManager::ReadFrameDimensionData(
    ANIMATION_DIMENSIONS &result, uint8_t frameIndex, bool isCorpse)
{
    Move(sizeof(uint16_t[256])); //Palette
    uint8_t *dataStart = Ptr;

    int frameCount = ReadUInt32LE();

    if (frameCount > 0 && frameIndex >= frameCount)
    {
        if (isCorpse)
        {
            frameIndex = frameCount - 1;
        }
        else
        {
            frameIndex = 0;
        }
    }

    if (frameIndex < frameCount)
    {
        uint32_t *frameOffset = (uint32_t *)Ptr;
        //Move(frameOffset[frameIndex]);
        Ptr = dataStart + frameOffset[frameIndex];

        result.CenterX = ReadInt16LE();
        result.CenterY = ReadInt16LE();
        result.Width = ReadInt16LE();
        result.Height = ReadInt16LE();
    }
}

void CAnimationManager::ReadFramesPixelData(CTextureAnimationDirection &direction)
{
    uint16_t *palette = (uint16_t *)Start;
    Move(sizeof(uint16_t[256])); //Palette
    uint8_t *dataStart = Ptr;

    uint32_t frameCount = ReadUInt32LE();
    direction.FrameCount = frameCount;

    uint32_t *frameOffset = (uint32_t *)Ptr;

    //uint16_t color = m_DataIndex[graphic].Color;

    direction.m_Frames = new CTextureAnimationFrame[frameCount];

    for (uint32_t i = 0; i < (int)frameCount; i++)
    {
        CTextureAnimationFrame &frame = direction.m_Frames[i];

        if (frame.Texture != 0)
        {
            continue;
        }

        Ptr = dataStart + frameOffset[i];

        uint32_t imageCenterX = ReadInt16LE();
        frame.CenterX = imageCenterX;

        uint32_t imageCenterY = ReadInt16LE();
        frame.CenterY = imageCenterY;

        uint32_t imageWidth = ReadInt16LE();

        uint32_t imageHeight = ReadInt16LE();

        if ((imageWidth == 0u) || (imageHeight == 0u))
        {
            Warning(
                Data,
                "CAnimationManager::LoadDirectionGroup no image size:%i, %i",
                imageWidth,
                imageHeight);
            continue;
        }

        int wantSize = imageWidth * imageHeight;

        vector<uint16_t> data(wantSize, 0);

        if (data.size() != wantSize)
        {
            Warning(
                Data,
                "Allocation pixels memory for LoadDirectionGroup failed (want size: %i)",
                wantSize);
            continue;
        }

        uint32_t header = ReadUInt32LE();

        while (header != 0x7FFF7FFF && !IsEOF())
        {
            uint16_t runLength = (header & 0x0FFF);

            int x = (header >> 22) & 0x03FF;

            if ((x & 0x0200) != 0)
            {
                x |= 0xFFFFFE00;
            }

            int y = (header >> 12) & 0x03FF;

            if ((y & 0x0200) != 0)
            {
                y |= 0xFFFFFE00;
            }

            x += imageCenterX;
            y += imageCenterY + imageHeight;

            int block = (y * imageWidth) + x;

            for (int k = 0; k < runLength; k++)
            {
                uint16_t val = palette[ReadUInt8()];

                if (val != 0u)
                {
                    data[block] = 0x8000 | val;
                }
                else
                {
                    data[block] = 0;
                }

                block++;
            }

            header = ReadUInt32LE();
        }

        g_GL_BindTexture16(frame, imageWidth, imageHeight, &data[0]);
    }
}
