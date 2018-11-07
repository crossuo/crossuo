// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CMultiMap
{
public:
    CMultiMap();
    ~CMultiMap();

    void LoadMap(class CGumpMap *gump, class CGUIExternalTexture *map);
    bool LoadFacet(class CGumpMap *gump, class CGUIExternalTexture *map, int facet);
};

extern CMultiMap g_MultiMap;
