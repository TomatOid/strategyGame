#include <SDL2/SDL.h>
#include <stdlib.h>
#include "vector.h"
#include "level.h"

#define TILE_HALF_WIDTH_PX 32
#define TILE_HALF_DEPTH_PX 16
#define TILE_HEIGHT_PX 38

Vector3 camera_position; // the center of the viewport
int camera_width = 256; 
int camera_height = 256;
SDL_Texture* tile_textures[256] = { 0 };

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
    STONE_TILE,
    GRASSY_STONE_TILE
};

int editor_selected_tile = GRASS_TILE;

int loadAllTextures(SDL_Renderer *renderer)
{
    SDL_Surface *temp_surface = SDL_LoadBMP("grass.bmp");
    if (!temp_surface) { puts("texture not found"); }
    tile_textures[GRASS_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);
    temp_surface = SDL_LoadBMP("stone.bmp");
    tile_textures[STONE_TILE] = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);
}

void worldToScreen(Vector3 world, int camera_x, int camera_y, int *screen_x, int *screen_y)
{
    *screen_x = (world.x - world.z) * TILE_HALF_WIDTH_PX - camera_x;
    *screen_y = world.y * TILE_HEIGHT_PX + (world.x + world.z) * TILE_HALF_DEPTH_PX - camera_y;
}

Vector3 screenToWorld(int screen_x, int screen_y, int camera_x, int camera_y, int world_y)
{
    // we need to assume a value for y so the equation has a unique solution
    Vector3 world_coords;
    int term_a = (screen_y + camera_y - world_y * TILE_HEIGHT_PX) / (2 * TILE_HALF_DEPTH_PX);
    int term_b = (screen_x + camera_x) / (2 * TILE_HALF_WIDTH_PX);
    world_coords.x = term_a + term_b;
    world_coords.y = world_y;
    world_coords.z = term_a - term_b;
    return world_coords;
}

int main()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window *main_window;
    SDL_Renderer *main_renderer;
    SDL_CreateWindowAndRenderer(camera_width, camera_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED, &main_window, &main_renderer);
    loadAllTextures(main_renderer);
    // Now initialize a level
    Level current_level;
    // loadLevel(&current_level, "level0");
    current_level.size.x = 16;
    current_level.size.y = 2;
    current_level.size.z = 16;
    current_level.tiles = calloc(16 * 2 * 16, 1);
    int mouse_x, mouse_y, last_mouse_x, last_mouse_y;
    int placement_world_y = 0;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    int left_click_down = 0;
    for (;;)
    {
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        SDL_Event user_event;
        SDL_PollEvent(&user_event);
        switch (user_event.type)
        {
        case SDL_QUIT:
            exit(0);
            break;
        case SDL_MOUSEBUTTONDOWN:
            switch (user_event.button.button)
            {
                // Go into panning mode when the left button is down
                case SDL_BUTTON_LEFT:
                {
                    left_click_down = 1;
                    break;
                }
                // Place tile   
                case SDL_BUTTON_RIGHT:
                {
                    Vector3 tile_location = screenToWorld(mouse_x, mouse_y, camera_position.x, camera_position.y, placement_world_y);
                    setTileAt(GRASS_TILE, tile_location, &current_level);
                    printf("Placing at %d, %d, %d\n", tile_location.x, tile_location.y, tile_location.z);
                }
            }
        case SDL_MOUSEBUTTONUP:
            switch (user_event.button.button)
            {
                case SDL_BUTTON_LEFT:
                {
                    left_click_down = 0;
                    break;
                }
            }
        }

        // do panning
        if (left_click_down)
        {
            camera_position.x += mouse_x - last_mouse_x;
            camera_position.y += mouse_y - last_mouse_y;
            puts("Panning\n");
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
        for (int y = 0; y < current_level.size.y; y++)
        {
            for (int z = block_min_z; z < block_max_z; z++)
            {
                for (int x = block_min_x; x < block_max_x; x++)
                {
                    Vector3 world = { x, y, z };
                    char current_tile = getTileAt(world, &current_level);
                    if (current_tile && tile_textures[current_tile])
                    {
                        // calculate the position at which to draw it
                        int screen_x, screen_y;
                        worldToScreen(world, camera_position.x, camera_position.y, &screen_x, &screen_y);
                        SDL_Rect destination_rectangle = { screen_x, screen_y, source_rectangle.w, source_rectangle.h };
                        puts("Drawing!\n");
                        SDL_RenderCopy(main_renderer, tile_textures[current_tile], &source_rectangle, &destination_rectangle);
                    }
                }
            }
        }
        SDL_RenderPresent(main_renderer);
        SDL_SetRenderDrawColor(main_renderer, 0, 0, 0, 0);
        //SDL_RenderClear(main_renderer);
    }
}