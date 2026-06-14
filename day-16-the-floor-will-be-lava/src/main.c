#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/math.h>
#include <scu/memory.h>
#include <scu/queue.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a tile on a grid. */
typedef enum Tile {
    TILE_EMPTY,
    TILE_MIRROR_SLASH,
    TILE_MIRROR_BACKSLASH,
    TILE_SPLITTER_VERTICAL,
    TILE_SPLITTER_HORIZONTAL
} Tile;

/** @brief Represents a two-dimensional grid of tiles. */
typedef struct Grid {

    /** @brief The width of the grid. */
    isize width;

    /** @brief The height of the grid. */
    isize height;

    /**
     * @brief The tiles of the grid.
     *
     * @note This is actually a two-dimensional array of `width * height` tiles
     * stored as a one-dimensional array in row-major order.
     */
    Tile* tiles;

} Grid;

/** @brief Represents a two-dimensional position. */
typedef struct Position {

    /** @brief The x-coordinate of the position. */
    isize x;

    /** @brief The y-coordinate of the position. */
    isize y;

} Position;

/** @brief Represents a direction of movement. */
typedef enum Direction {
    DIRECTION_LEFT = 1 << 0,
    DIRECTION_UP = 1 << 1,
    DIRECTION_RIGHT = 1 << 2,
    DIRECTION_DOWN = 1 << 3
} Direction;

/** @brief Represents a state of the energy beam. */
typedef struct State {

    /** @brief The current position of the energy beam. */
    Position position;

    /** @brief The current direction of the energy beam. */
    Direction direction;

} State;

/**
 * @brief Parses a tile from a specified character.
 *
 * The character must be either `'.'` (for `TILE_EMPTY`), `'/'` (for
 * `TILE_MIRROR_SLASH`), `'\\'` (for `TILE_MIRROR_BACKSLASH`), `'|'` (for
 * `TILE_SPLITTER_VERTICAL`) or `'-'` (for `TILE_SPLITTER_HORIZONTAL`).
 *
 * @param[in]  c     The character to parse.
 * @param[out] tile  The parsed tile on success, otherwise unspecified.
 * @return `true` if a tile was successfully parsed, otherwise `false`.
 */
static inline bool tile_parse(char c, Tile* tile) {
    SCU_ASSERT(tile != nullptr);
    switch (c) {
        case '.':
            *tile = TILE_EMPTY;
            return true;
        case '/':
            *tile = TILE_MIRROR_SLASH;
            return true;
        case '\\':
            *tile = TILE_MIRROR_BACKSLASH;
            return true;
        case '|':
            *tile = TILE_SPLITTER_VERTICAL;
            return true;
        case '-':
            *tile = TILE_SPLITTER_HORIZONTAL;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Deallocates a specified grid.
 *
 * @note If `grid` is a `nullptr`, this function does nothing.
 *
 * Note that this function only deallocates any resources associated with the
 * grid, but not the grid itself. The caller is responsible for deallocating the
 * grid manually if it was dynamically allocated.
 *
 * @warning The behavior is undefined if `grid` is used after it has been
 * deallocated.
 *
 * @param[in, out] grid The grid to deallocate.
 */
static inline void grid_free(Grid* grid) {
    if (grid != nullptr) {
        scu_list_free(grid->tiles);
        grid->tiles = nullptr;
    }
}

/**
 * @brief Parses a grid from the standard input stream.
 *
 * The input must consist of zero or more lines of text, each containing the
 * same number of characters. Each character must be one of the valid tiles
 * as described by the documentation of `tile_parse()`. The individual lines (if
 * any) must be separated by newline characters.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * .|...\....
 * |.-.\.....
 * .....|-...
 * ........|.
 * ..........
 * .........\
 * ..../.\\..
 * .-.-/..|..
 * .|....-|.\
 * ..//.|....
 * ```
 *
 * @param[out] grid The parsed grid on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if a grid was successfully parsed, otherwise an
 * appropriate error code.
 */
static ScuError grid_parse(Grid* grid) {
    SCU_ASSERT(grid != nullptr);
    grid->width = 0;
    grid->height = 0;
    grid->tiles = scu_list_new(SCU_SIZEOF(Tile));
    if (grid->tiles == nullptr) {
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
            grid->width = width;
            foundWidth = true;
        }
        else if (width != grid->width) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        char* temp = line;
        Tile tile;
        while (tile_parse(*temp, &tile)) {
            error = scu_list_add(&grid->tiles, &tile);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
            temp++;
        }
        if (*temp != '\0') {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        grid->height++;
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    grid_free(grid);
    return error;
}

/**
 * @brief Determines whether a position exists on a specified grid.
 *
 * @param[in]  grid      The grid to check.
 * @param[in]  position  The position to check.
 * @return `true` if the position exists on the specified grid, otherwise
 * `false`.
 */
static inline bool grid_exists(const Grid* grid, Position position) {
    SCU_ASSERT(grid != nullptr);
    return (position.x >= 0) && (position.x < grid->width)
        && (position.y >= 0) && (position.y < grid->height);
}

/**
 * @brief Returns the position resulting from moving from a specified position
 * in a specified direction.
 *
 * @param[in] position  The current position.
 * @param[in] direction The direction to move in.
 * @return The resulting position after moving from the current position in the
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
 * @brief Reflects a specified direction off a slash-shaped mirror (`'/'`).
 *
 * @param[in] direction The incoming direction.
 * @return The reflected direction.
 */
static inline Direction direction_reflect_slash(Direction direction) {
    switch (direction) {
        case DIRECTION_LEFT:
            return DIRECTION_DOWN;
        case DIRECTION_UP:
            return DIRECTION_RIGHT;
        case DIRECTION_RIGHT:
            return DIRECTION_UP;
        case DIRECTION_DOWN:
            return DIRECTION_LEFT;
        default:
            SCU_UNREACHABLE();
    }
}

/**
 * @brief Reflects a specified direction off a backslash-shaped mirror (`'\'`).
 *
 * @param[in] direction The incoming direction.
 * @return The reflected direction.
 */
static inline Direction direction_reflect_backslash(Direction direction) {
    switch (direction) {
        case DIRECTION_LEFT:
            return DIRECTION_UP;
        case DIRECTION_UP:
            return DIRECTION_LEFT;
        case DIRECTION_RIGHT:
            return DIRECTION_DOWN;
        case DIRECTION_DOWN:
            return DIRECTION_RIGHT;
        default:
            SCU_UNREACHABLE();
    }
}

/**
 * @brief Enqueues a state resulting from moving from a specified position in a
 * specified direction if the resulting position exists on a specified grid.
 *
 * @warning The behavior is undefined if `queue` is not a queue of states.
 *
 * @param[in]      grid      The grid to check.
 * @param[in, out] queue     The queue to enqueue the state into.
 * @param[in]      position  The current position.
 * @param[in]      direction The direction to move in.
 * @return `SCU_ERROR_NONE` if the state was enqueued or the position does not
 * exist, otherwise an appropriate error code.
 */
static inline ScuError enqueue_if_exists(
    const Grid* grid,
    ScuQueue* queue,
    Position position,
    Direction direction
) {
    SCU_ASSERT(grid != nullptr);
    SCU_ASSERT(grid_exists(grid, position));
    SCU_ASSERT(queue != nullptr);
    State next = {
        .position = position_move(position, direction),
        .direction = direction
    };
    return grid_exists(grid, next.position)
        ? scu_queue_enqueue(queue, &next)
        : SCU_ERROR_NONE;
}

/**
 * @brief Returns the number of energized tiles in a specified grid after
 * starting a beam of energy at a specified position and direction and letting
 * it propagate through the grid.
 *
 * @param[in] grid      The grid for the simulation.
 * @param[in] position  The initial position of the energy beam.
 * @param[in] direction The initial direction of the energy beam.
 * @return The number of energized tiles after simulating the beam, or `-1` if
 * an error occurred.
 */
static isize grid_energized_tiles(
    const Grid* grid,
    Position position,
    Direction direction
) {
    SCU_ASSERT(grid != nullptr);
    SCU_ASSERT(grid_exists(grid, position));
    u8* visited = scu_calloc(grid->width * grid->height, SCU_SIZEOF(u8));
    if (visited == nullptr) {
        return -1;
    }
    isize energizedTiles = -1;
    ScuQueue* queue = scu_queue_new(SCU_SIZEOF(State));
    if (queue == nullptr) {
        goto end;
    }
    ScuError error = scu_queue_enqueue(
        queue,
        &(State) { .position = position, .direction = direction }
    );
    if (error != SCU_ERROR_NONE) {
        goto end;
    }
    State state;
    while (scu_queue_try_dequeue(queue, &state)) {
        position = state.position;
        direction = state.direction;
        isize index = position.y * grid->width + position.x;
        if ((visited[index] & direction) != 0) {
            continue;
        }
        visited[index] |= (u8) direction;
        switch (grid->tiles[index]) {
            case TILE_EMPTY:
                error = enqueue_if_exists(grid, queue, position, direction);
                break;
            case TILE_MIRROR_SLASH:
                error = enqueue_if_exists(
                    grid,
                    queue,
                    position,
                    direction_reflect_slash(direction)
                );
                break;
            case TILE_MIRROR_BACKSLASH:
                error = enqueue_if_exists(
                    grid,
                    queue,
                    position,
                    direction_reflect_backslash(direction)
                );
                break;
            case TILE_SPLITTER_VERTICAL:
                if (
                    (direction == DIRECTION_UP) || (direction == DIRECTION_DOWN)
                ) {
                    error = enqueue_if_exists(grid, queue, position, direction);
                }
                else {
                    error = enqueue_if_exists(
                        grid,
                        queue,
                        position,
                        DIRECTION_UP
                    );
                    if (error == SCU_ERROR_NONE) {
                        error = enqueue_if_exists(
                            grid,
                            queue,
                            position,
                            DIRECTION_DOWN
                        );
                    }
                }
                break;
            case TILE_SPLITTER_HORIZONTAL:
                if (
                    (direction == DIRECTION_LEFT)
                        || (direction == DIRECTION_RIGHT)
                ) {
                    error = enqueue_if_exists(grid, queue, position, direction);
                }
                else {
                    error = enqueue_if_exists(
                        grid,
                        queue,
                        position,
                        DIRECTION_LEFT
                    );
                    if (error == SCU_ERROR_NONE) {
                        error = enqueue_if_exists(
                            grid,
                            queue,
                            position,
                            DIRECTION_RIGHT
                        );
                    }
                }
                break;
            default:
                SCU_UNREACHABLE();
        }
        if (error != SCU_ERROR_NONE) {
            goto end;
        }
    }
    energizedTiles = 0;
    for (isize i = 0; i < grid->width * grid->height; i++) {
        if (visited[i] != 0) {
            energizedTiles++;
        }
    }
end:
    scu_queue_free(queue);
    scu_free(visited);
    return energizedTiles;
}

/**
 * @brief Returns the maximum number of energized tiles by starting a beam of
 * energy at any position on the edge of a specified grid and letting it
 * propagate towards the inside of the grid.
 *
 * @param[in] grid The grid for the simulation.
 * @return The maximum number of energized tiles over all starting positions and
 * directions on the edge of the grid, or `-1` if an error occurred.
 */
static isize grid_max_energized_tiles(const Grid* grid) {
    SCU_ASSERT(grid != nullptr);
    isize maxEnergizedTiles = 0;
    for (isize x = 0; x < grid->width; x++) {
        isize energizedTiles = grid_energized_tiles(
            grid,
            (Position) { .x = x, .y = 0 },
            DIRECTION_DOWN
        );
        if (energizedTiles == -1) {
            return -1;
        }
        maxEnergizedTiles = SCU_MAX(maxEnergizedTiles, energizedTiles);
        energizedTiles = grid_energized_tiles(
            grid,
            (Position) { .x = x, .y = grid->height - 1 },
            DIRECTION_UP
        );
        if (energizedTiles == -1) {
            return -1;
        }
        maxEnergizedTiles = SCU_MAX(maxEnergizedTiles, energizedTiles);
    }
    for (isize y = 0; y < grid->height; y++) {
        isize energizedTiles = grid_energized_tiles(
            grid,
            (Position) { .x = 0, .y = y },
            DIRECTION_RIGHT
        );
        if (energizedTiles == -1) {
            return -1;
        }
        maxEnergizedTiles = SCU_MAX(maxEnergizedTiles, energizedTiles);
        energizedTiles = grid_energized_tiles(
            grid,
            (Position) { .x = grid->width - 1, .y = y },
            DIRECTION_LEFT
        );
        if (energizedTiles == -1) {
            return -1;
        }
        maxEnergizedTiles = SCU_MAX(maxEnergizedTiles, energizedTiles);
    }
    return maxEnergizedTiles;
}

int main() {
    Grid grid;
    ScuError error = grid_parse(&grid);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    isize energizedTiles = grid_energized_tiles(
        &grid,
        (Position) { .x = 0, .y = 0 },
        DIRECTION_RIGHT
    );
    isize maxEnergizedTiles = grid_max_energized_tiles(&grid);
    scu_printf(
        "Starting from the top-left corner and moving to the right, %"
            ISIZE_PRID " tiles will be energized.\n",
        energizedTiles
    );
    scu_printf(
        "The maximum number of energized tiles over all starting positions and "
            "directions is %" ISIZE_PRID ".\n",
        maxEnergizedTiles
    );
    grid_free(&grid);
    return EXIT_SUCCESS;
}