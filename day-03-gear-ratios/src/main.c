#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/array.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdint.h>
#include <stdlib.h>

/** @brief Represents a schematic used for diagnostic purposes. */
typedef struct Schematic {

    /** @brief The grid representation of the schematic. */
    char* grid;

    /** @brief The width of the schematic. */
    isize width;

    /** @brief The height of the schematic. */
    isize height;

} Schematic;

/** @brief Represents a two-dimensional vector. */
typedef struct Vector {

    /** @brief The x-coordinate of the vector. */
    isize x;

    /** @brief The y-coordinate of the vector. */
    isize y;

} Vector;

/** @brief Offsets used to find symbols to the left of a number. */
static constexpr Vector LEFT_OFFSETS[] = {
    { .x = -1, .y = -1 },
    { .x = -1, .y = 0 },
    { .x = -1, .y = 1 }
};

/** @brief Offsets used to find symbols above and below a number. */
static constexpr Vector MIDDLE_OFFSETS[] = {
    { .x = 0, .y = -1 },
    { .x = 0, .y = 1 }
};

/** @brief Offsets used to find symbols to the right of a number. */
static constexpr Vector RIGHT_OFFSETS[] = {
    { .x = 1, .y = -1 },
    { .x = 1, .y = 0 },
    { .x = 1, .y = 1 }
};

/** @brief Offsets used to find numbers around a gear symbol. */
static constexpr Vector OFFSETS[] = {
    { .x = -1, .y = -1 },
    { .x = 0, .y = -1 },
    { .x = 1, .y = -1 },
    { .x = 1, .y = 0 },
    { .x = 1, .y = 1 },
    { .x = 0, .y = 1 },
    { .x = -1, .y = 1 },
    { .x = -1, .y = 0 }
};

/** @brief The total number of numbers associated with each gear. */
static constexpr i32 NUMBERS_PER_GEAR = 2;

/**
 * @brief Determines whether two vectors are equal.
 *
 * @param[in] a The first vector.
 * @param[in] b The second vector.
 * @return `true` if `a` and `b` are equal, otherwise `false`.
 */
static inline bool vector_equals(Vector a, Vector b) {
    return (a.x == b.x) && (a.y == b.y);
}

/**
 * @brief Parses a schematic from a grid representation.
 *
 * @note The grid representation must be a null-terminated byte string. It must
 * have been allocated using `scu_malloc()`, `scu_calloc()`, `scu_realloc()` or
 * the underlying allocator used by these functions. Additionally, it must
 * consist of one or more lines, each of equal length. The last line must not
 * end with a newline character.
 *
 * @warning The schematic takes ownership of the grid. It modifies the grid
 * in-place by replacing newline characters and the terminating null character
 * with dots ('.') to simplify later processing. As such, the grid is no longer
 * treated as a null-terminated byte string, but rather as a two-dimensional
 * array of characters. However, the caller is still responsible for freeing the
 * grid with `scu_free()` when the schematic is no longer needed.
 *
 * @param[in, out]  grid      The grid representation of the schematic.
 * @param[out]      schematic The parsed schematic.
 * @return `true` if the parsing was successful, otherwise `false`.
 */
static bool schematic_parse(
    char* restrict grid,
    Schematic* restrict schematic
) {
    SCU_ASSERT(grid != nullptr);
    SCU_ASSERT(schematic != nullptr);
    isize width = scu_str_index_of(grid, '\n');
    if (width == -1) {
        return false;
    }
    width++;
    isize size = scu_strlen(grid) + 1;
    if ((size % width) != 0) {
        return false;
    }
    isize height = size / width;
    for (isize i = 0; grid[i] != '\0'; i++) {
        if (grid[i] == '\n') {
            grid[i] = '.';
        }
    }
    grid[size - 1] = '.';
    schematic->grid = grid;
    schematic->width = width;
    schematic->height = height;
    return true;
}

/**
 * @brief Determines whether a position is valid within a schematic.
 *
 * @param[in] schematic The schematic to check.
 * @param[in] position The position to check.
 * @return `true` if the position is valid, otherwise `false`.
 */
static inline bool schematic_is_valid_position(
    const Schematic* schematic,
    Vector position
) {
    SCU_ASSERT(schematic != nullptr);
    return (position.x >= 0) && (position.x < schematic->width)
        && (position.y >= 0) && (position.y < schematic->height);
}

/**
 * @brief Determines whether a schematic contains a digit at a specified
 * position.
 *
 * @param[in] schematic The schematic to check.
 * @param[in] position  The position to check.
 * @return `true` if the position is valid and the schematic contains a digit at
 * the specified position, otherwise `false`.
 */
static inline bool schematic_is_digit(
    const Schematic* schematic,
    Vector position
) {
    SCU_ASSERT(schematic != nullptr);
    if (!schematic_is_valid_position(schematic, position)) {
        return false;
    }
    char c = schematic->grid[position.y * schematic->width + position.x];
    return (c >= '0') && (c <= '9');
}

/**
 * @brief Determines whether a schematic contains a symbol at a specified
 * position.
 *
 * @note Any character that is not a digit ('0' – '9') or a dot ('.') counts as
 * being a symbol.
 *
 * @param[in] schematic The schematic to check.
 * @param[in] position  The position to check.
 * @return `true` if the position is valid and the schematic contains a symbol
 * at the specified position, otherwise `false`.
 */
static inline bool schematic_is_symbol(
    const Schematic* schematic,
    Vector position
) {
    SCU_ASSERT(schematic != nullptr);
    if (!schematic_is_valid_position(schematic, position)) {
        return false;
    }
    char c = schematic->grid[position.y * schematic->width + position.x];
    return ((c < '0') || (c > '9')) && (c != '.');
}

/**
 * @brief Determines whether a position is the start of a part number within a
 * schematic.
 *
 * @note A part number is defined as a contiguous sequence of one or more digits
 * that is immediately adjacent (either horizontally, vertically, or diagonally)
 * to at least one symbol. The start of a part number is simply the position of
 * its first digit.
 *
 * @param[in] schematic The schematic to check.
 * @param[in] position  The position to check.
 * @return `true` if the position is valid and the schematic contains the first
 * digit of a part number at the specified position, otherwise `false`.
 */
static inline bool schematic_is_start_of_part_number(
    const Schematic* schematic,
    Vector position
) {
    SCU_ASSERT(schematic != nullptr);
    if (!schematic_is_digit(schematic, position)) {
        return false;
    }
    Vector leftNeighbor = { .x = position.x - 1, .y = position.y };
    if (schematic_is_digit(schematic, leftNeighbor)) {
        return false;
    }
    do {
        Vector neighbor = { .x = position.x - 1, .y = position.y };
        if (!schematic_is_digit(schematic, neighbor)) {
            const Vector* offset;
            SCU_ARRAY_FOREACH(offset, LEFT_OFFSETS) {
                neighbor = (Vector) {
                    .x = position.x + offset->x,
                    .y = position.y + offset->y
                };
                if (schematic_is_symbol(schematic, neighbor)) {
                    return true;
                }
            }
        }
        const Vector* offset;
        SCU_ARRAY_FOREACH(offset, MIDDLE_OFFSETS) {
            neighbor = (Vector) {
                .x = position.x + offset->x,
                .y = position.y + offset->y
            };
            if (schematic_is_symbol(schematic, neighbor)) {
                return true;
            }
        }
        neighbor = (Vector) { .x = position.x + 1, .y = position.y };
        if (!schematic_is_digit(schematic, neighbor)) {
            SCU_ARRAY_FOREACH(offset, RIGHT_OFFSETS) {
                neighbor = (Vector) {
                    .x = position.x + offset->x,
                    .y = position.y + offset->y
                };
                if (schematic_is_symbol(schematic, neighbor)) {
                    return true;
                }
            }
        }
        position.x++;
    }
    while (schematic_is_digit(schematic, position));
    return false;
}

/**
 * @brief Returns a pointer to the character at a specified position within a
 * schematic.
 *
 * @param[in] schematic The schematic to access.
 * @param[in] position  The position to access.
 * @return A pointer to the character at the specified position within the
 * schematic.
 */
static inline const char* schematic_char_at(
    const Schematic* schematic,
    Vector position
) {
    SCU_ASSERT(schematic != nullptr);
    SCU_ASSERT(schematic_is_valid_position(schematic, position));
    return &schematic->grid[position.y * schematic->width + position.x];
}

/**
 * @brief Attempts to find the start of a number at or to the left of a
 * specified position within a schematic.
 *
 * @param[in]  schematic The schematic to search.
 * @param[in]  position  The position to start searching from.
 * @param[out] start     The start of the number, if found.
 * @return `true` if the position is valid and a number was found at or to the
 * left of the specified position within the schematic, otherwise `false`.
 */
static inline bool schematic_try_get_start_of_number(
    const Schematic* schematic,
    Vector position,
    Vector* start
) {
    SCU_ASSERT(schematic != nullptr);
    SCU_ASSERT(start != nullptr);
    if (!schematic_is_digit(schematic, position)) {
        return false;
    }
    do {
        position.x--;
    }
    while (schematic_is_digit(schematic, position));
    position.x++;
    *start = position;
    return true;
}

/**
 * @brief Returns the sum of all part numbers within a schematic.
 *
 * This function returns the sum of all part numbers found within the specified
 * schematic, i.e., the sum of all numbers (contiguous sequences of at least one
 * digit) that are adjacent (either horizontally, vertically, or diagonally) to
 * at least one symbol.
 *
 * @param[in] schematic The schematic to process.
 * @return The sum of all part numbers within the schematic.
 */
static i32 schematic_sum_of_part_numbers(const Schematic* schematic) {
    SCU_ASSERT(schematic != nullptr);
    i32 sumOfPartNumbers = 0;
    for (isize y = 0; y < schematic->height; y++) {
        for (isize x = 0; x < schematic->width; x++) {
            Vector position = { .x = x, .y = y };
            if (schematic_is_start_of_part_number(schematic, position)) {
                i32 partNumber = 0;
                scu_sscanf(
                    schematic_char_at(schematic, position),
                    "%" I32_SCND,
                    &partNumber
                );
                sumOfPartNumbers += partNumber;
            }
        }
    }
    return sumOfPartNumbers;
}

/**
 * @brief Returns the sum of all gear ratios within a schematic.
 *
 * This function returns the sum of all gear ratios found within the specified
 * schematic, i.e., the sum of the products of exactly two numbers (contiguous
 * sequences of at least one digit) that are adjacent (either horizontally,
 * vertically, or diagonally) to a gear symbol ('*').
 *
 * @param[in] schematic The schematic to process.
 * @return The sum of all gear ratios within the schematic.
 */
static i32 schematic_sum_of_gear_ratios(const Schematic* schematic) {
    SCU_ASSERT(schematic != nullptr);
    i32 sumOfGearRatios = 0;
    for (isize y = 0; y < schematic->height; y++) {
        for (isize x = 0; x < schematic->width; x++) {
            Vector position = { .x = x, .y = y };
            if (*schematic_char_at(schematic, position) == '*') {
                Vector starts[NUMBERS_PER_GEAR] = { };
                i32 numbers = 0;
                const Vector* offset;
                SCU_ARRAY_FOREACH(offset, OFFSETS) {
                    Vector neighbor = {
                        .x = position.x + offset->x,
                        .y = position.y + offset->y
                    };
                    Vector start;
                    bool foundStart = schematic_try_get_start_of_number(
                        schematic,
                        neighbor,
                        &start
                    );
                    if (foundStart) {
                        bool isUnique = true;
                        for (i32 i = 0; i < numbers; i++) {
                            if (vector_equals(starts[i], start)) {
                                isUnique = false;
                                break;
                            }
                        }
                        if (isUnique) {
                            if (numbers < NUMBERS_PER_GEAR) {
                                starts[numbers] = start;
                                numbers++;
                            }
                            else {
                                numbers++;
                                break;
                            }
                        }
                    }
                }
                if (numbers == NUMBERS_PER_GEAR) {
                    i32 gearRatio = 1;
                    const Vector* start;
                    SCU_ARRAY_FOREACH(start, starts) {
                        i32 number = 0;
                        scu_sscanf(
                            schematic_char_at(schematic, *start),
                            "%" I32_SCND,
                            &number
                        );
                        gearRatio *= number;
                    }
                    sumOfGearRatios += gearRatio;
                }
            }
        }
    }
    return sumOfGearRatios;
}

int main() {
    char* buffer = nullptr;
    isize size = 0;
    ScuError error = scu_readall(&buffer, &size);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    Schematic schematic;
    if (!schematic_parse(buffer, &schematic)) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while parsing the schematic.\n"
        );
        scu_free(buffer);
        return EXIT_FAILURE;
    }
    i32 sumOfPartNumbers = schematic_sum_of_part_numbers(&schematic);
    i32 sumOfGearRatios = schematic_sum_of_gear_ratios(&schematic);
    scu_printf(
        "The sum of the part numbers is %" I32_PRID ".\n",
        sumOfPartNumbers
    );
    scu_printf(
        "The sum of the gear ratios is %" I32_PRID ".\n",
        sumOfGearRatios
    );
    scu_free(schematic.grid);
    return EXIT_SUCCESS;
}