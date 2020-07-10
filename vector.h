#pragma once

typedef struct Vector3
{
    int x, y, z;
} Vector3;

Vector3 addVector3(Vector3 a, Vector3 b)
{
    Vector3 result = { a.x + b.x, a.y + b.y, a.z + b.z };
    return result;
}