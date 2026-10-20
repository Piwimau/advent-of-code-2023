#define SCU_SHORT_ALIASES

#include <scu/scu.h>
#include <stdlib.h>

/** @brief Represents a card in a deck for Camel Cards. */
typedef enum Card {
    CARD_JOKER,
    CARD_TWO,
    CARD_THREE,
    CARD_FOUR,
    CARD_FIVE,
    CARD_SIX,
    CARD_SEVEN,
    CARD_EIGHT,
    CARD_NINE,
    CARD_TEN,
    CARD_JACK,
    CARD_QUEEN,
    CARD_KING,
    CARD_ACE
} Card;

/** @brief Represents the type of a hand in Camel Cards. */
typedef enum HandType {
    HAND_TYPE_HIGH_CARD,
    HAND_TYPE_ONE_PAIR,
    HAND_TYPE_TWO_PAIR,
    HAND_TYPE_THREE_OF_A_KIND,
    HAND_TYPE_FULL_HOUSE,
    HAND_TYPE_FOUR_OF_A_KIND,
    HAND_TYPE_FIVE_OF_A_KIND
} HandType;

/** @brief Represents a hand of cards in Camel Cards. */
typedef struct Hand {

    /** @brief The cards in the hand. */
    Card cards[5];

    /** @brief The bid associated with the hand. */
    i32 bid;

    /** @brief The type of the hand. */
    HandType type;

} Hand;

/**
 * @brief Parses a card from a specified character.
 *
 * To successfully parse a card, the specified character must be either a digit
 * from '2' to '9' (corresponding to the number cards) or one of the characters
 * 'T', 'J', 'Q', 'K', or 'A' (corresponding to the ten, jack, queen, king, and
 * ace cards, respectively).
 *
 * @param[in]  c    The character to parse a card from.
 * @param[out] card The parsed card on success, otherwise unmodified.
 * @return `true` if a card was successfully parsed, otherwise `false`.
 */
static inline bool card_parse(char c, Card* card) {
    SCU_ASSERT(card);
    switch (c) {
        case '2':
            *card = CARD_TWO;
            return true;
        case '3':
            *card = CARD_THREE;
            return true;
        case '4':
            *card = CARD_FOUR;
            return true;
        case '5':
            *card = CARD_FIVE;
            return true;
        case '6':
            *card = CARD_SIX;
            return true;
        case '7':
            *card = CARD_SEVEN;
            return true;
        case '8':
            *card = CARD_EIGHT;
            return true;
        case '9':
            *card = CARD_NINE;
            return true;
        case 'T':
            *card = CARD_TEN;
            return true;
        case 'J':
            *card = CARD_JACK;
            return true;
        case 'Q':
            *card = CARD_QUEEN;
            return true;
        case 'K':
            *card = CARD_KING;
            return true;
        case 'A':
            *card = CARD_ACE;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Classifies a specified hand of cards and stores its type into the
 * corresponding field.
 *
 * @param[in, out] hand The hand to classify.
 */
static inline void hand_classify(Hand* hand) {
    SCU_ASSERT(hand);
    i32 counts[CARD_ACE + 1] = { };
    Card* card;
    SCU_ARRAY_FOREACH(card, hand->cards) {
        counts[*card]++;
    }
    i32 highestCount = I32_MIN;
    i32 secondHighestCount = I32_MIN;
    i32* count;
    SCU_ARRAY_FOREACH(count, counts) {
        if (*count > highestCount) {
            secondHighestCount = highestCount;
            highestCount = *count;
        }
        else if (*count > secondHighestCount) {
            secondHighestCount = *count;
        }
    }
    switch (highestCount) {
        case 5:
            hand->type = HAND_TYPE_FIVE_OF_A_KIND;
            break;
        case 4:
            hand->type = HAND_TYPE_FOUR_OF_A_KIND;
            break;
        case 3:
            hand->type = (secondHighestCount == 2)
                ? HAND_TYPE_FULL_HOUSE
                : HAND_TYPE_THREE_OF_A_KIND;
            break;
        case 2:
            hand->type = (secondHighestCount == 2)
                ? HAND_TYPE_TWO_PAIR
                : HAND_TYPE_ONE_PAIR;
            break;
        default:
            hand->type = HAND_TYPE_HIGH_CARD;
            break;
    }
}

/**
 * @brief Classifies a specified hand of cards with the joker rule and stores
 * its type into the corresponding field.
 *
 * @note In this variant, any jack card is replaced with a joker before
 * classifying the hand.
 *
 * @param[in, out] hand The hand to classify.
 */
static inline void hand_classify_with_joker(Hand* hand) {
    SCU_ASSERT(hand);
    i32 counts[CARD_ACE + 1] = { };
    Card* card;
    SCU_ARRAY_FOREACH(card, hand->cards) {
        if (*card == CARD_JACK) {
            *card = CARD_JOKER;
        }
        counts[*card]++;
    }
    i32 highestCount = I32_MIN;
    i32 secondHighestCount = I32_MIN;
    i32* count;
    SCU_ARRAY_FOREACH(count, counts) {
        if (*count > highestCount) {
            secondHighestCount = highestCount;
            highestCount = *count;
        }
        else if (*count > secondHighestCount) {
            secondHighestCount = *count;
        }
    }
    switch (highestCount) {
        case 5:
            hand->type = HAND_TYPE_FIVE_OF_A_KIND;
            break;
        case 4:
            hand->type = (counts[CARD_JOKER] > 0)
                ? HAND_TYPE_FIVE_OF_A_KIND
                : HAND_TYPE_FOUR_OF_A_KIND;
            break;
        case 3:
            if (counts[CARD_JOKER] > 0) {
                hand->type = (secondHighestCount == 2)
                    ? HAND_TYPE_FIVE_OF_A_KIND
                    : HAND_TYPE_FOUR_OF_A_KIND;
            }
            else {
                hand->type = (secondHighestCount == 2)
                    ? HAND_TYPE_FULL_HOUSE
                    : HAND_TYPE_THREE_OF_A_KIND;
            }
            break;
        case 2:
            if (secondHighestCount == 2) {
                hand->type = (counts[CARD_JOKER] > 1)
                    ? HAND_TYPE_FOUR_OF_A_KIND
                    : (counts[CARD_JOKER] > 0)
                        ? HAND_TYPE_FULL_HOUSE
                        : HAND_TYPE_TWO_PAIR;
            }
            else {
                hand-> type = (counts[CARD_JOKER] > 0)
                    ? HAND_TYPE_THREE_OF_A_KIND
                    : HAND_TYPE_ONE_PAIR;
            }
            break;
        default:
            hand->type = (counts[CARD_JOKER] > 0)
                ? HAND_TYPE_ONE_PAIR
                : HAND_TYPE_HIGH_CARD;
            break;
    }
}

/**
 * @brief Parses hands from the standard input stream.
 *
 * The input must consist of lines in the following format:
 *
 * ```plaintext
 * <Cards> <Bid>
 * ```
 *
 * &lt;Cards&gt; is a sequence of five characters representing the cards in the
 * hand, where each character is either a digit from '2' to '9' (corresponding
 * to the number cards) or one of the characters 'T', 'J', 'Q', 'K', or 'A'
 * (corresponding to the ten, jack, queen, king, and ace cards, respectively).
 * &lt;Bid&gt; is a positive integer representing the bid associated with the
 * hand.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * 32T3K 765
 * T55J5 684
 * KK677 28
 * KTJJT 220
 * QQQJA 483
 * ```
 *
 * @param[out] hands A list of the parsed hands on success, otherwise a
 *                   `nullptr`.
 * @return `SCU_ERROR_NONE` on success, or an appropriate error code on failure.
 */
static ScuError parse_hands(Hand** hands) {
    SCU_ASSERT(hands);
    *hands = scu_list_new(Hand);
    if (!*hands) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    ScuError error;
    char* line = nullptr;
    isize size = 0;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        char* temp = line;
        Hand hand;
        Card* card;
        SCU_ARRAY_FOREACH(card, hand.cards) {
            if (!card_parse(*temp, card)) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            temp++;
        }
        if (
            (scu_sscanf(temp, "%" I32_SCND, &hand.bid) != 1) || (hand.bid < 0)
        ) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        hand_classify(&hand);
        error = scu_list_add(hands, &hand);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    SCU_ARRAY_DELETE(line, size);
    return SCU_ERROR_NONE;
fail:
    SCU_ARRAY_DELETE(line, size);
    scu_list_delete(*hands);
    *hands = nullptr;
    return error;
}

/**
 * @brief Compares two specified hands.
 *
 * @param[in] a The first hand.
 * @param[in] b The second hand.
 * @return A negative value if `a` is less than `b`, zero if they compare equal,
 * or a positive value if `a` is greater than `b`.
 */
static int compare_hands(const void* a, const void* b) {
    SCU_ASSERT(a);
    SCU_ASSERT(b);
    const Hand* l = a;
    const Hand* r = b;
    int cmp = (l->type > r->type) - (l->type < r->type);
    if (cmp == 0) {
        for (isize i = 0; i < SCU_COUNTOF(l->cards); i++) {
            cmp = (l->cards[i] > r->cards[i]) - (l->cards[i] < r->cards[i]);
            if (cmp != 0) {
                break;
            }
        }
    }
    return cmp;
}

/**
 * @brief Returns the total winnings for a specified list of hands.
 *
 * @note This function modifies the order of the hands in the specified list.
 * However, the individual hands themselves are not modified.
 *
 * @param[in, out] hands The list of hands.
 * @return The total winnings for the specified list of hands.
 */
static i32 total_winnings(Hand* hands) {
    SCU_ASSERT(hands);
    scu_list_sort(hands, compare_hands);
    i32 totalWinnings = 0;
    for (isize i = 0; i < scu_list_count(hands); i++) {
        totalWinnings += hands[i].bid * (i32) (i + 1);
    }
    return totalWinnings;
}

/**
 * @brief Returns the total winnings for a specified list of hands using the new
 * joker rule.
 *
 * @note This function modifies both the order as well as the individual hands
 * in the specified list.
 *
 * @param[in, out] hands The list of hands.
 * @return The total winnings for the specified list of hands using the new
 * joker rule.
 */
static i32 total_winnings_with_joker(Hand* hands) {
    SCU_ASSERT(hands);
    Hand* hand;
    SCU_LIST_FOREACH(hand, hands) {
        hand_classify_with_joker(hand);
    }
    scu_list_sort(hands, compare_hands);
    i32 totalWinnings = 0;
    for (isize i = 0; i < scu_list_count(hands); i++) {
        totalWinnings += hands[i].bid * (i32) (i + 1);
    }
    return totalWinnings;
}

int main() {
    Hand* hands;
    ScuError error = parse_hands(&hands);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    i32 totalWinnings = total_winnings(hands);
    i32 totalWinningsWithJoker = total_winnings_with_joker(hands);
    scu_printf("The total winnings are %" I32_PRID ".\n", totalWinnings);
    scu_printf(
        "Using the new joker rule, the total winnings are %" I32_PRID ".\n",
        totalWinningsWithJoker
    );
    scu_list_delete(hands);
    return EXIT_SUCCESS;
}