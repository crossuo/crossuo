// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../BaseQueue.h"
#include <xuocore/mappedfile.h>
#include "../Globals.h" // MAX_MAPS_COUNT

class CMapBlock;
class CRenderWorldObject;
struct CIndexMap;
struct RADAR_MAP_BLOCK;

class CMapManager : public CBaseQueue
{
public:
    uint32_t MaxBlockIndex = 0;
    int PatchesCount = 0;

protected:
    CMapBlock **m_Blocks{ nullptr };
    bool m_BlockAccessList[0x1000];
    void ResetPatchesInBlockTable();

public:
    CMapManager() = default;
    virtual ~CMapManager();

    int m_MapPatchCount[MAX_MAPS_COUNT];
    int m_StaticPatchCount[MAX_MAPS_COUNT];

    CIndexMap *GetIndex(int map, int blockX, int blockY);
    void ApplyPatches(CDataReader &stream);
    void UpdatePatched();
    void ClearBlockAccess();
    char CalculateNearZ(char defaultZ, int x, int y, int z);
    int GetActualMap();
    void SetPatchedMapBlock(size_t block, size_t address);
    void LoadBlock(CMapBlock *block);
    void GetRadarMapBlock(int blockX, int blockY, RADAR_MAP_BLOCK &mb);
    void GetMapZ(int x, int y, int &groundZ, int &staticZ);
    void Init(bool delayed = false);
    CMapBlock *GetBlock(uint32_t index);
    CMapBlock *AddBlock(uint32_t index);
    void DeleteBlock(uint32_t index);
    void ClearUnusedBlocks();
    void ClearUsedBlocks();
    void AddRender(CRenderWorldObject *item);
};

extern CMapManager g_MapManager;
