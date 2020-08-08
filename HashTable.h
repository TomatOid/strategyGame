#pragma once
#include <stdint.h>
#include <stdbool.h>
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

typedef struct HashItemHead
{
    uint32_t birth_tick;
    HashItem *first;
} HashItemHead;

typedef struct ResettableHashTable
{
    HashItemHead *items;
    HashItem *last;
    size_t max_items;
    size_t items_count;
    HashItem *memory_arena;
    uint32_t current_tick; // reset counter
} ResettableHashTable;

bool insertToTable(HashTable *table, uint64_t key, void *value)
{
    size_t hash = key % table->len;
    // using a block allocator for this instead
    // of calloc to avoid heap fragmentation
    HashItem *curr = blockAlloc(&table->page);
    if (!curr) return false;
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
        HashItem *after = (curr)->next;
        void *value = (curr)->value;
        if (prev) (prev)->next = after;
        if (!blockFree(&table->page, curr)) return NULL;
        if (!prev) table->items[hash] = after;
        table->num--;
        return value;
    }
    else return NULL;
}

void removeFromTableByValue(HashTable *table, uint64_t key, void *value)
{
    size_t hash = key % table->len;
    HashItem *curr = table->items[hash];
    HashItem *prev = NULL;
    // loop untill either we hit the end of the list or the keys match
    while (curr && curr->key != key && curr->value != value) { prev = curr; curr = curr->next; }

    if (curr) 
    { 
        HashItem *after = (curr)->next;
        if (prev) (prev)->next = after;
        if (!blockFree(&table->page, curr)) return;
        if (!prev) table->items[hash] = after;
        table->num--;
        return;
    }
    else return;
}

bool insertToResettableTable(ResettableHashTable *table, uint64_t key, void *value)
{
    size_t hash = key % table->max_items;
    if (table->items_count >= table->max_items) return false;
    
    HashItem *current = &table->memory_arena[table->items_count++];
    // The next pointer should be null if the other item is out of date
    current->next = (table->items[hash].birth_tick == table->current_tick) ? table->items[hash].first : NULL;
    current->key = key;
    current->value = value;
    table->items[hash].first = current;
    table->items[hash].birth_tick = table->current_tick;
}

void *findInResettableHashTable(ResettableHashTable *table, uint64_t key)
{
    size_t hash = key % table->max_items;
    if (table->items[hash].birth_tick != table->current_tick) return NULL;
    HashItem *current = table->items[hash].first;

    while (current && current->key != key) current = current->next;

    if (current) { table->last = current; return current->value; }
    else return NULL;
}

void *findNextInResettableHashTable(ResettableHashTable *table, uint64_t key)
{
    HashItem *current = table->last->next;

    while (current && current->key != key) current = current->next;

    if (current) { table->last = current; return current->value; }
    else return NULL;
}

void resetHashTable(ResettableHashTable *table)
{
    table->current_tick++;
    table->items_count = 0;
    table->last = NULL;
}