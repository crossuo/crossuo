// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <stdint.h>
#include <common/str.h>
#include "../Config.h"
#include "RenderStaticObject.h"
#include "GLEngine/GLTexture.h"

class CGameEffect;

struct DRAW_FRAME_INFORMATION
{
    int OffsetX;
    int OffsetY;
    int Width;
    int Height;
};

class CGameObject : public CRenderStaticObject
{
public:
    uint32_t Container = 0xFFFFFFFF;
    uint8_t MapIndex = 0;
    uint32_t Count = 0;

protected:
    uint8_t m_Flags = 0;

public:
    uint8_t GetFlags() const { return m_Flags; };
    void SetFlags(uint8_t val);

protected:
    astr_t m_Name;

public:
    astr_t GetName() { return m_Name; };
    void SetName(const astr_t &newName);
    bool NPC = false;
    bool Clicked = false;
    char AnimIndex = 0;
    astr_t JournalPrefix;
    uint32_t LastAnimationChangeTime = 0;
    bool SA_Poisoned = false;
    bool ClosedObjectHandle = false;
    bool pvpCaller = false;

protected:
    CGameEffect *m_Effects = nullptr;
    CGLTexture m_TextureObjectHandles; // FIXME: gfx

public:
    CGameObject(int serial = 0);
    virtual ~CGameObject();

    DRAW_FRAME_INFORMATION m_FrameInfo;
    std::vector<class CGameItem *> m_DrawLayeredObjects;

    virtual void AddText(CTextData *msg);
    void GenerateObjectHandlesTexture(wstr_t text);
    void DrawObjectHandlesTexture();
    void SelectObjectHandlesTexture();
    virtual uint16_t GetGraphicForAnimation();
    virtual void OnGraphicChange(int direction = 0) {}
    virtual bool TranparentTest(int playerZ) { return false; }
    void DrawEffects(int x, int y);
    void UpdateEffects();
    void AddEffect(class CGameEffect *effect);
    void RemoveEffect(class CGameEffect *effect);
    void AddObject(CGameObject *obj);
    void AddItem(CGameObject *obj);
    void Reject(CGameObject *obj);
    void Clear();
    void ClearUnequipped();
    void ClearNotOpenedItems();

    // Object Flags
    // RunUO:
    // - https://github.com/runuo/runuo/blob/d715573172fc432a673825b0136444bdab7863b5/Server/Mobile.cs#L7832
    // - https://github.com/runuo/runuo/blob/d715573172fc432a673825b0136444bdab7863b5/Server/Item.cs#L1867
    // - https://github.com/runuo/runuo/blob/d715573172fc432a673825b0136444bdab7863b5/Scripts/Mobiles/PlayerMobile.cs#L676
    // POL: ?
    // Sphere:
    // https://github.com/Sphereserver/Source-experimental/blob/c8ddf528c803fe70b7593c5acb121f1829b22d16/src/game/chars/CCharStatus.cpp#L618
    inline bool IsParalyzed() const { return (m_Flags & 0x01) != 0; }
    // 0x02 = Female

    inline bool IsPoisoned() const
    {
        if (g_Config.ClientVersion >= CV_7000)
        {
            return SA_Poisoned;
        }
        return (m_Flags & 0x04) != 0;
    }

    inline bool IsFlying() const
    {
        if (g_Config.ClientVersion >= CV_7000)
        {
            return (m_Flags & 0x04) != 0;
        }
        return false;
    }

    // aka IsMouseControl
    inline bool IsDrivingBoat() const
    {
        const auto it = (CGameObject *)FindLayer(OL_MOUNT);
        return (it && it->Graphic == 0x3E96);
    }

    inline bool IsMounted() const
    {
        const auto it = (CGameObject *)FindLayer(OL_MOUNT);
        return (it && it->GetGraphicForAnimation() != 0xffff && !IsDrivingBoat());
    }

    bool YellowHits() { return (m_Flags & 0x08) != 0; }
    bool IgnoreCharacters() { return (m_Flags & 0x10) != 0; }
    bool Locked() { return (!(m_Flags & 0x20) && m_TiledataPtr->Weight > 90); }
    bool InWarMode() { return (m_Flags & 0x40) != 0; }
    bool Hidden() { return (m_Flags & 0x80) != 0; }

    bool Caller();
    virtual bool IsHuman() const { return false; }
    virtual bool IsPlayer() { return false; }
    static int IsGold(uint16_t graphic);
    uint16_t GetDrawGraphic(bool &doubleDraw);
    bool IsGameObject() const { return true; }
    bool IsCorpse() const { return (Graphic == 0x2006); }
    CGameObject *GetTopObject();
    CGameItem *FindLayer(int layer) const;
    virtual CGameItem *FindSecureTradeBox() { return nullptr; }
};
