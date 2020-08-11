#include <stdlib.h>
#include <stdint.h>

typedef struct
{
    uint64_t key;
    void *value;
} KeyValuePair;

typedef struct
{
    KeyValuePair *pairs;
    size_t max_size;
    size_t count;
} Heap;

int insertToMinHeap(Heap *min_heap, uint64_t key, void *value)
{
    if (min_heap->count >= min_heap->max_size) return 0;
    min_heap->pairs[min_heap->count].key = key;
    min_heap->pairs[min_heap->count].value = value;
    for (size_t i = min_heap->count; i > 0 && min_heap->pairs[i].key < min_heap->pairs[(i - 1) >> 1].key; i >>= 1)
    {
        KeyValuePair temporary = min_heap->pairs[i];
        min_heap->pairs[i] = min_heap->pairs[(i - 1) >> 1];
        min_heap->pairs[(i - 1) >> 1] = temporary;
    }
    min_heap->count++;
    return 1;
}

