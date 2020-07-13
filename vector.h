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

uint64_t hashVector3(Vector3 vec)
{
    uint64_t result = 0;
    result |= (uint64_t)vec.x & 0x00FFFFFF;
    result |= ((uint64_t)vec.z & 0x00FFFFFF) << 24;
    result |= ((uint64_t)vec.y & 0x0000FFFF) << 48;
    return result;
}