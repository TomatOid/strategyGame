#include <SDL2/SDL.h>
#include <stdint.h>

// Loop through a surface, making it black when a pixel is not transparent and white otherwise
int surfaceToMask(SDL_Surface *surface)
{
    if (!SDL_ISPIXELFORMAT_ALPHA(surface->format->format)) return 0;
    if (surface->format->BytesPerPixel != 4) return 0;

    uint32_t black = SDL_MapRGBA(surface->format, 0, 0, 0, SDL_ALPHA_OPAQUE);
    uint32_t white = SDL_MapRGBA(surface->format, 255, 255, 255, SDL_ALPHA_TRANSPARENT);
    uint8_t a, r, g, b;

    SDL_LockSurface(surface);
    uint32_t *pixels = (uint32_t *)surface->pixels;
    for (int i = 0; i < surface->w * surface->h; i++)
    {
        pixels[i] = ((pixels[i] & surface->format->Amask) > 0) ? black : white;
    }
    SDL_UnlockSurface(surface);
    return 1;
}

// Take a mask and flip black and white
int invertMask(SDL_Surface *surface)
{
    if (!SDL_ISPIXELFORMAT_ALPHA(surface->format->format)) return 0;
    if (surface->format->BytesPerPixel != 4) return 0;

    uint32_t black = SDL_MapRGBA(surface->format, 0, 0, 0, SDL_ALPHA_OPAQUE);
    uint32_t white = SDL_MapRGBA(surface->format, 255, 255, 255, SDL_ALPHA_TRANSPARENT);
    uint8_t a, r, g, b;

    SDL_LockSurface(surface);
    uint32_t *pixels = (uint32_t *)surface->pixels;
    for (int i = 0; i < surface->w * surface->h; i++)
    {
        pixels[i] = ((pixels[i] & surface->format->Rmask) > 0) ? black : white;
    }
    SDL_UnlockSurface(surface);
    return 1;
}