#define SCU_SHORT_ALIASES

#include <scu/scu.h>
#include <stdlib.h>

/** @brief Represents a scratchcard. */
typedef struct Card {

    /** @brief The number of winning numbers on the card. */
    i32 winningNumbers;

} Card;

/**
 * @brief Parses a card from a line of text.
 *
 * The line must have the format "Card &lt;number&gt;: &lt;winning numbers&gt; |
 * &lt;scratched numbers&gt;", where &lt;number&gt; is the number of the card.
 * Although the card number is mostly ignored, it must still be present in the
 * line and be a positive decimal integer. The &lt;winning numbers&gt; and
 * &lt;scratched numbers&gt; must be lists of positive decimal integers, where
 * each number is surrounded by one or more spaces.
 *
 * An example for a valid line of text might be "Card 1: 41 48 83 86 17 | 83 86
 * 6 31 17 9 48 53".
 *
 * @warning The behavior is undefined if `line` is not a pointer to a
 * null-terminated byte string.
 *
 * @param[in]  line The line to parse the card from.
 * @param[out] card The parsed card.
 * @return `true` if a card was successfully parsed, otherwise `false`.
 */
static bool card_parse(const char* restrict line, Card* restrict card) {
    SCU_ASSERT(line);
    SCU_ASSERT(card);
    i32 number;
    isize read;
    if (
        scu_sscanf(
            line,
            "Card %" I32_SCND ": %" ISIZE_SCNN,
            &number,
            &read
        ) != 1
    ) {
        return false;
    }
    line += read;
    ScuHashSet* winningNumbers = scu_hash_set_new(
        i32,
        scu_hash_i32,
        scu_equal_i32
    );
    while (scu_sscanf(line, "%" I32_SCND "%" ISIZE_SCNN, &number, &read) == 1) {
        scu_hash_set_try_add(winningNumbers, &number);
        line += read;
    }
    if (scu_sscanf(line, " | %" ISIZE_SCNN, &read) != 0) {
        scu_hash_set_delete(winningNumbers);
        return false;
    }
    line += read;
    card->winningNumbers = 0;
    while (scu_sscanf(line, "%" I32_SCND "%" ISIZE_SCNN, &number, &read) == 1) {
        if (scu_hash_set_contains(winningNumbers, &number)) {
            card->winningNumbers++;
        }
        line += read;
    }
    scu_hash_set_delete(winningNumbers);
    return true;
}

/**
 * @brief Returns the total number of points won by a specified list of cards.
 *
 * @param[in] cards The list of cards to evaluate.
 * @return The total number of points won by the specified list of cards.
 */
static i32 total_points(const Card* cards) {
    SCU_ASSERT(cards);
    i32 totalPoints = 0;
    const Card* card;
    SCU_LIST_FOREACH(card, cards) {
        totalPoints += 1 << (card->winningNumbers - 1);
    }
    return totalPoints;
}

/**
 * @brief Returns the total number of cards won by a specified list of cards.
 *
 * @param[in] cards The list of cards to evaluate.
 * @return The total number of cards won by the specified list of cards.
 */
static i32 total_cards(const Card* cards) {
    SCU_ASSERT(cards);
    isize count = scu_list_count(cards);
    i32* cardCounts = scu_list_new_with_capacity(i32, count);
    for (isize i = 0; i < count; i++) {
        scu_list_add(&cardCounts, &(i32) { 1 });
    }
    for (isize i = 0; i < count; i++) {
        for (i32 j = 0; j < cards[i].winningNumbers; j++) {
            cardCounts[i + 1 + j] += cardCounts[i];
        }
    }
    i32 totalCards = 0;
    i32* cardCount;
    SCU_LIST_FOREACH(cardCount, cardCounts) {
        totalCards += *cardCount;
    }
    scu_list_delete(cardCounts);
    return totalCards;
}

int main() {
    ScuError error = SCU_ERROR_NONE;
    char* line = nullptr;
    isize size = 0;
    Card* cards = scu_list_new(Card);
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        // Replace the newline (if present) to simplify parsing and avoid an
        // ugly line break if an error occurs.
        isize newlineIndex = scu_str_index_of(line, '\n');
        if (newlineIndex != -1) {
            line[newlineIndex] = '\0';
        }
        Card card;
        if (!card_parse(line, &card)) {
            scu_fprintf(
                SCU_STDERR,
                "The line '%s' does not represent a valid card.\n",
                line
            );
            scu_list_delete(cards);
            SCU_ARRAY_DELETE(line, size);
            return EXIT_FAILURE;
        }
        scu_list_add(&cards, &card);
    }
    SCU_ARRAY_DELETE(line, size);
    if (error != SCU_ERROR_END_OF_FILE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        scu_list_delete(cards);
        return EXIT_FAILURE;
    }
    i32 totalPoints = total_points(cards);
    i32 totalCards = total_cards(cards);
    scu_printf("The total number of points is %" I32_PRID ".\n", totalPoints);
    scu_printf("The total number of cards is %" I32_PRID ".\n", totalCards);
    scu_list_delete(cards);
    return EXIT_SUCCESS;
}