// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

class CVector
{
public:
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;

    CVector(double x = 0, double y = 0, double z = 0);
    ~CVector(){};

    //Копирование векторов
    void Link(const CVector &v);

    //Изменение координат
    void Link(double x, double y, double z);

    //Складывание векторов
    void Add(const CVector &v);

    //Складывание координат
    void Add(double x, double y, double z);

    //Слияние векторов
    void Merge(const CVector &v);

    //Слияние координат
    void Merge(double x, double y, double z);

    //Нормализация
    void Normalize();
};
