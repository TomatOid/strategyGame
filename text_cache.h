#include <string.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "HashTable.h"
#include "min_heap.h"

TTF_Font *default_font;
SDL_Color default_text_color = { 255, 255, 255 };

typedef struct
{
    int point_size;
    char *string;
    SDL_Texture *texture;
} TextCacheElement;

typedef struct
{
    size_t size;
    size_t count;
    size_t total_added;
    TextCacheElement *elements;
    HashTable *table;
    Heap *min_heap;
} TextCache;

uint64_t hashString(char *string, int point_size)
{
    uint64_t hash = 5381 + point_size;
    int c;
    while (c = *string++)
    {
        hash = ((hash << 5 | hash >> 59) + hash) + c; 
    }
    return hash;
}

SDL_Texture *getTextureFromString(SDL_Renderer *renderer, TextCache *cache, char *string, int point_size, char **old_string)
{
    uint64_t hash_value = hashString(string, point_size);
    // Since rendering text is slow, our first resort is to look for it in the cache
    TextCacheElement *result = findInTable(cache->table, hash_value);
    while (result)
    {
        if (result->point_size == point_size && strcmp(string, result->string) == 0)
        {
            if (old_string) *old_string = NULL;
            return result->texture;
        }
        result = findNextInTable(cache->table, hash_value);
    }
    // If we get here the string is not cached
    // If it is too full, delete the oldest element
    TextCacheElement *replace;
    if (cache->count + 1 >= cache->size)
    {
        replace = cache->min_heap->pairs[0].value;
        uint64_t deleting_hash = hashString(replace->string, replace->point_size);
        removeFromTableByValue(cache->table, deleting_hash, replace);
        minHeapify(cache->min_heap);
        SDL_DestroyTexture(replace->texture);
    }
    else replace = &cache->elements[cache->count++];

    SDL_Surface *temp_surface = TTF_RenderText_Solid(default_font, string, default_text_color);
    replace->texture = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);
    if (old_string) *old_string = replace->string;
    replace->string = string;
    replace->point_size = point_size;

    insertToTable(cache->table, hash_value, replace);
    insertToMinHeap(cache->min_heap, cache->total_added++, replace);
    return replace->texture;
}