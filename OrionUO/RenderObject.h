/***********************************************************************************
**
** RenderObject.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef RENDEROBJECT_H
#define RENDEROBJECT_H

//Класс для работы с объектами рендера
class CRenderObject : public CBaseQueueItem
{
public:
    uint32_t Serial = 0;
    uint16_t Graphic = 0;
    uint16_t Color = 0;

protected:
    int m_X = 0;

public:
    int GetX() { return m_X; };
    void SetX(int val);

protected:
    int m_Y = 0;

public:
    int GetY() { return m_Y; };
    void SetY(int val);
    int RealDrawX = 0;
    int RealDrawY = 0;
    int DrawX = 0;
    int DrawY = 0;
    bool Changed = true;

    CRenderObject(int serial, uint16_t graphic, uint16_t color, short x, short y);
    virtual ~CRenderObject();

    //Подготовка текстур
    virtual void PrepareTextures() {}

    //Отрисовка
    //virtual int Draw(bool &mode) {return 0;}

    virtual void OnMouseEnter() {}
    virtual void OnMouseExit() {}

    virtual bool IsGUI() { return false; }
    virtual bool IsText() { return false; }
    virtual bool IsWorldObject() { return false; }
    virtual bool IsGameObject() { return false; }

    virtual void UpdateRealDrawCoordinates() {}
    virtual void UpdateDrawCoordinates() {}
    virtual void UpdateTextCoordinates() {}
    virtual void FixTextCoordinates() {}
};

#endif
