#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include "vector.h"
#include "level.h"
#include "textures_generated.h"
#include "HashTable.h"
#include "entity.h"
#include "components.h"
#include "text_cache.h"
#include "math_utils.h"
#include "draw_level.h"

#define SCROLL_COOLDOWN 100
#define FRAME_MILISECONDS 20
#define TEXT_CACHE_SIZE 100

int camera_position_x, camera_position_y; // the top left corner of the viewport
int render_scale = 2;
int on_screen_tiles = 16;
HashTable entity_by_location = { 0 };

typedef struct 
{
    uint32_t last_scroll;
    unsigned int increase_level : 1;
    unsigned int decrease_level : 1;
    unsigned int pan : 1;
    unsigned int cycle_editor_mode : 1;
} Inputs;

int editor_selected_tile = AIR_TILE;


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
    SDL_CreateWindowAndRenderer(1280, 720, SDL_RENDERER_ACCELERATED | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED, &main_window, &main_renderer);
    loadAllTextures(main_renderer);

    // More SDL graphics stuff
    SDL_DisplayMode display_mode;
    SDL_GetDesktopDisplayMode(0, &display_mode);
    SDL_Rect window_rect = { 0, 0, display_mode.w, display_mode.h };
    {
        int maximum_dimension = (display_mode.w > display_mode.h) ? display_mode.w : display_mode.h;
        render_scale = maximum_dimension / (TILE_HALF_WIDTH_PX * on_screen_tiles);
    }
    // This represents the size of the non-pixelated layer
    SDL_Rect ui_layer_rect = window_rect;
    window_rect.w /= render_scale;
    window_rect.h /= render_scale;
    // This texture is for the pixelated game layer
    SDL_Texture *game_window_texture = SDL_CreateTexture(main_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, window_rect.w, window_rect.h);
    screen_grid_width = (window_rect.w + SCREEN_GRID_SIZE_PX - 1) / SCREEN_GRID_SIZE_PX;
    screen_grid_height = (window_rect.h + SCREEN_GRID_SIZE_PX - 1) / SCREEN_GRID_SIZE_PX;
    screen_grid = malloc(screen_grid_width * screen_grid_height * sizeof(uint64_t));

    // We will use these values later when drawing
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
    entity_by_location.len = MAX_ENTITIES;
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
    editor_cursor_entity.texture_data->temporary_frame_buffer = SDL_CreateTexture(main_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, texture_width, texture_height);
    SDL_SetTextureBlendMode(editor_cursor_entity.texture_data->temporary_frame_buffer, SDL_BLENDMODE_BLEND);
    {
        Vector3 size = { TILE_HALF_WIDTH_PX - 1, TILE_HEIGHT_PX - 1, TILE_HALF_WIDTH_PX - 1 };
        addEntity(&editor_cursor_entity, screenToEntity(mouse_x, mouse_y, camera_position_x, camera_position_y, 0), size, &entity_by_location, &current_level);
    }

    PlacementCursor dummy_cursor = { AIR_TILE };
    Entity dummy_entity = { .type = ENTITY_EDITOR_CURSOR, .draw_on_top = 0, .draw = drawEditorCursor, .specific_data = &dummy_cursor, .texture_data = &entity_texture_data[entity_texture_data_count++] };
    dummy_entity.texture_data->temporary_frame_buffer = SDL_CreateTexture(main_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, texture_width, texture_height);
    SDL_SetTextureBlendMode(dummy_entity.texture_data->temporary_frame_buffer, SDL_BLENDMODE_BLEND);
    {
        Vector3 size = { TILE_HALF_WIDTH_PX - 1, TILE_HEIGHT_PX - 1, TILE_HALF_WIDTH_PX - 1 };
        addEntity(&dummy_entity, addVector3(worldToEntityPosition((Vector3) { 10, 2, 10}), (Vector3) {64, 0, 0}), size, &entity_by_location, &current_level);
    }

    TTF_Init();
    TextCache text_cache = makeTextCache(TEXT_CACHE_SIZE);
    default_font = TTF_OpenFont("./Renogare-Regular.ttf", 100);
    if (!default_font) puts("error loading font");
    char position_string_buf[20];
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
                    }
                    window_rect = (SDL_Rect) { 0, 0, user_event.window.data1, user_event.window.data2 };
                    {
                        int maximum_dimension = (window_rect.w > window_rect.h) ? window_rect.w : window_rect.h;
                        render_scale = (maximum_dimension + TILE_HALF_WIDTH_PX * on_screen_tiles - 1) / (TILE_HALF_WIDTH_PX * on_screen_tiles);
                    }
                    ui_layer_rect = window_rect;
                    window_rect.h /= render_scale;
                    window_rect.w /= render_scale;
                    SDL_DestroyTexture(game_window_texture);
                    game_window_texture = SDL_CreateTexture(main_renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, window_rect.w, window_rect.h);
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
            camera_position_x -= mouse_x / render_scale - last_mouse_x / render_scale;
            camera_position_y -= mouse_y / render_scale - last_mouse_y / render_scale;
        }
        // now move the cursor entity
        {
            // if the draw_on_top property is set to true, that sprite should be grid-locked to avoid spoiling the 3d effect
            if (editor_cursor_entity.draw_on_top)
            {
                Vector3 cursor_world = screenToWorld(mouse_x / render_scale, mouse_y / render_scale - editor_cursor_entity.position.y,
                        camera_position_x, camera_position_y, editor_cursor_entity.position.y / TILE_HEIGHT_PX);
                cursor_world.y = editor_cursor_entity.position.y / (ENTITY_POSITION_MULTIPLIER * TILE_HEIGHT_PX);
                moveEntity(&editor_cursor_entity, worldToEntityPosition(cursor_world), &entity_by_location, &current_level);
            }
            else
            {
                moveEntity(&editor_cursor_entity, screenToEntity(mouse_x / render_scale - TILE_HALF_WIDTH_PX, mouse_y / render_scale - TILE_HALF_DEPTH_PX - editor_cursor_entity.position.y / ENTITY_POSITION_MULTIPLIER,
                        camera_position_x, camera_position_y, editor_cursor_entity.position.y), &entity_by_location, &current_level);
            }
        }

        drawLevel(main_renderer, current_level, game_window_texture, camera_position_x, camera_position_y);
        SDL_RenderCopy(main_renderer, game_window_texture, NULL, &ui_layer_rect);

        // UI stuff
        {
            memset(position_string_buf, 0, sizeof(position_string_buf));
            SDL_itoa(mouse_x / render_scale, position_string_buf, 10);
            SDL_Texture *text = getTextureFromString(main_renderer, &text_cache, position_string_buf, default_font);
            int text_width, text_height;
            SDL_QueryTexture(text, NULL, NULL, &text_width, &text_height);
            SDL_RenderCopy(main_renderer, text, NULL, &(SDL_Rect) { (ui_layer_rect.w - text_width) / 2, ui_layer_rect.h - text_height, text_width, text_height });
        }

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