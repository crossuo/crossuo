// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** PathNode.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

//------------------------------------CPathNode-------------------------------------

CPathNode::CPathNode()
{
}

CPathNode::~CPathNode()
{
    m_Parent = nullptr;
}

void CPathNode::Reset()
{
    m_Parent = nullptr;
    Used = false;
    X = Y = Z = Direction = Cost = DistFromStartCost = DistFromGoalCost = 0;
}

