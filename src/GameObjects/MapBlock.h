// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../BaseQueue.h"

#include <vector>

class CMapObject;
class CLandObject;
class CRenderWorldObject;

class CMapBlock : public CBaseQueueItem
{
public:
    uint32_t Index = 0;
    uint32_t LastAccessTime = 0;
    short X = 0;
    short Y = 0;

    // false until LoadBlock filled the terrain/statics in (blocks created by
    // AddRender before their terrain is available - or whose punctual fetch
    // failed - get (re)loaded by the later Init passes)
    bool Loaded = false;

private:
    // Get the land Z coordinate
    char GetLandZ(int x, int y, int map);

    // Check whether the land tile can be stretched at render time
    bool TestStretched(int x, int y, char z, int map, bool recurse);

public:
    CMapBlock(int index);
    virtual ~CMapBlock();

    // Block data
    CMapObject *Block[8][8];

    // Add an object
    CMapObject *AddObject(CMapObject *obj, int x, int y);

    // Get a pointer to the land object
    CLandObject *GetLand(int x, int y);

    // Add the object to the render queue
    void AddRender(CRenderWorldObject *item, int x, int y);

    // web streaming: objects AddRender deferred while the block had no
    // terrain yet (the XY sort in AddRender needs Block[x][y] as its anchor,
    // and a world object must never head that chain - it is AddObject's
    // m_Next list). LoadBlock flushes these once the terrain exists.
    std::vector<class CRenderWorldObject *> PendingRender;

    // Get the head of the render queue
    CRenderWorldObject *GetRender(int x, int y);

    // Get the radar pixel color
    uint16_t GetRadarColor(int x, int y);

    // Build the land texture vertices
    void CreateLandTextureRect();

    bool HasNoExternalData();
};
