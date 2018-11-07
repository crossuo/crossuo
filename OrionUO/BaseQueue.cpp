// MIT License
// Copyright (C) August 2016 Hotride

CBaseQueueItem::CBaseQueueItem()
    : CBaseQueue()
    , m_Next(nullptr)
    , m_Prev(nullptr)
{
}

CBaseQueueItem::~CBaseQueueItem()
{
    DEBUG_TRACE_FUNCTION;
    //Принудительная очистка при удалении
    Clear();

    CBaseQueueItem *item = m_Next;
    //while (item != nullptr && item->m_Next != nullptr)
    //	item = item->m_Next;

    while (item != nullptr && item != this)
    {
        CBaseQueueItem *next = item->m_Next;
        item->m_Next = nullptr;
        delete item;
        item = next;
    }

    //Если есть следующий элемент - улдалим его (контейнер очищается/удаляется)
    /*if (m_Next != nullptr)
	{
		delete m_Next;
		m_Next = nullptr;
	}*/
}

CBaseQueue::CBaseQueue()
    : m_Items(nullptr)
{
}

CBaseQueue::~CBaseQueue()
{
    DEBUG_TRACE_FUNCTION;
    //Принудительная очистка при удалении
    Clear();
}

void CBaseQueue::Clear()
{
    DEBUG_TRACE_FUNCTION;
    //Если в контейнере есть элементы - достаточно просто удалить первый, остальные удалятся вместе с ним
    if (m_Items != nullptr)
    {
        CBaseQueueItem *item = m_Items;
        m_Items = nullptr;

        while (item != nullptr)
        {
            CBaseQueueItem *next = item->m_Next;
            item->m_Next = nullptr;
            delete item;
            item = next;
        }
    }
}

CBaseQueueItem *CBaseQueue::Add(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    //Если вставляемый элемент не равен нулю
    if (item != nullptr)
    {
        //Если очередь пуста -  вставим элемент в самое начало очереди
        if (m_Items == nullptr)
            m_Items = item;
        else //Или, найдем последний элемент и запихаем его в зад
        {
            CBaseQueueItem *current = m_Items;

            while (current->m_Next != nullptr)
                current = current->m_Next;

            current->m_Next = item;
            item->m_Prev = current;
        }
    }

    //Вернем вставляемый элемент (для однострочных конструкций типа: item = Container->Add(new TItem());)
    return item;
}

void CBaseQueue::Delete(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    //Если элемент не равен нулю
    if (item != nullptr)
    {
        //Разлинкуем элемент
        Unlink(item);

        //Можно спокойно удалять его
        item->m_Next = nullptr;
        item->m_Prev = nullptr;
        delete item;
    }
}

void CBaseQueue::Delete(int index)
{
    DEBUG_TRACE_FUNCTION;
    //Получим элемент с указанным индексом и удалим его (если есть)
    Delete(Get(index));
}

int CBaseQueue::GetItemIndex(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    int index = 0;

    //Пройдемся по очереди
    BQFOR (current, m_Items)
    {
        //Если элемент нашелся - возвращаем вычисленный индекс
        if (current == item)
            return index;

        index++;
    }

    //В случае не удачного поиска - вернем -1 (не найдено)
    return -1;
}

int CBaseQueue::GetItemsCount()
{
    DEBUG_TRACE_FUNCTION;
    int count = 0;

    //Пройдемся по всем элементам очереди и запомним общее количество
    //Для экономии памяти не выводит это значение в отдельную переменную
    BQFOR (current, m_Items)
        count++;

    return count;
}

CBaseQueueItem *CBaseQueue::Get(int index)
{
    DEBUG_TRACE_FUNCTION;
    CBaseQueueItem *item = m_Items;

    //Пройдемся по всем элементам очереди до нахождения нужного или окончания списка
    for (; item != nullptr && index; item = item->m_Next, index--)
        ;

    return item;
}

void CBaseQueue::Insert(CBaseQueueItem *first, CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    if (first == nullptr)
    {
        item->m_Next = m_Items;
        item->m_Prev = nullptr;

        if (m_Items != nullptr)
            m_Items->m_Prev = item;

        m_Items = item;
    }
    else
    {
        CBaseQueueItem *next = first->m_Next;
        item->m_Next = next;
        item->m_Prev = first;

        first->m_Next = item;

        if (next != nullptr)
            next->m_Prev = item;
    }
}

void CBaseQueue::Unlink(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    //Если элемент не равен нулю
    if (item != nullptr)
    {
        //Если элемент - начало списка
        if (item == m_Items)
        {
            //Скорректируем его
            m_Items = m_Items->m_Next;

            if (m_Items != nullptr)
                m_Items->m_Prev = nullptr;
        }
        else
        {
            //Или подменим указатели предыдущего и следующего (при его наличии) элементов друг на друга
            item->m_Prev->m_Next = item->m_Next;

            if (item->m_Next != nullptr)
                item->m_Next->m_Prev = item->m_Prev;
        }
    }
}

void CBaseQueue::MoveToFront(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    //Если элемент не равен нулю и не равен началу очереди
    if (item != nullptr && item != m_Items)
    {
        //Разлинкуем
        Unlink(item);

        //Перелинкуем с началом очереди
        if (m_Items != nullptr)
            m_Items->m_Prev = item;

        item->m_Next = m_Items;
        item->m_Prev = nullptr;

        //Вставим в начало очереди
        m_Items = item;
    }
}

void CBaseQueue::MoveToBack(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    //Если элемент не равен нулю
    if (item != nullptr)
    {
        //Разлинкуем
        Unlink(item);

        //Получим указатель на конец очереди
        CBaseQueueItem *last = Last();

        //Перелинкуем элемент с последним элементом (или с началом очереди при пустой очереди)
        if (last == nullptr)
            m_Items = item;
        else
            last->m_Next = item;

        item->m_Prev = last;
        item->m_Next = nullptr;
    }
}

bool CBaseQueue::Move(CBaseQueueItem *item, bool up)
{
    DEBUG_TRACE_FUNCTION;
    //Немедленно запишем результат (и исходные данные для первой проверки) в переменную
    bool result = (item != nullptr);

    //Если элемент не равен нулю
    if (result)
    {
        //Перемещение "вверх"
        if (up)
        {
            CBaseQueueItem *prev = item->m_Prev;

            //Если предыдущий элемент не равен нулю (есть куда двигаться)
            result = (prev != nullptr);

            if (result)
            {
                //Предыдущий элемент - начало очереди
                if (prev == m_Items)
                {
                    prev->m_Prev = item;
                    prev->m_Next = item->m_Next;
                    m_Items = item;
                    item->m_Prev = nullptr;
                    item->m_Next = prev;
                }
                else //Где-то в теле очереди
                {
                    CBaseQueueItem *prevprev = prev->m_Prev;
                    prev->m_Prev = item;
                    prev->m_Next = item->m_Next;
                    prevprev->m_Next = item;
                    item->m_Prev = prevprev;
                    item->m_Next = prev;
                }
            }
        }
        else
        {
            CBaseQueueItem *next = item->m_Next;

            //Если следующий элемент не равен нулю (есть куда двигаться)
            result = (next != nullptr);

            if (result)
            {
                //Текущий элемент - начало очереди
                if (item == m_Items)
                {
                    item->m_Next = next->m_Next;
                    item->m_Prev = next;
                    m_Items = item;
                    m_Items->m_Prev = nullptr;
                    m_Items->m_Next = item;
                }
                else //Где-то в теле очереди
                {
                    CBaseQueueItem *prev = item->m_Prev;
                    prev->m_Next = next;
                    item->m_Next = next->m_Next;
                    item->m_Prev = next;
                    next->m_Prev = prev;
                    next->m_Next = item;
                }
            }
        }
    }

    //Если все проверки прошли успешно - элемент перемещен
    return result;
}

CBaseQueueItem *CBaseQueue::Last()
{
    DEBUG_TRACE_FUNCTION;
    //Начинаем поиск с начала очереди
    CBaseQueueItem *last = m_Items;

    //Пройдемся по всем элементам очереди до конца (если очередь не пуста)
    while (last != nullptr && last->m_Next != nullptr)
        last = last->m_Next;

    //Вернем что получилось в результате поиска
    return last;
}
