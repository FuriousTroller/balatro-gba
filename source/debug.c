/**
 * @file debug.c
 *
 * @brief Debug/cheat mode implementation for development and testing.
 *
 * All code in this file is compiled only when DEBUG_ENABLED is set to 1
 * in debug.h.
 *
 * In-game keybinds (active during gameplay when SELECT is held):
 *   SELECT + B     : Toggle joker picker overlay
 *   SELECT + A     : Add $100 to current money
 *   SELECT + DOWN  : Win current round (set score >= blind requirement)
 *   SELECT + L     : Add +1 hand and +1 discard
 *
 * Compile-time only (in debug.h):
 *   DEBUG_START_MONEY    : Override starting money
 *   DEBUG_FORCE_JOKER_ID : Force-add a joker at round start
 */

#include "debug.h"

#if DEBUG_ENABLED

#include "game.h"
#include "joker.h"
#include "graphic_utils.h"
#include "blind.h"
#include "list.h"
#include "util.h"

#include <tonc.h>
#include <stdio.h>
#include <string.h>

extern size_t get_modded_registry_size(void);

/* ========================================================================
 * Internal state
 * ======================================================================== */

/* Joker picker overlay state */
static bool overlay_active    = false;
static int  picker_cursor     = 0;
static int  picker_scroll_top = 0;
static bool picker_needs_redraw = false;

/* Number of visible rows in the picker (GBA screen = 160px, 8px per row,
 * reserve 2 rows for header/footer) */
#define PICKER_VISIBLE_ROWS 16
#define PICKER_HEADER_ROWS  2
#define PICKER_BODY_ROWS    (PICKER_VISIBLE_ROWS - PICKER_HEADER_ROWS)

/* Debounce: prevent repeated triggers while key is held */
static u16 prev_keys = 0;

/* Short joker name table – maps joker IDs to short display names.
 * Vanilla jokers: IDs 0-59 (indices match ID directly).
 * Modded jokers:  IDs start at 100 (MODDED_JOKER_START_ID in modded_joker_effects.c).
 *
 * HOW TO ADD A NEW MODDED JOKER:
 *   1. Add its effect + entry in modded_joker_effects.c (gets ID 100, 101, 102...).
 *   2. Append a matching entry to debug_modded_joker_names[] below, in the same order.
 *      The index in that array is the local modded index (0 = ID 100, 1 = ID 101, etc.).
 */

/* Vanilla joker names, indexed directly by joker ID 0-59. */
static const char* const joker_names[] = {
    [0]  = "Joker",
    [1]  = "Greedy",
    [2]  = "Lusty",
    [3]  = "Wrathful",
    [4]  = "Glutton",
    [5]  = "Jolly",
    [6]  = "Zany",
    [7]  = "Mad",
    [8]  = "Crazy",
    [9]  = "Droll",
    [10] = "Sly",
    [11] = "Wily",
    [12] = "Clever",
    [13] = "Devious",
    [14] = "Crafty",
    [15] = "Half",
    [16] = "Stencil",
    [17] = "Photo",
    [18] = "WalkTalk",
    [19] = "Banner",
    [20] = "Bboard",
    [21] = "MystSumt",
    [22] = "Misprint",
    [23] = "EvnStevn",
    [24] = "Blue",
    [25] = "OddTodd",
    [26] = "Shortcut",
    [27] = "BizCard",
    [28] = "ScaryFce",
    [29] = "Bootstrp",
    [30] = "Pareidol",
    [31] = "ResvPark",
    [32] = "Abstract",
    [33] = "Bull",
    [34] = "The Duo",
    [35] = "The Trio",
    [36] = "Family",
    [37] = "Order",
    [38] = "Tribe",
    [39] = "Blueprnt",
    [40] = "Brain",
    [41] = "RsdFist",
    [42] = "SmileyFc",
    [43] = "Acrobat",
    [44] = "Dusk",
    [45] = "Sock&Bus",
    [46] = "Hack",
    [47] = "HangChad",
    [48] = "4Fingers",
    [49] = "Scholar",
    [50] = "Fibonacc",
    [51] = "Seltzer",
    [52] = "Golden",
    [53] = "GrosMich",
    [54] = "Cavendsh",
    [55] = "Supernov",
    [56] = "Green",
    [57] = "Square",
    [58] = "Smeared",
    [59] = "FlshCard",
};

/* Modded joker names, indexed by LOCAL modded index (0 = ID 100, 1 = ID 101, ...).
 * Add a new entry here whenever you add a joker to modded_joker_effects.c. */
static const char* const debug_modded_joker_names[] = {
    [0] = "Recursion",       // ID 100
    [1] = "LastDance",       // ID 101
    [2] = "Joker Voorhees",  // ID 102
    [3] = "Jaker",           // ID 103
    [4] = "Capacocha",       // ID 104
    [5] = "Overkill",        // ID 105
    [6] = "Jamming",         // ID 106 (Clanker mode)
    [7] = "Captcha",         // ID 107 (Clanker mode)

    /* [4] = "YourNext",  // ID xxx – add your next modded joker here */
};

#define NUM_NAMED_JOKERS      (int)(sizeof(joker_names) / sizeof(joker_names[0]))
#define NUM_NAMED_MODDED      (int)(sizeof(debug_modded_joker_names) / sizeof(debug_modded_joker_names[0]))
#define MODDED_JOKER_START_ID 100

/* Returns the real in-game joker ID for a given picker display index.
 * Vanilla jokers occupy display indices 0 .. (vanilla_count-1).
 * Modded jokers follow immediately, mapping to IDs 100, 101, ... */
static int debug_picker_idx_to_joker_id(int idx)
{
    int vanilla_count = (int)(get_joker_registry_size() - get_modded_registry_size());
    if (idx < vanilla_count)
        return idx;
    return MODDED_JOKER_START_ID + (idx - vanilla_count);
}

static const char* debug_get_joker_name(int joker_id)
{
    if (joker_id < MODDED_JOKER_START_ID)
    {
        if (joker_id >= 0 && joker_id < NUM_NAMED_JOKERS && joker_names[joker_id] != NULL)
            return joker_names[joker_id];
    }
    else
    {
        int local = joker_id - MODDED_JOKER_START_ID;
        if (local >= 0 && local < NUM_NAMED_MODDED && debug_modded_joker_names[local] != NULL)
            return debug_modded_joker_names[local];
    }
    return NULL;
}

/* ========================================================================
 * Joker picker overlay drawing
 * ======================================================================== */

/* Full-screen text rect for the overlay (in pixels) */
static const Rect DEBUG_OVERLAY_RECT = {0, 0, 240, 160};

static void debug_draw_picker(void)
{
    picker_needs_redraw = false;

    /* Clear the text layer */
    tte_erase_rect_wrapper(DEBUG_OVERLAY_RECT);

    int total_jokers = (int)get_joker_registry_size();

    /* Header */
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}== JOKER PICKER (%d) ==",
        4, 0, TTE_WHITE_PB, total_jokers
    );
    tte_printf(
        "#{P:%d,%d; cx:0x%X000}UP/DN:Scroll A:Add B:Close",
        4, 8, TTE_YELLOW_PB
    );

    /* Body: list of jokers */
    int y = PICKER_HEADER_ROWS * 8;
    for (int i = 0; i < PICKER_BODY_ROWS; i++)
    {
        int idx = picker_scroll_top + i;
        if (idx >= total_jokers)
            break;

        int joker_id = debug_picker_idx_to_joker_id(idx);
        const char* name = debug_get_joker_name(joker_id);
        char line[30];

        if (name)
            snprintf(line, sizeof(line), "%s%3d %-8s", (idx == picker_cursor) ? ">" : " ", joker_id, name);
        else
            snprintf(line, sizeof(line), "%sID#%d", (idx == picker_cursor) ? ">" : " ", joker_id);

        int pb = (idx == picker_cursor) ? TTE_RED_PB : TTE_WHITE_PB;
        bool owned = is_joker_owned(joker_id);
        if (owned)
            pb = TTE_BLUE_PB;

        tte_printf(
            "#{P:%d,%d; cx:0x%X000}%s%s",
            4, y, pb, line, owned ? " [OWN]" : ""
        );

        y += 8;
    }
}

static void debug_picker_add_joker(int joker_id)
{
    List* jokers_list = get_jokers_list();
    if (list_get_len(jokers_list) >= MAX_JOKERS_HELD_SIZE)
        return;
    if (is_joker_owned(joker_id))
        return;

    Joker* joker = joker_new((u8)joker_id);
    if (!joker)
        return;

    JokerObject* joker_object = joker_object_new(joker);
    if (!joker_object)
    {
        joker_destroy(&joker);
        return;
    }

    /* Position off-screen; held_jokers_update_loop will animate it in */
    joker_object->sprite_object->x  = int2fx(108);
    joker_object->sprite_object->y  = int2fx(10);
    joker_object->sprite_object->tx = int2fx(108);
    joker_object->sprite_object->ty = int2fx(10);

    /* Use the public list interface to add */
    list_push_back(jokers_list, joker_object);
}

/* ========================================================================
 * Overlay input processing
 * ======================================================================== */

static void debug_process_picker_input(void)
{
    int total_jokers = (int)get_joker_registry_size();
    u16 keys_now = ~REG_KEYINPUT & KEY_MASK;
    u16 keys_hit = keys_now & ~prev_keys;

    /* Close overlay */
    if (keys_hit & KEY_B)
    {
        overlay_active = false;
        REG_DISPCNT |= DCNT_OBJ; /* restore sprites */
        tte_erase_rect_wrapper(DEBUG_OVERLAY_RECT); /* clear picker text */
        game_refresh_hud(); /* redraw all HUD text the picker erased */
        prev_keys = keys_now;
        return;
    }

    /* Navigate with wrap-around */
    if (keys_hit & KEY_UP)
    {
        if (total_jokers > 0)
        {
            if (picker_cursor == 0)
                picker_cursor = total_jokers - 1;
            else
                picker_cursor--;

            /* adjust scroll so cursor stays visible */
            if (picker_cursor < picker_scroll_top)
                picker_scroll_top = picker_cursor;
            else if (picker_cursor >= picker_scroll_top + PICKER_BODY_ROWS)
                picker_scroll_top = picker_cursor - PICKER_BODY_ROWS + 1;
        }
        picker_needs_redraw = true;
    }
    if (keys_hit & KEY_DOWN)
    {
        if (total_jokers > 0)
        {
            if (picker_cursor >= total_jokers - 1)
                picker_cursor = 0;
            else
                picker_cursor++;

            if (picker_cursor >= picker_scroll_top + PICKER_BODY_ROWS)
                picker_scroll_top = picker_cursor - PICKER_BODY_ROWS + 1;
            else if (picker_cursor < picker_scroll_top)
                picker_scroll_top = picker_cursor;
        }
        picker_needs_redraw = true;
    }

    /* Page up/down with L/R */
    if (keys_hit & KEY_L)
    {
        if (total_jokers > 0)
        {
            picker_cursor = (picker_cursor - PICKER_BODY_ROWS + total_jokers) % total_jokers;
            if (picker_cursor < picker_scroll_top)
                picker_scroll_top = picker_cursor;
            else if (picker_cursor >= picker_scroll_top + PICKER_BODY_ROWS)
                picker_scroll_top = picker_cursor - PICKER_BODY_ROWS + 1;
        }
        picker_needs_redraw = true;
    }
    if (keys_hit & KEY_R)
    {
        if (total_jokers > 0)
        {
            picker_cursor = (picker_cursor + PICKER_BODY_ROWS) % total_jokers;
            if (picker_cursor >= picker_scroll_top + PICKER_BODY_ROWS)
                picker_scroll_top = picker_cursor - PICKER_BODY_ROWS + 1;
            else if (picker_cursor < picker_scroll_top)
                picker_scroll_top = picker_cursor;
        }
        picker_needs_redraw = true;
    }

    /* Add joker */
    if (keys_hit & KEY_A)
    {
        debug_picker_add_joker(debug_picker_idx_to_joker_id(picker_cursor));
        picker_needs_redraw = true;
    }

    prev_keys = keys_now;

    /* Only redraw when something changed */
    if (picker_needs_redraw)
        debug_draw_picker();
}

/* ========================================================================
 * Public API
 * ======================================================================== */

void debug_on_game_init(void)
{
#if DEBUG_START_MONEY >= 0
    set_money(DEBUG_START_MONEY);
#endif
}

void debug_on_game_update(void)
{
    u16 keys_now = ~REG_KEYINPUT & KEY_MASK;
    u16 keys_hit = keys_now & ~prev_keys;

    /* If the picker overlay is active, only process picker input */
    if (overlay_active)
    {
        debug_process_picker_input();
        return;
    }

    /* All debug keybinds require SELECT to be held */
    if (!(keys_now & KEY_SELECT))
    {
        prev_keys = keys_now;
        return;
    }

    /* SELECT + B : Open joker picker */
    if (keys_hit & KEY_B)
    {
        overlay_active = true;
        REG_DISPCNT &= ~DCNT_OBJ; /* hide sprites so text is unobscured */
        picker_needs_redraw = true;
        debug_draw_picker();
        prev_keys = keys_now;
        return;
    }

    /* SELECT + A : Add $100 */
    if (keys_hit & KEY_A)
    {
        set_money(get_money() + 100);
        display_money();
    }

    /* SELECT + DOWN : Win current round by setting score very high */
    if (keys_hit & KEY_DOWN)
    {
        /* Set score to a huge value to guarantee passing the blind */
        set_chips(999999);
        set_mult(999);
    }

    /* SELECT + L : Add +1 hand and +1 discard */
    if (keys_hit & KEY_L)
    {
        set_num_hands_remaining(get_num_hands_remaining() + 1);
        set_num_discards_remaining(get_num_discards_remaining() + 1);
    }

    prev_keys = keys_now;
}

void debug_on_round_init(void)
{
#if DEBUG_FORCE_JOKER_ID >= 0
    /* Force-add a specific joker if not already owned */
    if (!is_joker_owned(DEBUG_FORCE_JOKER_ID))
    {
        debug_picker_add_joker(DEBUG_FORCE_JOKER_ID);
    }
#endif
}

bool debug_is_overlay_active(void)
{
    return overlay_active;
}

#endif /* DEBUG_ENABLED */
