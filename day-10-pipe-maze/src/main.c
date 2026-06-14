#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/array.h>
#include <scu/assert.h>
#include <scu/hash-set.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a cardinal direction. */
typedef enum Direction {
    DIRECTION_NONE = 0,
    DIRECTION_NORTH = 1 << 0,
    DIRECTION_EAST = 1 << 1,
    DIRECTION_SOUTH = 1 << 2,
    DIRECTION_WEST = 1 << 3
} Direction;

/** @brief Represents a tile in a two-dimensional grid. */
typedef enum Tile {
    TILE_GROUND = DIRECTION_NONE,
    TILE_PIPE_NORTH_SOUTH = DIRECTION_NORTH | DIRECTION_SOUTH,
    TILE_PIPE_EAST_WEST = DIRECTION_EAST | DIRECTION_WEST,
    TILE_PIPE_NORTH_EAST = DIRECTION_NORTH | DIRECTION_EAST,
    TILE_PIPE_NORTH_WEST = DIRECTION_NORTH | DIRECTION_WEST,
    TILE_PIPE_SOUTH_WEST = DIRECTION_SOUTH | DIRECTION_WEST,
    TILE_PIPE_SOUTH_EAST = DIRECTION_SOUTH | DIRECTION_EAST
} Tile;

/** @brief Represents a two-dimensional position. */
typedef struct Position {

    /** @brief The x-coordinate of the position. */
    isize x;

    /** @brief The y-coordinate of the position. */
    isize y;

} Position;

/** @brief Represents a two-dimensional grid of tiles. */
typedef struct Grid {

    /**
     * @brief The tiles of the grid.
     *
     * @note This is actually a two-dimensional array of `width * height` tiles,
     * stored as a one-dimensional array in row-major order.
     */
    Tile* tiles;

    /** @brief The width of the grid. */
    isize width;

    /** @brief The height of the grid. */
    isize height;

    /** @brief The starting position in the grid. */
    Position start;

} Grid;

/** @brief Represents the result of analyzing a grid. */
typedef struct GridAnalysis {

    /** @brief The distance to the farthest point in the loop of the grid. */
    i64 farthestDistance;

    /** @brief The number of tiles enclosed by the loop of the grid. */
    i64 enclosedTiles;

} GridAnalysis;

/** @brief An array of all cardinal directions. */
static constexpr Direction DIRECTIONS[] = {
    DIRECTION_NORTH,
    DIRECTION_EAST,
    DIRECTION_SOUTH,
    DIRECTION_WEST
};

/**
 * @brief Returns the opposite of a specified direction.
 *
 * @param[in] direction The direction to get the opposite of.
 * @return The opposite of the specified direction.
 */
static inline Direction direction_opposite(Direction direction) {
    switch (direction) {
        case DIRECTION_NORTH:
            return DIRECTION_SOUTH;
        case DIRECTION_EAST:
            return DIRECTION_WEST;
        case DIRECTION_SOUTH:
            return DIRECTION_NORTH;
        case DIRECTION_WEST:
            return DIRECTION_EAST;
        default:
            SCU_UNREACHABLE();
    }
}

/**
 * @brief Parses a tile from a specified character.
 *
 * The specified character must be one of the following:
 *
 * - `.`: A ground tile.
 *
 * - `|`: A pipe tile that connects north and south.
 *
 * - `-`: A pipe tile that connects east and west.
 *
 * - `L`: A pipe tile that connects north and east.
 *
 * - `J`: A pipe tile that connects north and west.
 *
 * - `7`: A pipe tile that connects south and west.
 *
 * - `F`: A pipe tile that connects south and east.
 *
 * @param[in]  c    The character to parse a tile from.
 * @param[out] tile The parsed tile on success, otherwise unmodified.
 * @return `true` if a tile was successfully parsed, otherwise `false`.
 */
static inline bool tile_parse(char c, Tile* tile) {
    SCU_ASSERT(tile != nullptr);
    switch (c) {
        case '.':
            *tile = TILE_GROUND;
            return true;
        case '|':
            *tile = TILE_PIPE_NORTH_SOUTH;
            return true;
        case '-':
            *tile = TILE_PIPE_EAST_WEST;
            return true;
        case 'L':
            *tile = TILE_PIPE_NORTH_EAST;
            return true;
        case 'J':
            *tile = TILE_PIPE_NORTH_WEST;
            return true;
        case '7':
            *tile = TILE_PIPE_SOUTH_WEST;
            return true;
        case 'F':
            *tile = TILE_PIPE_SOUTH_EAST;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Returns the neighbor of a specified position in a specified direction.
 *
 * @param[in] position  The position to get the neighbor of.
 * @param[in] direction The direction to move to.
 * @return The neighbor of the specified position in the specified direction.
 */
static Position position_neighbor(Position position, Direction direction) {
    switch (direction) {
        case DIRECTION_NORTH:
            return (Position) { .x = position.x, .y = position.y - 1 };
        case DIRECTION_EAST:
            return (Position) { .x = position.x + 1, .y = position.y };
        case DIRECTION_SOUTH:
            return (Position) { .x = position.x, .y = position.y + 1 };
        case DIRECTION_WEST:
            return (Position) { .x = position.x - 1, .y = position.y };
        default:
            SCU_UNREACHABLE();
    }
}

/**
 * @brief Returns a hash for a specified position.
 *
 * @warning The behavior is undefined if `value` is not a pointer to a position.
 *
 * @param[in] value The position to hash.
 * @return A hash for the specified position.
 */
static usize hash_position(const void* value) {
    SCU_ASSERT(value != nullptr);
    const Position* position = value;
    usize hash = 0;
    hash = scu_hash_combine(hash, scu_hash_isize(&position->x));
    hash = scu_hash_combine(hash, scu_hash_isize(&position->y));
    return hash;
}

/**
 * @brief Determines whether two specified positions are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a
 * position.
 *
 * @param[in] a The first position.
 * @param[in] b The second position.
 * @return `true` if `*a` and `*b` are equal, otherwise `false`.
 */
static bool equal_position(const void* a, const void* b) {
    SCU_ASSERT(a != nullptr);
    SCU_ASSERT(b != nullptr);
    const Position* l = a;
    const Position* r = b;
    return (l->x == r->x) && (l->y == r->y);
}

/**
 * @brief Returns a pointer to the tile at a specified position in a grid.
 *
 * @param[in]  grid     The grid to examine.
 * @param[in]  position The position to get the tile at.
 * @return A pointer to the tile at the specified position, or `nullptr` if the
 * position is out of bounds.
 */
static inline Tile* grid_tile_at(const Grid* grid, Position position) {
    SCU_ASSERT(grid != nullptr);
    if (
        (position.x < 0) || (position.x >= grid->width)
            || (position.y < 0) || (position.y >= grid->height)
    ) {
        return nullptr;
    }
    return &grid->tiles[position.y * grid->width + position.x];
}

/**
 * @brief Determines whether a tile at a specified position in a grid is a pipe
 * facing a specified direction.
 *
 * @param[in] grid      The grid to examine.
 * @param[in] position  The position of the tile to examine.
 * @param[in] direction The direction to check for.
 * @return `true` if the tile at the specified position is a pipe facing the
 * specified direction, otherwise `false`.
 */
static inline bool grid_is_pipe_facing(
    const Grid* grid,
    Position position,
    Direction direction
) {
    SCU_ASSERT(grid != nullptr);
    const Tile* tile = grid_tile_at(grid, position);
    return (tile != nullptr) && ((*tile & direction) != 0);
}

/**
 * @brief Deallocates all resources associated with a specified grid.
 *
 * @note If `grid` is a `nullptr` pointer, this function does nothing.
 *
 * Note that this function does not deallocate `grid` itself.
 *
 * @warning The behavior is undefined if `grid` is used after it has been
 * deallocated.
 *
 * @param[in, out] grid The grid to deallocate all resources of.
 */
static void grid_free(Grid* grid) {
    if (grid != nullptr) {
        scu_list_free(grid->tiles);
        grid->tiles = nullptr;
    }
}

/**
 * @brief Parses a grid from the standard input stream.
 *
 * The input is expected to consist of lines of equal length, each containing
 * a sequence of one or more characters representing tiles, followed by an
 * optional terminating newline. For more information about the recognized
 * characters, see `tile_parse()`.
 * 
 * Additionally, the input must contain exactly one 'S' character, which marks
 * the starting position in the grid. It must be possible to determine a valid
 * pipe tile at the starting position based on the surrounding tiles.
 * 
 * An example for a valid input might be the following:
 * 
 * ```plaintext
 * .F----7F7F7F7F-7....
 * .|F--7||||||||FJ....
 * .||.FJ||||||||L7....
 * FJL7L7LJLJ||LJ.L-7..
 * L--J.L7...LJS7F-7L7.
 * ....F-J..F7FJ|L7L7L7
 * ....L7.F7||L7|.L7L7|
 * .....|FJLJ|FJ|F7|.LJ
 * ....FJL-7.||.||||...
 * ....L---J.LJ.LJLJ...
 * ```
 *
 * @param[out] grid The parsed grid on success, or a zero-initialized grid on
 *                  failure.
 * @return `SCU_ERROR_NONE` on success, or an appropriate error code on failure.
 */
static ScuError grid_parse(Grid* grid) {
    SCU_ASSERT(grid != nullptr);
    grid->tiles = scu_list_new(SCU_SIZEOF(Tile));
    if (grid->tiles == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    grid->width = 0;
    grid->height = 0;
    grid->start = (Position) { };
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
        if (width == 0) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        if (!foundWidth) {
            grid->width = width;
            foundWidth = true;
        }
        else if (grid->width != width) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        for (isize i = 0; line[i] != '\0'; i++) {
            Tile tile;
            if (tile_parse(line[i], &tile)) {
                error = scu_list_add(&grid->tiles, &tile);
                if (error != SCU_ERROR_NONE) {
                    goto fail;
                }
            }
            else if (line[i] == 'S') {
                if (!foundStart) {
                    grid->start = (Position) { .x = i, .y = grid->height };
                    foundStart = true;
                    // We simply add a ground tile for now, but we figure out
                    // the actual tile later once the grid is fully parsed.
                    tile = TILE_GROUND;
                    error = scu_list_add(&grid->tiles, &tile);
                    if (error != SCU_ERROR_NONE) {
                        goto fail;
                    }
                }
                else {
                    error = SCU_ERROR_INVALID_FORMAT;
                    goto fail;
                }
            }
            else {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
        }
        grid->height++;
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    if (!foundStart || !foundWidth) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    Tile* start = grid_tile_at(grid, grid->start);
    const Direction* direction;
    SCU_ARRAY_FOREACH(direction, DIRECTIONS) {
        Position neighbor = position_neighbor(grid->start, *direction);
        Direction opposite = direction_opposite(*direction);
        if (grid_is_pipe_facing(grid, neighbor, opposite)) {
            *start |= *direction;
        }
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    grid_free(grid);
    *grid = (Grid) { };
    return error;
}

/**
 * @brief Determines whether a tile at a specified position in a grid is a
 * specified tile.
 *
 * @param[in] grid     The grid to examine.
 * @param[in] position The position of the tile to examine.
 * @param[in] tile     The tile to check for.
 * @return `true` if the tile at the specified position is the specified tile,
 * otherwise `false`.
 */
static inline bool grid_is_tile(
    const Grid* grid,
    Position position,
    Tile tile
) {
    SCU_ASSERT(grid != nullptr);
    const Tile* current = grid_tile_at(grid, position);
    return (current != nullptr) && (*current == tile);
}

/**
 * @brief Analyzes a specified grid to determine the distance to the farthest
 * point in the loop of the grid, as well as the number of tiles enclosed by it.
 *
 * @warning This function assumes that the grid contains a loop of pipes that
 * can be traversed starting from the grid's starting position and that
 * eventually returns back to it. If this assumption is not met, the behavior is
 * undefined.
 *
 * @param[in] grid The grid to analyze.
 * @return The analysis result for the specified grid.
 */
static GridAnalysis grid_analyze(const Grid* grid) {
    SCU_ASSERT(grid != nullptr);
    ScuHashSet* loop = scu_hash_set_new(
        SCU_SIZEOF(Position),
        hash_position,
        equal_position
    );
    if (
        (loop == nullptr)
            || (scu_hash_set_add(loop, &grid->start) != SCU_ERROR_NONE)
    ) {
        goto fail;
    }
    Direction prev = DIRECTION_NONE;
    const Direction* direction;
    SCU_ARRAY_FOREACH(direction, DIRECTIONS) {
        if (grid_is_pipe_facing(grid, grid->start, *direction)) {
            prev = *direction;
            break;
        }
    }
    Position current = position_neighbor(grid->start, prev);
    GridAnalysis analysis = { .farthestDistance = 1 };
    ScuError error;
    while ((error = scu_hash_set_try_add(loop, &current)) == SCU_ERROR_NONE) {
        const Tile* tile = grid_tile_at(grid, current);
        Direction next = *tile ^ direction_opposite(prev);
        current = position_neighbor(current, next);
        prev = next;
        analysis.farthestDistance++;
    }
    if (error != SCU_ERROR_ALREADY_PRESENT) {
        goto fail;
    }
    // At this point, the distance records the entire length of the loop, so the
    // distance to the farthest point is simply half of that.
    analysis.farthestDistance /= 2;
    for (isize y = 0; y < grid->height; y++) {
        bool isEnclosed = false;
        for (isize x = 0; x < grid->width; x++) {
            current = (Position) { .x = x, .y = y };
            if (scu_hash_set_contains(loop, &current)) {
                if (
                    grid_is_tile(grid, current, TILE_PIPE_NORTH_SOUTH)
                        || grid_is_tile(grid, current, TILE_PIPE_SOUTH_EAST)
                        || grid_is_tile(grid, current, TILE_PIPE_SOUTH_WEST)
                ) {
                    isEnclosed = !isEnclosed;
                }
            }
            else if (isEnclosed) {
                analysis.enclosedTiles++;
            }
        }
    }
    scu_hash_set_free(loop);
    return analysis;
fail:
    scu_hash_set_free(loop);
    return (GridAnalysis) { .farthestDistance = -1, .enclosedTiles = -1 };
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
    GridAnalysis analysis = grid_analyze(&grid);
    scu_printf(
        "The distance to the farthest point in the loop is %" I64_PRID ".\n",
        analysis.farthestDistance
    );
    scu_printf(
        "The number of tiles enclosed by the loop is %" I64_PRID ".\n",
        analysis.enclosedTiles
    );
    grid_free(&grid);
    return EXIT_SUCCESS;
}