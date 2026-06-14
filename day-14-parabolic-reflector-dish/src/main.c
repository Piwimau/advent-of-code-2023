#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/equal.h>
#include <scu/hash-map.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a field on a platform of a parabolic reflector dish. */
typedef enum Field {
    FIELD_EMPTY,
    FIELD_ROUND_ROCK,
    FIELD_CUBE_ROCK
} Field;

/** @brief Represents a platform of a parabolic reflector dish. */
typedef struct Platform {

    /** @brief The width of the platform. */
    isize width;

    /** @brief The height of the platform. */
    isize height;

    /**
     * @brief The fields of the platform.
     *
     * @note This is actually a two-dimensional array of `width * height` fields
     * stored as a one-dimensional array in row-major order.
     */
    Field* fields;

} Platform;


/** @brief The number of cycles for simulating the tilting of the platform. */
static constexpr isize TARGET_CYCLES = 1'000'000'000;

/**
 * @brief Parses a field from a specified character.
 *
 * The character must be either `'.'` (for `FIELD_EMPTY`), `'O'` (for
 * `FIELD_ROUND_ROCK`) or `'#'` (for `FIELD_CUBE_ROCK`).
 *
 * @param[in]  c     The character to parse.
 * @param[out] field The parsed field on success, otherwise unspecified.
 * @return `true` if a field was successfully parsed, otherwise `false`.
 */
static inline bool field_parse(char c, Field* field) {
    SCU_ASSERT(field != nullptr);
    switch (c) {
        case '.':
            *field = FIELD_EMPTY;
            return true;
        case 'O':
            *field = FIELD_ROUND_ROCK;
            return true;
        case '#':
            *field = FIELD_CUBE_ROCK;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Deallocates a specified platform.
 *
 * @note If `platform` is a `nullptr`, this function does nothing.
 *
 * Note that this function only deallocates any ressources associated with the
 * platform, but not the platform itself. The caller is responsible for
 * deallocating the platform itself if it was dynamically allocated.
 *
 * @warning The behavior is undefined if `platform` is used after it has been
 * deallocated.
 *
 * @param[in, out] platform The platform to deallocate.
 */
static inline void platform_free(Platform* platform) {
    if (platform != nullptr) {
        scu_list_free(platform->fields);
        platform->fields = nullptr;
    }
}

/**
 * @brief Parses a platform from the standard input stream.
 *
 * The input must consist of zero or more lines of text, each containing the
 * same number of characters. Each character must be either `'.'` (for
 * `FIELD_EMPTY`), `'O'` (for `FIELD_ROUND_ROCK`) or `'#'` (for
 * `FIELD_CUBE_ROCK`). The individual lines (if any) must be separated by
 * newline characters.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * O....#....
 * O.OO#....#
 * .....##...
 * OO.#O....O
 * .O.....O#.
 * O.#..O.#.#
 * ..O..#O..O
 * .......O..
 * #....###..
 * #OO..#....
 * ```
 *
 * @param[out] platform The parsed platform on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if a platform was successfully parsed, otherwise an
 * appropriate error code.
 */
static ScuError platform_parse(Platform* platform) {
    SCU_ASSERT(platform != nullptr);
    platform->width = 0;
    platform->height = 0;
    platform->fields = scu_list_new(SCU_SIZEOF(Field));
    if (platform->fields == nullptr) {
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
        if (!foundWidth) {
            platform->width = width;
            foundWidth = true;
        }
        else if (width != platform->width) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        char* temp = line;
        Field field;
        while (field_parse(*temp, &field)) {
            error = scu_list_add(&platform->fields, &field);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
            temp++;
        }
        if (*temp != '\0') {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        platform->height++;
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    platform_free(platform);
    return error;
}

/**
 * @brief Creates a deep copy of a specified platform.
 *
 * @param[in]  platform The platform to clone.
 * @param[out] clone    The cloned platform on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if the platform was successfully cloned, or
 * `SCU_ERROR_OUT_OF_MEMORY` if an out-of-memory condition occurred.
 */
static inline ScuError platform_clone(
    const Platform* restrict platform,
    Platform* restrict clone
) {
    SCU_ASSERT(platform != nullptr);
    SCU_ASSERT(clone != nullptr);
    clone->width = platform->width;
    clone->height = platform->height;
    clone->fields = scu_list_clone(platform->fields);
    return (clone->fields != nullptr)
        ? SCU_ERROR_NONE
        : SCU_ERROR_OUT_OF_MEMORY;
}

/**
 * @brief Tilts a specified platform to the north, causing all round rocks to
 * roll north as far as possible.
 *
 * @param[in, out] platform The platform to tilt.
 */
static inline void platform_tilt_north(Platform* platform) {
    SCU_ASSERT(platform != nullptr);
    isize width = platform->width;
    isize height = platform->height;
    Field* fields = platform->fields;
    for (isize x = 0; x < width; x++) {
        isize nextEmptyY = 0;
        for (isize y = 0; y < height; y++) {
            Field* field = &fields[y * width + x];
            if (*field == FIELD_CUBE_ROCK) {
                nextEmptyY = y + 1;
            }
            else if (*field == FIELD_ROUND_ROCK) {
                *field = FIELD_EMPTY;
                fields[nextEmptyY * width + x] = FIELD_ROUND_ROCK;
                nextEmptyY++;
            }
        }
    }
}

/**
 * @brief Tilts a specified platform to the west, causing all round rocks to
 * roll west as far as possible.
 *
 * @param[in, out] platform The platform to tilt.
 */
static inline void platform_tilt_west(Platform* platform) {
    SCU_ASSERT(platform != nullptr);
    isize width = platform->width;
    isize height = platform->height;
    Field* fields = platform->fields;
    for (isize y = 0; y < height; y++) {
        isize nextEmptyX = 0;
        for (isize x = 0; x < width; x++) {
            Field* field = &fields[y * width + x];
            if (*field == FIELD_CUBE_ROCK) {
                nextEmptyX = x + 1;
            }
            else if (*field == FIELD_ROUND_ROCK) {
                *field = FIELD_EMPTY;
                fields[y * width + nextEmptyX] = FIELD_ROUND_ROCK;
                nextEmptyX++;
            }
        }
    }
}

/**
 * @brief Tilts a specified platform to the south, causing all round rocks to
 * roll south as far as possible.
 *
 * @param[in, out] platform The platform to tilt.
 */
static inline void platform_tilt_south(Platform* platform) {
    SCU_ASSERT(platform != nullptr);
    isize width = platform->width;
    isize height = platform->height;
    Field* fields = platform->fields;
    for (isize x = 0; x < width; x++) {
        isize nextEmptyY = height - 1;
        for (isize y = height - 1; y >= 0; y--) {
            Field* field = &fields[y * width + x];
            if (*field == FIELD_CUBE_ROCK) {
                nextEmptyY = y - 1;
            }
            else if (*field == FIELD_ROUND_ROCK) {
                *field = FIELD_EMPTY;
                fields[nextEmptyY * width + x] = FIELD_ROUND_ROCK;
                nextEmptyY--;
            }
        }
    }
}

/**
 * @brief Tilts a specified platform to the east, causing all round rocks to
 * roll east as far as possible.
 *
 * @param[in, out] platform The platform to tilt.
 */
static inline void platform_tilt_east(Platform* platform) {
    SCU_ASSERT(platform != nullptr);
    isize width = platform->width;
    isize height = platform->height;
    Field* fields = platform->fields;
    for (isize y = 0; y < height; y++) {
        isize nextEmptyX = width - 1;
        for (isize x = width - 1; x >= 0; x--) {
            Field* field = &fields[y * width + x];
            if (*field == FIELD_CUBE_ROCK) {
                nextEmptyX = x - 1;
            }
            else if (*field == FIELD_ROUND_ROCK) {
                *field = FIELD_EMPTY;
                fields[y * width + nextEmptyX] = FIELD_ROUND_ROCK;
                nextEmptyX--;
            }
        }
    }
}

/**
 * @brief Returns the total load on the north support beams of a specified
 * platform.
 *
 * @param[in] platform The platform to compute the load of.
 * @return The total load on the north support beams of the specified platform.
 */
static inline isize platform_load(const Platform* platform) {
    SCU_ASSERT(platform != nullptr);
    isize width = platform->width;
    isize height = platform->height;
    const Field* fields = platform->fields;
    isize totalLoad = 0;
    for (isize y = 0; y < height; y++) {
        for (isize x = 0; x < width; x++) {
            if (fields[y * width + x] == FIELD_ROUND_ROCK) {
                totalLoad += height - y;
            }
        }
    }
    return totalLoad;
}

/**
 * @brief Returns a hash for a specified platform.
 *
 * @warning The behavior is undefined if `value` is not a pointer to a platform.
 *
 * @param[in] value A pointer to the platform to hash.
 * @return A hash for the specified platform.
 */
static usize platform_hash(const void* value) {
    SCU_ASSERT(value != nullptr);
    const Platform* platform = value;
    usize hash = 0;
    hash = scu_hash_combine(hash, scu_hash_isize(&platform->width));
    hash = scu_hash_combine(hash, scu_hash_isize(&platform->height));
    hash = scu_hash_combine(
        hash,
        scu_hash_bytes(
            platform->fields,
            platform->width * platform->height * SCU_SIZEOF(Field)
        )
    );
    return hash;
}

/**
 * @brief Determines whether two specified platforms are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a
 * platform.
 *
 * @param[in] a A pointer to the first platform.
 * @param[in] b A pointer to the second platform.
 * @return `true` if the specified platforms are equal, otherwise `false`.
 */
static bool platform_equal(const void* a, const void* b) {
    SCU_ASSERT(a != nullptr);
    SCU_ASSERT(b != nullptr);
    const Platform* l = a;
    const Platform* r = b;
    return (l->width == r->width) && (l->height == r->height)
        && scu_memcmp(
            l->fields,
            r->fields,
            l->width * l->height * SCU_SIZEOF(Field)
        ) == 0;
}

/**
 * @brief Returns the total load on the north support beams of a specified
 * platform after simulating the tilting of the platform for a specified number
 * of cycles.
 *
 * @param[in, out] platform The platform to simulate.
 * @param[in]      cycles   The number of cycles to simulate.
 * @return The total load on the north support beams after the specified number
 * of cycles, or `-1` if an error occurred.
 */
static isize platform_load_after(Platform* platform, isize cycles) {
    SCU_ASSERT(platform != nullptr);
    SCU_ASSERT(cycles > 0);
    ScuHashMap* cache = scu_hash_map_new(
        SCU_SIZEOF(Platform),
        SCU_SIZEOF(isize),
        platform_hash,
        platform_equal,
        scu_equal_isize
    );
    if (cache == nullptr) {
        return -1;
    }
    Platform* history = scu_list_new(SCU_SIZEOF(Platform));
    if (history == nullptr) {
        scu_hash_map_free(cache);
        return -1;
    }
    isize totalLoad = -1;
    for (isize cycle = 1; cycle <= cycles; cycle++) {
        platform_tilt_north(platform);
        platform_tilt_west(platform);
        platform_tilt_south(platform);
        platform_tilt_east(platform);
        isize* prevCycle;
        if (scu_hash_map_try_get(cache, platform, &prevCycle)) {
            isize length = cycle - *prevCycle;
            isize remaining = (cycles - *prevCycle) % length;
            totalLoad = platform_load(&history[*prevCycle - 1 + remaining]);
            break;
        }
        // Create a stable clone as we continue to modify the platform.
        Platform clone;
        if (platform_clone(platform, &clone) != SCU_ERROR_NONE) {
            break;
        }
        if (scu_list_add(&history, &clone) != SCU_ERROR_NONE) {
            // If we fail to add the clone to the history, we need to free it
            // manually here, as it won't be freed by the cleanup code at the
            // end of the function.
            platform_free(&clone);
            break;
        }
        if (scu_hash_map_add(cache, &clone, &cycle) != SCU_ERROR_NONE) {
            break;
        }
    }
    Platform* p;
    SCU_LIST_FOREACH(p, history) {
        platform_free(p);
    }
    scu_list_free(history);
    scu_hash_map_free(cache);
    return totalLoad;
}

int main() {
    Platform platform;
    ScuError error = platform_parse(&platform);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    // We need to clone the platform before tilting it, as we need the original
    // state for the second part of the puzzle as well.
    Platform clone;
    error = platform_clone(&platform, &clone);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while cloning the platform (code %d).\n",
            error
        );
        platform_free(&platform);
        return EXIT_FAILURE;
    }
    platform_tilt_north(&platform);
    isize initialLoad = platform_load(&platform);
    isize cyclesLoad = platform_load_after(&clone, TARGET_CYCLES);
    scu_printf(
        "The initial load on the north support beams is %" ISIZE_PRID ".\n",
        initialLoad
    );
    scu_printf(
        "The load on the north support beams after 1,000,000,000 cycles is %"
            ISIZE_PRID ".\n",
        cyclesLoad
    );
    platform_free(&platform);
    platform_free(&clone);
    return EXIT_SUCCESS;
}