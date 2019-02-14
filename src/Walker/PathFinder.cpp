// MIT License
// Copyright (C) August 2016 Hotride

#include <SDL_rect.h>
#include "PathFinder.h"
#include "Walker.h"
#include "../CrossUO.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/CustomHousesManager.h"
#include "../Managers/MapManager.h"
#include "../GameObjects/MapBlock.h"
#include "../GameObjects/GamePlayer.h"
#include "../GameObjects/LandObject.h"
#include "../GameObjects/CustomHouseMultiObject.h"
#include "../Network/Packets.h"
#include "../Gumps/GumpCustomHouse.h"

CPathFinder g_PathFinder;

CPathFinder::CPathFinder()
{
}

CPathFinder::~CPathFinder()
{
}

bool CPathFinder::CreateItemsList(vector<CPathObject> &list, int x, int y, int stepState)
{
    DEBUG_TRACE_FUNCTION;
    int blockX = x / 8;
    int blockY = y / 8;

    const uint32_t blockIndex = (blockX * g_MapBlockSize[g_CurrentMap].Height) + blockY;
    if (blockIndex >= g_MapManager.MaxBlockIndex)
    {
        return false;
    }

    CMapBlock *block = g_MapManager.GetBlock(blockIndex);
    if (block == nullptr)
    {
        block = g_MapManager.AddBlock(blockIndex);
        block->X = blockX;
        block->Y = blockY;
        g_MapManager.LoadBlock(block);
        //return false;
    }

    int bx = x % 8;
    int by = y % 8;

    bool ignoreGameCharacters =
        (IgnoreStaminaCheck || (stepState == PSS_DEAD_OR_GM) || g_Player->IgnoreCharacters() ||
         g_Player->Stam >= g_Player->MaxStam);
    const bool isGM = (g_Player->Graphic == 0x03DB);
    for (CRenderWorldObject *obj = block->GetRender(bx, by); obj != nullptr; obj = obj->m_NextXY)
    {
        if (g_CustomHouseGump != nullptr && obj->GetZ() < g_Player->GetZ())
        {
            continue;
        }

        const uint16_t graphic = obj->Graphic;
        if (obj->IsLandObject())
        {
            if ((graphic < 0x01AE && graphic != 2) || (graphic > 0x01B5 && graphic != 0x01DB))
            {
                CLandObject *land = (CLandObject *)obj;
                uint32_t flags = POF_IMPASSABLE_OR_SURFACE;
                const uint64_t tiledataFlags = g_Game.GetLandFlags(graphic);
                if (stepState == PSS_ON_SEA_HORSE)
                {
                    if (IsWet(tiledataFlags))
                    {
                        flags = POF_IMPASSABLE_OR_SURFACE | POF_SURFACE | POF_BRIDGE;
                    }
                }
                else
                {
                    if (!IsImpassable(tiledataFlags))
                    {
                        flags = POF_IMPASSABLE_OR_SURFACE | POF_SURFACE | POF_BRIDGE;
                    }

                    if (stepState == PSS_FLYING && IsNoDiagonal(tiledataFlags))
                    {
                        flags |= POF_NO_DIAGONAL;
                    }
                }

                const int landMinZ = land->MinZ;
                const int landAverageZ = land->AverageZ;
                const int landHeight = landAverageZ - landMinZ;
                list.push_back(CPathObject(flags, landMinZ, landAverageZ, landHeight, obj));
            }
        }
        else if (obj->IsStaticGroupObject())
        {
            bool canBeAdd = true;
            bool dropFlags = false;
            STATIC_TILES *tileInfo = obj->StaticGroupObjectPtr()->GetStaticData();
            if (obj->IsGameObject())
            {
                auto go = (CGameObject *)obj;
                if (go->NPC)
                {
                    CGameCharacter *gc = (CGameCharacter *)obj;

                    if (!ignoreGameCharacters && !gc->Dead() && !gc->IgnoreCharacters())
                    {
                        list.push_back(CPathObject(
                            POF_IMPASSABLE_OR_SURFACE,
                            obj->GetZ(),
                            obj->GetZ() + DEFAULT_CHARACTER_HEIGHT,
                            DEFAULT_CHARACTER_HEIGHT,
                            obj));
                    }
                    canBeAdd = false;
                }
                else if (((CGameItem *)obj)->MultiBody || obj->IsInternal())
                { //isMulti || InternalItem
                    canBeAdd = false;
                }
                else if (
                    stepState == PSS_DEAD_OR_GM &&
                    (go->IsDoor() || tileInfo->Weight <= 0x5A || (isGM && !go->Locked())))
                {
                    dropFlags = true;
                }
                else
                {
                    dropFlags = ((graphic >= 0x3946 && graphic <= 0x3964) || graphic == 0x0082);
                }
            }
            else if (
                g_CustomHouseGump != nullptr && obj->IsMultiObject() &&
                ((CMultiObject *)obj)->IsCustomHouseMulti() &&
                ((((CMultiObject *)obj)->State & CHMOF_GENERIC_INTERNAL) == 0))
            {
                canBeAdd = false;
            }

            if (canBeAdd)
            {
                uint32_t flags = 0;
                if (stepState == PSS_ON_SEA_HORSE)
                {
                    if (obj->IsWet())
                    {
                        flags = POF_SURFACE | POF_BRIDGE;
                    }
                }
                else
                {
                    if (obj->IsImpassable() || obj->IsSurface())
                    {
                        flags = POF_IMPASSABLE_OR_SURFACE;
                    }

                    if (!obj->IsImpassable())
                    {
                        if (obj->IsSurface())
                        {
                            flags |= POF_SURFACE;
                        }

                        if (obj->IsBridge())
                        {
                            flags |= POF_BRIDGE;
                        }
                    }

                    if (stepState == PSS_DEAD_OR_GM)
                    {
                        if (graphic <= 0x0846)
                        {
                            if (!(graphic != 0x0846 && graphic != 0x0692 &&
                                  (graphic <= 0x06F4 || graphic > 0x06F6)))
                            {
                                dropFlags = true;
                            }
                        }
                        else if (graphic == 0x0873)
                        {
                            dropFlags = true;
                        }
                    }

                    if (dropFlags)
                    {
                        flags &= 0xFFFFFFFE;
                    }

                    if (stepState == PSS_FLYING && obj->IsNoDiagonal())
                    {
                        flags |= POF_NO_DIAGONAL;
                    }
                }

                if (flags != 0u)
                {
                    const int objZ = obj->GetZ();
                    const int staticHeight = tileInfo->Height;
                    int staticAverageZ = staticHeight;
                    if (obj->IsBridge())
                    {
                        staticAverageZ /= 2;
                    }
                    list.push_back(
                        CPathObject(flags, objZ, staticAverageZ + objZ, staticHeight, obj));
                }
            }
        }
    }

    return !list.empty();
}

int CPathFinder::CalculateMinMaxZ(
    int &minZ, int &maxZ, int newX, int newY, int currentZ, int newDirection, int stepState)
{
    DEBUG_TRACE_FUNCTION;
    const int offsetX[10] = { 0, 1, 1, 1, 0, -1, -1, -1, 0, 1 };
    const int offsetY[10] = { -1, -1, 0, 1, 1, 1, 0, -1, -1, -1 };

    minZ = -128;
    maxZ = currentZ;

    newDirection &= 7;
    int direction = (newDirection ^ 4);

    newX += offsetX[direction];
    newY += offsetY[direction];

    vector<CPathObject> list;

    if (!CreateItemsList(list, newX, newY, stepState) || list.empty())
    {
        return 0;
    }

    for (const CPathObject &obj : list)
    {
        CRenderWorldObject *rwo = obj.m_Object;
        int averageZ = obj.AverageZ;

        if (averageZ <= currentZ && rwo->IsLandObject() && ((CLandObject *)rwo)->IsStretched)
        {
            int avgZ = ((CLandObject *)rwo)->CalculateCurrentAverageZ(newDirection);

            if (minZ < avgZ)
            {
                minZ = avgZ;
            }

            if (maxZ < avgZ)
            {
                maxZ = avgZ;
            }
        }
        else
        {
            if (((obj.Flags & POF_IMPASSABLE_OR_SURFACE) != 0u) && averageZ <= currentZ &&
                minZ < averageZ)
            {
                minZ = averageZ;
            }

            if (((obj.Flags & POF_BRIDGE) != 0u) && currentZ == averageZ)
            {
                int z = obj.Z;
                int height = z + obj.Height;

                if (maxZ < height)
                {
                    maxZ = height;
                }

                if (minZ > z)
                {
                    minZ = z;
                }
            }
        }
    }

    maxZ += 2;

    return maxZ;
}

bool CPathFinder::CalculateNewZ(int x, int y, char &z, int direction)
{
    DEBUG_TRACE_FUNCTION;
    int stepState = PSS_NORMAL;

    if (g_Player->Dead() || g_Player->Graphic == 0x03DB)
    {
        stepState = PSS_DEAD_OR_GM;
    }
    else
    {
        if (g_Player->Flying())
        { // && no mount?
            stepState = PSS_FLYING;
        }
        else
        {
            CGameItem *mount = g_Player->FindLayer(OL_MOUNT);

            if (mount != nullptr && mount->Graphic == 0x3EB3)
            { //Sea horse
                stepState = PSS_ON_SEA_HORSE;
            }
        }
    }

    int minZ = -128;
    int maxZ = z;

    CalculateMinMaxZ(minZ, maxZ, x, y, z, direction, stepState);

    vector<CPathObject> list;

    if (g_CustomHouseGump != nullptr)
    {
        SDL_Rect rect = { g_CustomHouseGump->StartPos.X,
                          g_CustomHouseGump->StartPos.Y,
                          g_CustomHouseGump->EndPos.X,
                          g_CustomHouseGump->EndPos.Y };
        SDL_Point pos = { x, y };

        if (SDL_PointInRect(&pos, &rect) == 0u)
        {
            return false;
        }
    }

    if (!CreateItemsList(list, x, y, stepState) || list.empty())
    {
        return false;
    }

    auto compareFunction = [](const void *obj1, const void *obj2) {
        int result = 0;

        if (obj1 != nullptr && obj2 != nullptr)
        {
            result = ((CPathObject *)obj1)->Z - ((CPathObject *)obj2)->Z;

            if (result == 0)
            {
                result = (((CPathObject *)obj1)->Height - ((CPathObject *)obj2)->Height);
            }
        }

        return result;
    };

    std::qsort(&list[0], list.size(), sizeof(CPathObject), compareFunction);

    list.push_back(CPathObject(POF_IMPASSABLE_OR_SURFACE, 128, 128, 128, nullptr));

    int resultZ = -128;

    if (z < minZ)
    {
        z = (char)minZ;
    }

    int currentTempObjZ = 1000000;
    int currentZ = -128;

    int listSize = (int)list.size();

    for (int i = 0; i < listSize; i++)
    {
        const CPathObject &obj = list[i];

        if (((obj.Flags & POF_NO_DIAGONAL) != 0u) && stepState == PSS_FLYING)
        {
            int objAverageZ = obj.AverageZ;

            int delta = abs(objAverageZ - (int)z);

            if (delta <= 25)
            {
                if (objAverageZ != -128)
                {
                    resultZ = objAverageZ;
                }
                else
                {
                    resultZ = currentZ;
                }

                break;
            }
        }

        if ((obj.Flags & POF_IMPASSABLE_OR_SURFACE) != 0u)
        {
            int objZ = obj.Z;

            if (objZ - minZ >= DEFAULT_BLOCK_HEIGHT)
            {
                for (int j = i - 1; j >= 0; j--)
                {
                    const CPathObject &tempObj = list[j];

                    if ((tempObj.Flags & (POF_SURFACE | POF_BRIDGE)) != 0u)
                    {
                        int tempAverageZ = tempObj.AverageZ;

                        if (tempAverageZ >= currentZ &&
                            objZ - tempAverageZ >= DEFAULT_BLOCK_HEIGHT &&
                            ((tempAverageZ <= maxZ && ((tempObj.Flags & POF_SURFACE) != 0u)) ||
                             (((tempObj.Flags & POF_BRIDGE) != 0u) && tempObj.Z <= maxZ)))
                        {
                            int delta = abs((int)z - tempAverageZ);

                            if (delta < currentTempObjZ)
                            {
                                currentTempObjZ = delta;
                                resultZ = tempAverageZ;
                            }
                        }
                    }
                }
            }

            if (g_ConfigManager.AutoOpenDoors && obj.m_Object && obj.m_Object->IsDoor())
            {
                g_Game.OpenDoor();
            }

            const int averageZ = obj.AverageZ;
            if (minZ < averageZ)
            {
                minZ = averageZ;
            }

            if (currentZ < averageZ)
            {
                currentZ = averageZ;
            }
        }
    }
    z = (char)resultZ;
    return (resultZ != -128);
}

void CPathFinder::GetNewXY(uint8_t direction, int &x, int &y)
{
    DEBUG_TRACE_FUNCTION;
    switch (direction & 7)
    {
        case 0:
        {
            y--;
            break;
        }
        case 1:
        {
            x++;
            y--;
            break;
        }
        case 2:
        {
            x++;
            break;
        }
        case 3:
        {
            x++;
            y++;
            break;
        }
        case 4:
        {
            y++;
            break;
        }
        case 5:
        {
            x--;
            y++;
            break;
        }
        case 6:
        {
            x--;
            break;
        }
        case 7:
        {
            x--;
            y--;
            break;
        }
    }
}

bool CPathFinder::CanWalk(uint8_t &direction, int &x, int &y, char &z)
{
    DEBUG_TRACE_FUNCTION;
    int newX = x;
    int newY = y;
    char newZ = z;
    uint8_t newDirection = direction;

    GetNewXY(direction, newX, newY);

    bool passed = CalculateNewZ(newX, newY, newZ, direction);

    if (((char)direction % 2) != 0) //diagonal
    {
        const char dirOffset[2] = { 1, -1 };

        if (passed) //test angleowner tiles
        {
            for (int i = 0; i < 2 && passed; i++)
            {
                int testX = x;
                int testY = y;
                char testZ = z;

                uint8_t testDir = (direction + dirOffset[i]) % 8;
                GetNewXY(testDir, testX, testY);

                passed = CalculateNewZ(testX, testY, testZ, testDir);
            }
        }

        if (!passed) //test neary tiles
        {
            for (int i = 0; i < 2 && !passed; i++)
            {
                newX = x;
                newY = y;
                newZ = z;

                newDirection = (direction + dirOffset[i]) % 8;
                GetNewXY(newDirection, newX, newY);

                passed = CalculateNewZ(newX, newY, newZ, newDirection);
            }
        }
    }

    if (passed)
    {
        x = newX;
        y = newY;
        z = newZ;
        direction = newDirection;
    }

    return passed;
}

int CPathFinder::GetWalkSpeed(bool run, bool onMount)
{
    DEBUG_TRACE_FUNCTION;
    bool mounted =
        (onMount ||
         (g_SpeedMode == CST_FAST_UNMOUNT || g_SpeedMode == CST_FAST_UNMOUNT_AND_CANT_RUN) ||
         g_Player->Flying());

    return CHARACTER_ANIMATION_DELAY_TABLE[mounted][run];
}

bool CPathFinder::Walk(bool run, uint8_t direction)
{
    DEBUG_TRACE_FUNCTION;
    if (BlockMoving || g_Walker.WalkingFailed || g_Walker.LastStepRequestTime > g_Ticks ||
        g_Walker.StepsCount >= MAX_STEPS_COUNT || g_Player == nullptr ||
        /*!g_Player->Frozen() ||*/ (g_DeathScreenTimer != 0u) || g_GameState != GS_GAME)
    {
        return false;
    }

    if (g_SpeedMode >= CST_CANT_RUN)
    {
        run = false;
    }
    else if (!run)
    {
        run = g_ConfigManager.AlwaysRun;
    }

    int x = g_Player->GetX();
    int y = g_Player->GetY();
    char z = g_Player->GetZ();
    uint8_t oldDirection = g_Player->Direction;

    bool onMount = (g_Player->FindLayer(OL_MOUNT) != nullptr);

    bool emptyStack = g_Player->m_Steps.empty();

    if (!emptyStack)
    {
        CWalkData &walker = g_Player->m_Steps.back();

        x = walker.X;
        y = walker.Y;
        z = walker.Z;
        oldDirection = walker.Direction;
    }

    char oldZ = z;
    uint16_t walkTime = TURN_DELAY;

    if (FastRotation)
    {
        walkTime = TURN_DELAY_FAST;
    }

    if ((oldDirection & 7) == (direction & 7)) //Повернуты куда надо
    {
        uint8_t newDir = direction;
        int newX = x;
        int newY = y;
        char newZ = z;

        if (!CanWalk(newDir, newX, newY, newZ))
        {
            return false;
        }

        if ((direction & 7) != newDir)
        {
            direction = newDir;
        }
        else
        {
            direction = newDir;
            x = newX;
            y = newY;
            z = newZ;

            walkTime = GetWalkSpeed(run, onMount);
        }
    }
    else
    {
        uint8_t newDir = direction;
        int newX = x;
        int newY = y;
        char newZ = z;

        if (!CanWalk(newDir, newX, newY, newZ))
        {
            if ((oldDirection & 7) == newDir)
            {
                return false;
            }
        }

        if ((oldDirection & 7) == newDir)
        {
            x = newX;
            y = newY;
            z = newZ;

            walkTime = GetWalkSpeed(run, onMount);
        }

        direction = newDir;
    }

    g_Player->CloseBank();

    if (emptyStack)
    {
        if (!g_Player->Walking())
        {
            g_Player->SetAnimation(0xFF);
        }

        g_Player->LastStepTime = g_Ticks;
    }

    CStepInfo &step = g_Walker.m_Step[g_Walker.StepsCount];
    step.Sequence = g_Walker.WalkSequence;
    step.Accepted = false;
    step.Running = run;
    step.OldDirection = oldDirection & 7;
    step.Direction = direction;
    step.Timer = g_Ticks;
    step.X = x;
    step.Y = y;
    step.Z = z;
    step.NoRotation = ((step.OldDirection == direction) && ((oldZ - z) >= 11));

    g_Walker.StepsCount++;

    if (run)
    {
        direction += 0x80;
    }

    g_Player->m_Steps.push_back(
        CWalkData(x, y, z, direction, g_Player->Graphic, g_Player->GetFlags()));

    CPacketWalkRequest(direction, g_Walker.WalkSequence, g_Player->m_FastWalkStack.GetValue())
        .Send();

    if (g_Walker.WalkSequence == 0xFF)
    {
        g_Walker.WalkSequence = 1;
    }
    else
    {
        g_Walker.WalkSequence++;
    }

    g_Walker.UnacceptedPacketsCount++;

    g_World->MoveToTop(g_Player);

    static bool lastRun = false;
    static bool lastMount = false;
    static int lastDir = -1;
    static int lastDelta = 0;
    static int lastStepTime = 0;

    //Высчитываем актуальную дельту с помощью разници во времени между прошлым и текущим шагом.
    int nowDelta = 0;

    if (lastDir == direction && lastMount == onMount && lastRun == run)
    {
        nowDelta = (g_Ticks - lastStepTime) - walkTime + lastDelta;

        if (abs(nowDelta) > 70)
        {
            nowDelta = 0;
        }

        lastDelta = nowDelta;
    }
    else
    {
        lastDelta = 0;
    }

    lastStepTime = g_Ticks;

    lastRun = run;
    lastMount = onMount;

    if (walkTime == TURN_DELAY_FAST)
    {
        lastDir = -1;
    }
    else
    {
        lastDir = direction;
    }

    g_Walker.LastStepRequestTime = g_Ticks + walkTime - nowDelta;
    g_Player->GetAnimationGroup();

    return true;
}

int CPathFinder::GetGoalDistCost(const CPoint2Di &p, int cost)
{
    DEBUG_TRACE_FUNCTION;
    return (abs(m_EndPoint.X - p.X) + abs(m_EndPoint.Y - p.Y)) * cost;
}

bool CPathFinder::DoesNotExistOnOpenList(int x, int y, int z)
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    for (int i = 0; i < PATHFINDER_MAX_NODES; i++)
    {
        CPathNode &node = m_OpenList[i];

        if (node.Used && node.X == x && node.Y == y && node.Z == z)
        {
            result = true;
            break;
        }
    }

    return result;
}

bool CPathFinder::DoesNotExistOnClosedList(int x, int y, int z)
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    for (int i = 0; i < PATHFINDER_MAX_NODES; i++)
    {
        CPathNode &node = m_ClosedList[i];

        if (node.Used && node.X == x && node.Y == y && node.Z == z)
        {
            result = true;
            break;
        }
    }

    return result;
}

int CPathFinder::AddNodeToList(
    int list, int direction, int x, int y, int z, CPathNode *parentNode, int cost)
{
    DEBUG_TRACE_FUNCTION;
    if (list == 0)
    {
        if (!DoesNotExistOnClosedList(x, y, z))
        {
            if (!DoesNotExistOnOpenList(x, y, z))
            {
                for (int i = 0; i < PATHFINDER_MAX_NODES; i++)
                {
                    CPathNode &node = m_OpenList[i];

                    if (!node.Used)
                    {
                        node.Used = true;

                        node.Direction = direction;
                        node.X = x;
                        node.Y = y;
                        node.Z = z;
                        CPoint2Di p(x, y);

                        node.DistFromGoalCost = GetGoalDistCost(p, cost);
                        node.DistFromStartCost = parentNode->DistFromStartCost + cost;
                        node.Cost = node.DistFromGoalCost + node.DistFromStartCost;

                        node.m_Parent = parentNode;

                        //if (x == m_EndX && y == m_EndY)
                        if (GetDistance(m_EndPoint, p) <= m_PathFindDistance)
                        {
                            m_GoalFound = true;
                            m_GoalNode = (int)i;
                        }

                        m_ActiveOpenNodes++;

                        return (int)i;
                    }
                }
            }
            else
            {
                for (int i = 0; i < PATHFINDER_MAX_NODES; i++)
                {
                    CPathNode &node = m_OpenList[i];

                    if (node.Used)
                    {
                        if (node.X == x && node.Y == y && node.Z == z)
                        {
                            int startCost = parentNode->DistFromStartCost + cost;

                            if (node.DistFromStartCost > startCost)
                            {
                                node.m_Parent = parentNode;
                                node.DistFromStartCost = startCost + cost;
                                node.Cost = node.DistFromGoalCost + node.DistFromStartCost;
                            }

                            return (int)i;
                        }
                    }
                }
            }
        }
        else
        {
            return 0;
        }
    }
    else
    {
        parentNode->Used = false;

        for (int i = 0; i < PATHFINDER_MAX_NODES; i++)
        {
            CPathNode &node = m_ClosedList[i];

            if (!node.Used)
            {
                node.Used = true;

                node.DistFromGoalCost = parentNode->DistFromGoalCost;
                node.DistFromStartCost = parentNode->DistFromStartCost;
                node.Cost = node.DistFromGoalCost + node.DistFromStartCost;
                node.Direction = parentNode->Direction;
                node.X = parentNode->X;
                node.Y = parentNode->Y;
                node.Z = parentNode->Z;
                node.m_Parent = parentNode->m_Parent;

                m_ActiveOpenNodes--;
                m_ActiveClosedNodes++;

                return (int)i;
            }
        }
    }

    return -1;
}

bool CPathFinder::OpenNodes(CPathNode *node)
{
    DEBUG_TRACE_FUNCTION;
    bool found = false;

    for (int i = 0; i < 8; i++)
    {
        uint8_t direction = (uint8_t)i;
        int x = node->X;
        int y = node->Y;
        char z = (char)node->Z;

        uint8_t oldDirection = direction;

        if (CanWalk(direction, x, y, z))
        {
            if (direction != oldDirection)
            {
                continue;
            }

            int diagonal = i % 2;

            if (diagonal != 0)
            {
                uint8_t wantDirection = (uint8_t)i;
                int wantX = node->X;
                int wantY = node->Y;

                GetNewXY(wantDirection, wantX, wantY);

                if (x != wantX || y != wantY)
                {
                    diagonal = -1;
                }
            }

            if (diagonal >= 0 && AddNodeToList(0, direction, x, y, z, node, 1 + diagonal) != -1)
            {
                found = true;
            }
            else
            {
                continue;
            }
        }
    }

    return found;
}

int CPathFinder::FindCheapestNode()
{
    DEBUG_TRACE_FUNCTION;
    int cheapestCost = 9999999;
    int cheapestNode = -1;

    for (int i = 0; i < PATHFINDER_MAX_NODES; i++)
    {
        if (m_OpenList[i].Used)
        {
            if (m_OpenList[i].Cost < cheapestCost)
            {
                cheapestNode = (int)i;
                cheapestCost = m_OpenList[i].Cost;
            }
        }
    }

    int result = -1;

    if (cheapestNode != -1)
    {
        result = AddNodeToList(1, 0, 0, 0, 0, &m_OpenList[cheapestNode], 2);
    }

    return result;
}

bool CPathFinder::FindPath(int maxNodes)
{
    DEBUG_TRACE_FUNCTION;
    int curNode = 0;

    m_ClosedList[0].Used = true;

    m_ClosedList[0].X = m_StartPoint.X;
    m_ClosedList[0].Y = m_StartPoint.Y;
    m_ClosedList[0].Z = g_Player->GetZ();
    m_ClosedList[0].m_Parent = nullptr;

    m_ClosedList[0].DistFromGoalCost = GetGoalDistCost(m_StartPoint, 0);
    m_ClosedList[0].Cost = m_ClosedList[0].DistFromGoalCost;

    while (AutoWalking)
    {
        OpenNodes(&m_ClosedList[curNode]);

        if (m_GoalFound)
        {
            int totalNodes = 0;

            CPathNode *GoalNode = &m_OpenList[m_GoalNode];

            while (GoalNode->m_Parent != nullptr && GoalNode != GoalNode->m_Parent)
            {
                GoalNode = GoalNode->m_Parent;
                totalNodes++;
            };

            totalNodes++;

            m_PathSize = totalNodes;

            GoalNode = &m_OpenList[m_GoalNode];

            while (totalNodes > 0)
            {
                totalNodes--;
                m_Path[totalNodes] = GoalNode;
                GoalNode = GoalNode->m_Parent;
            };

            break;
        }

        curNode = FindCheapestNode();

        if (curNode == -1)
        {
            return false;
        }

        if (m_ActiveClosedNodes >= maxNodes)
        {
            return false;
        }
    }

    return true;
}

bool CPathFinder::WalkTo(int x, int y, int z, int distance)
{
    DEBUG_TRACE_FUNCTION;
    for (int i = 0; i < PATHFINDER_MAX_NODES; i++)
    {
        m_OpenList[i].Reset();
    }

    for (int i = 0; i < PATHFINDER_MAX_NODES; i++)
    {
        m_ClosedList[i].Reset();
    }

    m_StartPoint.X = g_Player->GetX();
    m_StartPoint.Y = g_Player->GetY();
    m_EndPoint.X = x;
    m_EndPoint.Y = y;
    m_GoalNode = 0;
    m_GoalFound = false;
    m_ActiveOpenNodes = 0;
    m_ActiveClosedNodes = 0;
    m_PathFindDistance = distance;
    m_PathSize = 0;
    PathFindidngCanBeCancelled = true;

    StopAutoWalk();
    AutoWalking = true;

    if (FindPath(PATHFINDER_MAX_NODES))
    {
        m_PointIndex = 1;
        ProcessAutowalk();
    }
    else
    {
        AutoWalking = false;
    }

    return (m_PathSize != 0);
}

void CPathFinder::ProcessAutowalk()
{
    DEBUG_TRACE_FUNCTION;
    if (AutoWalking && g_Player != nullptr && (g_DeathScreenTimer == 0u) &&
        g_Walker.StepsCount < MAX_STEPS_COUNT && g_Walker.LastStepRequestTime <= g_Ticks)
    {
        if (m_PointIndex >= 0 && m_PointIndex < m_PathSize)
        {
            CPathNode *p = m_Path[m_PointIndex];

            uint8_t olddir = g_Player->Direction;

            if (!g_Player->m_Steps.empty())
            {
                olddir = g_Player->m_Steps.back().Direction;
            }

            if ((olddir & 7) == p->Direction)
            {
                m_PointIndex++;
            }

            if (!Walk(g_ConfigManager.AlwaysRun, p->Direction))
            {
                StopAutoWalk();
            }
        }
        else
        {
            StopAutoWalk();
        }
    }
}

void CPathFinder::StopAutoWalk()
{
    DEBUG_TRACE_FUNCTION;
    AutoWalking = false;
    m_PathSize = 0;
}
