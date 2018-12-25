// MIT License
// Copyright (C) August 2016 Hotride

CMapManager g_MapManager;

CIndexMap::CIndexMap()
{
}

CIndexMap::~CIndexMap()
{
}

CMapManager::CMapManager()

{
}

CMapManager::~CMapManager()
{
    DEBUG_TRACE_FUNCTION;
    if (m_Blocks != nullptr)
    {
        ClearUsedBlocks();

        delete[] m_Blocks;
        m_Blocks = nullptr;
    }

    MaxBlockIndex = 0;
}

void CMapManager::CreateBlocksTable()
{
    DEBUG_TRACE_FUNCTION;
    for (int map = 0; map < MAX_MAPS_COUNT; map++)
    {
        CreateBlockTable((int)map);
    }
}

void CMapManager::CreateBlockTable(int map)
{
    DEBUG_TRACE_FUNCTION;
    MAP_INDEX_LIST &list = m_BlockData[map];
    Wisp::CSize &size = g_MapBlockSize[map];

    int maxBlockCount = size.Width * size.Height;

    //Return and error notification?
    if (maxBlockCount < 1)
    {
        return;
    }

    list.resize(maxBlockCount);

    size_t mapAddress = (size_t)g_FileManager.m_MapMul[map].Start;
    size_t endMapAddress = mapAddress + g_FileManager.m_MapMul[map].Size;

    CUopMappedFile &uopFile = g_FileManager.m_MapUOP[map];
    const bool isUop = (uopFile.Start != nullptr);
    if (isUop)
    {
        mapAddress = (size_t)uopFile.Start;
        endMapAddress = mapAddress + uopFile.Size;
    }

    size_t staticIdxAddress = (size_t)g_FileManager.m_StaticIdx[map].Start;
    size_t endStaticIdxAddress = staticIdxAddress + g_FileManager.m_StaticIdx[map].Size;
    size_t staticAddress = (size_t)g_FileManager.m_StaticMul[map].Start;
    size_t endStaticAddress = staticAddress + g_FileManager.m_StaticMul[map].Size;
    if ((mapAddress == 0u) || (staticIdxAddress == 0u) || (staticAddress == 0u))
    {
        return;
    }

    int fileNumber = -1;
    size_t uopOffset = 0;
    for (int block = 0; block < maxBlockCount; block++)
    {
        CIndexMap &index = list[block];
        size_t realMapAddress = 0;
        size_t realStaticAddress = 0;
        int realStaticCount = 0;
        int blockNumber = (int)block;
        if (isUop)
        {
            blockNumber &= 4095;
            int shifted = (int)block >> 12;
            if (fileNumber != shifted)
            {
                fileNumber = shifted;
                char mapFilePath[200] = { 0 };
                sprintf_s(mapFilePath, "build/map%ilegacymul/%08i.dat", map, shifted);

                CUopBlockHeader *uopBlock = uopFile.GetBlock(COrion::CreateHash(mapFilePath));
                if (uopBlock != nullptr)
                {
                    uopOffset = (size_t)uopBlock->Offset;
                }
                else
                {
                    LOG("Hash not found in uop map %i file.\n", map);
                }
            }
        }

        size_t address = mapAddress + uopOffset + (blockNumber * sizeof(MAP_BLOCK));
        if (address < endMapAddress)
        {
            realMapAddress = address;
        }

        STAIDX_BLOCK *sidx = (STAIDX_BLOCK *)(staticIdxAddress + block * sizeof(STAIDX_BLOCK));
        if ((size_t)sidx < endStaticIdxAddress && sidx->Size > 0 && sidx->Position != 0xFFFFFFFF)
        {
            size_t address = staticAddress + sidx->Position;
            if (address < endStaticAddress)
            {
                realStaticAddress = address;
                realStaticCount = sidx->Size / sizeof(STATICS_BLOCK);
                if (realStaticCount > 1024)
                {
                    realStaticCount = 1024;
                }
            }
        }

        index.OriginalMapAddress = realMapAddress;
        index.OriginalStaticAddress = realStaticAddress;
        index.OriginalStaticCount = realStaticCount;

        index.MapAddress = realMapAddress;
        index.StaticAddress = realStaticAddress;
        index.StaticCount = realStaticCount;
    }
}

void CMapManager::SetPatchedMapBlock(size_t block, size_t address)
{
    DEBUG_TRACE_FUNCTION;
    MAP_INDEX_LIST &list = m_BlockData[0];
    Wisp::CSize &size = g_MapBlockSize[0];

    int maxBlockCount = size.Width * size.Height;

    if (maxBlockCount < 1)
    {
        return;
    }

    list[block].OriginalMapAddress = address;
    list[block].MapAddress = address;
}

void CMapManager::ResetPatchesInBlockTable()
{
    DEBUG_TRACE_FUNCTION;
    for (int map = 0; map < MAX_MAPS_COUNT; map++)
    {
        MAP_INDEX_LIST &list = m_BlockData[map];
        Wisp::CSize &size = g_MapBlockSize[map];

        int maxBlockCount = size.Width * size.Height;

        //Return and error notification?
        if (maxBlockCount < 1)
        {
            return;
        }

        if (g_FileManager.m_MapMul[map].Start == nullptr ||
            g_FileManager.m_StaticIdx[map].Start == nullptr ||
            g_FileManager.m_StaticMul[map].Start == nullptr)
        {
            return;
        }

        for (int block = 0; block < maxBlockCount; block++)
        {
            CIndexMap &index = list[block];

            index.MapAddress = index.OriginalMapAddress;
            index.StaticAddress = index.OriginalStaticAddress;
            index.StaticCount = index.OriginalStaticCount;
        }
    }
}

void CMapManager::ApplyPatches(Wisp::CDataReader &stream)
{
    DEBUG_TRACE_FUNCTION;
    ResetPatchesInBlockTable();

    PatchesCount = stream.ReadUInt32BE();

    if (PatchesCount < 0)
    {
        PatchesCount = 0;
    }

    if (PatchesCount > MAX_MAPS_COUNT)
    {
        PatchesCount = MAX_MAPS_COUNT;
    }

    memset(&m_MapPatchCount[0], 0, sizeof(m_MapPatchCount));
    memset(&m_StaticPatchCount[0], 0, sizeof(m_StaticPatchCount));

    for (int i = 0; i < PatchesCount; i++)
    {
        if (g_FileManager.m_MapMul[i].Start == nullptr)
        {
            stream.Move(8);
            continue;
        }

        intptr_t mapPatchesCount = stream.ReadUInt32BE();
        m_MapPatchCount[i] = (int)mapPatchesCount;
        intptr_t staticsPatchesCount = stream.ReadUInt32BE();
        m_StaticPatchCount[i] = (int)staticsPatchesCount;

        MAP_INDEX_LIST &list = m_BlockData[i];
        Wisp::CSize &size = g_MapBlockSize[i];

        uint32_t maxBlockCount = size.Height * size.Width;

        if (mapPatchesCount != 0)
        {
            Wisp::CMappedFile &difl = g_FileManager.m_MapDifl[i];
            Wisp::CMappedFile &dif = g_FileManager.m_MapDif[i];

            mapPatchesCount = std::min(mapPatchesCount, (intptr_t)difl.Size / 4);

            difl.ResetPtr();
            dif.ResetPtr();

            for (int j = 0; j < mapPatchesCount; j++)
            {
                uint32_t blockIndex = difl.ReadUInt32LE();

                if (blockIndex < maxBlockCount)
                {
                    list[blockIndex].MapAddress = (size_t)dif.Ptr;
                }

                dif.Move(sizeof(MAP_BLOCK));
            }
        }

        if (staticsPatchesCount != 0)
        {
            Wisp::CMappedFile &difl = g_FileManager.m_StaDifl[i];
            Wisp::CMappedFile &difi = g_FileManager.m_StaDifi[i];
            size_t startAddress = (size_t)g_FileManager.m_StaDif[i].Start;

            staticsPatchesCount = std::min(staticsPatchesCount, (intptr_t)difl.Size / 4);

            difl.ResetPtr();
            difi.ResetPtr();

            for (int j = 0; j < staticsPatchesCount; j++)
            {
                uint32_t blockIndex = difl.ReadUInt32LE();

                STAIDX_BLOCK *sidx = (STAIDX_BLOCK *)difi.Ptr;

                difi.Move(sizeof(STAIDX_BLOCK));

                if (blockIndex < maxBlockCount)
                {
                    size_t realStaticAddress = 0;
                    int realStaticCount = 0;

                    if (sidx->Size > 0 && sidx->Position != 0xFFFFFFFF)
                    {
                        realStaticAddress = startAddress + sidx->Position;
                        realStaticCount = sidx->Size / sizeof(STATICS_BLOCK);

                        if (realStaticCount > 0)
                        {
                            if (realStaticCount > 1024)
                            {
                                realStaticCount = 1024;
                            }
                        }
                    }

                    list[blockIndex].StaticAddress = realStaticAddress;
                    list[blockIndex].StaticCount = realStaticCount;
                }
            }
        }
    }

    UpdatePatched();
}

void CMapManager::UpdatePatched()
{
    DEBUG_TRACE_FUNCTION;
    if (g_Player == nullptr)
    {
        return;
    }

    deque<CRenderWorldObject *> objectsList;

    if (m_Blocks != nullptr)
    {
        QFOR(block, m_Items, CMapBlock *)
        {
            for (int x = 0; x < 8; x++)
            {
                for (int y = 0; y < 8; y++)
                {
                    for (CRenderWorldObject *item = block->GetRender((int)x, (int)y);
                         item != nullptr;
                         item = item->m_NextXY)
                    {
                        if (!item->IsLandObject() && !item->IsStaticObject())
                        {
                            objectsList.push_back(item);
                        }
                    }
                }
            }
        }
    }

    Init(false);

    for (CRenderWorldObject *item : objectsList)
    {
        AddRender(item);
    }

    CGumpMinimap *gump = (CGumpMinimap *)g_GumpManager.UpdateGump(0, 0, GT_MINIMAP);

    if (gump != nullptr)
    {
        gump->LastX = 0;
    }
}

CIndexMap *CMapManager::GetIndex(int map, int blockX, int blockY)
{
    DEBUG_TRACE_FUNCTION;
    if (map >= MAX_MAPS_COUNT)
    {
        return nullptr;
    }

    uint32_t block = (blockX * g_MapBlockSize[map].Height) + blockY;
    MAP_INDEX_LIST &list = m_BlockData[map];

    if (block >= list.size())
    {
        return nullptr;
    }

    return &list[block];
}

void CMapManager::ClearBlockAccess()
{
    memset(&m_BlockAccessList[0], 0, sizeof(m_BlockAccessList));
}

char CMapManager::CalculateNearZ(char defaultZ, int x, int y, int z)
{
    int blockX = x / 8;
    int blockY = y / 8;
    uint32_t index = (blockX * g_MapBlockSize[g_CurrentMap].Height) + blockY;

    bool &accessBlock = m_BlockAccessList[(x & 0x3F) + ((y & 0x3F) << 6)];

    if (accessBlock)
    {
        return defaultZ;
    }

    accessBlock = true;
    CMapBlock *block = GetBlock(index);

    if (block != nullptr)
    {
        CMapObject *item = block->Block[x % 8][y % 8];

        for (; item != nullptr; item = (CMapObject *)item->m_Next)
        {
            if (!item->IsGameObject())
            {
                if (!item->IsStaticObject() && !item->IsMultiObject())
                {
                    continue;
                }
            }
            else if (((CGameObject *)item)->NPC)
            {
                continue;
            }

            if (!item->IsRoof() || abs(z - item->GetZ()) > 6)
            {
                continue;
            }

            break;
        }

        if (item == nullptr)
        {
            return defaultZ;
        }

        char tileZ = item->GetZ();

        if (tileZ < defaultZ)
        {
            defaultZ = tileZ;
        }

        defaultZ = CalculateNearZ(defaultZ, x - 1, y, tileZ);
        defaultZ = CalculateNearZ(defaultZ, x + 1, y, tileZ);
        defaultZ = CalculateNearZ(defaultZ, x, y - 1, tileZ);
        defaultZ = CalculateNearZ(defaultZ, x, y + 1, tileZ);
    }

    return defaultZ;
}

void CMapManager::GetRadarMapBlock(int blockX, int blockY, RADAR_MAP_BLOCK &mb)
{
    DEBUG_TRACE_FUNCTION;
    CIndexMap *indexMap = GetIndex(GetActualMap(), blockX, blockY);

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
            outCell.Z = inCell.Z;
            outCell.IsLand = 1;
        }
    }

    STATICS_BLOCK *sb = (STATICS_BLOCK *)indexMap->StaticAddress;

    if (sb != nullptr)
    {
        int count = indexMap->StaticCount;

        for (int c = 0; c < count; c++)
        {
            if ((sb->Color != 0u) && sb->Color != 0xFFFF &&
                !CRenderStaticObject::IsNoDrawTile(sb->Color))
            {
                RADAR_MAP_CELLS &outCell = mb.Cells[sb->X][sb->Y];

                //int pos = (sb->GetY() * 8) + sb->GetX();
                //if (pos > 64) continue;

                if (outCell.Z <= sb->Z)
                {
                    outCell.Graphic = sb->Color;
                    outCell.Z = sb->Z;
                    outCell.IsLand = 0;
                }
            }

            sb++;
        }
    }
}

void CMapManager::GetMapZ(int x, int y, int &groundZ, int &staticZ)
{
    DEBUG_TRACE_FUNCTION;
    int blockX = x / 8;
    int blockY = y / 8;
    uint32_t index = (blockX * g_MapBlockSize[g_CurrentMap].Height) + blockY;

    if (index < MaxBlockIndex)
    {
        CMapBlock *block = GetBlock(index);

        if (block == nullptr)
        {
            block = AddBlock(index);
            block->X = blockX;
            block->Y = blockY;
            LoadBlock(block);
        }

        CMapObject *item = block->Block[x % 8][y % 8];

        while (item != nullptr)
        {
            if (item->IsLandObject())
            {
                groundZ = item->GetZ();
            }
            else if (staticZ < item->GetZ())
            {
                staticZ = item->GetZ();
            }

            item = (CMapObject *)item->m_Next;
        }
    }
}

void CMapManager::ClearUnusedBlocks()
{
    DEBUG_TRACE_FUNCTION;
    CMapBlock *block = (CMapBlock *)m_Items;
    uint32_t ticks = g_Ticks - CLEAR_TEXTURES_DELAY;
    int count = 0;

    while (block != nullptr)
    {
        CMapBlock *next = (CMapBlock *)block->m_Next;

        if (block->LastAccessTime < ticks && block->HasNoExternalData())
        {
            uint32_t index = block->Index;
            Delete(block);

            m_Blocks[index] = nullptr;

            if (++count >= MAX_MAP_OBJECT_REMOVED_BY_GARBAGE_COLLECTOR)
            {
                break;
            }
        }

        block = next;
    }
}

void CMapManager::ClearUsedBlocks()
{
    DEBUG_TRACE_FUNCTION;
    CMapBlock *block = (CMapBlock *)m_Items;

    while (block != nullptr)
    {
        CMapBlock *next = (CMapBlock *)block->m_Next;

        uint32_t index = block->Index;
        Delete(block);

        m_Blocks[index] = nullptr;

        block = next;
    }
}

void CMapManager::Init(bool delayed)
{
    DEBUG_TRACE_FUNCTION;
    if (g_Player == nullptr)
    {
        return;
    }

    int map = GetActualMap();
    if (!delayed)
    {
        if (m_Blocks != nullptr)
        {
            ClearUsedBlocks();

            delete[] m_Blocks;
            m_Blocks = nullptr;
        }

        MaxBlockIndex = g_MapBlockSize[map].Width * g_MapBlockSize[map].Height;
        m_Blocks = new CMapBlock *[MaxBlockIndex];
        memset(&m_Blocks[0], 0, sizeof(CMapBlock *) * MaxBlockIndex);
        ClearBlockAccess();
        PatchesCount = 0;
        memset(&m_MapPatchCount[0], 0, sizeof(m_MapPatchCount));
        memset(&m_StaticPatchCount[0], 0, sizeof(m_StaticPatchCount));
    }

    const int XY_Offset = 30; //70;
    int minBlockX = (g_Player->GetX() - XY_Offset) / 8 - 1;
    int minBlockY = (g_Player->GetY() - XY_Offset) / 8 - 1;
    int maxBlockX = ((g_Player->GetX() + XY_Offset) / 8) + 1;
    int maxBlockY = ((g_Player->GetY() + XY_Offset) / 8) + 1;

    if (minBlockX < 0)
    {
        minBlockX = 0;
    }

    if (minBlockY < 0)
    {
        minBlockY = 0;
    }

    if (maxBlockX >= g_MapBlockSize[map].Width)
    {
        maxBlockX = g_MapBlockSize[map].Width - 1;
    }

    if (maxBlockY >= g_MapBlockSize[map].Height)
    {
        maxBlockY = g_MapBlockSize[map].Height - 1;
    }

    uint32_t ticks = g_Ticks;
    uint32_t maxDelay = g_FrameDelay[WINDOW_ACTIVE] / 2;
    for (int i = minBlockX; i <= maxBlockX; i++)
    {
        uint32_t index = i * g_MapBlockSize[map].Height;
        for (int j = minBlockY; j <= maxBlockY; j++)
        {
            uint32_t realIndex = index + j;
            if (realIndex < MaxBlockIndex)
            {
                CMapBlock *block = GetBlock(realIndex);
                if (block == nullptr)
                {
                    if (delayed && g_Ticks - ticks >= maxDelay)
                    {
                        return;
                    }

                    block = AddBlock(realIndex);
                    block->X = i;
                    block->Y = j;
                    LoadBlock(block);
                }
            }
        }
    }
}

void CMapManager::LoadBlock(CMapBlock *block)
{
    DEBUG_TRACE_FUNCTION;
    int map = GetActualMap();

    CIndexMap *indexMap = GetIndex(GetActualMap(), block->X, block->Y);

    if (indexMap == nullptr || indexMap->MapAddress == 0)
    {
        return;
    }

    MAP_BLOCK *pmb = (MAP_BLOCK *)indexMap->MapAddress;

    int bx = block->X * 8;
    int by = block->Y * 8;

    for (int x = 0; x < 8; x++)
    {
        for (int y = 0; y < 8; y++)
        {
            int pos = (int)y * 8 + (int)x;
            CMapObject *obj = new CLandObject(
                pos,
                pmb->Cells[pos].TileID & 0x3FFF,
                0,
                bx + (int)x,
                by + (int)y,
                pmb->Cells[pos].Z);
            block->AddObject(obj, (int)x, (int)y);
        }
    }

    STATICS_BLOCK *sb = (STATICS_BLOCK *)indexMap->StaticAddress;

    if (sb != nullptr)
    {
        int count = indexMap->StaticCount;

        for (int c = 0; c < count; c++, sb++)
        {
            if ((sb->Color != 0u) && sb->Color != 0xFFFF)
            {
                int x = sb->X;
                int y = sb->Y;

                int pos = (y * 8) + x;

                if (pos >= 64)
                {
                    continue;
                }

                CRenderStaticObject *obj =
                    new CStaticObject(pos, sb->Color, sb->Hue, bx + x, by + y, sb->Z);

                block->AddObject(obj, x, y);
            }
        }
    }

    block->CreateLandTextureRect();
}

int CMapManager::GetActualMap()
{
    DEBUG_TRACE_FUNCTION;
    if (g_CurrentMap == 1 && (((g_FileManager.m_MapUOP[1].Start == nullptr) &&
                               (g_FileManager.m_MapMul[1].Start == nullptr)) ||
                              (g_FileManager.m_StaticIdx[1].Start == nullptr) ||
                              (g_FileManager.m_StaticMul[1].Start == nullptr)))
    {
        return 0;
    }

    return g_CurrentMap;
}

void CMapManager::AddRender(CRenderWorldObject *item)
{
    DEBUG_TRACE_FUNCTION;
    int itemX = item->GetX();
    int itemY = item->GetY();

    int x = itemX / 8;
    int y = itemY / 8;

    uint32_t index = (x * g_MapBlockSize[g_CurrentMap].Height) + y;

    if (index < MaxBlockIndex)
    {
        CMapBlock *block = GetBlock(index);

        if (block == nullptr)
        {
            block = AddBlock(index);
            block->X = x;
            block->Y = y;
            LoadBlock(block);
        }

        x = itemX % 8;
        y = itemY % 8;

        block->AddRender(item, x, y);
    }
}

CMapBlock *CMapManager::GetBlock(uint32_t index)
{
    DEBUG_TRACE_FUNCTION;
    CMapBlock *block = nullptr;

    if (index < MaxBlockIndex)
    {
        block = m_Blocks[index];

        if (block != nullptr)
        {
            block->LastAccessTime = g_Ticks;
        }
    }

    return block;
}

CMapBlock *CMapManager::AddBlock(uint32_t index)
{
    DEBUG_TRACE_FUNCTION;
    CMapBlock *block = (CMapBlock *)Add(new CMapBlock(index));

    m_Blocks[index] = block;

    return block;
}

void CMapManager::DeleteBlock(uint32_t index)
{
    DEBUG_TRACE_FUNCTION;
    CMapBlock *block = (CMapBlock *)m_Items;

    while (block != nullptr)
    {
        if (block->Index == index)
        {
            Delete(block);
            m_Blocks[index] = nullptr;

            break;
        }

        block = (CMapBlock *)block->m_Next;
    }
}
