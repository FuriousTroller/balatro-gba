#include "joker.h"
#include "game.h"
#include <stddef.h>

#include "custom_joker_sheet_0.h"
#include "custom_joker_sheet_1.h"
#include "custom_joker_sheet_2.h"
#include "custom_joker_sheet_3.h"

// #include "custom_joker_sheet_x.h" // Add this when you make IDs 1xx & 1xx!

// Creates a local shared memory struct specifically for your modded cards to use
static JokerEffect shared_joker_effect = {0};

// Tells the compiler to go find this variable inside game.c
extern int overkill_payout;
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

static u32 jaker_joker_effect(
    Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
)
{
    // Jaker modifies hands at the start of the round, so his scoring effect is empty!
    return JOKER_EFFECT_FLAG_NONE; 
}

static u32 voor_joker_effect(
    Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
)
{
    // Start with 2 Mult when conjured or bought
    if (joker_event == JOKER_EVENT_ON_JOKER_CREATED) {
        joker->persistent_state = 2; 
    }
    
    // Add the stored Mult to the score!
    if (joker_event == JOKER_EVENT_INDEPENDENT && joker->persistent_state > 0) {
        *joker_effect = &modded_shared_joker_effect;
        (*joker_effect)->mult = joker->persistent_state; 
        return JOKER_EFFECT_FLAG_MULT;
    }
    return JOKER_EFFECT_FLAG_NONE; 
}

static u32 capacocha_joker_effect(Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
) 
{
    if (joker_event == JOKER_EVENT_ON_JOKER_CREATED) {
        joker->persistent_state = 2; // Starts with exactly 2 uses
    }
    
    // Check for expiration at the end of the round
    if (joker_event == JOKER_EVENT_ON_ROUND_END) {
        if (joker->persistent_state <= 0) {
            *joker_effect = &shared_joker_effect;
            (*joker_effect)->message = "Sacrificed!";
            (*joker_effect)->expire = true;
            return JOKER_EFFECT_FLAG_MESSAGE | JOKER_EFFECT_FLAG_EXPIRE;
        }
    }
    return JOKER_EFFECT_FLAG_NONE;
}

static u32 overkill_joker_effect(Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
) 
{
    if (joker_event == JOKER_EVENT_ON_ROUND_END) {
        if (overkill_payout > 0) {
            *joker_effect = &shared_joker_effect;
            (*joker_effect)->money = overkill_payout; // The engine reads this directly
            return JOKER_EFFECT_FLAG_MONEY;           // Triggers the popup in joker.c
        }
    }
    return JOKER_EFFECT_FLAG_NONE;
}

// --- CLANKER MODE EXCLUSIVES ---

static u32 jamming_joker_effect(Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
) 
{
    // Passive: Logic handled externally during AI's turn
    return JOKER_EFFECT_FLAG_NONE;
}

static u32 captcha_joker_effect(Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
) 
{
    // Passive: Logic handled externally during AI's scoring loop
    return JOKER_EFFECT_FLAG_NONE;
}

static u32 ddos_joker_effect(Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
) 
{
    return JOKER_EFFECT_FLAG_NONE; // Passive: Handled at AI Turn Start
}

static u32 trojan_joker_effect(Joker* joker, 
    Card* scored_card, 
    enum JokerEvent joker_event, 
    JokerEffect** joker_effect
) 
{
    return JOKER_EFFECT_FLAG_NONE; // Passive: Handled at Score Compare
}

// --- 2. YOUR MODDED REGISTRY ---

// The engine knows to start reading this array at ID 100.
// So Index 0 is ID 100 (Mobius), Index 1 is ID 101 (Last Dance).
// Because we set NUM_JOKERS_PER_SPRITESHEET to 2, 
// Mobius reads the Left half, Last Dance reads the Right half!
const JokerInfo modded_joker_registry[] = {
    { UNCOMMON_JOKER,        7,      mobius_joker_effect           }, // Index 0 -> ID 100 (Mobius)
    { RARE_JOKER,            20,     last_dance_joker_effect       }, // Index 1 -> ID 101 (Last Dance)
    { COMMON_JOKER,          7,      voor_joker_effect             }, // Index 2 -> ID 102 (Voor)
    { UNCOMMON_JOKER,        10,     jaker_joker_effect            }, // Index 3 -> ID 103 (Jaker)
    { RARE_JOKER,            18,     capacocha_joker_effect        }, // Index 4 -> ID 104 (Capacocha)
    { COMMON_JOKER,          6,      overkill_joker_effect         }, // Index 5 -> ID 105 (Overkill)
    { RARE_JOKER,            17,     jamming_joker_effect          }, // ID 106 (Jamming) Clanker
    { RARE_JOKER,            13,     captcha_joker_effect          }, // ID 107 (CaptchA) Clanker
    { RARE_JOKER,            15,     ddos_joker_effect             }, // ID 108 (DDoS Attack)
    { UNCOMMON_JOKER,        12,     trojan_joker_effect           }, // ID 109 (Trojan Joker)
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