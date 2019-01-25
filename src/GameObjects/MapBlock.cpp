// MIT License
// Copyright (C) August 2016 Hotride

#include <SDL_timer.h>
#include "MapBlock.h"
#include "LandObject.h"
#include "CustomHouseMultiObject.h"
#include "../CrossUO.h"
#include "../Managers/MapManager.h"
#include "../Managers/CustomHousesManager.h"
#include "../Gumps/GumpCustomHouse.h"

CMapBlock::CMapBlock(int index)
    : Index(index)
    , LastAccessTime(SDL_GetTicks())
{
    DEBUG_TRACE_FUNCTION;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            Block[i][j] = nullptr;
        }
    }
}

CMapBlock::~CMapBlock()
{
    DEBUG_TRACE_FUNCTION;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            CMapObject *obj = Block[i][j];

            if (obj != nullptr)
            {
                CRenderWorldObject *render = GetRender((int)i, (int)j);

                while (render != nullptr)
                {
                    CRenderWorldObject *next = render->m_NextXY;
                    render->m_PrevXY = nullptr;
                    render->m_NextXY = nullptr;
                    render = next;
                }

                delete obj;
                Block[i][j] = nullptr;
            }
        }
    }

    m_Items = nullptr;
}

bool CMapBlock::HasNoExternalData()
{
    DEBUG_TRACE_FUNCTION;
    for (int x = 0; x < 8; x++)
    {
        for (int y = 0; y < 8; y++)
        {
            for (CRenderWorldObject *obj = GetRender((int)x, (int)y); obj != nullptr;
                 obj = obj->m_NextXY)
            {
                if (!obj->IsLandObject() && !obj->IsStaticObject())
                {
                    return false;
                }
            }
        }
    }

    return true;
}

uint16_t CMapBlock::GetRadarColor(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    CRenderWorldObject *obj = Block[x][y];

    while (obj != nullptr && obj->m_NextXY != nullptr)
    {
        obj = obj->m_NextXY;
    }

    for (; obj != nullptr; obj = obj->m_PrevXY)
    {
        if (obj->NoDrawTile)
        {
            continue;
        }

        switch (obj->RenderType)
        {
            case ROT_LAND_OBJECT:
            case ROT_STATIC_OBJECT:
                return 0;
            case ROT_MULTI_OBJECT:
                return obj->Graphic;
            default:
                break;
        }
    }
    return 0;
}

void CMapBlock::CreateLandTextureRect()
{
    DEBUG_TRACE_FUNCTION;
    int map = g_MapManager.GetActualMap();
    for (int x = 0; x < 8; x++)
    {
        for (int y = 0; y < 8; y++)
        {
            CLandObject *obj = GetLand((int)x, (int)y);
            if (obj != nullptr)
            {
                int tileX = obj->GetX();
                int tileY = obj->GetY();
                char tileZ1 = obj->GetZ();

                CGLTexture *th = g_Game.ExecuteTexture(obj->Graphic);
                if (obj->IsStretched || th == nullptr ||
                    !TestStretched(tileX, tileY, tileZ1, map, true))
                {
                    obj->IsStretched = false;

                    obj->MinZ = tileZ1;
                }
                else
                {
                    obj->IsStretched = true;

                    obj->UpdateZ(
                        GetLandZ(tileX, tileY + 1, map),
                        GetLandZ(tileX + 1, tileY + 1, map),
                        GetLandZ(tileX + 1, tileY, map));

                    CVector vec[3][3][4];

                    for (int i = -1; i < 2; i++)
                    {
                        int curX = tileX + (int)i;
                        int curI = (int)i + 1;

                        for (int j = -1; j < 2; j++)
                        {
                            int curY = tileY + (int)j;
                            int curJ = (int)j + 1;

                            char currentZ = GetLandZ(curX, curY, map);
                            char leftZ = GetLandZ(curX, curY + 1, map);
                            char rightZ = GetLandZ(curX + 1, curY, map);
                            char bottomZ = GetLandZ(curX + 1, curY + 1, map);

                            if (currentZ == leftZ && currentZ == rightZ && currentZ == bottomZ)
                            {
                                for (int k = 0; k < 4; k++)
                                {
                                    vec[curI][curJ][k].Link(0.0, 0.0, 1.0);
                                }
                            }
                            else
                            {
                                vec[curI][curJ][0].Link(-22.0, 22.0, (currentZ - rightZ) * 4);
                                vec[curI][curJ][0].Merge(-22.0, -22.0, (leftZ - currentZ) * 4);
                                vec[curI][curJ][0].Normalize();

                                vec[curI][curJ][1].Link(22.0, 22.0, (rightZ - bottomZ) * 4);
                                vec[curI][curJ][1].Merge(-22.0, 22.0, (currentZ - rightZ) * 4);
                                vec[curI][curJ][1].Normalize();

                                vec[curI][curJ][2].Link(22.0, -22.0, (bottomZ - leftZ) * 4);
                                vec[curI][curJ][2].Merge(22.0, 22.0, (rightZ - bottomZ) * 4);
                                vec[curI][curJ][2].Normalize();

                                vec[curI][curJ][3].Link(-22.0, -22.0, (leftZ - currentZ) * 4);
                                vec[curI][curJ][3].Merge(22.0, -22.0, (bottomZ - leftZ) * 4);
                                vec[curI][curJ][3].Normalize();
                            }
                        }
                    }

                    int i = 1;
                    int j = 1;

                    obj->m_Normals[0].Link(vec[i - 1][j - 1][2]);
                    obj->m_Normals[0].Add(vec[i - 1][j][1]);
                    obj->m_Normals[0].Add(vec[i][j - 1][3]);
                    obj->m_Normals[0].Add(vec[i][j][0]);
                    obj->m_Normals[0].Normalize();

                    obj->m_Normals[1].Link(vec[i][j - 1][2]);
                    obj->m_Normals[1].Add(vec[i][j][1]);
                    obj->m_Normals[1].Add(vec[i + 1][j - 1][3]);
                    obj->m_Normals[1].Add(vec[i + 1][j][0]);
                    obj->m_Normals[1].Normalize();

                    obj->m_Normals[2].Link(vec[i][j][2]);
                    obj->m_Normals[2].Add(vec[i][j + 1][1]);
                    obj->m_Normals[2].Add(vec[i + 1][j][3]);
                    obj->m_Normals[2].Add(vec[i + 1][j + 1][0]);
                    obj->m_Normals[2].Normalize();

                    obj->m_Normals[3].Link(vec[i - 1][j][2]);
                    obj->m_Normals[3].Add(vec[i - 1][j + 1][1]);
                    obj->m_Normals[3].Add(vec[i][j][3]);
                    obj->m_Normals[3].Add(vec[i][j + 1][0]);
                    obj->m_Normals[3].Normalize();

                    if (g_GL.CanUseBuffer)
                    {
                        GLuint positionBuffer = obj->PositionBuffer;
                        GLuint vertexBuffer = obj->VertexBuffer;
                        GLuint normalBuffer = obj->NormalBuffer;

                        if ((positionBuffer == 0u) || (vertexBuffer == 0u) || (normalBuffer == 0u))
                        {
                            GLuint vbo[3] = { 0 };
                            glGenBuffers(3, &vbo[0]);

                            positionBuffer = vbo[0];
                            vertexBuffer = vbo[1];
                            normalBuffer = vbo[2];

                            obj->PositionBuffer = positionBuffer;
                            obj->VertexBuffer = vertexBuffer;
                            obj->NormalBuffer = normalBuffer;

                            const auto &rc = obj->m_Rect;
                            CVector *normals = obj->m_Normals;

                            int positionArray[] = { 0, 0, 0, 1, 1, 0, 1, 1 };

                            glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
                            glBufferData(
                                GL_ARRAY_BUFFER,
                                sizeof(positionArray),
                                &positionArray[0],
                                GL_STATIC_DRAW);

                            int vertexArray[] = { 22, -rc.x,     0,  22 - rc.y,
                                                  44, 22 - rc.h, 22, 44 - rc.w };

                            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
                            glBufferData(
                                GL_ARRAY_BUFFER,
                                sizeof(vertexArray),
                                &vertexArray[0],
                                GL_STATIC_DRAW);

                            float normalArray[] = { (float)normals[0].X, (float)normals[0].Y,
                                                    (float)normals[0].Z, (float)normals[3].X,
                                                    (float)normals[3].Y, (float)normals[3].Z,
                                                    (float)normals[1].X, (float)normals[1].Y,
                                                    (float)normals[1].Z, (float)normals[2].X,
                                                    (float)normals[2].Y, (float)normals[2].Z };

                            glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
                            glBufferData(
                                GL_ARRAY_BUFFER,
                                sizeof(normalArray),
                                &normalArray[0],
                                GL_STATIC_DRAW);
                        }
                    }
                }

                AddRender(obj, (int)x, (int)y);
            }
        }
    }
}

bool CMapBlock::TestStretched(int x, int y, char z, int map, bool recurse)
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;
    for (int i = -1; i < 2 && !result; i++)
    {
        for (int j = -1; j < 2 && !result; j++)
        {
            if (recurse)
            {
                result = TestStretched(x + (int)i, y + (int)j, z, map, false);
            }
            else
            {
                char testZ = GetLandZ(x + (int)i, y + (int)j, map);

                result = (testZ != z && testZ != -125);
            }
        }
    }

    return result;
}

char CMapBlock::GetLandZ(int x, int y, int map)
{
    DEBUG_TRACE_FUNCTION;

    if (x < 0 || y < 0)
    {
        return -125;
    }

    CIndexMap *blockIndex = g_MapManager.GetIndex(map, x / 8, y / 8);
    if (blockIndex == nullptr || blockIndex->MapAddress == 0)
    {
        return -125;
    }

    int mX = x % 8;
    int mY = y % 8;

    return ((MAP_BLOCK *)blockIndex->MapAddress)->Cells[mY * 8 + mX].Z;
}

CLandObject *CMapBlock::GetLand(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    CMapObject *obj = Block[x][y];
    while (obj != nullptr)
    {
        if (obj->IsLandObject())
        {
            break;
        }

        obj = (CMapObject *)obj->m_Next;
    }

    return (CLandObject *)obj;
}

void CMapBlock::AddRender(CRenderWorldObject *item, int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    item->RemoveRender();

    int priorityZ = item->GetZ();

    if (item->IsLandObject())
    {
        if (((CLandObject *)item)->IsStretched)
        {
            priorityZ = ((CLandObject *)item)->AverageZ - 1;
        }
        else
        {
            priorityZ--;
        }
    }
    else if (item->IsStaticGroupObject())
    {
        if (item->IsGameObject())
        {
            //priorityZ++;
            if (((CGameObject *)item)->NPC || ((CGameObject *)item)->IsCorpse())
            {
                priorityZ++;
            }
            else
            {
                goto process_static;
            }
        }
        else if (
            item->IsMultiObject() &&
            ((((CMultiObject *)item)->State & CHMOF_GENERIC_INTERNAL) != 0))
        {
            priorityZ--;
        }
        else
        {
        process_static:
            if (item->IsBackground())
            {
                priorityZ--;
            }

            if (((CRenderStaticObject *)item)->GetStaticHeight() != 0u)
            {
                priorityZ++;
            }
        }
    }
    else if (item->IsEffectObject())
    {
        priorityZ += 2;
    }

    item->PriorityZ = priorityZ;

    CRenderWorldObject *obj = Block[x][y];
    if (obj == item)
    {
        if (obj->m_Prev != nullptr)
        {
            obj = (CRenderWorldObject *)obj->m_Prev;
        }
        else if (obj->m_Next != nullptr)
        {
            obj = (CRenderWorldObject *)obj->m_Next;
        }
        else
        {
            return;
        }
    }

    while (obj != nullptr && obj->m_PrevXY != nullptr)
    {
        obj = obj->m_PrevXY;
    }

    CRenderWorldObject *found = nullptr;
    CRenderWorldObject *start = obj;
    while (obj != nullptr)
    {
        int testPriorityZ = obj->PriorityZ;

        if (testPriorityZ > priorityZ ||
            (testPriorityZ == priorityZ && item->IsLandObject() && !obj->IsLandObject()))
        {
            break;
        }

        found = obj;
        obj = obj->m_NextXY;
    }

    if (found != nullptr)
    {
        item->m_PrevXY = found;
        CRenderWorldObject *next = found->m_NextXY;
        item->m_NextXY = next;
        found->m_NextXY = item;

        if (next != nullptr)
        {
            next->m_PrevXY = item;
        }
    }
    else if (start != nullptr)
    {
        item->m_NextXY = start;
        start->m_PrevXY = item;
        item->m_PrevXY = nullptr;
    }
}

CRenderWorldObject *CMapBlock::GetRender(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    CRenderWorldObject *obj = Block[x][y];
    while (obj != nullptr && obj->m_PrevXY != nullptr)
    {
        obj = obj->m_PrevXY;
    }
    return obj;
}

CMapObject *CMapBlock::AddObject(CMapObject *obj, int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    if (Block[x][y] != nullptr)
    {
        CMapObject *item = Block[x][y];
        while (item != nullptr)
        {
            if (!item->IsLandObject() && item->GetZ() > obj->GetZ())
            {
                if (item->m_Prev != nullptr)
                {
                    item = (CMapObject *)item->m_Prev;
                    break;
                }

                m_Items = obj;
                obj->m_Prev = nullptr;
                obj->m_Next = item;
                item->m_Prev = obj;
                AddRender(obj, x, y);
                return obj;
            }
            if (item->m_Next == nullptr)
            {
                break;
            }

            item = (CMapObject *)item->m_Next;
        }

        assert(item != nullptr);
        CMapObject *next = (CMapObject *)item->m_Next;
        item->m_Next = obj;
        obj->m_Next = next;
        obj->m_Prev = item;
        if (next != nullptr)
        {
            next->m_Prev = obj;
        }
        AddRender(obj, x, y);
    }
    else
    {
        Block[x][y] = obj;
        obj->m_Next = nullptr;
        obj->m_Prev = nullptr;
        obj->m_NextXY = nullptr;
        obj->m_PrevXY = nullptr;
    }

    return obj;
}
