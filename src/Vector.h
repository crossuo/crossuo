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

    // Copying vectors
    void Link(const CVector &v);

    // Changing coordinates
    void Link(double x, double y, double z);

    // Adding vectors
    void Add(const CVector &v);

    // Adding coordinates
    void Add(double x, double y, double z);

    // Merging vectors
    void Merge(const CVector &v);

    // Merging coordinates
    void Merge(double x, double y, double z);

    // Normalization
    void Normalize();
};
