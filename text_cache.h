#include <string.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "HashTable.h"
#include "min_heap.h"

TTF_Font *default_font;
SDL_Color default_text_color = { 0, 255, 0 };

#define MAX_STRING_SIZE 256

typedef struct
{
    TTF_Font *font;
    char string[MAX_STRING_SIZE];
    SDL_Texture *texture;
} TextCacheElement;

typedef struct
{
    size_t size;
    size_t count;
    size_t total_added;
    TextCacheElement *elements;
    HashTable table;
    Heap min_heap;
} TextCache;

TextCache makeTextCache(size_t size)
{
    BlockPage page;
    makePage(&page, size, sizeof(HashItem));
    HashTable table = { calloc(size, sizeof(HashItem *)), NULL, page, size, 0 }; 
    return (TextCache) { .size = size, .count = 0, .total_added = 0, .elements = calloc(size, sizeof(TextCacheElement)),
         .table = table, .min_heap = (Heap) { .max_size = size, .count = 0, .pairs = calloc(size, sizeof(KeyValuePair))}};
}

uint64_t hashString(char *string, intptr_t font)
{
    uint64_t hash = 5381 + font;
    int c;
    while (c = *string++)
    {
        hash = ((hash << 5 | hash >> 59) + hash) + c; 
    }
    return hash;
}

SDL_Texture *getTextureFromString(SDL_Renderer *renderer, TextCache *cache, char *string, TTF_Font *font)
{
    uint64_t hash_value = hashString(string, (intptr_t)font);
    // Since rendering text is slow, our first resort is to look for it in the cache
    TextCacheElement *result = findInTable(&cache->table, hash_value);
    while (result)
    {
        if (result->font == font && strncmp(string, result->string, MAX_STRING_SIZE) == 0)
        {
            return result->texture;
        }
        result = findNextInTable(&cache->table, hash_value);
    }
    // If we get here the string is not cached
    // If it is too full, delete the oldest element
    TextCacheElement *replace;
    if (cache->count + 1 >= cache->size)
    {
        replace = cache->min_heap.pairs[0].value;
        uint64_t deleting_hash = hashString(replace->string, (intptr_t)replace->font);
        removeFromTableByValue(&cache->table, deleting_hash, replace);
        minHeapify(&cache->min_heap);
        SDL_DestroyTexture(replace->texture);
    }
    else replace = &cache->elements[cache->count++];
    SDL_Surface *temp_surface = TTF_RenderText_Solid(font, string, default_text_color);
    replace->texture = SDL_CreateTextureFromSurface(renderer, temp_surface);
    SDL_FreeSurface(temp_surface);
    strncpy(replace->string, string, MAX_STRING_SIZE);
    replace->font = font;

    insertToTable(&cache->table, hash_value, replace);
    insertToMinHeap(&cache->min_heap, cache->total_added++, replace);
    return replace->texture;
}