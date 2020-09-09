#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include "vector.h"
#include "level.h"
#include "entity.h"
#include "HashTable.h"
#include "math_utils.h"
#include "textures_generated.h"

#define MAX_ENTITIES_PER_CELL 64
#define TOP_ENTITIES_PER_LAYER 64
#define SCREEN_GRID_SIZE_PX 70
#define RENDER_TARGET_STACK_MAX 32
#define MAX_ENTITIES 128

void *entity_search_results[MAX_ENTITIES_PER_CELL];
Entity *top_entity_array[TOP_ENTITIES_PER_LAYER];
SDL_Rect top_clipping_rectangle_array[TOP_ENTITIES_PER_LAYER];
TextureData entity_texture_data[MAX_ENTITIES] = { 0 };
SDL_Texture *render_target_stack[RENDER_TARGET_STACK_MAX];
size_t render_target_top = 0;
size_t entity_texture_data_count = 0;
uint64_t *screen_grid;
size_t screen_grid_width, screen_grid_height;
int texture_width, texture_height;
extern HashTable entity_by_location;

void pushRenderTarget(SDL_Renderer *renderer, SDL_Texture *target)
{
    assert(render_target_top < RENDER_TARGET_STACK_MAX);
    render_target_stack[render_target_top++] = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, target);
}

void popRenderTarget(SDL_Renderer *renderer)
{
    assert(render_target_top > 0);
    SDL_SetRenderTarget(renderer, render_target_stack[--render_target_top]);
}

int doOverlapTesting(SDL_Rect screen_rectangle)
{
    int min_x = clamp(screen_rectangle.x / SCREEN_GRID_SIZE_PX, 0, screen_grid_width - 1);
    int max_x = clamp((screen_rectangle.x + screen_rectangle.w) / SCREEN_GRID_SIZE_PX, 0, screen_grid_width - 1);
    int min_y = clamp(screen_rectangle.y / SCREEN_GRID_SIZE_PX, 0, screen_grid_height - 1);
    int max_y = clamp((screen_rectangle.y + screen_rectangle.h) / SCREEN_GRID_SIZE_PX, 0, screen_grid_height - 1);
    uint64_t checks = 0;
    for (int x = min_x; x <= max_x; x++)
    {
        for (int y = min_y; y <= max_y; y++)
        {
            checks |= screen_grid[x + y * screen_grid_width];
        }
    }
    if (checks)
    {
        for (int i = 0; i < 64; i++)
        {
            if ((checks >> i) & 1)
            {
                for (int j = 0; j < (MAX_ENTITIES + 63) / 64; j++)
                {
                    SDL_Rect *union_rect = &entity_texture_data[i * ((MAX_ENTITIES + 63) / 64) + j].union_rectangle;
                    SDL_Rect intersect_rect = rectangleIntersect(entity_texture_data[i * ((MAX_ENTITIES + 63) / 64) + j].bounds_rectangle, screen_rectangle);
                    *union_rect = (intersect_rect.w * intersect_rect.h > union_rect->w * union_rect->h) ? intersect_rect : *union_rect;
                }
            }
        }
        return 1;
    } else return 0;
}

void drawEditorCursor(Entity *cursor_entity, SDL_Renderer *renderer, int camera_x, int camera_y, SDL_Rect clipping_rectangle)
{
    clipping_rectangle.y++;
    clipping_rectangle.h--;
    char tile = ((PlacementCursor *)cursor_entity->specific_data)->tile_id;
    int screen_x, screen_y;
    entityToScreen(cursor_entity->position, camera_x, camera_y, &screen_x, &screen_y);
    if (tile_textures[tile])
    {
        int texture_width, texture_height;
        SDL_QueryTexture(tile_textures[tile], NULL, NULL, &texture_width, &texture_height);
        // To keep from spoiling the 3d effect, we only want to draw the texture in the intersection
        // between the tile's corresponding rectangle and the entities texture bounds
        int intersection_min_x = -min(-clipping_rectangle.x, -screen_x);
        int intersection_max_x = min(clipping_rectangle.x + clipping_rectangle.w, screen_x + texture_width);
        int intersection_min_y = -min(-clipping_rectangle.y, -screen_y);
        int intersection_max_y = min(clipping_rectangle.y + clipping_rectangle.h, screen_y + texture_height);
        SDL_Rect src_rect = { intersection_min_x - screen_x, intersection_min_y - screen_y,
            intersection_max_x - intersection_min_x, intersection_max_y - intersection_min_y };
        SDL_Rect dest_rect = { intersection_min_x, intersection_min_y, intersection_max_x - intersection_min_x,
            intersection_max_y - intersection_min_y };
        SDL_RenderCopy(renderer, tile_textures[tile], &src_rect, &dest_rect);
        cursor_entity->texture_data->amimation_frame = tile_textures[tile];
        cursor_entity->texture_data->animation_frame_mask = tile_mask_textures[tile];
        SDL_Rect bounds = { screen_x, screen_y, texture_width, texture_height };
        cursor_entity->texture_data->bounds_rectangle = bounds;
        cursor_entity->texture_data->union_rectangle = bounds;
        cursor_entity->texture_data->union_rectangle.w = 0;
        cursor_entity->texture_data->union_rectangle.h = 0;

        int min_x = clamp(bounds.x / SCREEN_GRID_SIZE_PX, 0, screen_grid_width - 1);
        int max_x = clamp((bounds.x + bounds.w) / SCREEN_GRID_SIZE_PX, 0, screen_grid_width - 1);
        int min_y = clamp(bounds.y / SCREEN_GRID_SIZE_PX, 0, screen_grid_height - 1);
        int max_y = clamp((bounds.y + bounds.h) / SCREEN_GRID_SIZE_PX, 0, screen_grid_height - 1);
        uint64_t index_bitflag = 1 << ((cursor_entity->texture_data - entity_texture_data) / (sizeof(TextureData) * ((MAX_ENTITIES + 63) / 64)));
        //printf("%lu flag\n", index_bitflag);
        for (int x = min_x; x <= max_x; x++)
        {
            for (int y = min_y; y <= max_y; y++)
            {
                screen_grid[x + y * screen_grid_width] |= index_bitflag;
            }
        }
    }
}

// TODO remove the window_rect argument
void drawLevel(SDL_Renderer *main_renderer, Level current_level, SDL_Texture *game_window_texture, int camera_position_x, int camera_position_y)
{
    // Find the game window's bounds
    SDL_Rect window_rect;
    {
        int window_width, window_height;
        SDL_QueryTexture(game_window_texture, NULL, NULL, &window_width, &window_height);
        window_rect = (SDL_Rect) { 0, 0, window_width, window_height };
    }

    // Find the world coordinates of the four corners of the screen so that we only draw what we need
    Vector3 camera_world_top_left = clampVector3(screenToWorld(0, -2 * TILE_HALF_DEPTH_PX - TILE_HEIGHT_PX, camera_position_x, camera_position_y, 0), (Vector3) { 0, 0, 0 }, current_level.size);
    Vector3 camera_world_top_right = clampVector3(screenToWorld(window_rect.w, -2 * TILE_HALF_DEPTH_PX - TILE_HEIGHT_PX, camera_position_x, camera_position_y, 0), (Vector3) { 0, 0, 0 }, current_level.size);
    Vector3 camera_world_bottom_left = clampVector3(screenToWorld(0, window_rect.h, camera_position_x, camera_position_y, current_level.size.y - 1), (Vector3) { 0, 0, 0 }, current_level.size);
    Vector3 camera_world_bottom_right = clampVector3(screenToWorld(window_rect.w, window_rect.h, camera_position_x, camera_position_y, current_level.size.y - 1), (Vector3) { 0, 0, 0 }, current_level.size);
    
    int a_min = clamp(camera_world_top_left.x + camera_world_top_left.y + camera_world_top_right.z, 0, 
        (current_level.size.x + current_level.size.y + current_level.size.z - 3));
    int a_max = clamp(camera_world_bottom_right.x + camera_world_bottom_right.y + camera_world_bottom_left.z, 0, 
        (current_level.size.x + current_level.size.y + current_level.size.z - 3));

    SDL_Rect source_rectangle = { 0, 0, texture_width, texture_height };

    // *** Drawing Code ***
    // It is critical that everything is drawn in the correct order.
    // We are drawing in "q-bert layers", where the components of the
    // coordinates each tile in each layer add up to 'a'. 
    // They remind me of the background in q-bert, hence the name.
    pushRenderTarget(main_renderer, game_window_texture);
    SDL_RenderClear(main_renderer);
    for (int a = a_min; a <= a_max; a++)
    {
        int top_entities_index = 0;
        int b_max = min(a, current_level.size.y - 1);
        for (int b = 0; b <= b_max; b++)
        {
            int c_max = min(a - b, camera_world_bottom_right.x - 1);
            for (int c = -min(-camera_world_top_left.x, -(a - camera_world_bottom_left.z - b + 1)); c <= c_max; c++)
            {
                Vector3 world = { c, b, a - b - c };
                char current_tile = getTileAtUnsafe(world, &current_level);
                int cell_has_entity = current_tile & CELL_HAS_ENTITY_FLAG;
                if (cell_has_entity)
                {
                    uint64_t key = hashVector3(world);
                    int screen_x, screen_y;
                    worldToScreen(world, camera_position_x, camera_position_y, &screen_x, &screen_y);
                    SDL_Rect clipping_rect = { screen_x, screen_y, texture_width, texture_height };
                    size_t return_count = findAllInTable(&entity_by_location, key, entity_search_results, MAX_ENTITIES_PER_CELL);
                    // If multiple entities are in the same cell, we want it to make sense to the eye
                    // So we sort first by the entities' layer value, then if that is the same, by the
                    // sum of their entity space x y and z components
                    if (return_count > 1) { qsort(entity_search_results, return_count, sizeof(Entity *), entityLayerCompare); printf("sorting %lu elements, key %lu\n", return_count, key); }
                    
                    for (size_t i = 0; i < return_count; i++)
                    {   
                        Entity *cell_entity = entity_search_results[i];
                        // Some entities need to be drawn on top of tiles, so we will save them for later
                        if (cell_entity->draw_on_top && top_entities_index < TOP_ENTITIES_PER_LAYER)
                        {
                            top_entity_array[top_entities_index] = cell_entity;
                            top_clipping_rectangle_array[top_entities_index++] = clipping_rect;
                        }
                        else if (cell_entity->draw) cell_entity->draw(cell_entity, main_renderer, camera_position_x, camera_position_y, clipping_rect);
                    }
                }
            }
        }
        
        for (int b = 0; b <= b_max; b++)
        {
            int c_max = min(a - b, camera_world_bottom_right.x - 1);
            for (int c = -min(-camera_world_top_left.x, -(a - camera_world_bottom_left.z - b + 1)); c <= c_max; c++)
            {
                Vector3 world = { c, b, a - b - c };
                char current_tile = getTileAtUnsafe(world, &current_level);
                int screen_x, screen_y;
                worldToScreen(world, camera_position_x, camera_position_y, &screen_x, &screen_y);
                current_tile &= (char)~CELL_HAS_ENTITY_FLAG;
                if (current_tile && tile_textures[current_tile])
                {
                    // calculate the position at which to draw it
                    SDL_Rect destination_rectangle = { screen_x, screen_y, source_rectangle.w, source_rectangle.h};
                    SDL_RenderCopy(main_renderer, tile_textures[current_tile], NULL, &destination_rectangle);
                    destination_rectangle.y += TILE_HALF_DEPTH_PX;
                    destination_rectangle.h -= TILE_HALF_DEPTH_PX;
                    doOverlapTesting(destination_rectangle);
                }
            }
        }
        // loop through and draw the entities that are meant to be drawn last on this q-bert layer
        for (int i = 0; i < top_entities_index; i++)
        {
            if (top_entity_array[i]->draw) 
            {
                top_entity_array[i]->draw(top_entity_array[i], main_renderer, 
                    camera_position_x, camera_position_y, top_clipping_rectangle_array[i]);
            }
        }
    }

    for (int i = 0; i < entity_texture_data_count; i++)
    {
        if (entity_texture_data[i].animation_frame_mask)
        {
            SDL_Rect union_rect = entity_texture_data[i].union_rectangle;
            // Set the cover shadow's alpha based on how much its corresponding entity is covered up
            SDL_SetTextureAlphaMod(entity_texture_data[i].animation_frame_mask, min(194 * (float)(union_rect.w * union_rect.h) / (float)(entity_texture_data[i].bounds_rectangle.w * entity_texture_data[i].bounds_rectangle.h), 64));
            SDL_RenderCopy(main_renderer, entity_texture_data[i].animation_frame_mask, NULL, &entity_texture_data[i].bounds_rectangle);
            SDL_SetTextureAlphaMod(entity_texture_data[i].animation_frame_mask, SDL_ALPHA_OPAQUE);
        }
    }

    popRenderTarget(main_renderer);
}