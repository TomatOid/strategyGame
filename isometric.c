#include <SDL2/SDL.h>
#include <stdlib.h>
#include "vector.h"
#include "level.h"
#include <string.h>
#include <stdint.h>

#define TILE_HALF_WIDTH_PX 32
#define TILE_HALF_DEPTH_PX 16
#define TILE_HEIGHT_PX 38
#define SCROLL_COOLDOWN 100
#define FRAME_MILISECONDS 20

Vector3 camera_position; // the center of the viewport
int camera_width = 512; 
int camera_height = 512;
int render_scale = 2;
int on_screen_tiles = 12;
SDL_Texture* tile_textures[256] = { NULL };

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

enum
{
    AIR_TILE,
    GRASS_TILE,
    WOOD_PLANKS_TILE,
    STONE_TILE,
    GRASSY_STONE_TILE
};

typedef struct 
{
    uint32_t last_scroll;
    unsigned int increase_level : 1;
    unsigned int decrease_level : 1;
    unsigned int pan : 1;
} Inputs;

int editor_selected_tile = AIR_TILE;

int loadAllTextures(SDL_Renderer *renderer)
{
    SDL_Surface *temp_surface = SDL_LoadBMP("grassblock.bmp");
    if (!temp_surface) { puts("texture not found"); }
    tile_textures[GRASS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);
    temp_surface = SDL_LoadBMP("woodplanks.bmp");
    if (!temp_surface) { puts("texture not found"); }
    tile_textures[WOOD_PLANKS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);
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
    int term_a = (screen_y + camera_y - world_y * TILE_HEIGHT_PX - TILE_HALF_DEPTH_PX / 2 - 1) / (TILE_HALF_DEPTH_PX);
    int term_b = (screen_x + camera_x - TILE_HALF_WIDTH_PX / 2 - 1) / (TILE_HALF_WIDTH_PX);
    world_coords.x = (term_a + term_b) / 2;
    world_coords.y = world_y;
    world_coords.z = (term_a - term_b) / 2;
    return world_coords;
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
        current_level.size.x = 16;
        current_level.size.y = 3;
        current_level.size.z = 16;
        current_level.tiles = calloc(16 * 3 * 16, 1);
    }
    int mouse_x, mouse_y, last_mouse_x, last_mouse_y;
    int placement_world_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    Inputs user_input = { 0 };
    Inputs last_user_input = { 0 };
    SDL_DisplayMode display_mode;
    SDL_GetDesktopDisplayMode(0, &display_mode);
    SDL_Rect window_rect = { 0, 0, display_mode.w, display_mode.h };
    {
        int maximum_dimension = (display_mode.w > display_mode.h) ? display_mode.w : display_mode.h;
        render_scale = maximum_dimension / (TILE_HALF_WIDTH_PX * on_screen_tiles);
    }
    SDL_RenderSetScale(main_renderer, render_scale, render_scale);
    if (window_rect.w > window_rect.h)
    {
        camera_height = (camera_height * window_rect.h) / window_rect.w;
    }
    else
    {
        camera_width = (camera_width * window_rect.w) / window_rect.h;
    }
    printf("%d, %d\n", camera_height, camera_width);
    //SDL_Texture *screen_texture = SDL_CreateTexture(main_renderer, 0, SDL_TEXTUREACCESS_TARGET, camera_width, camera_height);
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
                        Vector3 tile_location = screenToWorld(mouse_x, mouse_y, camera_position.x, camera_position.y, 0);
                        tile_location.y = placement_world_y;
                        if (setTileAt(editor_selected_tile, tile_location, &current_level) | 1) printf("Placing at %d, %d, %d\n", tile_location.x, tile_location.y, tile_location.z);
                    }
                    break;
                }
                break;
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
                        if (tile_textures[editor_selected_tile + 1])
                        {
                            editor_selected_tile++;
                        }
                    }
                    else if (user_event.wheel.y < 0)
                    {
                        if (tile_textures[editor_selected_tile - 1] || editor_selected_tile == 1)
                        {
                            editor_selected_tile--;
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
            if (placement_world_y > 0) placement_world_y--;
        }
        if (user_input.increase_level && !last_user_input.increase_level)
        {
            if (placement_world_y < current_level.size.y - 1) placement_world_y++;
        }

        // now put a special tile where the mouse is
        {
            Vector3 cursor_world = screenToWorld(mouse_x, mouse_y, camera_position.x, camera_position.y, 0);
            cursor_world.y = placement_world_y;
            setTileAt(getTileAt(cursor_world, &current_level) | 0x80, cursor_world, &current_level);
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
        SDL_Rect source_rectangle = { 0, 0, 2 * TILE_HALF_WIDTH_PX, 2 * TILE_HALF_DEPTH_PX + TILE_HEIGHT_PX };
        SDL_RenderClear(main_renderer);
        for (int y = 0; y < current_level.size.y; y++)
        {
            for (int z = 0; z < current_level.size.z; z++)
            {
                for (int x = 0; x < current_level.size.x; x++)
                {
                    Vector3 world = { x, y, z };
                    char current_tile = getTileAt(world, &current_level);
                    // check if it is the special cursor tile
                    if (current_tile & 0x80)
                    {
                        // is the cursor really there?
                        Vector3 mouse_world = screenToWorld(mouse_x, mouse_y, camera_position.x, camera_position.y, 0);
                        mouse_world.y = placement_world_y;
                        setTileAt(current_tile & 0x7f, world, &current_level);
                        if (memcmp(&mouse_world, &world, sizeof(Vector3)) == 0)
                        {
                            current_tile = editor_selected_tile;
                        }
                    }
                    current_tile &= 0x7f;
                    if (current_tile && tile_textures[current_tile])
                    {
                        // calculate the position at which to draw it
                        int screen_x, screen_y;
                        worldToScreen(world, camera_position.x, camera_position.y, &screen_x, &screen_y);
                        SDL_Rect destination_rectangle = { screen_x, screen_y, source_rectangle.w, source_rectangle.h };
                        SDL_RenderCopy(main_renderer, tile_textures[current_tile], NULL, &destination_rectangle);
                    }
                }
            }
        }
        SDL_SetRenderTarget(main_renderer, NULL);
        //SDL_RenderCopy(main_renderer, screen_texture, NULL, &window_rect);
        SDL_RenderPresent(main_renderer);
        SDL_SetRenderDrawColor(main_renderer, 255, 255, 255, 0);
        SDL_RenderClear(main_renderer);
        uint32_t diff_time = SDL_GetTicks() - start_time;
        if (diff_time < FRAME_MILISECONDS)
        {
            SDL_Delay(FRAME_MILISECONDS - diff_time);
        }
    }
}