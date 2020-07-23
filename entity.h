#pragma once
#include <SDL2/SDL.h>
#include <string.h>
#include <stdint.h>
#include "vector.h"
#include "HashTable.h"
#include "level.h"

Vector3 entityToWorldPosition(Vector3 entity_position)
{
    //int a = (entity_position.x + entity_position.z + TILE_HALF_WIDTH_PX - 1) / TILE_HALF_WIDTH_PX;
    //int b = (entity_position.x - entity_position.z + TILE_HALF_WIDTH_PX - 1) / TILE_HALF_WIDTH_PX;
    //entity_position.x = (a + b - 1) / 2;
    //entity_position.z = (a - b) / 2;
    entity_position.x /= TILE_HALF_WIDTH_PX;
    entity_position.z /= TILE_HALF_WIDTH_PX;
    entity_position.y /= TILE_HEIGHT_PX;
    return entity_position;
}

Vector3 entityToWorldPositionCeil(Vector3 entity_position)
{
    entity_position.x = (entity_position.x + TILE_HALF_WIDTH_PX - 1) / TILE_HALF_WIDTH_PX;
    entity_position.z = (entity_position.z + TILE_HALF_WIDTH_PX - 1) / TILE_HALF_WIDTH_PX;
    entity_position.y = (entity_position.y + TILE_HALF_WIDTH_PX - 1) / TILE_HEIGHT_PX;
    return entity_position;
}

Vector3 worldToEntityPosition(Vector3 world_position)
{
    Vector3 entity_position = world_position;
    entity_position.x *= TILE_HALF_WIDTH_PX;
    entity_position.z *= TILE_HALF_WIDTH_PX;
    entity_position.y *= TILE_HEIGHT_PX;
    return entity_position;
}

enum
{
    ENTITY_EDITOR_CURSOR
};

typedef struct Entity
{
    // entity positions have pixel-level precision
    // to convert to block coordinates, divide x and z by TILE_HALF_WIDTH_PX
    // and divide y by TILE_HEIGHT_PX
    Vector3 position;
    Vector3 size;
    //unsigned int entity_id;
    int type;
    int draw_on_top;
    void *specific_data;
    // interface function pointers
    void (*free_callback)(struct Entity *);
    void (*draw)(struct Entity *, SDL_Renderer *, int, int, SDL_Rect);
} Entity;

int pointIsInPrism(Vector3 prism_least_corner, Vector3 prism_most_corner, Vector3 point)
{
    return (point.x >= prism_least_corner.x) && (point.x <= prism_most_corner.x)
        && (point.y >= prism_least_corner.y) && (point.y <= prism_most_corner.y)
        && (point.z >= prism_least_corner.z) && (point.z <= prism_most_corner.z);
}

void moveEntity(Entity *entity, Vector3 new_position, HashTable *table, Level *level)
{
    // loop through the old prism, find the points that are not in the new prism and remove them
    // then loop through the new prism and find the points that are not in the old prism and add them
    Vector3 old_position_world_floor = entityToWorldPosition(entity->position);
    Vector3 old_position_world_ceil = entityToWorldPosition(addVector3(entity->position, entity->size));
    Vector3 new_position_world_floor = entityToWorldPosition(new_position);
    Vector3 new_position_world_ceil = entityToWorldPosition(addVector3(new_position, entity->size));
    for (int z = old_position_world_floor.z; z <= old_position_world_ceil.z; z++)
    {
        for (int x = old_position_world_floor.x; x <= old_position_world_ceil.x; x++)
        {
            for (int y = old_position_world_floor.y; y <= old_position_world_ceil.y; y++)
            {
                Vector3 old_point = { x, y, z };
                if (!pointIsInPrism(new_position_world_floor, new_position_world_ceil, old_point))
                {
                    clearFlagAt(old_point, level);
                    uint64_t key = hashVector3(old_point);
                    removeFromTableByValue(table, key, entity);
                }
            }
        }
    }
    for (int z = new_position_world_floor.z; z <= new_position_world_ceil.z; z++)
    {
        for (int x = new_position_world_floor.x; x <= new_position_world_ceil.x; x++)
        {
            for (int y = new_position_world_floor.y; y <= new_position_world_ceil.y; y++)
            {
                Vector3 new_point = { x, y, z };
                if (!pointIsInPrism(old_position_world_floor, old_position_world_ceil, new_point))
                {
                    setFlagAt(new_point, level);
                    uint64_t key = hashVector3(new_point);
                    insertToTable(table, key, entity);
                }
            }
        }
    }
    entity->position = new_position;
}

enum
{
    CURSOR_MODE_PLACE_TILE,
    CURSOR_MODE_PLACE_ENTITY
};

typedef struct PlacementCursor
{
    char tile_id;
    int mode;
} PlacementCursor;

typedef struct Tree
{
    int type;
};