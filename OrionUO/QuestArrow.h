/***********************************************************************************
**
** QuestArrow.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef QUESTARROW_H
#define QUESTARROW_H

//Класс для работы со стрелкой задания (над головой персонажа)
class CQuestArrow
{
public:
    uint32_t Timer = 0;
    uint16_t X = 0;
    uint16_t Y = 0;
    bool Enabled = false;

    CQuestArrow();
    ~CQuestArrow();

    //Индекс начальной картинки стрелки
    static const uint16_t m_Gump = 0x1194;

    //Отрисовать стрелку
    void Draw();
};

extern CQuestArrow g_QuestArrow;

#endif
