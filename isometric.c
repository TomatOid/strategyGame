#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "vector.h"
#include "level.h"
#include "textures_generated.h"
#include "HashTable.h"
#include "entity.h"

#define SCROLL_COOLDOWN 100
#define FRAME_MILISECONDS 20

Vector3 camera_position; // the center of the viewport
int camera_width = 512; 
int camera_height = 512;
int render_scale = 2;
int on_screen_tiles = 16;

int min(int a, int b)
{
    return (a < b) ? a : b;
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

typedef struct 
{
    uint32_t last_scroll;
    unsigned int increase_level : 1;
    unsigned int decrease_level : 1;
    unsigned int pan : 1;
} Inputs;

int editor_selected_tile = AIR_TILE;

void entityToScreen(Vector3 entity, int camera_x, int camera_y, int *screen_x, int *screen_y)
{
    *screen_x = (entity.x - entity.z) - camera_x;
    *screen_y = (entity.x + entity.z) / 2 - entity.y - camera_y;
}

void worldToScreen(Vector3 world, int camera_x, int camera_y, int *screen_x, int *screen_y)
{
    *screen_x = (world.x - world.z) * TILE_HALF_WIDTH_PX - camera_x;
    *screen_y = -world.y * (TILE_HEIGHT_PX) + (world.x + world.z) * TILE_HALF_DEPTH_PX - camera_y;
}

Vector3 screenToWorld(int screen_x, int screen_y, int camera_x, int camera_y, int world_y)
{
    // we need to assume a value for y so the equation has a unique solution
    Vector3 world_coords;
    int term_a = (screen_y + TILE_HALF_DEPTH_PX / 2 + camera_y + (world_y) * TILE_HEIGHT_PX) / (TILE_HALF_DEPTH_PX);
    int term_b = (screen_x + camera_x > 0) ? (screen_x + TILE_HALF_WIDTH_PX / 2 + camera_x) / (TILE_HALF_WIDTH_PX) : (screen_x - TILE_HALF_WIDTH_PX / 2 + camera_x) / (TILE_HALF_WIDTH_PX);
    world_coords.x = (term_a + term_b - 1) / 2;
    world_coords.y = world_y;
    world_coords.z = (term_a - term_b + 1) / 2;
    return world_coords;
}

Vector3 screenToEntity(int screen_x, int screen_y, int camera_x, int camera_y, int entity_y)
{
    Vector3 entity_coords;
    int term_a = (screen_y + camera_y + entity_y) * 2;
    int term_b = (screen_x + camera_x);
    entity_coords.x = (term_a + term_b) / 2;
    entity_coords.y = entity_y;
    entity_coords.z = (term_a - term_b) / 2;
    return entity_coords;
}

void drawEditorCursor(Entity *cursor_entity, SDL_Renderer *renderer, int camera_x, int camera_y, SDL_Rect clipping_rectangle)
{
    char tile = ((PlacementCursor *)cursor_entity->specific_data)->tile_id;
    Vector3 world_position = entityToWorldPosition(cursor_entity->position);
    int screen_x, screen_y;
    //worldToScreen(world_position, camera_x, camera_y, &screen_x, &screen_y);
    entityToScreen(cursor_entity->position, camera_x, camera_y, &screen_x, &screen_y);
    if (tile_textures[tile])
    {
        //SDL_SetTextureAlphaMod(tile_textures[tile], 128);
        int texture_width, texture_height;
        SDL_QueryTexture(tile_textures[tile], NULL, NULL, &texture_width, &texture_height);
        int intersection_min_x = -min(-clipping_rectangle.x, -screen_x);
        int intersection_max_x = min(clipping_rectangle.x + clipping_rectangle.w, screen_x + texture_width);
        int intersection_min_y = -min(-clipping_rectangle.y, -screen_y);
        int intersection_max_y = min(clipping_rectangle.y + clipping_rectangle.h, screen_y + texture_height);
        SDL_Rect src_rect = { intersection_min_x - screen_x, intersection_min_y - screen_y, intersection_max_x - intersection_min_x, intersection_max_y - intersection_min_y };
        SDL_Rect dest_rect = { intersection_min_x, intersection_min_y, intersection_max_x - intersection_min_x, intersection_max_y - intersection_min_y };
        SDL_RenderCopy(renderer, tile_textures[tile], &src_rect, &dest_rect);
        //SDL_SetTextureAlphaMod(tile_textures[tile], 255);
    }
}


uint32_t start_time;

int main()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window *main_window;
    SDL_Renderer *main_renderer;
    SDL_CreateWindowAndRenderer(1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED, &main_window, &main_renderer);
    loadAllTextures(main_renderer);
    // Now initialize a level
    Level current_level;
    if (!loadLevel(&current_level, "level0"))
    {
        // if the file does not exist, create a blank level
        current_level.size.x = 32;
        current_level.size.y = 4;
        current_level.size.z = 32;
        current_level.tiles = calloc(32 * 4 * 32, 1);
    }
    // Initialize the hash table
    HashTable entity_by_location;
    entity_by_location.len = 100;
    entity_by_location.items = calloc(entity_by_location.len, sizeof(HashItem *));
    makePage(&entity_by_location.page, entity_by_location.len, sizeof(HashItem));
    // Setup input stuff
    int mouse_x, mouse_y, last_mouse_x, last_mouse_y;
    int placement_world_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    Inputs user_input = { 0 };
    Inputs last_user_input = { 0 };
    // More SDL graphics stuff
    SDL_DisplayMode display_mode;
    SDL_GetDesktopDisplayMode(0, &display_mode);
    SDL_Rect window_rect = { 0, 0, display_mode.w, display_mode.h };
    {
        int maximum_dimension = (display_mode.w > display_mode.h) ? display_mode.w : display_mode.h;
        render_scale = maximum_dimension / (TILE_HALF_WIDTH_PX * on_screen_tiles);
    }
    SDL_RenderSetScale(main_renderer, render_scale, render_scale);
    // We will use these values later when drawing
    int texture_width, texture_height;
    SDL_QueryTexture(tile_textures[GRASS_TILE], NULL, NULL, &texture_width, &texture_height);
    // Initialize the entities for the editor mode
    PlacementCursor editor_cursor = { AIR_TILE };
    Entity editor_cursor_entity = { 0 };
    {
        Vector3 size = { TILE_HALF_WIDTH_PX - 1, TILE_HEIGHT_PX - 1, TILE_HALF_WIDTH_PX - 1 };
        editor_cursor_entity.size = size;
    }
    editor_cursor_entity.type = ENTITY_EDITOR_CURSOR;
    editor_cursor_entity.draw_on_top = 1;
    editor_cursor_entity.draw = drawEditorCursor;
    editor_cursor_entity.specific_data = &editor_cursor;
    insertToTable(&entity_by_location, 0, &editor_cursor_entity);
    SDL_Texture *top_texture = SDL_CreateTexture(main_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, window_rect.w, window_rect.h);
    SDL_SetTextureBlendMode(top_texture, SDL_BLENDMODE_BLEND);
    //SDL_SetRenderDrawBlendMode(main_renderer, SDL_BLENDMODE_BLEND);
    for (;;)
    {
        start_time = SDL_GetTicks();
        last_user_input = user_input;
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        mouse_x /= render_scale;
        mouse_y /= render_scale;
        SDL_Event user_event;
        while (SDL_PollEvent(&user_event))
        {
            switch (user_event.type)
            {
            case SDL_QUIT:
                saveLevel(&current_level, "level0");
                exit(0);
                break;
            case SDL_MOUSEBUTTONDOWN:
                switch (user_event.button.button)
                {
                    // Go into panning mode when the left button is down
                    case SDL_BUTTON_LEFT:
                    {
                        user_input.pan = 1;
                        break;
                    }
                    // Place tile   
                    case SDL_BUTTON_RIGHT:
                    {
                        Vector3 world_position = entityToWorldPosition(editor_cursor_entity.position);
                        setTileAt(editor_cursor.tile_id, world_position, &current_level);
                        printf("placed block at: %d %d %d\n", world_position.x, world_position.y, world_position.z);
                    }
                    break;
                }
                break;

            // reset the bitflags when the button is released
            case SDL_MOUSEBUTTONUP:
                switch (user_event.button.button)
                {
                    case SDL_BUTTON_LEFT:
                    {
                        user_input.pan = 0;
                        break;
                    }
                }
                break;
            case SDL_MOUSEWHEEL:
                if (SDL_GetTicks() - user_input.last_scroll >= SCROLL_COOLDOWN)
                {
                    if (user_event.wheel.y > 0)
                    {
                        if (tile_textures[editor_cursor.tile_id + 1])
                        {
                            editor_cursor.tile_id++;
                        }
                    }
                    else if (user_event.wheel.y < 0)
                    {
                        if (tile_textures[editor_cursor.tile_id - 1] || editor_cursor.tile_id == 1)
                        {
                            editor_cursor.tile_id--;
                        }
                    }
                    user_input.last_scroll = SDL_GetTicks();
                }
                break;
            case SDL_KEYDOWN:
            {
                switch (user_event.key.keysym.sym)
                {
                    case SDLK_w:
                    {
                        user_input.increase_level = 1;
                        break;
                    }
                    case SDLK_s:
                    {
                        user_input.decrease_level = 1;
                        break;
                    }
                }
                break;
            }
            case SDL_KEYUP:
            {
                switch (user_event.key.keysym.sym)
                {
                    case SDLK_w:
                    {
                        user_input.increase_level = 0;
                        break;
                    }
                    case SDLK_s:
                    {
                        user_input.decrease_level = 0;
                        break;
                    }
                }
                break;
            }
            }
        }

        // now do actions associated with each input
        if (user_input.decrease_level && !last_user_input.decrease_level)
        {
            if (editor_cursor_entity.position.y >= TILE_HEIGHT_PX) 
            {
                Vector3 new_cursor_pos = editor_cursor_entity.position;
                new_cursor_pos.y -= TILE_HEIGHT_PX;
                moveEntity(&editor_cursor_entity, new_cursor_pos, &entity_by_location, &current_level);
            }
        }
        if (user_input.increase_level && !last_user_input.increase_level)
        {
            if (editor_cursor_entity.position.y < (current_level.size.y - 1) * TILE_HEIGHT_PX)
            {
                Vector3 new_cursor_pos = editor_cursor_entity.position;
                new_cursor_pos.y += TILE_HEIGHT_PX;
                moveEntity(&editor_cursor_entity, new_cursor_pos, &entity_by_location, &current_level);
            }
        }

        // now move the cursor entity
        {
            if (editor_cursor_entity.draw_on_top)
            {
                Vector3 cursor_world = screenToWorld(mouse_x, mouse_y - editor_cursor_entity.position.y, camera_position.x, camera_position.y, editor_cursor_entity.position.y / TILE_HEIGHT_PX);
                cursor_world.y = editor_cursor_entity.position.y / TILE_HEIGHT_PX;
                moveEntity(&editor_cursor_entity, worldToEntityPosition(cursor_world), &entity_by_location, &current_level);
            }
            else
            {
                moveEntity(&editor_cursor_entity, screenToEntity(mouse_x, mouse_y - editor_cursor_entity.position.y, camera_position.x, camera_position.y, editor_cursor_entity.position.y), &entity_by_location, &current_level);
            }
        }

        // do panning
        if (user_input.pan)
        {
            camera_position.x -= mouse_x - last_mouse_x;
            camera_position.y -= mouse_y - last_mouse_y;
        }

        // Draw the tiles that are in view
        //     ^ Y
        //     |
        //     *
        //    / \
        // Z v   v X

        int block_min_x = clamp((camera_position.y / 2 + camera_position.x - (camera_width + 1) / 2 - (camera_height + 3) / 4) / TILE_HALF_WIDTH_PX, 0, current_level.size.x);
        int block_max_x = clamp((camera_position.y / 2 + camera_position.x + camera_width + (camera_height + 1) / 2 + current_level.size.y + TILE_HALF_WIDTH_PX - 1) / TILE_HALF_WIDTH_PX, 0, current_level.size.x);
        // the reason why I add the level height is because geometry
        //          camera
        // |     | /|
        // |     |/ |
        // y = 0  -- level height  
        int block_min_z = clamp(((camera_position.y - camera_position.x) / 2 - (camera_width + 1) / 2 - (camera_height + 3) / 4) / TILE_HALF_WIDTH_PX, 0, current_level.size.z);
        int block_max_z = clamp(((camera_position.y - camera_position.x) / 2 + camera_width + (camera_height + 1) / 2 + current_level.size.y + TILE_HALF_WIDTH_PX - 1) / TILE_HALF_WIDTH_PX, 0, current_level.size.z);

        SDL_Rect source_rectangle = { 0, 0, texture_width, texture_height };

        for (int a = 0; a <= (current_level.size.x + current_level.size.y + current_level.size.z - 3); a++)
        {
        //int a = 11;
            int b_max = min(a, current_level.size.y - 1);
            for (int b = 0; b <= b_max; b++)
            {
                int c_max = min(a - b, current_level.size.x - 1);
                for (int c = -min(0, -(a - current_level.size.z - b + 1)); c <= c_max; c++)
                {
                    Vector3 world = { c, b, a - b - c };
                    char current_tile = getTileAt(world, &current_level);
                    int cell_has_entity = current_tile & CELL_HAS_ENTITY_FLAG;
                    int screen_x, screen_y;
                    worldToScreen(world, camera_position.x, camera_position.y, &screen_x, &screen_y);
                    if (cell_has_entity)
                    {
                        uint64_t key = hashVector3(world);
                        Entity *cell_entity = findInTable(&entity_by_location, key);
                        SDL_Rect clipping_rect = { screen_x, screen_y, texture_width, texture_height };
                        while (cell_entity)
                        {   
                            if (cell_entity->draw_on_top) SDL_SetRenderTarget(main_renderer, top_texture);
                            if (cell_entity->draw) cell_entity->draw(cell_entity, main_renderer, camera_position.x, camera_position.y, clipping_rect);
                            SDL_SetRenderTarget(main_renderer, NULL);
                            cell_entity = findNextInTable(&entity_by_location, key);
                        }
                    }
                }
            }
            for (int b = 0; b <= b_max; b++)
            {
                //int b = 2;
                int c_max = min(a - b, current_level.size.x - 1);
                for (int c = -min(0, -(a - current_level.size.z - b + 1)); c <= c_max; c++)
                {
                    Vector3 world = { c, b, a - b - c };
                    char current_tile = getTileAt(world, &current_level);
                    int screen_x, screen_y;
                    worldToScreen(world, camera_position.x, camera_position.y, &screen_x, &screen_y);
                    current_tile &= (char)~CELL_HAS_ENTITY_FLAG;
                    if (current_tile && tile_textures[current_tile])
                    {
                        // calculate the position at which to draw it
                        SDL_Rect destination_rectangle = { screen_x, screen_y, source_rectangle.w, source_rectangle.h};
                        SDL_RenderCopy(main_renderer, tile_textures[current_tile], NULL, &destination_rectangle);
                    }

                }
            }
            // Put top texture on top of everything on this layer
            SDL_RenderCopy(main_renderer, top_texture, NULL, &window_rect);
            SDL_SetRenderTarget(main_renderer, top_texture);
            SDL_SetRenderDrawColor(main_renderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
            SDL_RenderClear(main_renderer);
            SDL_SetRenderTarget(main_renderer, NULL);
        }
        SDL_SetRenderTarget(main_renderer, NULL);
        //SDL_RenderCopy(main_renderer, screen_texture, NULL, &window_rect);
        SDL_RenderPresent(main_renderer);
        SDL_SetRenderDrawColor(main_renderer, 255, 255, 255, 255);
        SDL_RenderClear(main_renderer);
        uint32_t diff_time = SDL_GetTicks() - start_time;
        if (diff_time < FRAME_MILISECONDS)
        {
            SDL_Delay(FRAME_MILISECONDS - diff_time);
        }
    }
}
