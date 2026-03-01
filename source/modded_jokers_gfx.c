// --- source/modded_jokers_gfx.c ---
#include "modded_jokers_gfx.h"
#include <stddef.h>

// 1. Include your 64x32 spritesheet header here
// (Make sure you ran 'make' so grit generates this from your .png!)
#include "custom_joker_sheet_0.h"
#include "custom_joker_sheet_1.h"
#include "custom_joker_sheet_2.h"
#include "custom_joker_sheet_3.h"
#include "custom_joker_sheet_4.h"

#define MODDED_JOKER_START_ID 100
#define NUM_JOKERS_PER_SPRITESHEET 2

// 2. Put your sheets into the arrays
static const unsigned int* modded_joker_tiles[] = { 
    custom_joker_sheet_0Tiles, 
    custom_joker_sheet_1Tiles,
    custom_joker_sheet_2Tiles,
    custom_joker_sheet_3Tiles,
    custom_joker_sheet_4Tiles
};

static const unsigned short* modded_joker_pals[] = { 
    custom_joker_sheet_0Pal, 
    custom_joker_sheet_1Pal,
    custom_joker_sheet_2Pal,
    custom_joker_sheet_3Pal,
    custom_joker_sheet_4Pal
};

#define NUM_MODDED_SHEETS (sizeof(modded_joker_tiles) / sizeof(modded_joker_tiles[0]))

// 3. The Bypass Function the linker is looking for!
bool get_modded_joker_gfx(int joker_id, 
    const unsigned int** out_tiles, 
    const unsigned short** out_pal
) 
{
    if (joker_id >= MODDED_JOKER_START_ID) 
    {
        // Math to figure out WHICH sheet the Joker is on
        int local_idx = joker_id - MODDED_JOKER_START_ID;
        int sheet_idx = local_idx / NUM_JOKERS_PER_SPRITESHEET;
        
        if (sheet_idx >= NUM_MODDED_SHEETS) 
        {
            sheet_idx = 0; // Fallback to sheet 0 to prevent a crash
        }
        
        // Pass back the whole sheet!
        *out_tiles = modded_joker_tiles[sheet_idx];
        *out_pal = modded_joker_pals[sheet_idx];
        return true; 
    }
    
    return false;
}