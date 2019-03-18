// MIT License
// Copyright (C) August 2016 Hotride

#include "IndexObject.h"
#include "Config.h"

Index g_Index;

// FIXME: Texture should be managed elsewhere
CIndexObject::~CIndexObject()
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != nullptr)
    {
        delete Texture;
        Texture = nullptr;
    }
}

void CIndexObject::ReadIndexFile(size_t address, IndexBlock *ptr)
{
    Address = ptr->Position;
    DataSize = ptr->Size;
    if (Address == 0xFFFFFFFF || (DataSize == 0) || DataSize == 0xFFFFFFFF)
    {
        Address = 0;
        DataSize = 0;
    }
    else
    {
        Address = Address + address;
    }
};

void CIndexMulti::ReadIndexFile(size_t address, IndexBlock *block)
{
    CIndexObject::ReadIndexFile(address, block);
    if (g_Config.ClientVersion >= CV_7090)
    {
        Count = (uint16_t)(DataSize / sizeof(MULTI_BLOCK_NEW));
    }
    else
    {
        Count = (uint16_t)(DataSize / sizeof(MULTI_BLOCK));
    }
};

void CIndexLight::ReadIndexFile(size_t address, IndexBlock *block)
{
    CIndexObject::ReadIndexFile(address, block);
    Width = block->LightData.Width;
    Height = block->LightData.Height;
};

void CIndexGump::ReadIndexFile(size_t address, IndexBlock *block)
{
    CIndexObject::ReadIndexFile(address, block);
    Width = block->LightData.Width;
    Height = block->LightData.Height;
};
