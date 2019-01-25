// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "MapObject.h"
#include "../api/mulstruct.h"

class CTextContainer;
class CTextData;
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
    //Указатель на структуру данных тайлдаты
    STATIC_TILES *m_TiledataPtr{ nullptr };

public:
    CRenderStaticObject(
        RENDER_OBJECT_TYPE renderType,
        int serial,
        uint16_t graphic,
        uint16_t color,
        short x,
        short y,
        char z);
    virtual ~CRenderStaticObject();

    CTextContainer *m_TextControl{ nullptr }; //Ссылка на контейнер для текста

    virtual void UpdateTextCoordinates();
    virtual void FixTextCoordinates();

    static bool IsNoDrawTile(uint16_t graphic);

    //Отрисовать объект
    virtual void Draw(int x, int y);

    //Выбрать объект
    virtual void Select(int x, int y);

    //Добавить текст в контейнер
    virtual void AddText(CTextData *msg);

    //Проверка прозрачности (для круга прозрачности)
    virtual bool TranparentTest(int playerZPlus5);

    //Проверка, может ли текст быть прозрачным
    bool TextCanBeTransparent(CRenderTextObject *text);

    STATIC_TILES *GetStaticData() { return m_TiledataPtr; }

    //Получить высоту объекта (по данным из тайлдаты)
    uint8_t GetStaticHeight() { return m_TiledataPtr->Height; }

    //Получить индекс света
    uint16_t GetLightID() { return m_TiledataPtr->Layer /*LightIndex*/; } //m_TiledataPtr->Layer

    //Стандартные состояния по флагам из тайлдаты (вердаты)
    bool IsBackground() { return (m_TiledataPtr->Flags & 0x00000001) != 0; }
    bool IsWeapon() { return (m_TiledataPtr->Flags & 0x00000002) != 0; }
    bool IsTransparent() { return (m_TiledataPtr->Flags & 0x00000004) != 0; }
    bool IsTranslucent() { return (m_TiledataPtr->Flags & 0x00000008) != 0; }
    bool IsWall() { return (m_TiledataPtr->Flags & 0x00000010) != 0; }
    bool IsDamaging() { return (m_TiledataPtr->Flags & 0x00000020) != 0; }
    bool IsImpassable() { return (m_TiledataPtr->Flags & 0x00000040) != 0; }
    bool IsWet() { return (m_TiledataPtr->Flags & 0x00000080) != 0; }
    bool IsUnknown() { return (m_TiledataPtr->Flags & 0x00000100) != 0; }
    bool IsSurface() { return (m_TiledataPtr->Flags & 0x00000200) != 0; }
    bool IsBridge() { return (m_TiledataPtr->Flags & 0x00000400) != 0; }
    bool IsStackable() { return (m_TiledataPtr->Flags & 0x00000800) != 0; }
    bool IsWindow() { return (m_TiledataPtr->Flags & 0x00001000) != 0; }
    bool IsNoShoot() { return (m_TiledataPtr->Flags & 0x00002000) != 0; }
    bool IsPrefixA() { return (m_TiledataPtr->Flags & 0x00004000) != 0; }
    bool IsPrefixAn() { return (m_TiledataPtr->Flags & 0x00008000) != 0; }
    bool IsInternal() { return (m_TiledataPtr->Flags & 0x00010000) != 0; }
    bool IsFoliage() { return (m_TiledataPtr->Flags & 0x00020000) != 0; }
    bool IsPartialHue() { return (m_TiledataPtr->Flags & 0x00040000) != 0; }
    bool IsUnknown1() { return (m_TiledataPtr->Flags & 0x00080000) != 0; }
    bool IsMap() { return (m_TiledataPtr->Flags & 0x00100000) != 0; }
    bool IsContainer() { return (m_TiledataPtr->Flags & 0x00200000) != 0; }
    bool IsWearable() { return (m_TiledataPtr->Flags & 0x00400000) != 0; }
    bool IsLightSource() { return (m_TiledataPtr->Flags & 0x00800000) != 0; }
    bool IsAnimated() { return (m_TiledataPtr->Flags & 0x01000000) != 0; }
    bool IsNoDiagonal() { return (m_TiledataPtr->Flags & 0x02000000) != 0; }
    bool IsUnknown2() { return (m_TiledataPtr->Flags & 0x04000000) != 0; }
    bool IsArmor() { return (m_TiledataPtr->Flags & 0x08000000) != 0; }
    bool IsRoof() { return (m_TiledataPtr->Flags & 0x10000000) != 0; }
    bool IsDoor() { return (m_TiledataPtr->Flags & 0x20000000) != 0; }
    bool IsStairBack() { return (m_TiledataPtr->Flags & 0x40000000) != 0; }
    bool IsStairRight() { return (m_TiledataPtr->Flags & 0x80000000) != 0; }

    //Это объект группы объектов статики
    bool IsStaticGroupObject() { return true; }

    virtual CRenderStaticObject *StaticGroupObjectPtr() { return this; }

    bool CheckDrawFoliage();
    bool CheckDrawVegetation();
};
