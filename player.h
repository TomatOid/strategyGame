#include "vector.h"
#pragma once

enum
{
    ARCHER_CLASS,
    SORCERER_CLASS,
    SWORDSMAN_CLASS,
};

typedef struct Player
{
    int player_class;
} Player;