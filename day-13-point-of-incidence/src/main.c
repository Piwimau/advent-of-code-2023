#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a field in a pattern. */
typedef enum Field {
    FIELD_ASH,
    FIELD_ROCK
} Field;

/** @brief Represents a pattern of fields. */
typedef struct Pattern {

    /** @brief The width of the pattern. */
    isize width;

    /** @brief The height of the pattern. */
    isize height;

    /**
     * @brief The fields of the pattern.
     *
     * @note This is actually a two-dimensional array of `width * height`
     * fields stored as a one-dimensional array in row-major order.
     */
    Field* fields;

} Pattern;

/**
 * @brief Parses a field from a specified character.
 *
 * The character must be either `'.'` (for `FIELD_ASH`) or `'#'` (for
 * `FIELD_ROCK`).
 *
 * @param[in]  c     The character to parse.
 * @param[out] field The parsed field on success, otherwise unspecified.
 * @return `true` if a field was successfully parsed, otherwise `false`.
 */
static inline bool field_parse(char c, Field* field) {
    SCU_ASSERT(field != nullptr);
    switch (c) {
        case '.':
            *field = FIELD_ASH;
            return true;
        case '#':
            *field = FIELD_ROCK;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Deallocates a specified pattern.
 *
 * @note If `pattern` is a `nullptr`, this function does nothing.
 *
 * Note that this function only deallocates any resources associated with the
 * pattern, but not the pattern itself. The caller is responsible for
 * deallocating the pattern if it was dynamically allocated.
 *
 * @warning The behavior is undefined if `pattern` is used after it has been
 * deallocated.
 *
 * @param[in, out] pattern The pattern to deallocate.
 */
static inline void pattern_free(Pattern* pattern) {
    if (pattern != nullptr) {
        scu_list_free(pattern->fields);
        pattern->fields = nullptr;
    }
}

/**
 * @brief Parses a pattern from the standard input stream.
 *
 * The input must consist of zero or more lines of text, each containing the
 * same number of characters. Each character must be either `'.'` (for
 * `FIELD_ASH`) or `'#'` (for `FIELD_ROCK`). The individual lines (if any) must
 * be separated by newline characters. An empty line (i.e., a line consisting of
 * at most a newline character) is considered the end of a pattern's input.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * #.##..##.
 * ..#.##.#.
 * ##......#
 * ##......#
 * ..#.##.#.
 * ..##..##.
 * #.#.##.#.
 * ```
 *
 * @param[out] pattern The parsed pattern on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if a pattern was successfully parsed,
 * `SCU_ERROR_END_OF_FILE` if a pattern was successfully parsed but the
 * end-of-file condition occurred, otherwise an appropriate error code.
 */
static inline ScuError pattern_parse(Pattern* pattern) {
    SCU_ASSERT(pattern != nullptr);
    pattern->width = 0;
    pattern->height = 0;
    pattern->fields = scu_list_new(SCU_SIZEOF(Field));
    if (pattern->fields == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    bool foundWidth = false;
    char* line = nullptr;
    isize size = 0;
    ScuError error;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        isize newlineIndex = scu_str_index_of(line, '\n');
        if (newlineIndex != -1) {
            line[newlineIndex] = '\0';
        }
        isize width = scu_strlen(line);
        if (width == 0) {
            break;
        }
        if (!foundWidth) {
            pattern->width = width;
            foundWidth = true;
        }
        else if (width != pattern->width) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        char* temp = line;
        Field field;
        while (field_parse(*temp, &field)) {
            error = scu_list_add(&pattern->fields, &field);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
            temp++;
        }
        if (*temp != '\0') {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        pattern->height++;
    }
    if ((error != SCU_ERROR_NONE) && (error != SCU_ERROR_END_OF_FILE)) {
        goto fail;
    }
    scu_free(line);
    return error;
fail:
    scu_free(line);
    pattern_free(pattern);
    return error;
}

/**
 * @brief Searches for a horizontal reflection in a specified pattern with
 * exactly `targetDiff` differences on either side.
 *
 * @param[in] pattern    The pattern to search for a horizontal reflection in.
 * @param[in] targetDiff The required number of differences in order to achieve
 *                       a valid reflection.
 * @return The number of rows above the horizontal reflection, or `-1` if no
 * valid reflection was found.
 */
static inline isize pattern_find_horizontal(
    const Pattern* pattern,
    isize targetDiff
) {
    SCU_ASSERT(pattern != nullptr);
    SCU_ASSERT(targetDiff >= 0);
    const Field* fields = pattern->fields;
    isize width = pattern->width;
    isize height = pattern->height;
    for (isize y = 0; y < (height - 1); y++) {
        isize diff = 0;
        for (
            isize offset = 0;
            ((y - offset) >= 0) && ((y + 1 + offset) < height);
            offset++
        ) {
            for (isize x = 0; x < width; x++) {
                Field l = fields[(y - offset) * width + x];
                Field r = fields[(y + 1 + offset) * width + x];
                if (l != r) {
                    diff++;
                    if (diff > targetDiff) {
                        break;
                    }
                }
            }
            if (diff > targetDiff) {
                break;
            }
        }
        if (diff == targetDiff) {
            return y + 1;
        }
    }
    return -1;
}

/**
 * @brief Searches for a vertical reflection in a specified pattern with
 * exactly `targetDiff` differences on either side.
 *
 * @param[in] pattern    The pattern to search for a vertical reflection in.
 * @param[in] targetDiff The required number of differences in order to achieve
 *                       a valid reflection.
 * @return The number of columns to the left of the vertical reflection, or `-1`
 * if no valid reflection was found.
 */
static inline isize pattern_find_vertical(
    const Pattern* pattern,
    isize targetDiff
) {
    SCU_ASSERT(pattern != nullptr);
    SCU_ASSERT(targetDiff >= 0);
    const Field* fields = pattern->fields;
    isize width = pattern->width;
    isize height = pattern->height;
    for (isize x = 0; x < (width - 1); x++) {
        isize diff = 0;
        for (
            isize offset = 0;
            ((x - offset) >= 0) && ((x + 1 + offset) < width);
            offset++
        ) {
            for (isize y = 0; y < height; y++) {
                Field l = fields[y * width + (x - offset)];
                Field r = fields[y * width + (x + 1 + offset)];
                if (l != r) {
                    diff++;
                    if (diff > targetDiff) {
                        break;
                    }
                }
            }
            if (diff > targetDiff) {
                break;
            }
        }
        if (diff == targetDiff) {
            return x + 1;
        }
    }
    return -1;
}

/**
 * @brief Returns the score of a specified pattern by searching for a horizontal
 * or vertical reflection with exactly `targetDiff` differences on either side.
 *
 * @param[in] pattern    The pattern to compute the score of.
 * @param[in] targetDiff The required number of differences in order to achieve
 *                       a valid reflection.
 * @return The score of the pattern, or `-1` if no valid reflection was found.
 */
static inline isize pattern_score(const Pattern* pattern, isize targetDiff) {
    SCU_ASSERT(pattern != nullptr);
    SCU_ASSERT(targetDiff >= 0);
    isize horizontal = pattern_find_horizontal(pattern, targetDiff);
    if (horizontal != -1) {
        return horizontal * 100;
    }
    return pattern_find_vertical(pattern, targetDiff);
}

/**
 * @brief Deallocates a specified list of patterns.
 *
 * @note If `patterns` is a `nullptr`, this function does nothing.
 *
 * @warning The behavior is undefined if `patterns` is used after it has been
 * deallocated.
 *
 * @param[in, out] patterns The list of patterns to deallocate.
 */
static void patterns_free(Pattern* patterns) {
    if (patterns != nullptr) {
        Pattern* pattern;
        SCU_LIST_FOREACH(pattern, patterns) {
            pattern_free(pattern);
        }
        scu_list_free(patterns);
    }
}

/**
 * @brief Parses a list of patterns from the standard input stream.
 *
 * The input must consist of zero or more patterns, each of which must be
 * formatted as described in the documentation of `pattern_parse()`. The
 * individual patterns (if any) must be separated by empty lines (i.e., lines
 * consisting of at most a newline character).
 *
 * @param[out] patterns The parsed list of patterns on success, otherwise a
 *                      `nullptr`.
 * @return `SCU_ERROR_NONE` if a list of patterns was successfully parsed,
 * otherwise an appropriate error code.
 */
static ScuError patterns_parse(Pattern** patterns) {
    SCU_ASSERT(patterns != nullptr);
    *patterns = scu_list_new(SCU_SIZEOF(Pattern));
    if (*patterns == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    ScuError error;
    Pattern pattern;
    while ((error = pattern_parse(&pattern)) == SCU_ERROR_NONE) {
        error = scu_list_add(patterns, &pattern);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    // If we reach the end-of-file condition, we still need to add the last
    // pattern, as this is not considered an error by `pattern_parse()`.
    error = scu_list_add(patterns, &pattern);
    if (error != SCU_ERROR_NONE) {
        goto fail;
    }
    return SCU_ERROR_NONE;
fail:
    patterns_free(*patterns);
    return error;
}

/**
 * @brief Returns the total score of a specified list of patterns by summing up
 * the individual scores of the patterns.
 *
 * @note If a pattern in the list does not have a valid reflection with exactly
 * `targetDiff` differences on either side, it contributes with a value of `0`
 * to the total score.
 *
 * @param[in] patterns   The list of patterns to compute the total score of.
 * @param[in] targetDiff The required number of differences in order to achieve
 *                       a valid reflection.
 * @return The total score of the patterns.
 */
static isize patterns_score(const Pattern* patterns, isize targetDiff) {
    SCU_ASSERT(patterns != nullptr);
    SCU_ASSERT(targetDiff >= 0);
    isize totalScore = 0;
    const Pattern* pattern;
    SCU_LIST_FOREACH(pattern, patterns) {
        isize patternScore = pattern_score(pattern, targetDiff);
        if (patternScore != -1) {
            totalScore += patternScore;
        }
    }
    return totalScore;
}

int main() {
    Pattern* patterns;
    ScuError error = patterns_parse(&patterns);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    isize perfectScore = patterns_score(patterns, 0);
    isize smudgedScore = patterns_score(patterns, 1);
    scu_printf(
        "The total score of the patterns for perfect reflections is %"
            ISIZE_PRID ".\n",
        perfectScore
    );
    scu_printf(
        "The total score of the patterns for smudged reflections is %"
            ISIZE_PRID ".\n",
        smudgedScore
    );
    patterns_free(patterns);
    return EXIT_SUCCESS;
}