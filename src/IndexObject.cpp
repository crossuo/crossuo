﻿// MIT License
// Copyright (C) August 2016 Hotride

#include "IndexObject.h"
#include "Config.h"

CIndexObject::~CIndexObject()
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != nullptr)
    {
        delete Texture;
        Texture = nullptr;
    }
}

void CIndexObject::ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id)
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

    ID = id;
};

void CIndexMulti::ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id)
{
    CIndexObject::ReadIndexFile(address, ptr, id);
    if (g_Config.ClientVersion >= CV_7090)
    {
        Count = (uint16_t)(DataSize / sizeof(MULTI_BLOCK_NEW));
    }
    else
    {
        Count = (uint16_t)(DataSize / sizeof(MULTI_BLOCK));
    }
};

void CIndexLight::ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id)
{
    CIndexObject::ReadIndexFile(address, ptr, id);
    LIGHT_IDX_BLOCK *realPtr = (LIGHT_IDX_BLOCK *)ptr;
    Width = realPtr->Width;
    Height = realPtr->Height;
};

void CIndexGump::ReadIndexFile(size_t address, BASE_IDX_BLOCK *ptr, const uint16_t id)
{
    CIndexObject::ReadIndexFile(address, ptr, id);
    GUMP_IDX_BLOCK *realPtr = (GUMP_IDX_BLOCK *)ptr;
    Width = realPtr->Width;
    Height = realPtr->Height;
};
