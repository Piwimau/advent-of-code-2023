#define SCU_SHORT_ALIASES

#include <scu/scu.h>
#include <stdlib.h>

/** @brief Represents a color of a cube. */
typedef enum Color {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE
} Color;

/** @brief Represents a token used to parse the input. */
typedef struct Token {

    /** @brief The value of the token, e.g., "red". */
    const char* value;

    /** @brief The length of the token, e.g., three for "red". */
    isize length;

} Token;

/** @brief Represents a game. */
typedef struct Game {

    /** @brief The unique identifier of the game. */
    i32 id;

    /** @brief The minimum cubes of each color required to play the game. */
    i32 minCubes[COLOR_BLUE + 1];

} Game;

/** @brief The maximum number of cubes available in each color. */
static constexpr i32 MAX_CUBES[COLOR_BLUE + 1] = {
    [COLOR_RED] = 12,
    [COLOR_GREEN] = 13,
    [COLOR_BLUE] = 14
};

/** @brief An array of all possible colors. */
static constexpr Color COLORS[COLOR_BLUE + 1] = {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE
};

/** @brief The tokens used to parse the colors of cubes in the input. */
static const Token TOKENS[COLOR_BLUE + 1] = {
    [COLOR_RED] = { .value = "red", .length = SCU_SIZEOF("red") - 1 },
    [COLOR_GREEN] = { .value = "green", .length = SCU_SIZEOF("green") - 1 },
    [COLOR_BLUE] = { .value = "blue", .length = SCU_SIZEOF("blue") - 1 }
};

/**
 * @brief Parses a game from a line of text.
 *
 * The line must have a specific format. It must start with "Game &lt;id&gt;: ",
 * where &lt;id&gt; represents the unique identifier of the game and is greater
 * than zero. It is followed by zero or more sets of cubes revealed during that
 * game, which are separated by a semicolon and a space. Each set consists of
 * zero or more cube specifications (separated by a comma and a space), which in
 * turn have the format "&lt;count&gt; &lt;color&gt;". Here, &lt;count&gt; is an
 * positive integer and &lt;color&gt; is one of "red", "green" or "blue".
 *
 * An example for a valid line might be "Game 1: 3 blue, 4 red; 1 red, 2 green,
 * 6 blue; 2 green".
 *
 * @note Although `line` is considered to be a line of text, it must not contain
 * an actual newline character.
 *
 * @warning The behavior is undefined if `line` is not a pointer to a
 * null-terminated byte string.
 *
 * @param[in]  line The line to parse.
 * @param[out] game The parsed game.
 * @return `true` if a game was parsed successfully, otherwise `false`.
 */
static inline bool game_parse(const char* restrict line, Game* restrict game) {
    SCU_ASSERT(line);
    SCU_ASSERT(game);
    *game = (Game) { .id = -1, .minCubes = { -1, -1, -1 } };
    isize read = 0;
    if (
        scu_sscanf(
            line,
            "Game %" I32_SCND ": %" ISIZE_SCNN,
            &game->id,
            &read
        ) != 1
    ) {
        return false;
    }
    line += read;
    for (isize i = 0; line[i] != '\0'; i++) {
        if (line[i] == ' ' || line[i] == ',' || line[i] == ';') {
            continue;
        }
        if (line[i] < '0' || line[i] > '9') {
            return false;
        }
        i32 cubes = 0;
        if (
            scu_sscanf(line + i, "%" I32_SCND "%" ISIZE_SCNN, &cubes, &read) != 1
                || cubes < 0
        ) {
            return false;
        }
        i += read;
        // The number must be separated from the color by a single space.
        if (line[i] != ' ') {
            return false;
        }
        i++;
        bool matchedColor = false;
        const Color* color;
        SCU_ARRAY_FOREACH(color, COLORS) {
            const Token* token = &TOKENS[*color];
            if (scu_strncmp(line + i, token->value, token->length) == 0) {
                matchedColor = true;
                game->minCubes[*color] = SCU_MAX(game->minCubes[*color], cubes);
                // Advance to the end of the matched color token, but one less
                // than the actual length, as it will be incremented by the main
                // loop as well.
                i += token->length - 1;
                break;
            }
        }
        if (!matchedColor) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Determines whether a game is possible with the available cubes.
 *
 * @param[in] game The game to check.
 * @return `true` if the game is possible, otherwise `false`.
 */
static inline bool game_is_possible(const Game* game) {
    SCU_ASSERT(game);
    return game->minCubes[COLOR_RED] <= MAX_CUBES[COLOR_RED]
        && game->minCubes[COLOR_GREEN] <= MAX_CUBES[COLOR_GREEN]
        && game->minCubes[COLOR_BLUE] <= MAX_CUBES[COLOR_BLUE];
}

/**
 * @brief Calculates the power of a game.
 *
 * @note The power of a game is defined as the product of the minimum cubes of
 * each color required to play the game.
 *
 * @param[in] game The game to calculate the power for.
 * @return The power of the game.
 */
static inline i32 game_power(const Game* game) {
    SCU_ASSERT(game);
    return game->minCubes[COLOR_RED] * game->minCubes[COLOR_GREEN]
        * game->minCubes[COLOR_BLUE];
}

int main() {
    ScuError error = SCU_ERROR_NONE;
    char* line = nullptr;
    isize size = 0;
    i32 sumOfIds = 0;
    i32 sumOfPowers = 0;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        // Replace the newline (if present) to simplify parsing and avoid an
        // ugly line break if an error occurs.
        isize newlineIndex = scu_str_index_of(line, '\n');
        if (newlineIndex != -1) {
            line[newlineIndex] = '\0';
        }
        Game game;
        if (!game_parse(line, &game)) {
            scu_fprintf(
                SCU_STDERR,
                "The line '%s' does not represent a valid game.\n",
                line
            );
            SCU_ARRAY_DELETE(line, size);
            return EXIT_FAILURE;
        }
        if (game_is_possible(&game)) {
            sumOfIds += game.id;
        }
        sumOfPowers += game_power(&game);
    }
    SCU_ARRAY_DELETE(line, size);
    if (error != SCU_ERROR_END_OF_FILE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    scu_printf(
        "The sum of the IDs of the possible games is %" I32_PRID ".\n",
        sumOfIds
    );
    scu_printf(
        "The sum of the powers of all games is %" I32_PRID ".\n",
        sumOfPowers
    );
    return EXIT_SUCCESS;
}