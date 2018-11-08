// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

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
    uint8_t GetFlags() { return m_Flags; };
    void SetFlags(uint8_t val);

protected:
    string m_Name = "";

public:
    string GetName() { return m_Name; };
    void SetName(const string &newName);
    bool NPC = false;
    bool Clicked = false;
    char AnimIndex = 0;
    string JournalPrefix = "";
    uint32_t LastAnimationChangeTime = 0;
    bool SA_Poisoned = false;
    bool ClosedObjectHandle = false;
    bool pvpCaller = false;

protected:
    class CGameEffect *m_Effects{ nullptr };
    CGLTexture m_TextureObjectHalndes{ CGLTexture() };

public:
    CGameObject(int serial = 0);
    virtual ~CGameObject();

    DRAW_FRAME_INFORMATION m_FrameInfo;
    vector<class CGameItem *> m_DrawLayeredObjects;

    virtual void AddText(CTextData *msg);
    void GenerateObjectHandlesTexture(wstring text);
    void DrawObjectHandlesTexture();
    void SelectObjectHandlesTexture();
    virtual uint16_t GetMountAnimation();
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
    bool Frozen() { return (m_Flags & 0x01); }
    bool Poisoned();
    bool Flying();
    bool Caller();
    bool YellowHits() { return (m_Flags & 0x08); }
    bool IgnoreCharacters() { return (m_Flags & 0x10); }
    bool Locked() { return (!(m_Flags & 0x20) && m_TiledataPtr->Weight > 90); }
    bool InWarMode() { return (m_Flags & 0x40); }
    bool Hidden() { return (m_Flags & 0x80); }
    virtual bool IsHuman() { return false; }
    virtual bool IsPlayer() { return false; }
    static int IsGold(uint16_t graphic);
    uint16_t GetDrawGraphic(bool &doubleDraw);
    bool IsGameObject() { return true; }
    bool IsCorpse() { return (Graphic == 0x2006); }
    CGameObject *GetTopObject();
    CGameItem *FindLayer(int layer);
    virtual CGameItem *FindSecureTradeBox() { return nullptr; }
};
