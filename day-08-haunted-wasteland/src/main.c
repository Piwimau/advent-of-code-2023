#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/hash-map.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a direction for navigating a network of nodes. */
typedef enum Direction {
    DIRECTION_LEFT,
    DIRECTION_RIGHT
} Direction;

/** @brief Represents the name of a node. */
typedef char NodeName[4];

/** @brief Represents a node in a network. */
typedef struct Node {

    /** @brief The name of the node. */
    NodeName name;

    /** @brief The name of left neighbor node. */
    NodeName left;

    /** @brief The name of right neighbor node. */
    NodeName right;

} Node;

/** @brief Represents a network of nodes. */
typedef struct Network {

    /** @brief A list of directions for navigating the network. */
    Direction* directions;

    /** @brief A hash map of node names to nodes in the network. */
    ScuHashMap* nodes;

} Network;

/**
 * @brief Returns a hash for a specified node name.
 *
 * @warning The behavior is undefined if `value` is not a pointer to a node
 * name.
 *
 * @param[in] value A pointer to the node name to hash.
 * @return A hash for the specified node name.
 */
static usize hash_node_name(const void* value) {
    return scu_hash_bytes(value, SCU_SIZEOF(NodeName));
}

/**
 * @brief Determines whether two specified node names are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a node
 * name.
 *
 * @param[in] a A pointer to the first node name.
 * @param[in] b A pointer to the second node name.
 * @return `true` if `*a` and `*b` are equal, otherwise `false`.
 */
static bool equal_node_name(const void* a, const void* b) {
    SCU_ASSERT(a != nullptr);
    SCU_ASSERT(b != nullptr);
    return scu_strncmp(a, b, SCU_SIZEOF(NodeName)) == 0;
}

/**
 * @brief Determines whether two specified nodes are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a node.
 *
 * @param[in] a A pointer to the first node.
 * @param[in] b A pointer to the second node.
 * @return `true` if `*a` and `*b` are equal, otherwise `false`.
 */
static bool equal_node(const void* a, const void* b) {
    SCU_ASSERT(a != nullptr);
    SCU_ASSERT(b != nullptr);
    const Node* l = a;
    const Node* r = b;
    return (scu_strncmp(l->name, r->name, SCU_SIZEOF(NodeName)) == 0)
        && (scu_strncmp(l->left, r->left, SCU_SIZEOF(NodeName)) == 0)
        && (scu_strncmp(l->right, r->right, SCU_SIZEOF(NodeName)) == 0);
}

/**
 * @brief Parses a direction from a specified character.
 *
 * The character must be either an 'L' (for `DIRECTION_LEFT`) or an 'R' (for
 * `DIRECTION_RIGHT`).
 *
 * @param[in]  c         The character to parse a direction from.
 * @param[out] direction The parsed direction on success, otherwise unmodified.
 * @return `true` if a direction was successfully parsed, otherwise `false`.
 */
static inline bool direction_parse(char c, Direction* direction) {
    SCU_ASSERT(direction != nullptr);
    switch (c) {
        case 'L':
            *direction = DIRECTION_LEFT;
            return true;
        case 'R':
            *direction = DIRECTION_RIGHT;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Parses a network from the standard input stream.
 *
 * The input must consist of lines in the following format:
 *
 * ```plaintext
 * <Directions>
 *
 * <Nodes>
 * ```
 *
 * &lt;Directions&gt; is a sequence of zero or more characters (either 'L' or
 * 'R' each), representing a list of directions for navigating the network. It
 * must end with a newline and is followed by an empty line (i.e., a line that
 * consists of a single newline character). &lt;Nodes&gt; is a sequence of
 * lines, each representing a node in the network in the following format:
 *
 * ```plaintext
 * <Name> = (<Left>, <Right>)
 * ```
 *
 * &lt;Name&gt;, &lt;Left&gt;, and &lt;Right&gt; are strings of three characters
 * each, representing the name of the node, as well as the names of its left and
 * right neighbor nodes, respectively. Just like above, each line must end with
 * a newline character.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * RL
 *
 * AAA = (BBB, CCC)
 * BBB = (DDD, EEE)
 * CCC = (ZZZ, GGG)
 * DDD = (DDD, DDD)
 * EEE = (EEE, EEE)
 * GGG = (GGG, GGG)
 * ZZZ = (ZZZ, ZZZ)
 * ```
 *
 * @param[out] network The parsed network on success, or a zero-initialized
 *                     network on failure.
 * @return `SCU_ERROR_NONE` on success, or an appropriate error code on failure.
 */
static ScuError network_parse(Network* network) {
    SCU_ASSERT(network != nullptr);
    ScuError error = SCU_ERROR_NONE;
    network->directions = scu_list_new(SCU_SIZEOF(Direction));
    if (network->directions == nullptr) {
        error = SCU_ERROR_OUT_OF_MEMORY;
        goto fail;
    }
    network->nodes = scu_hash_map_new(
        SCU_SIZEOF(NodeName),
        SCU_SIZEOF(Node),
        hash_node_name,
        equal_node_name,
        equal_node
    );
    if (network->nodes == nullptr) {
        error = SCU_ERROR_OUT_OF_MEMORY;
        goto fail;
    }
    char* line = nullptr;
    isize size = 0;
    error = scu_readln(&line, &size);
    if (error != SCU_ERROR_NONE) {
        goto fail;
    }
    for (isize i = 0; line[i] != '\0'; i++) {
        Direction direction;
        if (direction_parse(line[i], &direction)) {
            error = scu_list_add(&network->directions, &direction);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
        }
        else if (line[i] != '\n') {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
    }
    error = scu_readln(&line, &size);
    if (error != SCU_ERROR_NONE) {
        goto fail;
    }
    if (scu_strcmp(line, "\n") != 0) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        Node node;
        isize read = scu_sscanf(
            line,
            "%3s = (%3s, %3s)\n",
            node.name,
            node.left,
            node.right
        );
        if (read != 3) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        error = scu_hash_map_add(network->nodes, &node.name, &node);
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
    scu_list_free(network->directions);
    scu_hash_map_free(network->nodes);
    network->directions = nullptr;
    network->nodes = nullptr;
    return error;
}

/**
 * @brief Returns the number of steps required to reach the node named `dest` by
 * starting from the node named "AAA" and repeatedly following the directions in
 * the network.
 *
 * @warning This function assumes that the network contains nodes named "AAA"
 * and `dest`, and that following the directions will eventually lead to the
 * node named `dest`. If these assumptions are not met, the behavior is
 * undefined.
 *
 * The behavior is also undefined if `dest` is not a pointer to a
 * null-terminated byte string.
 *
 * @param[in] network The network to navigate.
 * @param[in] dest    The name of the destination node.
 * @return The number of steps required to reach the node named `dest`.
 */
static i64 network_required_steps(
    const Network* network,
    const NodeName* dest
) {
    SCU_ASSERT(network != nullptr);
    SCU_ASSERT(dest != nullptr);
    const Node* current = scu_hash_map_get(
        network->nodes,
        &(NodeName) { "AAA" }
    );
    isize count = scu_list_count(network->directions);
    i64 steps = 0;
    while (scu_strcmp(current->name, *dest) != 0) {
        Direction direction = network->directions[steps % count];
        if (direction == DIRECTION_LEFT) {
            current = scu_hash_map_get(network->nodes, &current->left);
        }
        else {
            current = scu_hash_map_get(network->nodes, &current->right);
        }
        steps++;
    }
    return steps;
}

/**
 * @brief Returns the number of steps required to reach any end node (i.e., a
 * node whose name ends with 'Z') by starting from a specified source node and
 * repeatedly following the directions in the network.
 *
 * @warning This function assumes that the network contains a node named `src`
 * and that following the directions will eventually lead to an end node. If
 * these assumptions are not met, the behavior is undefined.
 *
 * The behavior is also undefined if `src` is not a pointer to a null-terminated
 * byte string.
 *
 * @param[in] network The network to navigate.
 * @param[in] src     The name of the source node.
 * @return The number of steps required to reach any end node.
 */
static inline i64 network_required_steps_any(
    const Network* network,
    const NodeName* src
) {
    SCU_ASSERT(network != nullptr);
    SCU_ASSERT(src != nullptr);
    const Node* current = scu_hash_map_get(network->nodes, src);
    isize count = scu_list_count(network->directions);
    i64 steps = 0;
    while (!scu_str_ends_with(current->name, 'Z')) {
        Direction direction = network->directions[steps % count];
        if (direction == DIRECTION_LEFT) {
            current = scu_hash_map_get(network->nodes, &current->left);
        }
        else {
            current = scu_hash_map_get(network->nodes, &current->right);
        }
        steps++;
    }
    return steps;
}

/**
 * @brief Returns the least common multiple of two specified `i64` values.
 *
 * @param[in] a The first value.
 * @param[in] b The second value.
 * @return The least common multiple of `a` and `b`.
 */
static inline i64 lcm(i64 a, i64 b) {
    i64 tempA = a;
    i64 tempB = b;
    while (tempB != 0) {
        i64 temp = tempB;
        tempB = tempA % tempB;
        tempA = temp;
    }
    i64 gcd = tempA;
    return a / gcd * b;
}

/**
 * @brief Returns the number of steps required to reach all end nodes (i.e.,
 * nodes whose names end with 'Z') simultaneously by starting from all start
 * nodes (i.e., nodes whose names end with 'A') and repeatedly following the
 * directions in the network.
 *
 * @warning This function assumes that the network contains at least one start
 * node and at least one end node, and that following the directions will
 * eventually lead to all end nodes simultaneously. If these assumptions are not
 * met, the behavior is undefined.
 *
 * @param[in] network The network to navigate.
 * @return The number of steps required to reach all end nodes simultaneously.
 */
static i64 network_required_steps_all(const Network* network) {
    SCU_ASSERT(network != nullptr);
    i64 steps = 1;
    ScuHashMapEntry entry;
    SCU_HASH_MAP_FOREACH(entry, network->nodes) {
        Node* node = entry.value;
        if (scu_str_ends_with(node->name, 'A')) {
            steps = lcm(
                steps,
                network_required_steps_any(network, &node->name)
            );
        }
    }
    return steps;
}

/**
 * @brief Deallocates all resources associated with a specified network.
 *
 * @note If `network` is a `nullptr`, this function does nothing.
 *
 * Note that this function does not deallocate `network` itself.
 *
 * @warning The behavior is undefined if `network` is used after it has been
 * deallocated.
 *
 * @param[in, out] network The network to deallocate all resources of.
 */
static void network_free(Network* network) {
    if (network != nullptr) {
        scu_list_free(network->directions);
        scu_hash_map_free(network->nodes);
        network->directions = nullptr;
        network->nodes = nullptr;
    }
}

int main() {
    Network network;
    ScuError error = network_parse(&network);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    i64 requiredSteps = network_required_steps(&network, &(NodeName) { "ZZZ" });
    i64 requiredStepsAll = network_required_steps_all(&network);
    scu_printf(
        "It takes %" I64_PRID " steps to reach node 'ZZZ'.\n",
        requiredSteps
    );
    scu_printf(
        "It takes %" I64_PRID " steps to reach all nodes ending with 'Z' "
            "simultaneously.\n",
        requiredStepsAll
    );
    network_free(&network);
    return EXIT_SUCCESS;
}