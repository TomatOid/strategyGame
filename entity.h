#pragma once
#include <SDL2/SDL.h>
#include <string.h>
#include <stdint.h>
#include "vector.h"
#include "HashTable.h"
#include "level.h"

// This determines the size of the fractional part of the entity position
#define ENTITY_POSITION_MULTIPLIER 16

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

// Fill screen_x and screen_y with the screen coordinates of the entity position
void entityToScreen(Vector3 entity, int camera_x, int camera_y, int *screen_x, int *screen_y)
{
    *screen_x = (entity.x - entity.z) / ENTITY_POSITION_MULTIPLIER - camera_x;
    *screen_y = (entity.x + entity.z) / (2 * ENTITY_POSITION_MULTIPLIER) - entity.y / ENTITY_POSITION_MULTIPLIER - camera_y;
}

void worldToScreen(Vector3 world, int camera_x, int camera_y, int *screen_x, int *screen_y)
{
    *screen_x = (world.x - world.z) * TILE_HALF_WIDTH_PX - camera_x;
    *screen_y = -world.y * (TILE_HEIGHT_PX) + (world.x + world.z) * TILE_HALF_DEPTH_PX - camera_y;
}

// Convert screen coordinates to world coordinates
// In order to have a unique solution, the world y must be provided
Vector3 screenToWorld(int screen_x, int screen_y, int camera_x, int camera_y, int world_y)
{
    Vector3 world_coords;
    int term_a = (screen_y + TILE_HALF_DEPTH_PX / 2 + camera_y + (world_y) * TILE_HEIGHT_PX) / (TILE_HALF_DEPTH_PX);
    int sign_compensation = (screen_x + camera_x > 0) * TILE_HALF_WIDTH_PX - TILE_HALF_WIDTH_PX / 2;
    int term_b = (screen_x + sign_compensation + camera_x) / (TILE_HALF_WIDTH_PX); 
    world_coords.x = (term_a + term_b - 1) / 2;
    world_coords.y = world_y;
    world_coords.z = (term_a - term_b + 1) / 2;
    return world_coords;
}

Vector3 screenToEntity(int screen_x, int screen_y, int camera_x, int camera_y, int entity_y)
{
    Vector3 entity_coords;
    int term_a = (screen_y + camera_y + entity_y / ENTITY_POSITION_MULTIPLIER) * 2;
    int term_b = (screen_x + camera_x);
    entity_coords.x = ENTITY_POSITION_MULTIPLIER * (term_a + term_b) / 2;
    entity_coords.y = entity_y;
    entity_coords.z = ENTITY_POSITION_MULTIPLIER * (term_a - term_b) / 2;
    return entity_coords;
}

enum
{
    ENTITY_EDITOR_CURSOR
};

typedef struct TextureData
{
    SDL_Texture *amimation_frame;
    SDL_Texture *temporary_frame_buffer;
    SDL_Texture *animation_frame_mask;
    SDL_Rect bounds_rectangle;
    SDL_Rect union_rectangle;
} TextureData;

typedef struct Entity
{
    // entity positions have sub-pixel-level precision
    // to convert to block coordinates, divide x and z by TILE_HALF_WIDTH_PX
    // and divide y by TILE_HEIGHT_PX
    Vector3 position;
    Vector3 size;
    //unsigned int entity_id;
    int draw_on_top;
    int layer;
    int type;
    void *specific_data;
    TextureData *texture_data;
    // interface function pointers
    void (*free_callback)(struct Entity *);
    void (*draw)(struct Entity *, SDL_Renderer *, int, int, SDL_Rect);
} Entity;

int entityLayerCompare(const void *a, const void *b)
{
    Entity *a_e = *(Entity **)a;
    Entity *b_e = *(Entity **)b;
    int a_layer = a_e->layer;
    int b_layer = b_e->layer;
    if (a_layer != b_layer) return a_layer - b_layer;
    //if (a_e->position.y != b_e->position.y) return (a_e->position.y - b_e->position.y);
    //puts("Here");
    return componentSum(addVector3(a_e->position, a_e->size)) - componentSum(addVector3(b_e->position, b_e->size));

    //return -componentSum(addVector3(((Entity *)a)->position, ((Entity *)a)->size)) + componentSum(addVector3(((Entity *)b)->position, ((Entity *)b)->size));
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
    Vector3 world_floor = entityToWorldPosition(position);
    Vector3 world_ceil = entityToWorldPosition(addVector3(position, size));
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
                    uint64_t key = hashVector3(old_point);
                    // check if we are the last entity in the cell
                    if (removeFromTableByValue(table, key, entity) < 2) clearFlagAt(old_point, level);
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
                    assert(insertToTable(table, key, entity));
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
