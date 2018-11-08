// MIT License
// Copyright (C) August 2016 Hotride

CVector::CVector(double x, double y, double z)
    : X(x)
    , Y(y)
    , Z(z)
{
}

void CVector::Link(const CVector &v)
{
    X = v.X;
    Y = v.Y;
    Z = v.Z;
}

void CVector::Link(double x, double y, double z)
{
    X = x;
    Y = y;
    Z = z;
}

void CVector::Add(const CVector &v)
{
    X += v.X;
    Y += v.Y;
    Z += v.Z;
}

void CVector::Add(double x, double y, double z)
{
    X += x;
    Y += y;
    Z += z;
}

void CVector::Merge(const CVector &v)
{
    double newX = Y * v.Z - Z * v.Y;
    double newY = Z * v.X - X * v.Z;
    double newZ = X * v.Y - Y * v.X;

    X = newX;
    Y = newY;
    Z = newZ;
}

void CVector::Merge(double x, double y, double z)
{
    double newX = Y * z - Z * y;
    double newY = Z * x - X * z;
    double newZ = X * y - Y * x;

    X = newX;
    Y = newY;
    Z = newZ;
}

void CVector::Normalize()
{
    DEBUG_TRACE_FUNCTION;
    if ((X == 0.0) && (Y == 0.0) && (Z == 0.0))
    {
        return;
    }

    double result = X * X + Y * Y + Z * Z;

    //if (result)
    {
        result = sqrtl(result);

        X /= result;
        Y /= result;
        Z /= result;
    }
}
