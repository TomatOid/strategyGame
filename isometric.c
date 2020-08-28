#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "vector.h"
#include "level.h"
#include "textures_generated.h"
#include "HashTable.h"
#include "entity.h"
#include "components.h"

#define SCROLL_COOLDOWN 100
#define FRAME_MILISECONDS 20
#define MAX_ENTITIES_PER_CELL 64
#define TOP_ENTITIES_PER_LAYER 64
#define SCREEN_GRID_SIZE_PX 70

int camera_position_x, camera_position_y; // the top left corner of the viewport
int render_scale = 2;
int on_screen_tiles = 16;

void *entity_search_results[MAX_ENTITIES_PER_CELL];
Entity *top_entity_array[TOP_ENTITIES_PER_LAYER];
SDL_Rect top_clipping_rectangle_array[TOP_ENTITIES_PER_LAYER];
TextureData entity_texture_data[MAX_ENTITIES] = { 0 };
size_t entity_texture_data_count = 0;
uint64_t *screen_grid;
size_t screen_grid_width, screen_grid_height;

int min(int a, int b)
{
    return (a < b) ? a : b;
}

int abs(int a)
{
    return (a < 0) ? -a : a;
}

SDL_Rect rectangleIntersect(SDL_Rect a, SDL_Rect b)
{
    int intersection_min_x = -min(-a.x, -b.x);
    int intersection_max_x = min(a.x + a.w, b.x + b.w);
    int intersection_min_y = -min(-a.y, -b.y);
    int intersection_max_y = min(a.y + a.h, b.y + b.h);
    //a.x = (a.w == 0) ? b.x : (b.w == 0) ? a.x : intersection_min_x;
    //a.y = (a.h == 0) ? b.y : (b.h == 0) ? a.y : intersection_min_y;
    // if the rectangles do not intersect in the
    //a.x = ((intersection_max_x < intersection_min_x) && (intersection_max_y < intersection_min_y)) ? a.x : intersection_min_x;
    if ((intersection_max_x <= intersection_min_x) || (intersection_max_y <= intersection_min_y))
    {
        a.w = 0;
        a.h = 0;
        return a;
    }
    a.x = intersection_min_x;
    a.y = intersection_min_y;
    a.w = intersection_max_x - intersection_min_x;
    a.h = intersection_max_y - intersection_min_y;
    //a.w = ((intersection_max_x < intersection_min_x) || (intersection_max_y < intersection_min_y)) ? 0 : intersection_max_x - intersection_min_x;
    //a.h = ((intersection_max_x < intersection_min_x) || (intersection_max_y < intersection_min_y)) ? 0 : intersection_max_y - intersection_min_y;
    return a;
}

SDL_Rect rectangleUnion(SDL_Rect a, SDL_Rect b)
{
    //if (!b.w && !b.h) return a; 
    //if (!a.w && !a.h) return b;
    int union_min_x = min(a.x, b.x);
    int union_max_x = -min(-a.x - a.w, -b.x - b.w);
    int union_min_y = min(a.y, b.y);
    int union_max_y = -min(-a.y - a.h, -b.y - b.h);
    SDL_Rect res;
    //res.x = union_min_x;
    //res.y = union_min_y;
    res.x = (b.w == 0 && b.h == 0) ? a.x : ((a.w == 0 && a.h == 0) ? b.x : union_min_x);
    res.y = (b.h == 0 && b.w == 0) ? a.y : ((a.h == 0 && a.w == 0) ? b.y : union_min_y);
    //res.w = union_max_x - union_min_x;
    //res.h = union_max_y - union_min_y;
    res.w = (b.w == 0 && b.h == 0) ? a.w : ((a.w == 0 && a.h == 0) ? b.w : union_max_x - union_min_x);
    res.h = (b.h == 0 && b.w == 0) ? a.h : ((a.h == 0 && a.w == 0) ? b.h : union_max_y - union_min_y);
    return res;
}

typedef struct 
{
    uint32_t last_scroll;
    unsigned int increase_level : 1;
    unsigned int decrease_level : 1;
    unsigned int pan : 1;
    unsigned int cycle_editor_mode : 1;
} Inputs;

int editor_selected_tile = AIR_TILE;

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
    int term_b = (screen_x + camera_x > 0) ? (screen_x + TILE_HALF_WIDTH_PX / 2 + camera_x) / (TILE_HALF_WIDTH_PX) 
        : (screen_x - TILE_HALF_WIDTH_PX / 2 + camera_x) / (TILE_HALF_WIDTH_PX);
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
                    //printf("%d\n", i * ((MAX_ENTITIES + 63) / 64) + j);
                    SDL_Rect *union_rect = &entity_texture_data[i * ((MAX_ENTITIES + 63) / 64) + j].union_rectangle;
                    *union_rect = rectangleUnion(*union_rect, rectangleIntersect(entity_texture_data[i * ((MAX_ENTITIES + 63) / 64) + j].bounds_rectangle, screen_rectangle));
                }
            }
        }
        return 1;
    } else return 0;
}

void drawEditorCursor(Entity *cursor_entity, SDL_Renderer *renderer, int camera_x, int camera_y, SDL_Rect clipping_rectangle)
{
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

int periodicLogAverage(uint32_t value, uint32_t period, uint32_t *sum, uint32_t *count, uint32_t *last_print_time)
{
    uint32_t ticks = SDL_GetTicks();
    if (ticks - *last_print_time >= period)
    {
        printf("%f\n", (double)(*sum + value) / (double)(*count + 1));
        *count = 0;
        *sum = 0;
        *last_print_time = ticks;
        return 1;
    }
    else
    {
        *sum += value;
        (*count)++;
        return 0;
    }
}

uint32_t start_time;

int main()
{
    // All of that gross initialization code that always ends up at the start of main()
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window *main_window;
    SDL_Renderer *main_renderer;
    SDL_CreateWindowAndRenderer(1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED, &main_window, &main_renderer);
    loadAllTextures(main_renderer);

    // More SDL graphics stuff
    SDL_DisplayMode display_mode;
    SDL_GetDesktopDisplayMode(0, &display_mode);
    SDL_Rect window_rect = { 0, 0, display_mode.w, display_mode.h };
    {
        int maximum_dimension = (display_mode.w > display_mode.h) ? display_mode.w : display_mode.h;
        render_scale = maximum_dimension / (TILE_HALF_WIDTH_PX * on_screen_tiles);
    }
    SDL_RenderSetScale(main_renderer, render_scale, render_scale);
    window_rect.w /= render_scale;
    window_rect.h /= render_scale;
    screen_grid_width = (window_rect.w + SCREEN_GRID_SIZE_PX - 1) / SCREEN_GRID_SIZE_PX;
    screen_grid_height = (window_rect.h + SCREEN_GRID_SIZE_PX - 1) / SCREEN_GRID_SIZE_PX;
    screen_grid = malloc(screen_grid_width * screen_grid_height * sizeof(uint64_t));

    // We will use these values later when drawing
    int texture_width, texture_height;
    SDL_QueryTexture(tile_textures[GRASS_TILE], NULL, NULL, &texture_width, &texture_height);

    // Now initialize a level
    Level current_level;
    if (!loadLevel(&current_level, "level0"))
    {
        // if the file does not exist, create a blank level
        current_level.size.x = 128;
        current_level.size.y = 6;
        current_level.size.z = 128;
        current_level.tiles = calloc(current_level.size.x * current_level.size.y * current_level.size.z, 1);
    }

    // Initialize the hash table
    HashTable entity_by_location;
    entity_by_location.len = 100;
    entity_by_location.items = calloc(entity_by_location.len, sizeof(HashItem *));
    makePage(&entity_by_location.page, entity_by_location.len, sizeof(HashItem));

    // Setup input stuff
    int mouse_x, mouse_y, last_mouse_x, last_mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    Inputs user_input = { 0 };
    Inputs last_user_input = { 0 };

    // Initialize the entities for the editor mode
    PlacementCursor editor_cursor = { AIR_TILE };
    Entity editor_cursor_entity = { 0 };
    editor_cursor_entity.type = ENTITY_EDITOR_CURSOR;
    editor_cursor_entity.draw_on_top = 0;
    editor_cursor_entity.draw = drawEditorCursor;
    editor_cursor_entity.specific_data = &editor_cursor;
    editor_cursor_entity.texture_data = &entity_texture_data[entity_texture_data_count++];
    {
        Vector3 size = { TILE_HALF_WIDTH_PX - 1, TILE_HEIGHT_PX - 1, TILE_HALF_WIDTH_PX - 1 };
        addEntity(&editor_cursor_entity, screenToEntity(mouse_x, mouse_y, camera_position_x, camera_position_y, 0), size, &entity_by_location, &current_level);
    }
    // logging variables
    uint32_t ticks_log_sum, ticks_log_count, ticks_last_print;
    for (;;)
    {
        // We want to reach a fixed frame rate, so we need to time the rendering 
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
                SDL_DestroyRenderer(main_renderer);
                SDL_DestroyWindow(main_window);
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
                        exit(0);
                    }
                    break;
                }
                break;

            // Reset the bitflags when the button is released
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
            // We need to enforce a delay between scroll events to avoid overscrolling
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
                        if ((editor_cursor.tile_id > 0) && tile_textures[editor_cursor.tile_id - 1])
                        {
                            editor_cursor.tile_id--;
                        }
                    }
                    user_input.last_scroll = SDL_GetTicks();
                }
                break;
            // Set each input bit when the corresponding key is pressed
            // TODO: User-configurable controls
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
                    case SDLK_e:
                    {
                        user_input.cycle_editor_mode = 1;
                        break;
                    }
                }
                break;
            }
            // We also need to unset the bits when the keys are released
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
                    case SDLK_e:
                    {
                        user_input.cycle_editor_mode = 0;
                    }
                }
                break;
            }
            case SDL_WINDOWEVENT:
            {
                switch (user_event.window.event)
                {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                {
                    if ((user_event.window.data1 + SCREEN_GRID_SIZE_PX - 1) / SCREEN_GRID_SIZE_PX != screen_grid_width || (user_event.window.data2 + SCREEN_GRID_SIZE_PX - 1) / SCREEN_GRID_SIZE_PX)
                    {
                        screen_grid_width = (user_event.window.data1 + SCREEN_GRID_SIZE_PX - 1) / SCREEN_GRID_SIZE_PX;
                        screen_grid_height = (user_event.window.data2 + SCREEN_GRID_SIZE_PX - 1) / SCREEN_GRID_SIZE_PX;
                        screen_grid = realloc(screen_grid, screen_grid_width * screen_grid_height * sizeof(uint64_t));
                        printf("%d %d\n", screen_grid_width, screen_grid_height);
                    }
                    SDL_Rect window_rect = { 0, 0, user_event.window.data1, user_event.window.data2 };
                    {
                        int maximum_dimension = (window_rect.w > window_rect.h) ? window_rect.w : window_rect.h;
                        render_scale = (maximum_dimension + TILE_HALF_WIDTH_PX * on_screen_tiles - 1) / (TILE_HALF_WIDTH_PX * on_screen_tiles);
                    }
                    SDL_RenderSetScale(main_renderer, (float)render_scale, (float)render_scale);
                }
                break;
                }
            }
            }
        }

        // clear the screen grid
        memset(screen_grid, 0, screen_grid_width * screen_grid_height * sizeof(uint64_t));

        // now do actions associated with each input
        if (user_input.decrease_level && !last_user_input.decrease_level)
        {
            if (editor_cursor_entity.position.y >= TILE_HEIGHT_PX * ENTITY_POSITION_MULTIPLIER) 
            {
                Vector3 new_cursor_pos = editor_cursor_entity.position;
                new_cursor_pos.y -= TILE_HEIGHT_PX * ENTITY_POSITION_MULTIPLIER;
                moveEntity(&editor_cursor_entity, new_cursor_pos, &entity_by_location, &current_level);
            }
        }
        if (user_input.increase_level && !last_user_input.increase_level)
        {
            if (editor_cursor_entity.position.y < (current_level.size.y - 1) * TILE_HEIGHT_PX * ENTITY_POSITION_MULTIPLIER)
            {
                Vector3 new_cursor_pos = editor_cursor_entity.position;
                new_cursor_pos.y += TILE_HEIGHT_PX * ENTITY_POSITION_MULTIPLIER;
                moveEntity(&editor_cursor_entity, new_cursor_pos, &entity_by_location, &current_level);
            }
        }
        if (user_input.cycle_editor_mode && !last_user_input.cycle_editor_mode)
        {
            editor_cursor_entity.draw_on_top = !editor_cursor_entity.draw_on_top;
        }
        // do panning
        if (user_input.pan)
        {
            camera_position_x -= mouse_x - last_mouse_x;
            camera_position_y -= mouse_y - last_mouse_y;
        }
        // now move the cursor entity
        {
            // if the draw_on_top property is set to true, that sprite should be grid-locked to avoid spoiling the 3d effect
            if (editor_cursor_entity.draw_on_top)
            {
                Vector3 cursor_world = screenToWorld(mouse_x, mouse_y - editor_cursor_entity.position.y,
                        camera_position_x, camera_position_y, editor_cursor_entity.position.y / TILE_HEIGHT_PX);
                cursor_world.y = editor_cursor_entity.position.y / (ENTITY_POSITION_MULTIPLIER * TILE_HEIGHT_PX);
                moveEntity(&editor_cursor_entity, worldToEntityPosition(cursor_world), &entity_by_location, &current_level);
            }
            else
            {
                moveEntity(&editor_cursor_entity, screenToEntity(mouse_x - TILE_HALF_WIDTH_PX, mouse_y - TILE_HALF_DEPTH_PX - editor_cursor_entity.position.y / ENTITY_POSITION_MULTIPLIER,
                        camera_position_x, camera_position_y, editor_cursor_entity.position.y), &entity_by_location, &current_level);
            }
        }

        // Draw the tiles that are in view
        //     ^ Y
        //     |
        //     *
        //    / \
        // Z v   v X

        // the reason why I add the level height is because geometry
        //          camera
        // | *   | /|
        // |## # |/ |
        // y = 0  -- level height  

        // Find the world coordinates of the four corners of the screen so that we only draw what we need
        Vector3 camera_world_top_left = screenToWorld(0, -2 * TILE_HALF_DEPTH_PX - TILE_HEIGHT_PX, camera_position_x, camera_position_y, 0);
        Vector3 camera_world_top_right = screenToWorld(window_rect.w / 2, -2 * TILE_HALF_DEPTH_PX - TILE_HEIGHT_PX, camera_position_x, camera_position_y, 0);
        Vector3 camera_world_bottom_left = screenToWorld(0, window_rect.h, camera_position_x, camera_position_y, current_level.size.y - 1);
        Vector3 camera_world_bottom_right = screenToWorld(window_rect.w, window_rect.h, camera_position_x, camera_position_y, current_level.size.y - 1);
        
        int a_min = clamp(camera_world_top_left.x + camera_world_top_left.y + camera_world_top_right.z, 0, 
            (current_level.size.x + current_level.size.y + current_level.size.z - 3));
        int a_max = clamp(camera_world_bottom_right.x + camera_world_bottom_right.y + camera_world_bottom_left.z, 0, 
            (current_level.size.x + current_level.size.y + current_level.size.z - 3));

        SDL_Rect source_rectangle = { 0, 0, texture_width, texture_height };

        // *** Drawing Code ***
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
                    char current_tile = getTileAt(world, &current_level);
                    int cell_has_entity = current_tile & CELL_HAS_ENTITY_FLAG;
                    if (cell_has_entity)
                    {
                        uint64_t key = hashVector3(world);
                        int screen_x, screen_y;
                        worldToScreen(world, camera_position_x, camera_position_y, &screen_x, &screen_y);
                        SDL_Rect clipping_rect = { screen_x, screen_y, texture_width, texture_height };
                        size_t return_count = findAllInTable(&entity_by_location, key, entity_search_results, MAX_ENTITIES_PER_CELL);
                        if (return_count > 1) { qsort(entity_search_results, return_count, sizeof(Entity *), entityLayerCompare); }
                        
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
                //int b = 2;
                int c_max = min(a - b, camera_world_bottom_right.x - 1);
                for (int c = -min(-camera_world_top_left.x, -(a - camera_world_bottom_left.z - b + 1)); c <= c_max; c++)
                {
                    Vector3 world = { c, b, a - b - c };
                    char current_tile = getTileAt(world, &current_level);
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
                        if (doOverlapTesting(destination_rectangle));// puts("overlap");
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
                //printf("%f\n", 128 * (float)(union_rect.w * union_rect.h) / (float)(entity_texture_data[i].bounds_rectangle.w * entity_texture_data[i].bounds_rectangle.h));
                SDL_SetTextureAlphaMod(entity_texture_data[i].animation_frame_mask, 194 * (float)(union_rect.w * union_rect.h) / (float)(entity_texture_data[i].bounds_rectangle.w * entity_texture_data[i].bounds_rectangle.h));
                SDL_RenderCopy(main_renderer, entity_texture_data[i].animation_frame_mask, NULL, &entity_texture_data[i].bounds_rectangle);
                SDL_SetTextureAlphaMod(entity_texture_data[i].animation_frame_mask, SDL_ALPHA_OPAQUE);
            }
        }

        SDL_SetRenderTarget(main_renderer, NULL);
        SDL_RenderPresent(main_renderer);
        SDL_SetRenderDrawColor(main_renderer, 255, 255, 255, 255);
        SDL_RenderClear(main_renderer);

        uint32_t diff_time = SDL_GetTicks() - start_time;
        if (diff_time < FRAME_MILISECONDS)
        {
            periodicLogAverage(diff_time, 1000, &ticks_log_sum, &ticks_log_count, &ticks_last_print);
            SDL_Delay(FRAME_MILISECONDS - diff_time);
        }
    }
}