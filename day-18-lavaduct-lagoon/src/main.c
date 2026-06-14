#define SCU_SHORT_ALIASES

#include <ctype.h>
#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/math.h>
#include <scu/memory.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a direction of movement. */
typedef enum Direction {
    DIRECTION_RIGHT,
    DIRECTION_DOWN,
    DIRECTION_LEFT,
    DIRECTION_UP
} Direction;

/**
 * @brief The length of the code associated with each instruction, including the
 * terminating null byte.
 */
static constexpr isize CODE_LENGTH = 8;

/** @brief Represents an instruction for digging the lagoon. */
typedef struct Instruction {

    /** @brief The direction to dig in. */
    Direction direction;

    /** @brief The distance to dig (in meters). */
    isize distance;

    /** @brief The code associated with the instruction. */
    char code[CODE_LENGTH];

} Instruction;

/** @brief Represents a two-dimensional position. */
typedef struct Position {

    /** @brief The x-coordinate of the position. */
    isize x;

    /** @brief The y-coordinate of the position. */
    isize y;

} Position;

/**
 * @brief Parses a direction from a specified character.
 *
 * The character must be `'R'` (for `DIRECTION_RIGHT`), `'D'` (for
 * `DIRECTION_DOWN`), `'L'` (for `DIRECTION_LEFT`), or `'U'` (for
 * `DIRECTION_UP`).
 *
 * @param[in]  c         The character to parse.
 * @param[out] direction The parsed direction on success, otherwise unspecified.
 * @return `true` if a direction was successfully parsed, otherwise `false`.
 */
static inline bool direction_parse(char c, Direction* direction) {
    SCU_ASSERT(direction != nullptr);
    switch (c) {
        case 'R':
            *direction = DIRECTION_RIGHT;
            return true;
        case 'D':
            *direction = DIRECTION_DOWN;
            return true;
        case 'L':
            *direction = DIRECTION_LEFT;
            return true;
        case 'U':
            *direction = DIRECTION_UP;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Parses an instruction from a line of text.
 *
 * The line must have the following format:
 *
 * ```plaintext
 * <direction> <distance> (<code>)
 * ```
 *
 * Here, `<direction>` must be a single character that indicates the direction
 * to dig in. It must be one of the valid characters as described by the
 * documentation of `direction_parse()`. `<distance>` represents the distance to
 * dig in meters and must be an integer greater than or equal to `1`. `<code>`
 * must be a string of the format `#xxxxxxx`, where each `x` is a hexadecimal
 * digit (i.e., a digit from `'0'` to `'9'` or a letter from `'a'` to `'f'` or
 * from `'A'` to `'F'`).
 *
 * An example for a valid line of text might be the following:
 *
 * ```plaintext
 * R 6 (#70c710)
 * ```
 *
 * @param[in]  line        The line of text to parse.
 * @param[out] instruction The parsed instruction on success, otherwise
 *                         unspecified.
 * @return `true` if an instruction was successfully parsed, otherwise `false`.
 */
static inline bool instruction_parse(
    const char* restrict line,
    Instruction* restrict instruction
) {
    SCU_ASSERT(line != nullptr);
    SCU_ASSERT(instruction != nullptr);
    char direction;
    isize count = scu_sscanf(
        line,
        "%c %" ISIZE_SCND " (%7s)",
        &direction,
        &instruction->distance,
        instruction->code
    );
    if (
        (count != 3)
            || !direction_parse(direction, &instruction->direction)
            || (instruction->distance < 1)
            || (instruction->code[0] != '#')
            || (scu_strlen(instruction->code) != (CODE_LENGTH - 1))
    ) {
        return false;
    }
    for (isize i = 1; instruction->code[i] != '\0'; i++) {
        if (!isxdigit(instruction->code[i])) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Fixes a specified instruction by extracting the actual direction and
 * distance from the instruction's code.
 *
 * @param[in, out] instruction The instruction to fix.
 */
static inline void instruction_fix(Instruction* instruction) {
    SCU_ASSERT(instruction != nullptr);
    char direction = instruction->code[CODE_LENGTH - 2];
    SCU_ASSERT((direction >= '0') && (direction <= '3'));
    instruction->direction = direction - '0';
    char distance[CODE_LENGTH] = "0x";
    scu_strncat(
        distance,
        SCU_SIZEOF(distance),
        instruction->code + 1,
        CODE_LENGTH - 3
    );
    if (scu_sscanf(distance, "%" ISIZE_SCNI, &instruction->distance) != 1) {
        SCU_UNREACHABLE();
    }
}

/**
 * @brief Parses a list of instructions from the standard input stream.
 *
 * The input must consist of zero or more lines of text, each containing an
 * instruction in the format as described by the documentation of
 * `instruction_parse()`. The individual lines (if any) must be separated by
 * newline characters.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * R 6 (#70c710)
 * D 5 (#0dc571)
 * L 2 (#5713f0)
 * D 2 (#d2c081)
 * R 2 (#59c680)
 * D 2 (#411b91)
 * L 5 (#8ceee2)
 * U 2 (#caa173)
 * L 1 (#1b58a2)
 * U 2 (#caa171)
 * R 2 (#7807d2)
 * U 3 (#a77fa3)
 * L 2 (#015232)
 * U 2 (#7a21e3)
 * ```
 *
 * @param[out] instructions The parsed list of instructions on success,
 *                          otherwise a `nullptr`.
 * @return `SCU_ERROR_NONE` if a list of instructions was successfully parsed,
 * otherwise an appropriate error code.
 */
static ScuError instructions_parse(Instruction** instructions) {
    SCU_ASSERT(instructions != nullptr);
    *instructions = scu_list_new(SCU_SIZEOF(Instruction));
    if (*instructions == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    ScuError error;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        Instruction instruction;
        if (!instruction_parse(line, &instruction)) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        error = scu_list_add(instructions, &instruction);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    scu_list_free(*instructions);
    *instructions = nullptr;
    return error;
}

/**
 * @brief Fixes all instructions in a specified list of instructions by
 * extracting the actual directions and distances from the instructions' codes.
 *
 * @param[in, out] instructions The list of instructions to fix.
 */
static inline void instructions_fix(Instruction* instructions) {
    SCU_ASSERT(instructions != nullptr);
    Instruction* instruction;
    SCU_LIST_FOREACH(instruction, instructions) {
        instruction_fix(instruction);
    }
}

/**
 * @brief Returns the volume of the lagoon that can be dug out by following a
 * specified list of instructions.
 *
 * @param[in] instructions The list of instructions to follow.
 * @return The volume of the lagoon that can be dug out by following the
 * specified list of instructions.
 */
static isize lagoon_volume(const Instruction* instructions) {
    SCU_ASSERT(instructions != nullptr);
    Position prev = { .x = 0, .y = 0 };
    isize doubleArea = 0;
    isize perimeter = 0;
    const Instruction* instruction;
    SCU_LIST_FOREACH(instruction, instructions) {
        Position next = prev;
        switch (instruction->direction) {
            case DIRECTION_RIGHT:
                next.x += instruction->distance;
                break;
            case DIRECTION_DOWN:
                next.y += instruction->distance;
                break;
            case DIRECTION_LEFT:
                next.x -= instruction->distance;
                break;
            case DIRECTION_UP:
                next.y -= instruction->distance;
                break;
        }
        doubleArea += prev.x * next.y - next.x * prev.y;
        perimeter += instruction->distance;
        prev = next;
    }
    return (SCU_ABS(doubleArea) + perimeter) / 2 + 1;
}

int main() {
    Instruction* instructions;
    ScuError error = instructions_parse(&instructions);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    isize volumeBroken = lagoon_volume(instructions);
    instructions_fix(instructions);
    isize volumeFixed = lagoon_volume(instructions);
    scu_printf(
        "Using the broken instructions, the lagoon can hold %" ISIZE_PRID
            " cubic meters of lava.\n",
        volumeBroken
    );
    scu_printf(
        "Using the fixed instructions, the lagoon can hold %" ISIZE_PRID
            " cubic meters of lava.\n",
        volumeFixed
    );
    scu_list_free(instructions);
    return EXIT_SUCCESS;
}