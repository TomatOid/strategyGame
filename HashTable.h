#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "BlockAllocate.h"

typedef struct HashItem
{
    uint64_t key;
    void *value;
    struct HashItem *next;
} HashItem;

// A seprate chaining hash table struct
typedef struct HashTable
{
    HashItem **items;
    HashItem *last;
    BlockPage page;
    size_t len;
    size_t num;
} HashTable;

bool insertToTable(HashTable *table, uint64_t key, void *value)
{
    size_t hash = key % table->len;
    // using a block allocator for this instead
    // of calloc to avoid heap fragmentation
    HashItem *curr = blockAlloc(&table->page);
    //if (!curr) return false;
    assert(curr);
    curr->key = key;
    curr->value = value;
    // insert the item at the beginning of the linked list
    curr->next = table->items[hash];
    table->items[hash] = curr;
    table->num++;
    return true;
}

void *findInTable(HashTable *table, uint64_t key)
{
    size_t hash = key % table->len;
    HashItem *curr = table->items[hash];

    while (curr && curr->key != key) { curr = curr->next; }

    if (curr) { table->last = curr; return curr->value; }
    else { return NULL; }
}

void *findNextInTable(HashTable *table, uint64_t key)
{
    HashItem *curr = table->last->next;

    while (curr && curr->key != key) { curr = curr->next; }

    if (curr) { table->last = curr; return curr->value; }
    else { return NULL; }
}

size_t findAllInTable(HashTable *table, uint64_t key, void **return_buffer, size_t buffer_size)
{
    size_t hash = key % table->len;
    size_t index = 0;
    for (HashItem *curr = table->items[hash]; curr && (index < buffer_size); curr = curr->next)
    {
        if (curr->key == key)
        {
            return_buffer[index++] = curr->value;
        }
    }
    table->last = NULL;
    return index;
}

void *removeFromTable(HashTable *table, uint64_t key)
{
    size_t hash = key % table->len;
    HashItem *curr = table->items[hash];
    HashItem *prev = NULL;
    // loop untill either we hit the end of the list or the keys match
    while (curr && curr->key != key) { prev = curr; curr = curr->next; }

    if (curr) 
    { 
        HashItem *after = curr->next;
        void *value = curr->value;
        if (prev) prev->next = after;
        if (!blockFree(&table->page, curr)) return NULL;
        if (!prev) table->items[hash] = after;
        table->num--;
        return value;
    }
    else return NULL;
}

int removeFromTableByValue(HashTable *table, uint64_t key, void *value)
{
    size_t hash = key % table->len;
    HashItem *curr = table->items[hash];
    HashItem *prev = NULL;
    int key_matches_count = 0;
    // loop untill either we hit the end of the list or the keys match
    while (curr && (curr->key != key || curr->value != value)) { key_matches_count += curr->key == key; prev = curr; curr = curr->next; }
    HashItem *remaining = curr;
    while (remaining) {key_matches_count += remaining->key == key; remaining = remaining->next; }

    if (curr) 
    { 
        HashItem *after = curr->next;
        if (prev) prev->next = after;
        if (!blockFree(&table->page, curr)) return false;
        if (!prev) table->items[hash] = after;
        table->num--;
        return key_matches_count;
    } else return 0;
}