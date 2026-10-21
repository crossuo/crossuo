// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "MapObject.h"
#include <xuocore/mulstruct.h>

class CTextContainer;
struct CTextData;
class CRenderTextObject;

class CRenderStaticObject : public CMapObject
{
public:
    char FoliageTransparentIndex = -1;
    char CanBeTransparent = 0;
    bool Vegetation = false;

    uint16_t RenderGraphic = 0;
    uint16_t RenderColor = 0;

protected:
    // Pointer to the tiledata structure
    MulStaticTile2 *m_TiledataPtr{ nullptr };

public:
    CRenderStaticObject(
        RENDER_OBJECT_TYPE renderType,
        int serial,
        uint16_t graphic,
        uint16_t color,
        short x,
        short y,
        char z);
    virtual ~CRenderStaticObject() override;

    CTextContainer *m_TextControl{ nullptr }; // Reference to the text container

    virtual void UpdateTextCoordinates() override;
    virtual void FixTextCoordinates() override;

    static bool IsNoDrawTile(uint16_t graphic);

    // Draw the object
    virtual void Draw(int x, int y) override;

    // Select the object
    virtual void Select(int x, int y) override;

    // Add text to the container
    virtual void AddText(CTextData *msg) override;

    // Transparency check (for the circle of transparency)
    virtual bool TranparentTest(int playerZPlus5) override;

    // Check whether the text can be transparent
    bool TextCanBeTransparent(CRenderTextObject *text) override;

    MulStaticTile2 *GetStaticData() { return m_TiledataPtr; }

    // Get the object height (from the tiledata)
    uint8_t GetStaticHeight() { return m_TiledataPtr->Height; }

    // Get the light index
    uint16_t GetLightID() override { return m_TiledataPtr->Layer /*LightIndex*/; } //m_TiledataPtr->Layer

    // Standard states derived from the tiledata (verdata) flags
    bool IsBackground() override { return (m_TiledataPtr->Flags & 0x00000001) != 0; }
    bool IsWeapon() override { return (m_TiledataPtr->Flags & 0x00000002) != 0; }
    bool IsTransparent() override { return (m_TiledataPtr->Flags & 0x00000004) != 0; }
    bool IsTranslucent() override { return (m_TiledataPtr->Flags & 0x00000008) != 0; }
    bool IsWall() override { return (m_TiledataPtr->Flags & 0x00000010) != 0; }
    bool IsDamaging() override { return (m_TiledataPtr->Flags & 0x00000020) != 0; }
    bool IsImpassable() override { return (m_TiledataPtr->Flags & 0x00000040) != 0; }
    bool IsWet() override { return (m_TiledataPtr->Flags & 0x00000080) != 0; }
    bool IsUnknown() override { return (m_TiledataPtr->Flags & 0x00000100) != 0; }
    bool IsSurface() override { return (m_TiledataPtr->Flags & 0x00000200) != 0; }
    bool IsBridge() override { return (m_TiledataPtr->Flags & 0x00000400) != 0; }
    bool IsStackable() override { return (m_TiledataPtr->Flags & 0x00000800) != 0; }
    bool IsWindow() override { return (m_TiledataPtr->Flags & 0x00001000) != 0; }
    bool IsNoShoot() override { return (m_TiledataPtr->Flags & 0x00002000) != 0; }
    bool IsPrefixA() override { return (m_TiledataPtr->Flags & 0x00004000) != 0; }
    bool IsPrefixAn() override { return (m_TiledataPtr->Flags & 0x00008000) != 0; }
    bool IsPrefixThe() override { return (m_TiledataPtr->Flags & 0x0000C000) != 0; } // "the" == "a" + "an"
    bool IsInternal() override { return (m_TiledataPtr->Flags & 0x00010000) != 0; }
    bool IsFoliage() override { return (m_TiledataPtr->Flags & 0x00020000) != 0; }
    bool IsPartialHue() override { return (m_TiledataPtr->Flags & 0x00040000) != 0; }
    bool IsUseNewArt() override { return (m_TiledataPtr->Flags & 0x00080000) != 0; }
    bool IsMap() override { return (m_TiledataPtr->Flags & 0x00100000) != 0; }
    bool IsContainer() override { return (m_TiledataPtr->Flags & 0x00200000) != 0; }
    bool IsWearable() override { return (m_TiledataPtr->Flags & 0x00400000) != 0; }
    bool IsLightSource() override { return (m_TiledataPtr->Flags & 0x00800000) != 0; }
    bool IsAnimated() override { return (m_TiledataPtr->Flags & 0x01000000) != 0; }
    // hover over, gargoyles can fly over
    bool IsNoDiagonal() override { return (m_TiledataPtr->Flags & 0x02000000) != 0; }
    bool IsArtUsed() override { return (m_TiledataPtr->Flags & 0x04000000) != 0; }
    bool IsArmor() override { return (m_TiledataPtr->Flags & 0x08000000) != 0; }
    bool IsRoof() override { return (m_TiledataPtr->Flags & 0x10000000) != 0; }
    bool IsDoor() override { return (m_TiledataPtr->Flags & 0x20000000) != 0; }
    bool IsStairBack() override { return (m_TiledataPtr->Flags & 0x40000000) != 0; }
    bool IsStairRight() override { return (m_TiledataPtr->Flags & 0x80000000) != 0; }
    // Flags2
    // NoHouse = 0x01,
    // NoDraw = 0x02,
    // Unused1 = 0x04,
    // AlphaBlend = 0x08,
    // NoShadow = 0x10,
    // PixelBleed = 0x20,
    // Unused2 = 0x40,
    // PlayOnce = 0x80,
    // MultiMovable = 0x0100,

    // This object groups static objects
    bool IsStaticGroupObject() override { return true; }

    virtual CRenderStaticObject *StaticGroupObjectPtr() override { return this; }

    bool CheckDrawFoliage();
    bool CheckDrawVegetation();
};
