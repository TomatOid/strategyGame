#pragma once
#include "min_heap.h"
#include "HashTable.h"
#include "level.h"
#include "vector.h"

typedef struct AStarNode
{
    Vector3 node_position;
    int g_score;
    int f_score;
    struct AStarNode *came_from;
} AStarNode;

typedef struct SearchData
{
    BlockPage nodes; // maybe this should just be an array
    HashTable open_set;
    Heap open_f_min_heap;
} SearchData;

SearchData createSearchData(int max_nodes)
{
    SearchData result = { 0 };
    makePage(&result.nodes, max_nodes, sizeof(AStarNode));
    result.open_set.items = calloc(max_nodes, sizeof(HashItem *));
    result.open_set.len = max_nodes;
    result.open_set.num = 0;
    makePage(&result.open_set.page, max_nodes, sizeof(HashItem));
    result.open_f_min_heap.max_size = max_nodes;
    result.open_f_min_heap.pairs = calloc(max_nodes, sizeof(KeyValuePair));
    return result;
}

AStarNode *aStarPathFind(Vector3 start_position, Vector3 goal, SearchData *search_data)
{
    // reset the SearchData
}