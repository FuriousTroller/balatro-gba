#include "hand_analysis.h"

#include "card.h"
#include "game.h"

void get_hand_distribution(u8 ranks_out[NUM_RANKS], u8 suits_out[NUM_SUITS])
{
    for (int i = 0; i < NUM_RANKS; i++)
        ranks_out[i] = 0;
    for (int i = 0; i < NUM_SUITS; i++)
        suits_out[i] = 0;

    CardObject** cards = get_hand_array();
    int top = get_hand_top();
    for (int i = 0; i <= top; i++)
    {
        if (cards[i] && card_object_is_selected(cards[i]))
        {
            ranks_out[cards[i]->card->rank]++;
            suits_out[cards[i]->card->suit]++;
        }
    }
}

void get_played_distribution(u8 ranks_out[NUM_RANKS], u8 suits_out[NUM_SUITS])
{
    for (int i = 0; i < NUM_RANKS; i++)
        ranks_out[i] = 0;
    for (int i = 0; i < NUM_SUITS; i++)
        suits_out[i] = 0;

    CardObject** played = get_played_array();
    int top = get_played_top();
    for (int i = 0; i <= top; i++)
    {
        /* The difference from get_hand_distribution() (not checking if card is selected)
         * is in line Balatro behavior,
         * see https://github.com/GBALATRO/balatro-gba/issues/341#issuecomment-3691363488
         */
        if (!played[i])
            continue;
        ranks_out[played[i]->card->rank]++;
        suits_out[played[i]->card->suit]++;
    }
}

// Returns the highest N of a kind. So a full-house would return 3.
u8 hand_contains_n_of_a_kind(u8* ranks)
{
    u8 highest_n = 0;
    for (int i = 0; i < NUM_RANKS; i++)
    {
        if (ranks[i] > highest_n)
            highest_n = ranks[i];
    }
    return highest_n;
}

bool hand_contains_two_pair(u8* ranks)
{
    bool contains_other_pair = false;
    for (int i = 0; i < NUM_RANKS; i++)
    {
        if (ranks[i] >= 2)
        {
            if (contains_other_pair)
                return true;
            contains_other_pair = true;
        }
    }
    return false;
}

bool hand_contains_full_house(u8* ranks)
{
    int count_three = 0;
    int count_pair = 0;
    for (int i = 0; i < NUM_RANKS; i++)
    {
        if (ranks[i] >= 3)
        {
            count_three++;
        }
        else if (ranks[i] >= 2)
        {
            count_pair++;
        }
    }
    // Full house if there is:
    // - at least one three-of-a-kind and at least one other pair,
    // - OR at least two three-of-a-kinds (second "three" acts as pair).
    // This accounts for hands with 6 or more cards even though
    // they are currently not possible and probably never will be.
    return (count_three >= 2 || (count_three && count_pair));
}

// This is mostly from Google Gemini
bool hand_contains_straight(u8* ranks)
{
    bool mobius = is_joker_owned(100);

    if (!is_shortcut_joker_active())
    {
        int straight_size = get_straight_and_flush_size();
        int run = 0;
        // If Mobius is active, loop further to allow ACE to wrap back to TWO
        int limit = mobius ? NUM_RANKS + straight_size : NUM_RANKS;
        
        for (int i = 0; i < limit; ++i)
        {
            if (ranks[i % NUM_RANKS])
            {
                if (++run >= straight_size)
                    return true;
            }
            else
            {
                run = 0;
            }
        }

        // Vanilla Ace-Low check (Only run if Mobius is OFF, as Mobius handles it naturally)
        if (!mobius && straight_size >= 2 && ranks[ACE])
        {
            int last_needed = TWO + (straight_size - 2);
            if (last_needed <= FIVE)
            {
                bool ok = true;
                for (int r = TWO; r <= last_needed; ++r)
                {
                    if (!ranks[r]) { ok = false; break; }
                }
                if (ok) return true;
            }
        }
        return false;
    }
    else
    {
        // Shortcut Joker is active (Dynamic Programming Approach)
        u8 longest_short_cut_at[NUM_RANKS] = {0};
        int ace_low_len = ranks[ACE] ? 1 : 0;
        int limit = mobius ? NUM_RANKS * 2 : NUM_RANKS; // Double scan for Mobius!
        
        for (int i = 0; i < limit; i++)
        {
            int r = i % NUM_RANKS;
            if (ranks[r] == 0)
            {
                longest_short_cut_at[r] = 0;
                continue;
            }

            int prev_len1 = 0;
            int prev_len2 = 0;

            if (r == TWO)
            {
                prev_len1 = mobius ? longest_short_cut_at[ACE] : ace_low_len;
                prev_len2 = mobius ? longest_short_cut_at[KING] : 0;
            }
            else if (r == THREE)
            {
                prev_len1 = longest_short_cut_at[TWO];
                prev_len2 = mobius ? longest_short_cut_at[ACE] : ace_low_len;
            }
            else if (r == ACE)
            {
                prev_len1 = longest_short_cut_at[KING];
                prev_len2 = longest_short_cut_at[QUEEN];
            }
            else
            {
                prev_len1 = longest_short_cut_at[r - 1];
                prev_len2 = longest_short_cut_at[r - 2];
            }

            longest_short_cut_at[r] = 1 + max(prev_len1, prev_len2);

            if (longest_short_cut_at[r] >= get_straight_and_flush_size())
                return true;
        }
    }
    return false;
}

bool hand_contains_flush(u8* suits)
{
    for (int i = 0; i < NUM_SUITS; i++)
    {
        if (suits[i] >= get_straight_and_flush_size())
        {
            return true;
        }
    }
    return false;
}

// Returns the number of cards in the best flush found
// or 0 if no flush of min_len is found, and marks them in out_selection.
/**
 * Finds the largest flush (set of cards with the same suit) in the given array of played cards.
 * Marks the cards belonging to the best flush in the out_selection array.
 *
 * @param played        Array of pointers to CardObject representing played cards.
 * @param top           Index of the top of the played stack.
 * @param min_len       Minimum number of cards required for a flush.
 * @param out_selection Output array of bools; set to true for cards in the best flush, false
 * otherwise.
 * @return              The number of cards in the best flush found, or 0 if no flush meets min_len.
 */
int find_flush_in_played_cards(CardObject** played, int top, int min_len, bool* out_selection)
{
    if (top < 0)
        return 0;
    for (int i = 0; i <= top; i++)
        out_selection[i] = false;

    int suit_counts[NUM_SUITS] = {0};
    for (int i = 0; i <= top; i++)
    {
        if (played[i] && played[i]->card)
        {
            suit_counts[played[i]->card->suit]++;
        }
    }

    int best_suit = -1;
    int best_count = 0;
    for (int i = 0; i < NUM_SUITS; i++)
    {
        if (suit_counts[i] > best_count)
        {
            best_count = suit_counts[i];
            best_suit = i;
        }
    }

    if (best_count >= min_len)
    {
        for (int i = 0; i <= top; i++)
        {
            if (played[i] && played[i]->card && played[i]->card->suit == best_suit)
            {
                out_selection[i] = true;
            }
        }
        return best_count;
    }
    return 0;
}

// Returns the number of cards in the best straight or 0 if no straight of min_len is found, marks
// as true them in out_selection[]. This is mostly from Google Gemini
int find_straight_in_played_cards(
    CardObject** played, int top, bool shortcut_active, int min_len, bool* out_selection)
{
    if (top < 0) return 0;
    for (int i = 0; i <= top; i++) out_selection[i] = false;

    u8 longest_straight_at[NUM_RANKS] = {0};
    int parent[NUM_RANKS];
    for (int i = 0; i < NUM_RANKS; i++) parent[i] = -1;

    u8 ranks[NUM_RANKS] = {0};
    for (int i = 0; i <= top; i++)
    {
        if (played[i] && played[i]->card)
            ranks[played[i]->card->rank]++;
    }

    bool mobius = is_joker_owned(100);
    int ace_low_len = ranks[ACE] ? 1 : 0;
    int limit = mobius ? NUM_RANKS * 2 : NUM_RANKS;

    for (int i = 0; i < limit; i++)
    {
        int r = i % NUM_RANKS;
        if (ranks[r] > 0)
        {
            int prev1 = 0, prev2 = 0;
            int parent1 = -1, parent2 = -1;

            if (shortcut_active)
            {
                if (r == TWO) {
                    prev1 = mobius ? longest_straight_at[ACE] : ace_low_len; parent1 = ACE;
                    prev2 = mobius ? longest_straight_at[KING] : 0; parent2 = KING;
                } else if (r == THREE) {
                    prev1 = longest_straight_at[TWO]; parent1 = TWO;
                    prev2 = mobius ? longest_straight_at[ACE] : ace_low_len; parent2 = ACE;
                } else if (r == ACE) {
                    prev1 = longest_straight_at[KING]; parent1 = KING;
                    prev2 = longest_straight_at[QUEEN]; parent2 = QUEEN;
                } else {
                    prev1 = longest_straight_at[r - 1]; parent1 = r - 1;
                    if (r > 1) { prev2 = longest_straight_at[r - 2]; parent2 = r - 2; }
                }
            }
            else
            {
                if (r == TWO) {
                    prev1 = mobius ? longest_straight_at[ACE] : ace_low_len; parent1 = ACE;
                } else if (r == ACE) {
                    prev1 = longest_straight_at[KING]; parent1 = KING;
                } else {
                    prev1 = longest_straight_at[r - 1]; parent1 = r - 1;
                }
            }

            if (prev1 >= prev2) {
                longest_straight_at[r] = 1 + prev1; parent[r] = parent1;
            } else {
                longest_straight_at[r] = 1 + prev2; parent[r] = parent2;
            }
        }
    }

    int best_len = 0;
    int end_rank = -1;
    for (int i = 0; i < NUM_RANKS; i++)
    {
        if (longest_straight_at[i] >= best_len)
        {
            best_len = longest_straight_at[i];
            end_rank = i;
        }
    }

    if (best_len >= min_len)
    {
        u8 needed_ranks[NUM_RANKS] = {0};
        int current_rank = end_rank;
        while (current_rank != -1 && best_len > 0)
        {
            needed_ranks[current_rank]++;
            current_rank = parent[current_rank];
            best_len--;
        }

        for (int i = 0; i <= top; i++)
        {
            if (played[i] && played[i]->card && needed_ranks[played[i]->card->rank] > 0)
            {
                out_selection[i] = true;
                needed_ranks[played[i]->card->rank]--;
            }
        }

        int final_card_count = 0;
        for (int i = 0; i <= top; i++) if (out_selection[i]) final_card_count++;
        return final_card_count;
    }
    return 0;
}

// This is used for the special case in "Four Fingers" where you can add a pair into a straight
// (e.g. AA234 should score all 5 cards)
void select_paired_cards_in_hand(CardObject** played, int played_top, bool* selection)
{
    // Build a set of ranks that are already selected
    bool rank_selected[NUM_RANKS] = {0};
    bool any_selected_rank = false;

    for (int i = 0; i <= played_top; i++)
    {
        if (selection[i] && played[i] && played[i]->card)
        {
            rank_selected[played[i]->card->rank] = true;
            any_selected_rank = true;
        }
    }

    // If no ranks were selected initially, nothing to do
    if (!any_selected_rank)
        return;

    // Add any unselected card to the selection if if shares a rank with the selected ranks
    for (int i = 0; i <= played_top; i++)
    {
        if (played[i] && played[i]->card && !selection[i])
        {
            if (rank_selected[played[i]->card->rank])
            {
                selection[i] = true;
            }
        }
    }
}
