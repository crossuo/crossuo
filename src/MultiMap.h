// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

// FIXME: move to centralized data code
struct CMultiMap
{
    void LoadMap(class CGumpMap *gump, class CGUIExternalTexture *map);
    bool LoadFacet(class CGumpMap *gump, class CGUIExternalTexture *map, int facet);

    CMultiMap() = default;
    ~CMultiMap() = default;
};

extern CMultiMap g_MultiMap;
