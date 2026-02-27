#ifndef AI_PLAYER_H
#define AI_PLAYER_H

#include "card.h"
#include "game.h"

#include <tonc.h>

// Number of frames the AI "thinks" before selecting and playing its hand
#define AI_THINK_DELAY_FRAMES 40

/**
 * @brief Selects the best subset of cards (1–5 cards) for the AI to play.
 *
 * Evaluates all non-empty subsets of the given hand (up to MAX_SELECTION_SIZE),
 * estimates the score for each subset (hand type × joker bonuses), and marks
 * the highest-scoring subset in out_sel.
 *
 * @param hand           Array of Card* representing the AI's current hand.
 * @param count          Number of cards available (>= 0).
 * @param out_sel        Output boolean array (same size as hand). Set to true
 *                       for selected cards, false otherwise. Caller must
 *                       zero-initialise.
 * @param out_hand_type  If non-NULL, receives the HandType of the best combo.
 * @return               Number of cards selected (0 if count == 0).
 */
int ai_select_best_hand(Card** hand, int count, bool* out_sel,
                        enum HandType* out_hand_type);

#endif // AI_PLAYER_H
