#ifndef MODDED_JOKERS_GFX_H
#define MODDED_JOKERS_GFX_H

#include <stdbool.h>

// This is the only function we expose to the vanilla game
bool get_modded_joker_gfx(int joker_id, const unsigned int** out_tiles, const unsigned short** out_pal);

#endif