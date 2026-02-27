/**
 * @file debug.h
 *
 * @brief Debug/cheat mode for development and testing.
 *
 * All debug features are gated behind DEBUG_ENABLED.
 * Set DEBUG_ENABLED to 1 in this file to activate debug mode.
 * Individual compile-time overrides can be configured below.
 *
 * In-game keybinds (only active when DEBUG_ENABLED is 1):
 *   SELECT + B     : Open joker picker list (scrollable, press A to add)
 *   SELECT + A     : Add $100 to current money
 *   SELECT + DOWN  : Instantly win current round (set score to requirement)
 *   SELECT + L     : Add +1 hand and +1 discard
 *
 * Compile-time only (edit debug.h):
 *   DEBUG_FORCE_JOKER_ID : Force-add a joker at round start
 *   DEBUG_START_MONEY    : Override starting money
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <tonc.h>
#include <stdbool.h>

/* ========================================================================
 * MASTER SWITCH - set to 1 to enable all debug features, 0 to disable
 * ======================================================================== */
#define DEBUG_ENABLED 1

/* ========================================================================
 * COMPILE-TIME OVERRIDES
 * These only take effect when DEBUG_ENABLED is 1.
 * Set a value to UNDEFINED (-1) to leave it at the game's default.
 * ======================================================================== */
#if DEBUG_ENABLED

/* Starting money override (-1 = use default) */
#define DEBUG_START_MONEY         (-1)

/* Force a specific joker to be added at round start (-1 = don't force) */
#define DEBUG_FORCE_JOKER_ID      (-1)

#endif /* DEBUG_ENABLED */

/* ========================================================================
 * DEBUG API - called from game.c
 * These are all no-ops when DEBUG_ENABLED is 0.
 * ======================================================================== */

#if DEBUG_ENABLED

/**
 * @brief Called once during game_init(). Applies compile-time overrides.
 */
void debug_on_game_init(void);

/**
 * @brief Called every frame from game_update(). Handles debug input.
 *
 * Must be called after key_poll() / VBlankIntrWait.
 */
void debug_on_game_update(void);

/**
 * @brief Called at the start of each round (game_round_on_init).
 *        Applies per-round overrides like forced jokers.
 */
void debug_on_round_init(void);

/**
 * @brief Returns true if the debug joker picker overlay is currently active.
 *        When active, normal game input should be suppressed.
 */
bool debug_is_overlay_active(void);



#else /* !DEBUG_ENABLED */

/* No-op stubs so game.c doesn't need #if everywhere */
static inline void debug_on_game_init(void)   {}
static inline void debug_on_game_update(void)  {}
static inline void debug_on_round_init(void)   {}
static inline bool debug_is_overlay_active(void) { return false; }

#endif /* DEBUG_ENABLED */

#endif /* DEBUG_H */
