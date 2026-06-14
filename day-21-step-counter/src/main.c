#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/queue.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a tile in a map. */
typedef enum Tile {
    TILE_GARDEN,
    TILE_ROCK
} Tile;

/** @brief Represents a two-dimensional position. */
typedef struct Position {

    /** @brief The x-coordinate of the position. */
    isize x;

    /** @brief The y-coordinate of the position. */
    isize y;

} Position;

/** @brief Represents a map of the garden. */
typedef struct Map {

    /** @brief The width of the map. */
    isize width;

    /** @brief The height of the map. */
    isize height;

    /**
     * @brief The tiles of the map.
     *
     * @note This is actually a two-dimensional array of `width * height` tiles,
     * stored as a one-dimensional array in row-major order.
     */
    Tile* tiles;

    /** @brief The starting position. */
    Position start;

} Map;

/** @brief Represents a direction of movement. */
typedef enum Direction {
    DIRECTION_LEFT,
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN
} Direction;

/**
 * @brief The number of steps for counting the reachable garden plots on a
 * finite map.
 */
static constexpr isize STEPS_FINITE = 64;

/**
 * @brief The number of steps for counting the reachable garden plots on an
 * infinite map.
 */
static constexpr isize STEPS_INFINITE = 26'501'365;

/**
 * @brief Parses a tile from a character.
 *
 * The character must be a `.` (for `TILE_GARDEN`) or a `#` (for `TILE_ROCK`) in
 * order to be successfully parsed.
 *
 * @param[in]  c    The character to parse.
 * @param[out] tile The parsed tile on success, otherwise unspecified.
 * @return `true` if a tile was successfully parsed, otherwise `false`.
 */
static inline bool tile_parse(char c, Tile* tile) {
    SCU_ASSERT(tile != nullptr);
    switch (c) {
        case 'S':
            [[fallthrough]];
        case '.':
            *tile = TILE_GARDEN;
            return true;
        case '#':
            *tile = TILE_ROCK;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Returns the position resulting from moving from a specified position
 * in a specified direction.
 *
 * @param[in] position  The position to start from.
 * @param[in] direction The direction to move in.
 * @return The position resulting from moving from the specified position in the
 * specified direction.
 */
static inline Position position_move(Position position, Direction direction) {
    switch (direction) {
        case DIRECTION_LEFT:
            return (Position) { .x = position.x - 1, .y = position.y };
        case DIRECTION_UP:
            return (Position) { .x = position.x, .y = position.y - 1 };
        case DIRECTION_RIGHT:
            return (Position) { .x = position.x + 1, .y = position.y };
        case DIRECTION_DOWN:
            return (Position) { .x = position.x, .y = position.y + 1 };
        default:
            SCU_UNREACHABLE();
    }
}

/**
 * @brief Deallocates a specified map.
 *
 * @note If `map` is a `nullptr`, this function does nothing.
 *
 * Note that this function only deallocates any resources associated with the
 * map, but not the map itself. The caller is responsible for deallocating the
 * map itself if it was dynamically allocated.
 *
 * @warning The behavior is undefined if `map` is used after it has been
 * deallocated.
 *
 * @param[in, out] map The map to deallocate.
 */
static inline void map_free(Map* map) {
    if (map != nullptr) {
        scu_list_free(map->tiles);
        map->tiles = nullptr;
    }
}

/**
 * @brief Parses a map from the standard input stream.
 *
 * The input must consist of one or more lines of text, each containing the
 * same number of characters. Each character must be either `'.'` (for
 * `TILE_GARDEN`) or `'#'` (for `TILE_ROCK`). Additionally, there must be
 * exactly one `S` character (marking the starting position). The individual
 * lines (if any) must be separated by newline characters.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * ...........
 * .....###.#.
 * .###.##..#.
 * ..#.#...#..
 * ....#.#....
 * .##..S####.
 * .##..#...#.
 * .......##..
 * .##.#.####.
 * .##..##.##.
 * ...........
 * ```
 *
 * @param[out] map The parsed map on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if a map was successfully parsed, otherwise an
 * appropriate error code.
 */
static ScuError map_parse(Map* map) {
    SCU_ASSERT(map != nullptr);
    map->width = 0;
    map->height = 0;
    map->tiles = scu_list_new(SCU_SIZEOF(Tile));
    if (map->tiles == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    map->start = (Position) {};
    bool foundWidth = false;
    bool foundStart = false;
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
            map->width = width;
            foundWidth = true;
        }
        else if (width != map->width) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        char* temp = line;
        Tile tile;
        while (tile_parse(*temp, &tile)) {
            error = scu_list_add(&map->tiles, &tile);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
            if (*temp == 'S') {
                if (!foundStart) {
                    map->start = (Position) {
                        .x = temp - line,
                        .y = map->height
                    };
                    foundStart = true;
                }
                else {
                    error = SCU_ERROR_INVALID_FORMAT;
                    goto fail;
                }
            }
            temp++;
        }
        if (*temp != '\0') {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        map->height++;
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    if (!foundStart) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    map_free(map);
    return error;
}

/**
 * @brief Counts the number of garden plots that can be reached from the
 * starting position of a specified map in a specified number of steps.
 *
 * @param[in] map        The map to check.
 * @param[in] steps      The exact number of steps to take.
 * @param[in] isInfinite Whether to treat the map as infinite (i.e., repeating
 *                       infinitely in all directions) or not.
 * @return The number of reachable garden plots, or -1 on failure.
 */
static isize map_reachable(const Map* map, isize steps, bool isInfinite) {
    SCU_ASSERT(map != nullptr);
    SCU_ASSERT(steps >= 0);
    isize width = isInfinite ? 2 * steps + 1 : map->width;
    isize height = isInfinite ? 2 * steps + 1 : map->height;
    Position offset = {
        .x = isInfinite ? steps : 0,
        .y = isInfinite ? steps : 0
    };
    isize* dists = scu_malloc(width * height * SCU_SIZEOF(isize));
    if (dists == nullptr) {
        return -1;
    }
    for (isize i = 0; i < (width * height); i++) {
        dists[i] = -1;
    }
    Position start = isInfinite ? (Position) { .x = 0, .y = 0 } : map->start;
    dists[(start.y + offset.y) * width + start.x + offset.x] = 0;
    ScuQueue* queue = scu_queue_new(SCU_SIZEOF(Position));
    if (queue == nullptr) {
        scu_free(dists);
        return -1;
    }
    ScuError error = scu_queue_enqueue(queue, &start);
    if (error != SCU_ERROR_NONE) {
        goto fail;
    }
    Position prev;
    while (scu_queue_try_dequeue(queue, &prev)) {
        isize prevDist = dists[
            (prev.y + offset.y) * width + prev.x + offset.x
        ];
        for (Direction dir = DIRECTION_LEFT; dir <= DIRECTION_DOWN; dir++) {
            Position next = position_move(prev, dir);
            Position actualNext = {
                .x = next.x + offset.x,
                .y = next.y + offset.y
            };
            if (
                (actualNext.x < 0) || (actualNext.x >= width)
                    || (actualNext.y < 0) || (actualNext.y >= height)
                    || (dists[actualNext.y * width + actualNext.x] != -1)
            ) {
                continue;
            }
            Position tile = {
                .x = isInfinite
                    ? ((next.x + map->start.x) % map->width + map->width)
                        % map->width
                    : next.x,
                .y = isInfinite
                    ? ((next.y + map->start.y) % map->height + map->height)
                        % map->height
                    : next.y,
            };
            if (map->tiles[tile.y * map->width + tile.x] != TILE_GARDEN) {
                continue;
            }
            dists[actualNext.y * width + actualNext.x] = prevDist + 1;
            error = scu_queue_enqueue(queue, &next);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
        }
    }
    isize reachablePlots = 0;
    isize parity = steps % 2;
    for (isize i = 0; i < (width * height); i++) {
        isize dist = dists[i];
        if ((dist >= 0) && (dist <= steps) && ((dist % 2) == parity)) {
            reachablePlots++;
        }
    }
    scu_queue_free(queue);
    scu_free(dists);
    return reachablePlots;
fail:
    scu_queue_free(queue);
    scu_free(dists);
    return -1;
}

/**
 * @brief Returns the number of garden plots that can be reached from the
 * starting position of a specified map in exactly `STEPS_FINITE` if the map is
 * treated as finite.
 *
 * @param[in] map The map to check.
 * @return The number of reachable garden plots, or -1 on failure.
 */
static inline isize map_reachable_finite(const Map* map) {
    return map_reachable(map, STEPS_FINITE, false);
}

/**
 * @brief Returns the number of garden plots that can be reached from the
 * starting position of a specified map in exactly `STEPS_INFINITE` if the map
 * is treated as infinite (i.e., repeating infinitely in all directions).
 *
 * @param[in] map The map to check.
 * @return The number of reachable garden plots, or -1 on failure.
 */
static inline isize map_reachable_infinite(const Map* map) {
    SCU_ASSERT(map != nullptr);
    isize half = map->width / 2;
    isize period = map->width;
    isize f0 = map_reachable(map, half, true);
    isize f1 = map_reachable(map, half + period, true);
    isize f2 = map_reachable(map, half + 2 * period, true);
    isize d1 = f1 - f0;
    isize d2 = f2 - f1;
    isize n = (STEPS_INFINITE - half) / period;
    return f0 + n * d1 + n * (n - 1) / 2 * (d2 - d1);
}

int main() {
    Map map;
    ScuError error = map_parse(&map);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    isize reachableFinite = map_reachable_finite(&map);
    isize reachableInfinite = map_reachable_infinite(&map);
    scu_printf(
        "With exactly %" ISIZE_PRID " steps, the Elf could reach %" ISIZE_PRID
            " garden plots on the finite map.\n",
        STEPS_FINITE,
        reachableFinite
    );
    scu_printf(
        "With exactly %" ISIZE_PRID " steps, the Elf could reach %" ISIZE_PRID
            " garden plots on the infinite map.\n",
        STEPS_INFINITE,
        reachableInfinite
    );
    map_free(&map);
    return EXIT_SUCCESS;
}