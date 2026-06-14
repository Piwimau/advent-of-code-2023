#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/array.h>
#include <scu/assert.h>
#include <scu/compare.h>
#include <scu/equal.h>
#include <scu/hash-map.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/prio-queue.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a two-dimensional grid of heat losses. */
typedef struct Grid {

    /** @brief The width of the grid. */
    isize width;

    /** @brief The height of the grid. */
    isize height;

    /**
     * @brief The heat losses of the grid.
     *
     * @note This is actually a two-dimensional array of `width * height` heat
     * losses stored as a one-dimensional array in row-major order.
     */
    isize* losses;

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
    DIRECTION_LEFT,
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN
} Direction;

/** @brief Represents the state of a crucible. */
typedef struct State {

    /** @brief The current position. */
    Position position;

    /** @brief The current direction of movement. */
    Direction direction;

    /** @brief The number of straight moves since the last turn (if any). */
    isize straightMoves;

} State;

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
        scu_list_free(grid->losses);
        grid->losses = nullptr;
    }
}

/**
 * @brief Parses a grid from the standard input stream.
 *
 * The input must consist of zero or more lines of text, each containing the
 * same number of characters. Each character must be a digit from `'0'` to `'9'`
 * representing the heat loss at that position. The individual lines (if any)
 * must be separated by newline characters.
 *
 * @param[out] grid The parsed grid on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if a grid was successfully parsed, otherwise an
 * appropriate error code.
 */
static ScuError grid_parse(Grid* grid) {
    SCU_ASSERT(grid != nullptr);
    grid->width = 0;
    grid->height = 0;
    grid->losses = scu_list_new(SCU_SIZEOF(isize));
    if (grid->losses == nullptr) {
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
        for (char* c = line; *c != '\0'; c++) {
            if ((*c < '0') || (*c > '9')) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            isize loss = *c - '0';
            error = scu_list_add(&grid->losses, &loss);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
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
 * @brief Returns a hash for a specified state.
 *
 * @warning The behavior is undefined if `value` is not a pointer to a state.
 *
 * @param[in] value A pointer to the state to hash.
 * @return A hash for the specified state.
 */
static usize state_hash(const void* value) {
    SCU_ASSERT(value != nullptr);
    const State* state = value;
    usize hash = 0;
    hash = scu_hash_combine(hash, scu_hash_isize(&state->position.x));
    hash = scu_hash_combine(hash, scu_hash_isize(&state->position.y));
    hash = scu_hash_combine(
        hash,
        scu_hash_isize(&(isize) { state->direction })
    );
    hash = scu_hash_combine(hash, scu_hash_isize(&state->straightMoves));
    return hash;
}

/**
 * @brief Determines whether two specified states are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a state.
 *
 * @param[in] a A pointer to the first state.
 * @param[in] b A pointer to the second state.
 * @return `true` if the specified states are equal, otherwise `false`.
 */
static bool state_equal(const void* a, const void* b) {
    SCU_ASSERT(a != nullptr);
    SCU_ASSERT(b != nullptr);
    const State* l = a;
    const State* r = b;
    return (l->position.x == r->position.x)
        && (l->position.y == r->position.y)
        && (l->direction == r->direction)
        && (l->straightMoves == r->straightMoves);
}

/**
 * @brief Returns the position resulting from moving from a specified position
 * in a specified direction.
 *
 * @param[in] position  The current position.
 * @param[in] direction The direction to move in.
 * @return The resulting position after moving from the specified position in
 * the specified direction.
 */
static inline Position position_move(Position position, Direction direction) {
    switch (direction) {
        case DIRECTION_LEFT:
            return (Position) { position.x - 1, position.y };
        case DIRECTION_UP:
            return (Position) { position.x, position.y - 1 };
        case DIRECTION_RIGHT:
            return (Position) { position.x + 1, position.y };
        case DIRECTION_DOWN:
            return (Position) { position.x, position.y + 1 };
        default:
            SCU_UNREACHABLE();
    }
}

/**
 * @brief Determines whether a specified position is the destination on a
 * specified grid.
 *
 * @param[in] grid     The grid to check.
 * @param[in] position The position to check.
 * @return `true` if the position is the destination on the specified grid,
 * otherwise `false`.
 */
static inline bool grid_is_dest(const Grid* grid, Position position) {
    SCU_ASSERT(grid != nullptr);
    return (position.x == (grid->width - 1))
        && (position.y == (grid->height - 1));
}

/**
 * @brief Determines whether a position exists on a specified grid.
 *
 * @param[in] grid     The grid to check.
 * @param[in] position The position to check.
 * @return `true` if the position exists on the specified grid, otherwise
 * `false`.
 */
static inline bool grid_exists(const Grid* grid, Position position) {
    SCU_ASSERT(grid != nullptr);
    return (position.x >= 0) && (position.x < grid->width)
        && (position.y >= 0) && (position.y < grid->height);
}

/**
 * @brief Returns the heat loss at a specified position on a specified grid.
 *
 * @warning The behavior is undefined if `position` does not exist on `grid`.
 *
 * @param[in] grid     The grid to check.
 * @param[in] position The position to check.
 * @return The heat loss at the specified position on the specified grid.
 */
static inline isize grid_loss_at(const Grid* grid, Position position) {
    SCU_ASSERT(grid != nullptr);
    SCU_ASSERT(grid_exists(grid, position));
    return grid->losses[position.y * grid->width + position.x];
}

/**
 * @brief Returns the minimum heat loss that can be incurred by directing the
 * crucible from the lava pool to the machine parts factory through a specified
 * grid of heat losses, given the minimum and maximum number of straight moves
 * that the crucible must make before turning or reaching the destination.
 *
 * @param[in] grid             The grid of heat losses.
 * @param[in] minStraightMoves The minimum number of straight moves.
 * @param[in] maxStraightMoves The maximum number of straight moves.
 * @return The minimum heat loss for the specified grid and constraints.
 */
static isize grid_min_heat_loss(
    const Grid* grid,
    isize minStraightMoves,
    isize maxStraightMoves
) {
    SCU_ASSERT(grid != nullptr);
    SCU_ASSERT(minStraightMoves >= 1);
    SCU_ASSERT(maxStraightMoves >= minStraightMoves);
    isize minLoss = -1;
    ScuHashMap* minLossByState = scu_hash_map_new(
        SCU_SIZEOF(State),
        SCU_SIZEOF(isize),
        state_hash,
        state_equal,
        scu_equal_isize
    );
    if (minLossByState == nullptr) {
        return -1;
    }
    ScuPrioQueue* queue = scu_prio_queue_new(
        SCU_SIZEOF(State),
        SCU_SIZEOF(isize),
        scu_compare_isize
    );
    if (queue == nullptr) {
        goto end;
    }
    State startRight = {
        .position = { .x = 0, .y = 0 },
        .direction = DIRECTION_RIGHT,
        .straightMoves = 0
    };
    isize zeroLoss = 0;
    ScuError error = scu_hash_map_add(minLossByState, &startRight, &zeroLoss);
    if (error != SCU_ERROR_NONE) {
        goto end;
    }
    error = scu_prio_queue_enqueue(queue, &startRight, &zeroLoss);
    if (error != SCU_ERROR_NONE) {
        goto end;
    }
    State startDown = {
        .position = { .x = 0, .y = 0 },
        .direction = DIRECTION_DOWN,
        .straightMoves = 0
    };
    error = scu_hash_map_add(minLossByState, &startDown, &zeroLoss);
    if (error != SCU_ERROR_NONE) {
        goto end;
    }
    error = scu_prio_queue_enqueue(queue, &startDown, &zeroLoss);
    if (error != SCU_ERROR_NONE) {
        goto end;
    }
    State state;
    isize loss;
    while (scu_prio_queue_try_dequeue(queue, &state, &loss)) {
        Position position = state.position;
        Direction direction = state.direction;
        isize straightMoves = state.straightMoves;
        if (
            grid_is_dest(grid, position) && (straightMoves >= minStraightMoves)
        ) {
            minLoss = loss;
            break;
        }
        if (straightMoves < maxStraightMoves) {
            Position neighbor = position_move(position, direction);
            if (grid_exists(grid, neighbor)) {
                State newState = {
                    .position = neighbor,
                    .direction = direction,
                    .straightMoves = straightMoves + 1
                };
                isize newLoss = loss + grid_loss_at(grid, neighbor);
                isize* prevLoss;
                if (!scu_hash_map_try_get(
                        minLossByState,
                        &newState,
                        &prevLoss
                    )) {
                    error = scu_hash_map_add(
                        minLossByState,
                        &newState,
                        &newLoss
                    );
                    if (error != SCU_ERROR_NONE) {
                        goto end;
                    }
                    error = scu_prio_queue_enqueue(queue, &newState, &newLoss);
                    if (error != SCU_ERROR_NONE) {
                        goto end;
                    }
                }
                else if (newLoss < *prevLoss) {
                    *prevLoss = newLoss;
                    error = scu_prio_queue_enqueue(queue, &newState, &newLoss);
                    if (error != SCU_ERROR_NONE) {
                        goto end;
                    }
                }
            }
        }
        if (straightMoves >= minStraightMoves) {
            Direction turns[] = {
                (direction + DIRECTION_DOWN) % (DIRECTION_DOWN + 1),
                (direction + DIRECTION_UP) % (DIRECTION_DOWN + 1)
            };
            Direction* turn;
            SCU_ARRAY_FOREACH(turn, turns) {
                Position neighbor = position_move(position, *turn);
                if (grid_exists(grid, neighbor)) {
                    State newState = {
                        .position = neighbor,
                        .direction = *turn,
                        .straightMoves = 1
                    };
                    isize newLoss = loss + grid_loss_at(grid, neighbor);
                    isize* prevLoss;
                    if (!scu_hash_map_try_get(
                            minLossByState,
                            &newState,
                            &prevLoss
                        )) {
                        error = scu_hash_map_add(
                            minLossByState,
                            &newState,
                            &newLoss
                        );
                        if (error != SCU_ERROR_NONE) {
                            goto end;
                        }
                        error = scu_prio_queue_enqueue(
                            queue,
                            &newState,
                            &newLoss
                        );
                        if (error != SCU_ERROR_NONE) {
                            goto end;
                        }
                    }
                    else if (newLoss < *prevLoss) {
                        *prevLoss = newLoss;
                        error = scu_prio_queue_enqueue(
                            queue,
                            &newState,
                            &newLoss
                        );
                        if (error != SCU_ERROR_NONE) {
                            goto end;
                        }
                    }
                }
            }
        }
    }
end:
    scu_prio_queue_free(queue);
    scu_hash_map_free(minLossByState);
    return minLoss;
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
    isize minLossStandard = grid_min_heat_loss(&grid, 1, 3);
    isize minLossUltra = grid_min_heat_loss(&grid, 4, 10);
    scu_printf(
        "The minimum heat loss for a standard crucible is %" ISIZE_PRID ".\n",
        minLossStandard
    );
    scu_printf(
        "The minimum heat loss for an ultra crucible is %" ISIZE_PRID ".\n",
        minLossUltra
    );
    grid_free(&grid);
    return EXIT_SUCCESS;
}