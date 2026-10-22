#define SCU_SHORT_ALIASES

#include <scu/scu.h>
#include <stdlib.h>

/** @brief Represents an edge in a graph. */
typedef struct Edge {

    /** @brief The index of the source node of the edge. */
    i32 src;

    /** @brief The index of the destination node of the edge. */
    i32 dst;

} Edge;

/** @brief Represents a graph. */
typedef struct Graph {

    /** @brief The number of nodes in the graph. */
    i32 nodes;

    /**
     * @brief The adjacency list of the graph, where each element represents the
     * number of edges of the corresponding node.
     *
     * @note This is actually a two-dimensional array of `nodes * nodes`
     * elements, stored as a one-dimensional array in row-major order.
     */
    i32* capacities;

} Graph;

/** @brief The maximum size of a name, including the terminating null byte. */
static constexpr isize MAX_NAME_SIZE = 4;

/** @brief The number of edges to cut. */
static constexpr isize EDGES_CUT = 3;

/**
 * @brief Returns a hash for a specified name.
 *
 * @warning The behavior is undefined if `value` is not a pointer to a name.
 *
 * @param[in] value A pointer to the name to hash.
 * @return A hash for the specified name.
 */
static usize name_hash(const void* value) {
    SCU_ASSERT(value);
    const char (*name)[MAX_NAME_SIZE] = value;
    const char* tmp = *name;
    return scu_hash_str(&tmp);
}

/**
 * @brief Determines whether two specified names are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a name.
 *
 * @param[in] a A pointer to the first name.
 * @param[in] b A pointer to the second name.
 * @return `true` if the specified names are equal, otherwise `false`.
 */
static bool name_equal(const void* a, const void* b) {
    SCU_ASSERT(a);
    SCU_ASSERT(b);
    const char (*l)[MAX_NAME_SIZE] = a;
    const char (*r)[MAX_NAME_SIZE] = b;
    return scu_equal_str(&l, &r);
}

/**
 * @brief Parses a graph from the standard input stream.
 *
 * The input must consist of zero or more lines, each of which must have the
 * following format:
 *
 * ```plaintext
 * <name>: <neighbors>
 * ```
 *
 * Here, `<name>` is the name of a node in the graph, and `<neighbors>` is a
 * space-separated list of the names of the neighbors of that node. Each name
 * must have at most `MAX_NAME_SIZE - 1` characters, and must not contain any
 * whitespace or colon.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * jqt: rhn xhk nvd
 * rsh: frs pzl lsr
 * xhk: hfx
 * cmg: qnr nvd lhk bvb
 * rhn: xhk bvb hfx
 * bvb: xhk hfx
 * pzl: lsr hfx nvd
 * qnr: nvd
 * ntq: jqt hfx bvb xhk
 * nvd: lhk
 * lsr: lhk
 * rzs: qnr cmg lsr rsh
 * frs: qnr lhk lsr
 * ```
 *
 * @param[out] graph The parsed graph on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError graph_parse(Graph* graph) {
    SCU_ASSERT(graph);
    ScuHashMap* nameToIdx = scu_hash_map_new(
        char[MAX_NAME_SIZE],
        i32,
        name_hash,
        name_equal,
        scu_equal_i32
    );
    if (!nameToIdx) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    Edge* edges = scu_list_new(Edge);
    if (!edges) {
        scu_hash_map_delete(nameToIdx);
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    ScuError error;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        isize newlineIdx = scu_str_index_of(line, '\n');
        if (newlineIdx != -1) {
            line[newlineIdx] = '\0';
        }
        char name[MAX_NAME_SIZE];
        isize read;
        if (scu_sscanf(line, "%3s:%" ISIZE_SCNN, name, &read) != 1) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        i32 idx;
        i32* existingIdx;
        if (scu_hash_map_try_get(nameToIdx, &name, &existingIdx)) {
            idx = *existingIdx;
        }
        else {
            idx = (i32) scu_hash_map_count(nameToIdx);
            error = scu_hash_map_add(nameToIdx, &name, &idx);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
        }
        char* rest = line + read;
        while (*rest != '\0') {
            char neighbor[MAX_NAME_SIZE];
            if (scu_sscanf(rest, " %3s%" ISIZE_SCNN, neighbor, &read) != 1) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            i32 neighborIdx;
            if (scu_hash_map_try_get(nameToIdx, &neighbor, &existingIdx)) {
                neighborIdx = *existingIdx;
            }
            else {
                neighborIdx = (i32) scu_hash_map_count(nameToIdx);
                error = scu_hash_map_add(nameToIdx, &neighbor, &neighborIdx);
                if (error != SCU_ERROR_NONE) {
                    goto fail;
                }
            }
            Edge edge = {.src = idx, .dst = neighborIdx};
            error = scu_list_add(&edges, &edge);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
            rest += read;
        }
    }
    SCU_ARRAY_DELETE(line, size);
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    graph->nodes = (i32) scu_hash_map_count(nameToIdx);
    scu_hash_map_delete(nameToIdx);
    graph->capacities = SCU_ARRAY_NEW_ZEROED(i32, graph->nodes * graph->nodes);
    if (!graph->capacities) {
        error = SCU_ERROR_OUT_OF_MEMORY;
        goto fail;
    }
    Edge* edge;
    SCU_LIST_FOREACH(edge, edges) {
        graph->capacities[edge->src * graph->nodes + edge->dst]++;
        graph->capacities[edge->dst * graph->nodes + edge->src]++;
    }
    scu_list_delete(edges);
    return SCU_ERROR_NONE;
fail:
    SCU_ARRAY_DELETE(line, size);
    scu_list_delete(edges);
    scu_hash_map_delete(nameToIdx);
    return error;
}

/**
 * @brief Deallocates a specified graph.
 *
 * @note This function only deallocates any resources associated with the graph,
 * but not the graph itself. The caller is responsible for deallocating the
 * graph itself if it was dynamically allocated.
 *
 * @warning The behavior is undefined if `graph` is used after it has been
 * deallocated.
 *
 * @param[in, out] graph The graph to deallocate.
 */
static inline void graph_delete(Graph* graph) {
    if (graph) {
        SCU_ARRAY_DELETE(graph->capacities, graph->nodes * graph->nodes);
    }
}

/**
 * @brief Tries to find a path from a specified source node to a specified
 * destination node in a graph with specified residual capacities, using
 * breadth-first search (BFS).
 *
 * @note The path is stored in the specified `parent` array, where `parent[i]`
 * is the index of the parent of node `i` in the path. The `visited` array is
 * used to keep track of visited nodes during the BFS, and the specified `queue`
 * is used for the BFS itself.
 *
 * @param[in]      residuals The residual capacities of the graph.
 * @param[in]      count     The number of nodes in the graph.
 * @param[in]      src       The source node.
 * @param[in]      dst       The destination node.
 * @param[out]     parent    An array to store the parent of each node in the
 *                           path.
 * @param[out]     visited   An array to keep track of visited nodes.
 * @param[in, out] queue     A queue used for the breadth-first search.
 * @return `true` if a path is found, otherwise `false`.
 */
static inline bool find_path(
    const i32* restrict residuals,
    i32 count,
    i32 src,
    i32 dst,
    i32* restrict parent,
    bool* visited,
    ScuQueue* queue
) {
    SCU_ASSERT(residuals);
    SCU_ASSERT(count >= 0);
    SCU_ASSERT(src >= 0 && src < count);
    SCU_ASSERT(dst >= 0 && dst < count);
    SCU_ASSERT(parent);
    SCU_ASSERT(visited);
    SCU_ASSERT(queue);
    scu_memset(visited, 0, count * SCU_SIZEOF(bool));
    scu_queue_clear(queue);
    parent[src] = src;
    visited[src] = true;
    scu_queue_enqueue(queue, &src);
    i32 u;
    while (scu_queue_try_dequeue(queue, &u)) {
        for (i32 v = 0; v < count; v++) {
            if (!visited[v] && residuals[u * count + v] > 0) {
                parent[v] = u;
                visited[v] = true;
                if (v == dst) {
                    return true;
                }
                scu_queue_enqueue(queue, &v);
            }
        }
    }
    return false;
}

/**
 * @brief Counts the number of nodes reachable from a specified source node in a
 * graph with specified residual capacities, using breadth-first search (BFS).
 *
 * @note The `visited` array is used to keep track of visited nodes during the
 * BFS, and the specified `queue` is used for the BFS itself.
 *
 * @param[in]      residuals The residual capacities of the graph.
 * @param[in]      count     The number of nodes in the graph.
 * @param[in]      src       The source node.
 * @param[out]     visited   An array to keep track of visited nodes.
 * @param[in, out] queue     A queue used for the breadth-first search.
 * @return The number of nodes reachable from the source node.
 */
static inline i32 count_reachable(
    const i32* residuals,
    i32 count,
    i32 src,
    bool* visited,
    ScuQueue* queue
) {
    SCU_ASSERT(residuals);
    SCU_ASSERT(count >= 0);
    SCU_ASSERT(src >= 0 && src < count);
    SCU_ASSERT(visited);
    SCU_ASSERT(queue);
    scu_memset(visited, 0, count * SCU_SIZEOF(bool));
    scu_queue_clear(queue);
    visited[src] = true;
    scu_queue_enqueue(queue, &src);
    i32 reachable = 0;
    i32 u;
    while (scu_queue_try_dequeue(queue, &u)) {
        reachable++;
        for (i32 v = 0; v < count; v++) {
            if (!visited[v] && residuals[u * count + v] > 0) {
                visited[v] = true;
                scu_queue_enqueue(queue, &v);
            }
        }
    }
    return reachable;
}

/**
 * @brief Computes the product of the sizes of two disjoint groups of nodes
 * created by disconnecting three edges in a specified graph.
 *
 * @param[in] graph The graph for the computation.
 * @return The product of the sizes of the two groups of nodes.
 */
static i32 graph_product_of_group_sizes(const Graph* graph) {
    SCU_ASSERT(graph);
    i32 product = -1;
    i32* residuals = SCU_ARRAY_NEW(i32, graph->nodes * graph->nodes);
    i32* parent = SCU_ARRAY_NEW(i32, graph->nodes);
    bool* visited = SCU_ARRAY_NEW(bool, graph->nodes);
    ScuQueue* queue = scu_queue_new(i32);
    if (!residuals || !parent || !visited || !queue) {
        goto end;
    }
    for (i32 sink = 1; sink < graph->nodes; sink++) {
        scu_memcpy(
            residuals,
            graph->capacities,
            graph->nodes * graph->nodes * SCU_SIZEOF(i32)
        );
        i32 flow = 0;
        while (
            flow < EDGES_CUT + 1
                && find_path(
                    residuals,
                    graph->nodes,
                    0,
                    sink,
                    parent,
                    visited,
                    queue
                )
        ) {
            for (i32 v = sink; v != 0; v = parent[v]) {
                i32 u = parent[v];
                residuals[u * graph->nodes + v]--;
                residuals[v * graph->nodes + u]++;
            }
            flow++;
        }
        if (flow == EDGES_CUT) {
            i32 reachable = count_reachable(
                residuals,
                graph->nodes,
                0,
                visited,
                queue
            );
            product = reachable * (graph->nodes - reachable);
            break;
        }
    }
end:
    scu_queue_delete(queue);
    SCU_ARRAY_DELETE(visited, graph->nodes);
    SCU_ARRAY_DELETE(parent, graph->nodes);
    SCU_ARRAY_DELETE(residuals, graph->nodes * graph->nodes);
    return product;
}

int main() {
    Graph graph;
    ScuError error = graph_parse(&graph);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    i32 product = graph_product_of_group_sizes(&graph);
    scu_printf("The product of the sizes of the groups is %d.\n", product);
    graph_delete(&graph);
    return EXIT_SUCCESS;
}