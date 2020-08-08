#pragma once
#include <SDL2/SDL.h>
#include <string.h>
#include <stdint.h>
#include "vector.h"
#include "HashTable.h"
#include "level.h"

// This determines the size of the fractional part of the entity position
#define ENTITY_POSITION_MULTIPLIER 2

Vector3 entityToWorldPosition(Vector3 entity_position)
{
    entity_position.x /= TILE_HALF_WIDTH_PX * ENTITY_POSITION_MULTIPLIER;
    entity_position.z /= TILE_HALF_WIDTH_PX * ENTITY_POSITION_MULTIPLIER;
    entity_position.y /= TILE_HEIGHT_PX * ENTITY_POSITION_MULTIPLIER;
    return entity_position;
}

Vector3 worldToEntityPosition(Vector3 world_position)
{
    Vector3 entity_position = world_position;
    entity_position.x *= TILE_HALF_WIDTH_PX * ENTITY_POSITION_MULTIPLIER;
    entity_position.z *= TILE_HALF_WIDTH_PX * ENTITY_POSITION_MULTIPLIER;
    entity_position.y *= TILE_HEIGHT_PX * ENTITY_POSITION_MULTIPLIER;
    return entity_position;
}

enum
{
    ENTITY_EDITOR_CURSOR
};

typedef struct EntityCoverShadow
{
    SDL_Texture *cover_accumulation;
    SDL_Rect bounding_box;
    SDL_Texture *current_animation_frame;
    SDL_Texture *current_animation_frame_mask;
    SDL_Texture *current_animation_frame_inverted_mask;
} EntityCoverShadow;

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
    int layer;
    void *specific_data;
    EntityCoverShadow *cover_shadow;
    // interface function pointers
    void (*free_callback)(struct Entity *);
    void (*draw)(struct Entity *, SDL_Renderer *, int, int, SDL_Rect);
} Entity;

int entityLayerCompare(const void *a, const void *b)
{
    int a_layer = ((Entity *)a)->layer;
    int b_layer = ((Entity *)b)->layer;
    if (a_layer != b_layer) return a_layer - b_layer;
    return componentSum(((Entity *)a)->position) - componentSum(((Entity *)b)->position);
}

int pointIsInPrism(Vector3 prism_least_corner, Vector3 prism_most_corner, Vector3 point)
{
    return (point.x >= prism_least_corner.x) && (point.x <= prism_most_corner.x)
        && (point.y >= prism_least_corner.y) && (point.y <= prism_most_corner.y)
        && (point.z >= prism_least_corner.z) && (point.z <= prism_most_corner.z);
}

// this does not allocate
void addEntity(Entity *entity, Vector3 position, Vector3 size, HashTable *table, Level *level)
{
    entity->position = position;
    size.x *= ENTITY_POSITION_MULTIPLIER;
    size.y *= ENTITY_POSITION_MULTIPLIER;
    size.z *= ENTITY_POSITION_MULTIPLIER;
    entity->size = size;
    Vector3 world_floor = entityToWorldPosition(entity->position);
    Vector3 world_ceil = entityToWorldPosition(addVector3(entity->position, size));
    for (int z = world_floor.z; z <= world_ceil.z; z++)
    {
        for (int x = world_floor.x; x <= world_ceil.x; x++)
        {
            for (int y = world_floor.y; y <= world_ceil.y; y++)
            {
                Vector3 world = { x, y, z };
                setFlagAt(world, level);
                insertToTable(table, hashVector3(world), entity);
            }
        }
    }
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
} Tree;