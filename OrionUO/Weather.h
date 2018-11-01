/***********************************************************************************
**
** Weather.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef WEATHER_H
#define WEATHER_H

//Класс для работы с погодой
class CWeather
{
public:
    //Тип вогоды
    uint8_t Type = 0;
    //Максимальное количество эффектов на экране
    uint8_t Count = 0;
    //Текущее количество эффектов на экране
    uint8_t CurrentCount = 0;
    //Температура
    uint8_t Temperature = 0;
    //Время отрисовки
    uint32_t Timer = 0;
    //Направление ветра
    char Wind = 0;
    //Таймер смены ветра
    uint32_t WindTimer = 0;
    //Время последней смены анимации погоды
    uint32_t LastTick = 0;
    //Скорость общей симуляции зависит от этого значения. Больше - медленней
    float SimulationRatio = 37.0f;

    CWeather();
    ~CWeather() { m_Effects.clear(); }

    //Погодные эффекты
    deque<CWeatherEffect> m_Effects;

    //Выключить погодные эффекты
    void Reset();

    //Сгенерировать эффекты
    void Generate();

    //Отрисовать погодные эффекты
    void Draw(int x, int y);
};

extern CWeather g_Weather;

#endif
