#include "joker.h"
#include "game.h"
#include <stddef.h>

#include "custom_joker_sheet_0.h"
// #include "custom_joker_sheet_1.h" // Add this when you make IDs 102 & 103!

#define MODDED_JOKER_START_ID 100
#define NUM_JOKERS_PER_SPRITESHEET 2

// --- 0. LOCAL EFFECT OBJECT ---
// We use this local object so we don't conflict with the vanilla file's locked memory
static JokerEffect modded_shared_joker_effect = {0};


// --- 1. YOUR CUSTOM JOKER LOGIC ---

static u32 mobius_joker_effect(
    Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
)
{
    // You can add your actual Mobius logic here whenever you are ready!
    return JOKER_EFFECT_FLAG_NONE; 
}

static u32 last_dance_joker_effect(
    Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
)
{
    if (joker_event == JOKER_EVENT_INDEPENDENT) {
        // Point to our safe local variable
        *joker_effect = &modded_shared_joker_effect;
        
        // x3 Total Mult
        (*joker_effect)->xmult = 3; 
        
        // x2 Total Chips (By adding 100% of our current chips to the pool!)
        (*joker_effect)->chips = get_chips(); 
        
        return JOKER_EFFECT_FLAG_XMULT | JOKER_EFFECT_FLAG_CHIPS;
    }
    
    return JOKER_EFFECT_FLAG_NONE; 
}


// --- 2. YOUR MODDED REGISTRY ---

// The engine knows to start reading this array at ID 100.
// So Index 0 is ID 100 (Mobius), Index 1 is ID 101 (Last Dance).
// Because we set NUM_JOKERS_PER_SPRITESHEET to 2, 
// Mobius reads the Left half, Last Dance reads the Right half!
const JokerInfo modded_joker_registry[] = {
    { UNCOMMON_JOKER,  7, mobius_joker_effect     }, // Local Index 0 -> In-game ID: 100
    { RARE_JOKER, 14, last_dance_joker_effect }, // Local Index 1 -> In-game ID: 101
};


// --- 3. HELPER FUNCTIONS FOR THE ENGINE ---
// (Do not change these! The vanilla game uses them to read your mods safely)

size_t get_modded_registry_size(void) 
{
    return sizeof(modded_joker_registry) / sizeof(modded_joker_registry[0]);
}

const JokerInfo* get_modded_registry_entry(int local_id) 
{
    return &modded_joker_registry[local_id];
}