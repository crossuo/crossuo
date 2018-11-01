/***********************************************************************************
**
** Container.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef CONTAINER_H
#define CONTAINER_H

//!Структура данных с информацией о доступных координатах дропа предметов в гампы контейнеров
class CContainerOffsetRect
{
public:
    int MinX = 0;
    int MinY = 0;
    int MaxX = 0;
    int MaxY = 0;

    CContainerOffsetRect() {}
    CContainerOffsetRect(int minX, int minY, int maxX, int maxY)
        : MinX(minX)
        , MinY(minY)
        , MaxX(maxX)
        , MaxY(maxY)
    {
    }
    ~CContainerOffsetRect() {}
};

//!Информация о гампе и звуках контейнеров
class CContainerOffset
{
public:
    //!Индекс картинки гампа
    uint16_t Gump = 0;

    //!Индекс звука открытия (0 - нет звука)
    uint16_t OpenSound = 0;

    //!Индекс звука закрытия (0 - нет звука)
    uint16_t CloseSound = 0;

    //!Координаты в гампе для дропа
    CContainerOffsetRect Rect = CContainerOffsetRect();

    CContainerOffset() {}
    CContainerOffset(
        uint16_t gump, uint16_t openSound, uint16_t closeSound, const CContainerOffsetRect &rect)
        : Gump(gump)
        , OpenSound(openSound)
        , CloseSound(closeSound)
        , Rect(rect)
    {
    }
    ~CContainerOffset() {}
};

//!Класс для вычисления смещения открытия гампов контейнеров
class CContainerRect
{
public:
    short DefaultX = 40;
    short DefaultY = 40;

    short X = 40;
    short Y = 40;

    CContainerRect() {}
    ~CContainerRect() {}

    /*!
	Функция вычисления смещения для текущей картинки
	@param [__in] gumpID Индекс картинки
	@return 
	*/
    void Calculate(uint16_t gumpID);

    /*!
	Выставим параметры по-умолчанию
	@return 
	*/
    void MakeDefault()
    {
        X = DefaultX;
        Y = DefaultY;
    }
};

//!Данные о контейнерах
extern vector<CContainerOffset> g_ContainerOffset;

//!Глобальное смещение открытия контейнеров
extern CContainerRect g_ContainerRect;

#endif
