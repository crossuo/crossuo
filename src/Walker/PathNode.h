// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

struct PATH_POINT
{
    int X;
    int Y;
    int Direction;
};

class CPathObject
{
public:
    uint32_t Flags = 0;
    int Z = 0;
    int AverageZ = 0;
    int Height = 0;

    CPathObject(int flags, int z, int averageZ, int height, class CRenderWorldObject *object)
        : Flags(flags)
        , Z(z)
        , AverageZ(averageZ)
        , Height(height)
        , m_Object(object)
    {
    }
    virtual ~CPathObject() {}

    class CRenderWorldObject *m_Object;
};

class CPathNode
{
public:
    int X = 0;
    int Y = 0;
    int Z = 0;
    int Direction = 0;
    bool Used = false;
    int Cost = 0;
    int DistFromStartCost = 0;
    int DistFromGoalCost = 0;

    CPathNode();
    virtual ~CPathNode();

    CPathNode *m_Parent{ nullptr };

    void Reset();
};
