#pragma once
#include <stdint.h>

typedef struct Vector3
{
    int x, y, z;
} Vector3;

Vector3 addVector3(Vector3 a, Vector3 b)
{
    Vector3 result = { a.x + b.x, a.y + b.y, a.z + b.z };
    return result;
}

Vector3 subtractVector3(Vector3 a, Vector3 b)
{
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}

uint64_t hashVector3(Vector3 vec)
{
    uint64_t result = 0;
    result |=  (uint64_t)vec.x & 0x00FFFFFF;
    result |= ((uint64_t)vec.z & 0x00FFFFFF) << 24;
    result |= ((uint64_t)vec.y) << 48;
    return result;
}

int clamp(int number, int minimum, int maximum)
{
    if (number < minimum)
    {
        return minimum;
    }
    else if (number > maximum)
    {
        return maximum;
    }
    else return number;
}

Vector3 clampVector3(Vector3 vector, Vector3 floor, Vector3 ceiling)
{
    vector.x = clamp(vector.x, floor.x, ceiling.x - 1);
    vector.y = clamp(vector.y, floor.y, ceiling.y - 1);
    vector.z = clamp(vector.z, floor.z, ceiling.z - 1);
    return vector;
}

int componentSum(Vector3 vector)
{
    return vector.x + vector.y + vector.z;
}