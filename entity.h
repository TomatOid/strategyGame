#pragma once
#include <SDL2/SDL.h>
#include <string.h>
#include <stdint.h>
#include "vector.h"
#include "HashTable.h"
#include "level.h"


Vector3 entityToWorldPosition(Vector3 entity_position)
{
    Vector3 world_position = entity_position;
    world_position.x = (world_position.x) / TILE_HALF_WIDTH_PX;
    world_position.z = (world_position.z) / TILE_HALF_WIDTH_PX;
    world_position.y = (world_position.y + TILE_HEIGHT_PX - 1) / TILE_HEIGHT_PX;
    return world_position;
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
    void *specific_data;
    // interface function pointers
    void (*free_callback)(struct Entity *);
    void (*draw)(struct Entity *, SDL_Renderer *, int, int);
} Entity;

void moveEntity(Entity *entity, Vector3 new_position, HashTable *table, Level* level)
{
    Vector3 position_world = entityToWorldPosition(entity->position);
    Vector3 new_position_world = entityToWorldPosition(new_position);
    // check if we need to move to a new cell
    if (memcmp(&position_world, &new_position_world, sizeof(Vector3)) != 0)
    {
        // remove the flag from the old cell
        clearFlagAt(position_world, level);
        // put the flag in the new cell
        setFlagAt(new_position_world, level);
        uint64_t old_key = hashVector3(position_world);
        uint64_t new_key = hashVector3(new_position_world);
        removeFromTableByValue(table, old_key, entity);
        insertToTable(table, new_key, entity);
    } 
    entity->position = new_position;
}

typedef struct PlacementCursor
{
    char tile_id;
    int mode;
} PlacementCursor;

typedef struct Tree
{

};