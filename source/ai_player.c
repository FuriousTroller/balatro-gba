/**
 * ai_player.c — AI opponent logic for the vs-AI mod.
 *
 * Provides a purely algorithmic card-selection routine that the game loop can
 * invoke instead of player input when it is the AI's turn.
 *
 * The AI does NOT simulate joker effects in its hand-evaluation heuristic.
 * The actual engine scoring path runs unchanged during the AI's turn, so
 * every joker the player owns (DEFAULT_JOKER, GREEDY_JOKER, etc.) applies
 * its effect to the AI's scored cards exactly as it would for the player.
 */

#include "ai_player.h"

#include "card.h"
#include "game.h"
#include "hand_analysis.h"

#include <tonc.h>

/* -----------------------------------------------------------------------
 * Local copy of hand base values (mirrors the static table in game.c so
 * ai_player.c does not need an exported getter).
 * ----------------------------------------------------------------------- */
typedef struct
{
    u32 chips;
    u32 mult;
} AIHandBaseValues;

static const AIHandBaseValues ai_hand_base[] = {
    {  0,   0 }, // NONE
    {  5,   1 }, // HIGH_CARD
    { 10,   2 }, // PAIR
    { 20,   2 }, // TWO_PAIR
    { 30,   3 }, // THREE_OF_A_KIND
    { 30,   4 }, // STRAIGHT
    { 35,   4 }, // FLUSH
    { 40,   4 }, // FULL_HOUSE
    { 60,   7 }, // FOUR_OF_A_KIND
    {100,   8 }, // STRAIGHT_FLUSH
    {100,   8 }, // ROYAL_FLUSH
    {120,  12 }, // FIVE_OF_A_KIND
    {140,  14 }, // FLUSH_HOUSE
    {160,  16 }, // FLUSH_FIVE
};

/* -----------------------------------------------------------------------
 * Standalone hand-type computation.
 *
 * Mirrors compute_contained_hand_types() / compute_hand_type() in game.c
 * but operates on an explicit Card** array so it doesn't touch the global
 * hand[]/selection state.
 * ----------------------------------------------------------------------- */
static enum HandType ai_compute_hand_type(Card** cards, int count)
{
    if (count <= 0)
        return NONE;

    u8 ranks[NUM_RANKS] = {0};
    u8 suits[NUM_SUITS] = {0};

    for (int i = 0; i < count; i++)
    {
        if (cards[i])
        {
            ranks[cards[i]->rank]++;
            suits[cards[i]->suit]++;
        }
    }

    /* Build a ContainedHandTypes bitfield.
     * This replicates compute_contained_hand_types() logic exactly. */
    ContainedHandTypes ct = {0};

    ct.HIGH_CARD = 1;

    u8 n = hand_contains_n_of_a_kind(ranks);

    if (n >= 2)
    {
        ct.PAIR = 1;
        if (hand_contains_two_pair(ranks))
            ct.TWO_PAIR = 1;
    }

    if (n >= 3)
        ct.THREE_OF_A_KIND = 1;

    /* NOTE: hand_contains_straight / hand_contains_flush use
     * get_straight_and_flush_size() and is_shortcut_joker_active() from game.c.
     * During the AI's turn those globals reflect the AI's (empty) joker list,
     * so straight_size == 5 and shortcut == false — which is the desired
     * behaviour. */
    if (hand_contains_straight(ranks))
        ct.STRAIGHT = 1;

    if (hand_contains_flush(suits))
        ct.FLUSH = 1;

    if (n >= 3 && hand_contains_full_house(ranks))
        ct.FULL_HOUSE = 1;

    if (n >= 4)
        ct.FOUR_OF_A_KIND = 1;

    if (ct.STRAIGHT && ct.FLUSH)
        ct.STRAIGHT_FLUSH = 1;

    if (ct.STRAIGHT_FLUSH &&
        ranks[TEN] && ranks[JACK] && ranks[QUEEN] && ranks[KING] && ranks[ACE])
        ct.ROYAL_FLUSH = 1;

    if (n >= 5)
        ct.FIVE_OF_A_KIND = 1;

    if (ct.FLUSH)
    {
        if (ct.FULL_HOUSE)
            ct.FLUSH_HOUSE = 1;
        if (ct.FIVE_OF_A_KIND)
            ct.FLUSH_FIVE = 1;
    }

    /* Return the highest hand type by iterating from strongest to weakest. */
    for (enum HandType h = FLUSH_FIVE; h > NONE; h--)
    {
        if ((ct.value >> (h - 1)) & 0x1)
            return h;
    }

    return NONE;
}

/* -----------------------------------------------------------------------
 * Score estimate for a card combination.
 *
 * Used only for comparison during AI hand selection; does NOT modify any
 * global game state.
 * ----------------------------------------------------------------------- */
static u32 ai_score_combo(Card** cards, int count)
{
    enum HandType ht = ai_compute_hand_type(cards, count);
    if (ht == NONE)
        return 0;

    u32 c = ai_hand_base[ht].chips;
    u32 m = ai_hand_base[ht].mult;

    /* Add chip value of each played card (mirrors PLAY_SCORING_CARDS). */
    for (int i = 0; i < count; i++)
    {
        if (cards[i])
            c += card_get_value(cards[i]);
    }

    /* Guard against 32-bit overflow (mirrors u32_protected_mult in game.c). */
    if (c == 0 || m == 0)
        return 0;
    if (c > 0xFFFFFFFF / m)
        return 0xFFFFFFFF;

    return c * m;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

int ai_select_best_hand(Card** hand, int count, bool* out_sel,
                        enum HandType* out_hand_type)
{
    if (count <= 0)
    {
        if (out_hand_type) *out_hand_type = NONE;
        return 0;
    }

    for (int i = 0; i < count; i++)
        out_sel[i] = false;

    /* Limit to MAX_SELECTION_SIZE cards and to what is actually available. */
    int max_sel = (count < MAX_SELECTION_SIZE) ? count : MAX_SELECTION_SIZE;

    u32           best_score = 0;
    int           best_mask  = 0;
    int           best_count = 0;
    enum HandType best_ht    = NONE;

    /* Enumerate every non-empty subset by bitmask.
     * count <= AI_HAND_SIZE (8) so the limit is at most 255 iterations. */
    int limit = 1 << count;

    Card* combo[MAX_SELECTION_SIZE];

    for (int mask = 1; mask < limit; mask++)
    {
        /* Count bits (Kernighan method). */
        int n   = 0;
        int tmp = mask;
        while (tmp) { n += tmp & 1; tmp >>= 1; }

        if (n > max_sel)
            continue;

        /* Build the combo array for this subset. */
        int ci = 0;
        for (int i = 0; i < count && ci < MAX_SELECTION_SIZE; i++)
        {
            if (mask & (1 << i))
                combo[ci++] = hand[i];
        }

        u32 s = ai_score_combo(combo, n);

        if (s > best_score)
        {
            best_score = s;
            best_mask  = mask;
            best_count = n;
            best_ht    = ai_compute_hand_type(combo, n);
        }
    }

    /* Commit the winning selection. */
    for (int i = 0; i < count; i++)
        out_sel[i] = (best_mask & (1 << i)) != 0;

    if (out_hand_type) *out_hand_type = best_ht;
    return best_count;
}
