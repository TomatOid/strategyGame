#pragma once
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct Level
{
    Vector3 size;
    char *tiles;
    Vector3 start_positions[3];
    // TODO: number of enemies and such
} Level;

// remember to free the last level before running again
int loadLevel(Level *level, const char *path)
{
    FILE *level_file = fopen(path, "r");
    if (!level_file) { return 0; }
    
    // first thing to read is the size
    fread(&level->size, sizeof(Vector3), 1, level_file);
    // the number of bytes to copy will be the product of x, y, and z
    size_t next_copy_size = level->size.x * level->size.y * level->size.z;
    level->tiles = calloc(next_copy_size, 1);
    fread(level->tiles, sizeof(char), next_copy_size, level_file);
    // now, copy the start position
    fread(&level->start_positions, sizeof(Vector3), sizeof(level->start_positions) / sizeof(Vector3), level_file);
    return 1;
}

int saveLevel(Level *level, const char *path)
{
    FILE *level_file = fopen(path, "w+");
    if (!level_file) { return 0; }

    // first write the size
    fwrite(&level->size, sizeof(Vector3), 1, level_file);

    fwrite(level->tiles, sizeof(char), level->size.x * level->size.y * level->size.z, level_file);

    fwrite(&level->start_positions, sizeof(Vector3), sizeof(level->start_positions) / sizeof(Vector3), level_file);
    return 1;
}

char getTileAt(Vector3 position, Level *level)
{
    return level->tiles[position.x + position.z * level->size.x + position.y * level->size.x * level->size.z];
}

int setTileAt(char tile, Vector3 position, Level *level)
{
    if (position.x >= 0 && position.x < level->size.x
    && position.y >= 0 && position.y < level->size.y
    && position.z >= 0 && position.z < level->size.z)
    {
        level->tiles[position.x + position.z * level->size.x + position.y * level->size.x * level->size.z] = tile;
        return 1;
    } else return 0;
}