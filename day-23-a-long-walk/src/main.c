#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/math.h>
#include <scu/memory.h>
#include <scu/queue.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a tile in a two-dimensional grid. */
typedef enum Tile {
    TILE_PATH,
    TILE_FOREST,
    TILE_SLOPE_LEFT,
    TILE_SLOPE_UP,
    TILE_SLOPE_RIGHT,
    TILE_SLOPE_DOWN
} Tile;

/** @brief Represents a two-dimensional position. */
typedef struct Position {
    isize x;
    isize y;
} Position;

/** @brief Represents a two-dimensional grid of tiles. */
typedef struct Grid {

    /** @brief The width of the grid. */
    isize width;

    /** @brief The height of the grid. */
    isize height;

    /**
     * @brief The tiles of the grid.
     *
     * @note This is actually a two-dimensional array of `width * height` tiles,
     * stored as a one-dimensional array in row-major order.
     */
    Tile* tiles;

    /** @brief The starting position in the grid. */
    Position start;

    /** @brief The ending position in the grid. */
    Position end;

} Grid;

/** @brief Represents an edge in a graph. */
typedef struct Edge {

    /** @brief The index of the destination junction. */
    isize dest;

    /** @brief The weight of the edge. */
    isize weight;

} Edge;

/** @brief The maximum number of edges per junction. */
static constexpr isize MAX_EDGES = 4;

/** @brief The maximum number of junctions in the graph. */
static constexpr isize MAX_JUNCTIONS = 64;

/** @brief Represents a junction in a graph. */
typedef struct Junction {

    /** @brief The position of the junction. */
    Position position;

    /** @brief The edges from the junction to other junctions. */
    Edge edges[MAX_EDGES];

    /** @brief The number of edges from the junction to other junctions. */
    isize edgeCount;

    /** @brief A bitmask representing the neighboring junctions. */
    u64 neighbors;

} Junction;

/** @brief Represents a graph of junctions and edges. */
typedef struct Graph {

    /** @brief The junctions in the graph. */
    Junction junctions[MAX_JUNCTIONS];

    /** @brief The number of junctions in the graph. */
    isize junctionCount;

    /** @brief The index of the starting junction. */
    isize start;

    /** @brief The index of the ending junction. */
    isize end;

} Graph;

/** @brief Represents a state in the compression of a grid to a graph. */
typedef struct CompressionState {

    /** @brief The current position in the grid. */
    Position position;

    /** @brief The number of steps taken so far. */
    isize steps;

} CompressionState;

/** @brief Represents a state in the search for longest hike. */
typedef struct State {

    /** @brief The index of the current junction. */
    isize junction;

    /** @brief The index of the next edge to explore. */
    isize nextEdge;

    /** @brief The number of steps taken so far. */
    isize steps;

    /** @brief A bitmask representing the visited junctions. */
    u64 visited;

} State;

/** @brief Represents a direction of movement. */
typedef enum Direction {
    DIRECTION_LEFT,
    DIRECTION_UP,
    DIRECTION_RIGHT,
    DIRECTION_DOWN
} Direction;

/**
 * @brief Parses a tile from a specified character.
 *
 * The character must be `'.'` (for `TILE_PATH`), `'#'` (for `TILE_FOREST`),
 * `'<'` (for `TILE_SLOPE_LEFT`), `'^'` (for `TILE_SLOPE_UP`), `'>'` (for
 * `TILE_SLOPE_RIGHT`), or `'v'` (for `TILE_SLOPE_DOWN`) in order to be parsed
 * successfully.
 *
 * @param[in]  c    The character to parse.
 * @param[out] tile The parsed tile on success, otherwise unspecified.
 * @return `true` if a tile was successfully parsed, otherwise `false`.
 */
static inline bool tile_parse(char c, Tile* tile) {
    SCU_ASSERT(tile != nullptr);
    switch (c) {
        case '.':
            *tile = TILE_PATH;
            return true;
        case '#':
            *tile = TILE_FOREST;
            return true;
        case '<':
            *tile = TILE_SLOPE_LEFT;
            return true;
        case '^':
            *tile = TILE_SLOPE_UP;
            return true;
        case '>':
            *tile = TILE_SLOPE_RIGHT;
            return true;
        case 'v':
            *tile = TILE_SLOPE_DOWN;
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
 * grid itself if it was dynamically allocated.
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
 * The input must consist of one or more lines of text, each containing the same
 * number of characters. Each character must represent a tile as described in
 * the documentation of `tile_parse()`. The individual lines must be separated
 * with a newline character.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * #.#####################
 * #.......#########...###
 * #######.#########.#.###
 * ###.....#.>.>.###.#.###
 * ###v#####.#v#.###.#.###
 * ###.>...#.#.#.....#...#
 * ###v###.#.#.#########.#
 * ###...#.#.#.......#...#
 * #####.#.#.#######.#.###
 * #.....#.#.#.......#...#
 * #.#####.#.#.#########v#
 * #.#...#...#...###...>.#
 * #.#.#v#######v###.###v#
 * #...#.>.#...>.>.#.###.#
 * #####v#.#.###v#.#.###.#
 * #.....#...#...#.#.#...#
 * #.#########.###.#.#.###
 * #...###...#...#...#.###
 * ###.###.#.###v#####v###
 * #...#...#.#.>.>.#.>.###
 * #.###.###.#.###.#.#v###
 * #.....###...###...#...#
 * #####################.#
 * ```
 *
 * @param[out] grid The parsed grid on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError grid_parse(Grid* grid) {
    SCU_ASSERT(grid != nullptr);
    *grid = (Grid) {};
    grid->tiles = scu_list_new(SCU_SIZEOF(Tile));
    if (grid->tiles == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    bool foundWidth = false;
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
    if (!foundWidth) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    scu_free(line);
    for (isize i = 0; i < grid->width; i++) {
        if (grid->tiles[i] == TILE_PATH) {
            grid->start = (Position) {.x = i, .y = 0};
            break;
        }
    }
    for (isize i = 0; i < grid->width; i++) {
        if (grid->tiles[(grid->height - 1) * grid->width + i] == TILE_PATH) {
            grid->end = (Position) { .x = i, .y = grid->height - 1 };
            break;
        }
    }
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    grid_free(grid);
    return error;
}

/**
 * @brief Determines whether two specified positions are equal.
 *
 * @param[in] a The first position.
 * @param[in] b The second position.
 * @return `true` if the specified positions are equal, otherwise `false`.
 */
static inline bool position_equal(Position a, Position b) {
    return (a.x == b.x) && (a.y == b.y);
}

/**
 * @brief Returns the position resulting from moving from a specified position
 * in a specified direction.
 *
 * @param[in] position  The position to move from.
 * @param[in] direction The direction to move in.
 * @return The resulting position after moving in the specified direction.
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
 * @brief Determines whether a specified position exists on a specified grid.
 *
 * @param[in] grid     The grid to examine.
 * @param[in] position The position to check.
 * @return `true` if the specified position exists on the specified grid,
 * otherwise `false`.
 */
static inline bool grid_exists(const Grid* grid, Position position) {
    SCU_ASSERT(grid != nullptr);
    return (position.x >= 0) && (position.x < grid->width)
        && (position.y >= 0) && (position.y < grid->height);
}

/**
 * @brief Returns the index of a specified position in a specified grid.
 *
 * @warning The behavior is undefined if the specified position does not exist
 * on the specified grid.
 *
 * @param[in] grid     The grid to examine.
 * @param[in] position The position to get the index of.
 * @return The index of the specified position in the specified grid.
 */
static inline isize grid_index(const Grid* grid, Position position) {
    SCU_ASSERT(grid != nullptr);
    SCU_ASSERT(grid_exists(grid, position));
    return position.y * grid->width + position.x;
}

/**
 * @brief Converts a specified grid to a compressed graph representation.
 *
 * @param[in]  grid          The grid to convert.
 * @param[in]  respectSlopes Whether to respect slopes, i.e., create a directed
 *                           edge from a slope tile to the next tile in the
 *                           direction of the slope, but not the other way
 *                           around.
 * @param[out] graph The compressed graph on success, otherwise unspecified.
 * @return `true` if the grid was successfully converted, otherwise `false`.
 */
static bool grid_to_compressed_graph(
    const Grid* grid,
    bool respectSlopes,
    Graph* graph
) {
    SCU_ASSERT(grid != nullptr);
    SCU_ASSERT(graph != nullptr);
    *graph = (Graph) { };
    for (isize y = 0; y < grid->height; y++) {
        for (isize x = 0; x < grid->width; x++) {
            Position position = { .x = x, .y = y };
            if (grid->tiles[grid_index(grid, position)] == TILE_FOREST) {
                continue;
            }
            bool isStart = position_equal(position, grid->start);
            bool isEnd = position_equal(position, grid->end);
            isize edges = 0;
            for (
                Direction direction = DIRECTION_LEFT;
                direction <= DIRECTION_DOWN;
                direction++
            ) {
                Position next = position_move(position, direction);
                if (
                    grid_exists(grid, next)
                        && (grid->tiles[grid_index(grid, next)] != TILE_FOREST)
                ) {
                    edges++;
                }
            }
            if (isStart || isEnd || (edges >= 3)) {
                if (graph->junctionCount >= MAX_JUNCTIONS) {
                    return false;
                }
                graph->junctions[graph->junctionCount] = (Junction) {
                    .position = position,
                    .edgeCount = 0
                };
                if (isStart) {
                    graph->start = graph->junctionCount;
                }
                if (isEnd) {
                    graph->end = graph->junctionCount;
                }
                graph->junctionCount++;
            }
        }
    }
    bool* visited = scu_calloc(grid->width * grid->height, SCU_SIZEOF(bool));
    if (visited == nullptr) {
        return false;
    }
    ScuQueue* queue = scu_queue_new(SCU_SIZEOF(CompressionState));
    if (queue == nullptr) {
        scu_free(visited);
        return false;
    }
    for (isize i = 0; i < graph->junctionCount; i++) {
        Junction* junction = &graph->junctions[i];
        scu_memset(
            visited,
            false,
            grid->width * grid->height * SCU_SIZEOF(bool)
        );
        visited[grid_index(grid, junction->position)] = true;
        ScuError error = scu_queue_enqueue(
            queue,
            &(CompressionState) { .position = junction->position, .steps = 0 }
        );
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        CompressionState state;
        while (scu_queue_try_dequeue(queue, &state)) {
            for (
                Direction direction = DIRECTION_LEFT;
                direction <= DIRECTION_DOWN;
                direction++
            ) {
                Position next = position_move(state.position, direction);
                if (
                    !grid_exists(grid, next)
                        || (grid->tiles[grid_index(grid, next)] == TILE_FOREST)
                        || visited[grid_index(grid, next)]
                ) {
                    continue;
                }
                visited[grid_index(grid, next)] = true;
                if (respectSlopes) {
                    Tile nextTile = grid->tiles[grid_index(grid, next)];
                    bool blockedBySlope = false;
                    switch (nextTile) {
                        case TILE_SLOPE_LEFT:
                            blockedBySlope = (direction != DIRECTION_LEFT);
                            break;
                        case TILE_SLOPE_UP:
                            blockedBySlope = (direction != DIRECTION_UP);
                            break;
                        case TILE_SLOPE_RIGHT:
                            blockedBySlope = (direction != DIRECTION_RIGHT);
                            break;
                        case TILE_SLOPE_DOWN:
                            blockedBySlope = (direction != DIRECTION_DOWN);
                            break;
                        default:
                            break;
                    }
                    if (blockedBySlope) {
                        continue;
                    }
                }
                isize neighborJunction = -1;
                for (isize j = 0; j < graph->junctionCount; j++) {
                    if (position_equal(graph->junctions[j].position, next)) {
                        neighborJunction = j;
                        break;
                    }
                }
                if (neighborJunction != -1) {
                    junction->edges[junction->edgeCount] = (Edge) {
                        .dest = neighborJunction,
                        .weight = state.steps + 1
                    };
                    junction->edgeCount++;
                }
                else {
                    error = scu_queue_enqueue(
                        queue,
                        &(CompressionState) {
                            .position = next,
                            .steps = state.steps + 1
                        }
                    );
                    if (error != SCU_ERROR_NONE) {
                        goto fail;
                    }
                }
            }
        }
        for (isize j = 0; j < junction->edgeCount; j++) {
            junction->neighbors |= (1ULL << junction->edges[j].dest);
        }
    }
    scu_queue_free(queue);
    scu_free(visited);
    return true;
fail:
    scu_queue_free(queue);
    scu_free(visited);
    return false;
}

/**
 * @brief Returns the number of steps in the longest hike in a specified graph
 * that does not visit any node more than once.
 *
 * @param[in] graph The graph to examine.
 * @return The number of steps in the longest hike, or `-1` on failure.
 */
static isize graph_longest_hike(const Graph* graph) {
    SCU_ASSERT(graph != nullptr);
    State stack[MAX_JUNCTIONS];
    isize stackCount = 0;
    stack[stackCount] = (State) {
        .junction = graph->start,
        .nextEdge = 0,
        .steps = 0,
        .visited = 1ULL << graph->start
    };
    stackCount++;
    isize longestHike = 0;
    while (stackCount > 0) {
        State* state = &stack[stackCount - 1];
        if (state->junction == graph->end) {
            longestHike = SCU_MAX(longestHike, state->steps);
            stackCount--;
            continue;
        }
        const Junction* junction = &graph->junctions[state->junction];
        if (
            (state->nextEdge >= junction->edgeCount)
                || ((junction->neighbors & ~state->visited) == 0)
        ) {
            stackCount--;
            continue;
        }
        Edge edge = junction->edges[state->nextEdge];
        state->nextEdge++;
        if ((state->visited & (1ULL << edge.dest)) != 0) {
            continue;
        }
        stack[stackCount] = (State) {
            .junction = edge.dest,
            .nextEdge = 0,
            .steps = state->steps + edge.weight,
            .visited = state->visited | (1ULL << edge.dest)
        };
        stackCount++;
    }
    return longestHike;
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
    Graph wetGraph;
    Graph dryGraph;
    if (
        !grid_to_compressed_graph(&grid, true, &wetGraph)
            || !grid_to_compressed_graph(&grid, false, &dryGraph)
    ) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while converting the grid to a compressed graph.\n"
        );
        grid_free(&grid);
        return EXIT_FAILURE;
    }
    grid_free(&grid);
    isize longestWetHike = graph_longest_hike(&wetGraph);
    isize longestDryHike = graph_longest_hike(&dryGraph);
    scu_printf(
        "The longest wet hike is %" ISIZE_PRID " steps.\n",
        longestWetHike
    );
    scu_printf(
        "The longest dry hike is %" ISIZE_PRID " steps.\n",
        longestDryHike
    );
    return EXIT_SUCCESS;
}