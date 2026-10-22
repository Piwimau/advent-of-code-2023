#define SCU_SHORT_ALIASES

#include <scu/scu.h>
#include <stdlib.h>

/** @brief Represents a three-dimensional position. */
typedef struct Position {

    /** @brief The x-coordinate of the position. */
    isize x;

    /** @brief The y-coordinate of the position. */
    isize y;

    /** @brief The z-coordinate of the position. */
    isize z;

} Position;

/** @brief Represents a rectangular brick. */
typedef struct Brick {

    /** @brief The first position of the brick. */
    Position first;

    /** @brief The last position of the brick. */
    Position last;

} Brick;

/** @brief Represents a support graph for a list of bricks. */
typedef struct SupportGraph {

    /** @brief The list of bricks. */
    const Brick* bricks;

    /**
     * @brief The list of bricks supported by each brick.
     *
     * @note This is a dynamically allocated array of `scu_list_count(bricks)`
     * elements, where the `i`-th element is a list of indices of bricks that
     * are supported by the `i`-th brick in `bricks`.
     */
    isize** supports;

    /**
     * @brief The list of bricks that support each brick.
     *
     * @note This is a dynamically allocated array of `scu_list_count(bricks)`
     * elements, where the `i`-th element is a list of indices of bricks that
     * support the `i`-th brick in `bricks`.
     */
    isize** supportedBy;

} SupportGraph;

/**
 * @brief Parses a brick from a specified line of text.
 *
 * The line must have the following format:
 *
 * ```plaintext
 * <x1>,<y1>,<z1>~<x2>,<y2>,<z2>
 * ```
 *
 * Here, `<x1>`, `<y1>`, `<z1>`, `<x2>`, `<y2>`, and `<z2>` are integers
 * representing the coordinates of the first and last positions of the brick,
 * respectively.
 *
 * Some examples for valid lines of text might be the following:
 *
 * ```plaintext
 * 1,0,1~1,2,1
 * 0,0,2~2,0,2
 * 0,2,3~2,2,3
 * 0,0,4~0,2,4
 * 2,0,5~2,2,5
 * 0,1,6~2,1,6
 * 1,1,8~1,1,9
 * ```
 *
 * @param[in]  line  The line of text to parse.
 * @param[out] brick The parsed brick on success, otherwise unspecified.
 * @return `true` if a brick was successfully parsed, otherwise `false`.
 */
static inline bool brick_parse(
    const char* restrict line,
    Brick* restrict brick
) {
    SCU_ASSERT(line);
    SCU_ASSERT(brick);
    isize read = scu_sscanf(
        line,
        "%" ISIZE_SCND ",%" ISIZE_SCND ",%" ISIZE_SCND "~%" ISIZE_SCND
        ",%" ISIZE_SCND ",%" ISIZE_SCND,
        &brick->first.x,
        &brick->first.y,
        &brick->first.z,
        &brick->last.x,
        &brick->last.y,
        &brick->last.z
    );
    return read == 6;
}

/**
 * @brief Deallocates a specified list of bricks.
 *
 * @note If `bricks` is a `nullptr`, this function does nothing.
 *
 * @param[in, out] bricks The list of bricks to deallocate.
 */
static inline void bricks_delete(Brick* bricks) {
    if (bricks) {
        scu_list_delete(bricks);
    }
}

/**
 * @brief Parses a list of bricks from the standard input stream.
 *
 * The input must consist of zero or more lines of text, each of which must have
 * the format as described in the documentation of `brick_parse()`. The
 * individual lines (if any) must be separated by a newline character.
 *
 * An example for valid input might be the following:
 *
 * ```plaintext
 * 1,0,1~1,2,1
 * 0,0,2~2,0,2
 * 0,2,3~2,2,3
 * 0,0,4~0,2,4
 * 2,0,5~2,2,5
 * 0,1,6~2,1,6
 * 1,1,8~1,1,9
 * ```
 *
 * @param[out] bricks The parsed list of bricks on success, otherwise a
 *                    `nullptr`.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError bricks_parse(Brick** bricks) {
    SCU_ASSERT(bricks);
    *bricks = scu_list_new(Brick);
    if (!*bricks) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    ScuError error;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        isize newlineIndex = scu_str_index_of(line, '\n');
        if (newlineIndex != -1) {
            line[newlineIndex] = '\0';
        }
        Brick brick;
        if (!brick_parse(line, &brick)) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        error = scu_list_add(bricks, &brick);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    SCU_ARRAY_DELETE(line, size);
    return SCU_ERROR_NONE;
fail:
    SCU_ARRAY_DELETE(line, size);
    bricks_delete(*bricks);
    *bricks = nullptr;
    return error;
}

/**
 * @brief Compares two specified bricks by their minimum z-coordinate.
 *
 * @param[in] a A pointer to the first brick.
 * @param[in] b A pointer to the second brick.
 * @return `-1` if the minimum z-coordinate of `a` is less than that of `b`, `0`
 * if they compare equal, or `1` if the minimum z-coordinate of `a` is greater
 * than that of `b`.
 */
static int bricks_compare_by_min_z(const void* a, const void* b) {
    SCU_ASSERT(a);
    SCU_ASSERT(b);
    const Brick* l = a;
    const Brick* r = b;
    isize lMinZ = SCU_MIN(l->first.z, l->last.z);
    isize rMinZ = SCU_MIN(r->first.z, r->last.z);
    return (lMinZ < rMinZ) ? -1 : (lMinZ > rMinZ) ? 1 : 0;
}

/**
 * @brief Settles the bricks in a specified list of bricks.
 *
 * @note This function possibly modifies both the order of the bricks in the
 * specified list, as well as their z-coordinates.
 *
 * @param[in, out] bricks The list of bricks to settle.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError bricks_settle(Brick* bricks) {
    SCU_ASSERT(bricks);
    scu_list_sort(bricks, bricks_compare_by_min_z);
    isize* settledBrickIndices = scu_list_new(isize);
    if (!settledBrickIndices) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    for (isize i = 0; i < scu_list_count(bricks); i++) {
        Brick* brick = &bricks[i];
        isize maxSettledZ = 0;
        isize* settledBrickIndex;
        SCU_LIST_FOREACH(settledBrickIndex, settledBrickIndices) {
            Brick* settledBrick = &bricks[*settledBrickIndex];
            if (
                brick->first.x > settledBrick->last.x
                    || brick->last.x < settledBrick->first.x
                    || brick->first.y > settledBrick->last.y
                    || brick->last.y < settledBrick->first.y
            ) {
                continue;
            }
            isize settledZ = SCU_MAX(
                settledBrick->first.z,
                settledBrick->last.z
            );
            maxSettledZ = SCU_MAX(maxSettledZ, settledZ);
        }
        isize minZ = SCU_MIN(brick->first.z, brick->last.z);
        isize maxZ = SCU_MAX(brick->first.z, brick->last.z);
        isize height = maxZ - minZ;
        brick->first.z = maxSettledZ + 1;
        brick->last.z = maxSettledZ + 1 + height;
        ScuError error = scu_list_add(&settledBrickIndices, &i);
        if (error != SCU_ERROR_NONE) {
            scu_list_delete(settledBrickIndices);
            return error;
        }
    }
    scu_list_delete(settledBrickIndices);
    return SCU_ERROR_NONE;
}

/**
 * @brief Deallocates a specified support graph.
 *
 * @note If `supportGraph` is a `nullptr`, this function does nothing.
 *
 * Note that this function only deallocates the resources owned by the support
 * graph, but not the list of bricks or the support graph itself. The caller is
 * responsible for deallocating those resources if they were dynamically
 * allocated.
 *
 * @param[in, out] supportGraph The support graph to deallocate.
 */
static inline void support_graph_delete(SupportGraph* supportGraph) {
    if (supportGraph) {
        isize count = scu_list_count(supportGraph->bricks);
        supportGraph->bricks = nullptr;
        for (isize i = 0; i < count; i++) {
            scu_list_delete(supportGraph->supports[i]);
            supportGraph->supports[i] = nullptr;
        }
        SCU_ARRAY_DELETE(supportGraph->supports, count);
        supportGraph->supports = nullptr;
        for (isize i = 0; i < count; i++) {
            scu_list_delete(supportGraph->supportedBy[i]);
            supportGraph->supportedBy[i] = nullptr;
        }
        SCU_ARRAY_DELETE(supportGraph->supportedBy, count);
        supportGraph->supportedBy = nullptr;
    }
}

/**
 * @brief Builds a support graph out of a specified list of bricks.
 *
 * @note This function possibly modifies both the order of the bricks in the
 * specified list, as well as their z-coordinates.
 *
 * @warning The support graph does not take ownership of the list of bricks, but
 * retains a reference to it. The caller is responsible for ensuring that the
 * list of bricks remains valid for the entire lifetime of the support graph,
 * and for deallocating the list of bricks if it is no longer needed.
 *
 * @param[in, out] bricks       The list of bricks to build the support graph
 *                              out of.
 * @param[out]     supportGraph The support graph on success, otherwise
 *                              unspecified.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError bricks_build_support_graph(
    Brick* bricks,
    SupportGraph* supportGraph
) {
    SCU_ASSERT(bricks);
    SCU_ASSERT(supportGraph);
    ScuError error = bricks_settle(bricks);
    if (error != SCU_ERROR_NONE) {
        return error;
    }
    supportGraph->bricks = bricks;
    isize count = scu_list_count(bricks);
    supportGraph->supports = SCU_ARRAY_NEW_ZEROED(isize*, count);
    if (!supportGraph->supports) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    supportGraph->supportedBy = SCU_ARRAY_NEW_ZEROED(isize*, count);
    if (!supportGraph->supportedBy) {
        SCU_ARRAY_DELETE(supportGraph->supports, count);
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    for (isize i = 0; i < count; i++) {
        supportGraph->supports[i] = scu_list_new(isize);
        if (!supportGraph->supports[i]) {
            error = SCU_ERROR_OUT_OF_MEMORY;
            goto fail;
        }
        supportGraph->supportedBy[i] = scu_list_new(isize);
        if (!supportGraph->supportedBy[i]) {
            error = SCU_ERROR_OUT_OF_MEMORY;
            goto fail;
        }
    }
    for (isize i = 0; i < count; i++) {
        isize maxZ = SCU_MAX(bricks[i].first.z, bricks[i].last.z);
        for (isize j = 0; j < count; j++) {
            if (i == j) {
                continue;
            }
            isize minZ = SCU_MIN(bricks[j].first.z, bricks[j].last.z);
            if (minZ != (maxZ + 1)) {
                continue;
            }
            if (
                bricks[i].first.x > bricks[j].last.x
                    || bricks[i].last.x < bricks[j].first.x
                    || bricks[i].first.y > bricks[j].last.y
                    || bricks[i].last.y < bricks[j].first.y
            ) {
                continue;
            }
            error = scu_list_add(&supportGraph->supports[i], &j);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
            error = scu_list_add(&supportGraph->supportedBy[j], &i);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
        }
    }
    return SCU_ERROR_NONE;
fail:
    support_graph_delete(supportGraph);
    return error;
}

/**
 * @brief Returns the number of bricks in a specified support graph that can
 * safely be disintegrated.
 *
 * @param[in] supportGraph The support graph to check.
 * @return The number of bricks that can safely be disintegrated, or `-1` on
 * failure.
 */
static isize support_graph_disintegratable(const SupportGraph* supportGraph) {
    SCU_ASSERT(supportGraph);
    isize disintegratable = 0;
    for (isize i = 0; i < scu_list_count(supportGraph->bricks); i++) {
        bool isSafelyDisintegratable = true;
        isize* supported;
        SCU_LIST_FOREACH(supported, supportGraph->supports[i]) {
            if (scu_list_count(supportGraph->supportedBy[*supported]) < 2) {
                isSafelyDisintegratable = false;
                break;
            }
        }
        if (isSafelyDisintegratable) {
            disintegratable++;
        }
    }
    return disintegratable;
}

/**
 * @brief Returns the total number of bricks that would be disintegrated in a
 * chain reaction if each brick in a specified support graph were disintegrated.
 *
 * @param[in] supportGraph The support graph to check.
 * @return The total number of bricks that would disintegrate in a chain
 * reaction, or `-1` on failure.
 */
static isize support_graph_chain_disintegrated(
    const SupportGraph* supportGraph
) {
    SCU_ASSERT(supportGraph);
    isize count = scu_list_count(supportGraph->bricks);
    isize* remainingSupports = SCU_ARRAY_NEW(isize, count);
    if (!remainingSupports) {
        return -1;
    }
    ScuQueue* queue = scu_queue_new(isize);
    if (!queue) {
        SCU_ARRAY_DELETE(remainingSupports, count);
        return -1;
    }
    isize totalDisintegrated = 0;
    for (isize i = 0; i < count; i++) {
        for (isize j = 0; j < count; j++) {
            remainingSupports[j] = scu_list_count(supportGraph->supportedBy[j]);
        }
        scu_queue_clear(queue);
        ScuError error = scu_queue_enqueue(queue, &i);
        if (error != SCU_ERROR_NONE) {
            totalDisintegrated = -1;
            goto end;
        }
        isize disintegrated;
        while (scu_queue_try_dequeue(queue, &disintegrated)) {
            isize* supported;
            SCU_LIST_FOREACH(supported, supportGraph->supports[disintegrated]) {
                remainingSupports[*supported]--;
                if (remainingSupports[*supported] == 0) {
                    totalDisintegrated++;
                    error = scu_queue_enqueue(queue, supported);
                    if (error != SCU_ERROR_NONE) {
                        totalDisintegrated = -1;
                        goto end;
                    }
                }
            }
        }
    }
end:
    scu_queue_delete(queue);
    SCU_ARRAY_DELETE(remainingSupports, count);
    return totalDisintegrated;
}

int main() {
    Brick* bricks;
    ScuError error = bricks_parse(&bricks);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    SupportGraph supportGraph;
    error = bricks_build_support_graph(bricks, &supportGraph);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while building the support graph (code %d).\n",
            error
        );
        bricks_delete(bricks);
        return EXIT_FAILURE;
    }
    isize disintegratable = support_graph_disintegratable(&supportGraph);
    isize chainDisintegrated = support_graph_chain_disintegrated(&supportGraph);
    scu_printf(
        "%" ISIZE_PRID " bricks can safely be disintegrated.\n",
        disintegratable
    );
    scu_printf(
        "%" ISIZE_PRID " bricks would be disintegrated in a chain reaction.\n",
        chainDisintegrated
    );
    support_graph_delete(&supportGraph);
    bricks_delete(bricks);
    return EXIT_SUCCESS;
}