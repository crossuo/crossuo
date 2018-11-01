/***********************************************************************************
**
** SkillGroup.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef SKILLGROUP_H
#define SKILLGROUP_H

//Класс группы навыков
class CSkillGroupObject
{
public:
    int Count = 0;
    bool Maximized = false;
    string Name = "No Name";

private:
    //Номера навыков
    uint8_t m_Items[60];

public:
    //Ссылки на следующую и предыдущую группы
    CSkillGroupObject *m_Next{ nullptr };
    CSkillGroupObject *m_Prev{ nullptr };

    CSkillGroupObject();
    ~CSkillGroupObject();

    uint8_t GetItem(intptr_t index);

    //Добавить навык в группу
    void Add(uint8_t index);

    //Добавить навык и отсортировать
    void AddSorted(uint8_t index);

    //Удалить навык
    void Remove(uint8_t index);

    //Проверка, содержит ли группа навык
    bool Contains(uint8_t index);

    //Сортировать навыки
    void Sort();

    //Передать навык другой группе
    void TransferTo(CSkillGroupObject *group);
};

#endif
