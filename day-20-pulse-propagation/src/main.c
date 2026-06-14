#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/equal.h>
#include <scu/hash-map.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/queue.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents the type of a module. */
typedef enum ModuleType {
    MODULE_TYPE_FLIP_FLOP,
    MODULE_TYPE_CONJUNCTION,
    MODULE_TYPE_BROADCASTER
} ModuleType;

/**
 * @brief The maximum length for module names (including the terminating null
 * byte).
 */
static constexpr isize MAX_NAME_LENGTH = 16;

/** @brief Represents a module in the pulse propagation system. */
typedef struct Module {

    /** @brief The type of the module. */
    ModuleType type;

    /** @brief The name of the module. */
    char name[MAX_NAME_LENGTH];

    /** @brief The names of the output modules. */
    char (*outputs)[MAX_NAME_LENGTH];

    /**
     * @brief Whether the module is currently on.
     *
     * @note This field is only relevant for modules of type
     * `MODULE_TYPE_FLIP_FLOP`.
     */
    bool isOn;

    /**
     * @brief A map of the names of the input modules to their inputs.
     *
     * @note This field is only relevant for modules of type
     * `MODULE_TYPE_CONJUNCTION`. For modules of type `MODULE_TYPE_FLIP_FLOP` or
     * `MODULE_TYPE_BROADCASTER`, this is a `nullptr`.
     */
    ScuHashMap* inputs;

} Module;

/** @brief Represents a pulse in the pulse propagation system. */
typedef struct Pulse {

    /** @brief The name of the source module. */
    const char* src;
    
    /** @brief The name of the destination module. */
    const char* dest;

    /** @brief Whether the pulse is high or low. */
    bool isHigh;

} Pulse;

/** @brief The number of button presses to simulate. */
static constexpr isize BUTTON_PRESSES = 1000;

/**
 * @brief Returns a hash for a specified module name.
 *
 * @warning The behavior is undefined if `value` is not a pointer to a module
 * name.
 *
 * @param[in] value A pointer to the module name to hash.
 * @return A hash for the specified module name.
 */
static usize module_name_hash(const void* value) {
    SCU_ASSERT(value != nullptr);
    const char (*name)[MAX_NAME_LENGTH] = value;
    const char* temp = *name;
    return scu_hash_str(&temp);
}

/**
 * @brief Determines whether two specified module names are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a module
 * name.
 *
 * @param[in] a A pointer to the first module name.
 * @param[in] b A pointer to the second module name.
 * @return `true` if the specified module names are equal, otherwise `false`.
 */
static bool module_name_equal(const void* a, const void* b) {
    SCU_ASSERT(a != nullptr);
    SCU_ASSERT(b != nullptr);
    const char (*l)[MAX_NAME_LENGTH] = a;
    const char (*r)[MAX_NAME_LENGTH] = b;
    return scu_strncmp(*l, *r, MAX_NAME_LENGTH) == 0;
}

/**
 * @brief Deallocates a specified module.
 *
 * @note If `module` is a `nullptr`, this function does nothing.
 *
 * Note that this function only deallocates any resources associated with the
 * module, but not the module itself. The caller is responsible for deallocating
 * the module manually it it was dynamically allocated.
 *
 * @warning The behavior is undefined if `module` is used after it has been
 * deallocated.
 *
 * @param[in, out] module The module to deallocate.
 */
static inline void module_free(Module* module) {
    if (module != nullptr) {
        scu_list_free(module->outputs);
        module->outputs = nullptr;
        scu_hash_map_free(module->inputs);
        module->inputs = nullptr;
    }
}

/**
 * @brief Parses a module from a specified line of text.
 *
 * The line of text must have one of the following formats:
 *
 * ```plaintext
 * %<name> -> <outputs>
 * &<name> -> <outputs>
 * broadcaster -> <outputs>
 * ```
 *
 * In the first and second case, the module is a module of type
 * `MODULE_TYPE_FLIP_FLOP` or `MODULE_TYPE_CONJUNCTION`, respectively, and
 * `<name>` represents the name of the module. In the third case, the module is
 * a special module of type `MODULE_TYPE_BROADCASTER` that has the name
 * "broadcaster". In all cases, `<outputs>` is a list of the names of the output
 * modules, separated by commas (and optionally a space after each comma).
 *
 * Some example of valid lines of text might be the following:
 *
 * ```plaintext
 * broadcaster -> a, b, c
 * %a -> b
 * %b -> c
 * %c -> inv
 * &inv -> a
 * ```
 *
 * @param[in]  line   The line of text to parse.
 * @param[out] module The parsed module on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if a module was successfully parsed, otherwise an
 * appropriate error code.
 */
static inline ScuError module_parse(
    const char* restrict line,
    Module* restrict module
) {
    SCU_ASSERT(line != nullptr);
    SCU_ASSERT(module != nullptr);
    isize spaceIndex = scu_str_index_of(line, ' ');
    if ((spaceIndex == -1) || ((spaceIndex - 1) >= MAX_NAME_LENGTH)) {
        return SCU_ERROR_INVALID_FORMAT;
    }
    if ((*line == '%') || (*line == '&')) {
        module->type = (*line == '%')
            ? MODULE_TYPE_FLIP_FLOP
            : MODULE_TYPE_CONJUNCTION;
        line++;
        scu_strncpy(
            module->name,
            SCU_SIZEOF(module->name),
            line,
            spaceIndex - 1
        );
        line += spaceIndex;
    }
    else if (scu_strncmp(line, "broadcaster", spaceIndex) == 0) {
        module->type = MODULE_TYPE_BROADCASTER;
        scu_strncpy(module->name, SCU_SIZEOF(module->name), line, spaceIndex);
        line += spaceIndex + 1;
    }
    else {
        return SCU_ERROR_INVALID_FORMAT;
    }
    spaceIndex = scu_str_index_of(line, ' ');
    if ((spaceIndex == -1) || (scu_strncmp(line, "->", spaceIndex) != 0)) {
        return SCU_ERROR_INVALID_FORMAT;
    }
    line += spaceIndex + 1;
    module->outputs = scu_list_new(SCU_SIZEOF(char[MAX_NAME_LENGTH]));
    if (module->outputs == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    module->isOn = false;
    ScuError error = SCU_ERROR_NONE;
    if (module->type == MODULE_TYPE_CONJUNCTION) {
        module->inputs = scu_hash_map_new(
            SCU_SIZEOF(char[MAX_NAME_LENGTH]),
            SCU_SIZEOF(bool),
            module_name_hash,
            module_name_equal,
            scu_equal_bool
        );
        if (module->inputs == nullptr) {
            error = SCU_ERROR_OUT_OF_MEMORY;
            goto fail;
        }
    }
    else {
        module->inputs = nullptr;
    }
    while (*line != '\0') {
        isize commaIndex = scu_str_index_of(line, ',');
        if (commaIndex == -1) {
            commaIndex = scu_strlen(line);
        }
        if (commaIndex >= MAX_NAME_LENGTH) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        char output[MAX_NAME_LENGTH];
        scu_strncpy(output, SCU_SIZEOF(output), line, commaIndex);
        error = scu_list_add(&module->outputs, output);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        line += commaIndex;
        if (*line == ',') {
            line++;
        }
        if (*line == ' ') {
            line++;
        }
    }
    if (scu_list_count(module->outputs) == 0) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    return SCU_ERROR_NONE;
fail:
    module_free(module);
    return error;
}

/**
 * @brief Determines whether two specified modules are equal.
 *
 * @warning The behavior is undefined if `a` or `b` is not a pointer to a
 * module.
 *
 * @param[in] a A pointer to the first module.
 * @param[in] b A pointer to the second module.
 * @return `true` if the specified modules are equal, otherwise `false`.
 */
static bool module_equal(const void* a, const void* b) {
    SCU_ASSERT(a != nullptr);
    SCU_ASSERT(b != nullptr);
    const Module* l = a;
    const Module* r = b;
    // Realistically speaking, we should probably also compare the inputs and
    // outputs, but the type and name should be sufficient for our purposes.
    return (l->type == r->type)
        && scu_strncmp(l->name, r->name, MAX_NAME_LENGTH) == 0;
}

/**
 * @brief Deallocates a specified map of modules.
 *
 * @note If `modules` is a `nullptr`, this function does nothing.
 *
 * @warning The behavior is undefined if `modules` is used after it has been
 * deallocated.
 *
 * @param[in, out] modules The map of modules to deallocate.
 */
static inline void modules_free(ScuHashMap* modules) {
    if (modules != nullptr) {
        ScuHashMapEntry entry;
        SCU_HASH_MAP_FOREACH(entry, modules) {
            module_free(entry.value);
        }
        scu_hash_map_free(modules);
    }
}

/**
 * @brief Parses a map of modules from the standard input stream.
 *
 * The input is expected to consist of zero or more lines of text, each of which
 * represents a module in the format as described in the documentation of
 * `module_parse`. The individual lines (if any) must be separated by newline
 * characters.
 *
 * @param[out] modules The parsed map of modules on success, otherwise a
 *                     `nullptr`.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError modules_parse(ScuHashMap** modules) {
    SCU_ASSERT(modules != nullptr);
    *modules = scu_hash_map_new(
        SCU_SIZEOF(char[MAX_NAME_LENGTH]),
        SCU_SIZEOF(Module),
        module_name_hash,
        module_name_equal,
        module_equal
    );
    if (*modules == nullptr) {
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
        Module module;
        error = module_parse(line, &module);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        error = scu_hash_map_add(*modules, module.name, &module);
        if (error != SCU_ERROR_NONE) {
            // If we fail to add the module to the map, we need to deallocate it
            // manually here since it won't be caught by the cleanup code at the
            // end of this function.
            module_free(&module);
            goto fail;
        }
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    ScuHashMapEntry entry;
    SCU_HASH_MAP_FOREACH(entry, *modules) {
        char (*name)[MAX_NAME_LENGTH] = entry.key;
        Module* module = entry.value;
        char (*output)[MAX_NAME_LENGTH];
        SCU_LIST_FOREACH(output, module->outputs) {
            Module* dest;
            if (
                scu_hash_map_try_get(*modules, *output, &dest)
                    && (dest->type == MODULE_TYPE_CONJUNCTION)
            ) {
                scu_hash_map_add(dest->inputs, *name, &(bool) { false });
            }
        }
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    modules_free(*modules);
    *modules = nullptr;
    return error;
}

/**
 * @brief Returns the product of the number of low and high pulses that are
 * propagated through the modules in a specified map of modules when a low pulse
 * is sent from the "button" module to the "broadcaster" module 1000 times.
 *
 * @param[in, out] modules The map of modules to simulate.
 * @return The product of the number of low and high pulses, or `-1` on failure.
 */
static isize modules_product_of_pulses(ScuHashMap* modules) {
    SCU_ASSERT(modules != nullptr);
    ScuQueue* queue = scu_queue_new(SCU_SIZEOF(Pulse));
    if (queue == nullptr) {
        return -1;
    }
    isize lowPulses = 0;
    isize highPulses = 0;
    for (isize i = 0; i < BUTTON_PRESSES; i++) {
        ScuError error = scu_queue_enqueue(
            queue,
            &(Pulse) {
                .src = "button",
                .dest = "broadcaster",
                .isHigh = false
            }
        );
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        Pulse pulse;
        while (scu_queue_try_dequeue(queue, &pulse)) {
            if (pulse.isHigh) {
                highPulses++;
            }
            else {
                lowPulses++;
            }
            Module* module;
            if (!scu_hash_map_try_get(modules, pulse.dest, &module)) {
                continue;
            }
            switch (module->type) {
                case MODULE_TYPE_FLIP_FLOP: {
                    if (!pulse.isHigh) {
                        module->isOn = !module->isOn;
                        const char (*output)[MAX_NAME_LENGTH];
                        SCU_LIST_FOREACH(output, module->outputs) {
                            error = scu_queue_enqueue(
                                queue,
                                &(Pulse) {
                                    .src = module->name,
                                    .dest = *output,
                                    .isHigh = module->isOn
                                }
                            );
                            if (error != SCU_ERROR_NONE) {
                                goto fail;
                            }
                        }
                    }
                    break;
                }
                case MODULE_TYPE_CONJUNCTION: {
                    error = scu_hash_map_set(
                        module->inputs,
                        pulse.src,
                        &pulse.isHigh
                    );
                    if (error != SCU_ERROR_NONE) {
                        goto fail;
                    }
                    bool allHigh = true;
                    ScuHashMapEntry entry;
                    SCU_HASH_MAP_FOREACH(entry, module->inputs) {
                        bool isHigh = *(bool*) entry.value;
                        if (!isHigh) {
                            allHigh = false;
                            break;
                        }
                    }
                    const char (*output)[MAX_NAME_LENGTH];
                    SCU_LIST_FOREACH(output, module->outputs) {
                        error = scu_queue_enqueue(
                            queue,
                            &(Pulse) {
                                .src = module->name,
                                .dest = *output,
                                .isHigh = !allHigh
                            }
                        );
                        if (error != SCU_ERROR_NONE) {
                            goto fail;
                        }
                    }
                    break;
                }
                case MODULE_TYPE_BROADCASTER: {
                    const char (*output)[MAX_NAME_LENGTH];
                    SCU_LIST_FOREACH(output, module->outputs) {
                        error = scu_queue_enqueue(
                            queue,
                            &(Pulse) {
                                .src = module->name,
                                .dest = *output,
                                .isHigh = pulse.isHigh
                            }
                        );
                        if (error != SCU_ERROR_NONE) {
                            goto fail;
                        }
                    }
                    break;
                }
                default:
                    SCU_UNREACHABLE();
            }
        }
    }
    scu_queue_free(queue);
    return lowPulses * highPulses;
fail:
    scu_queue_free(queue);
    return -1;
}

/**
 * @brief Resets the state of the modules in a specified map of modules to their
 * default state.
 *
 * @param[in, out] modules The map of modules to reset.
 */
static void modules_reset(ScuHashMap* modules) {
    SCU_ASSERT(modules != nullptr);
    ScuHashMapEntry entry;
    SCU_HASH_MAP_FOREACH(entry, modules) {
        Module* module = entry.value;
        if (module->type == MODULE_TYPE_FLIP_FLOP) {
            module->isOn = false;
        }
        else if (module->type == MODULE_TYPE_CONJUNCTION) {
            ScuHashMapEntry inputEntry;
            SCU_HASH_MAP_FOREACH(inputEntry, module->inputs) {
                bool* isHigh = inputEntry.value;
                *isHigh = false;
            }
        }
    }
}

/**
 * @brief Returns the least common multiple of two specified integers.
 *
 * @param[in] a The first integer.
 * @param[in] b The second integer.
 * @return The least common multiple of the two specified integers.
 */
static inline isize lcm(isize a, isize b) {
    isize m = a;
    isize n = b;
    while (n != 0) {
        isize temp = n;
        n = m % n;
        m = temp;
    }
    return a / m * b;
}

/**
 * @brief Returns the minimum number of button presses required to deliver a
 * single low pulse to the module named "rx".
 *
 * @param[in, out] modules The map of modules to simulate.
 * @return The minimum number of button presses, or `-1` on failure.
 */
static isize modules_min_button_presses(ScuHashMap* modules) {
    SCU_ASSERT(modules != nullptr);
    modules_reset(modules);
    Module* rxInput = nullptr;
    ScuHashMapEntry entry;
    SCU_HASH_MAP_FOREACH(entry, modules) {
        Module* module = entry.value;
        const char (*output)[MAX_NAME_LENGTH];
        SCU_LIST_FOREACH(output, module->outputs) {
            if (scu_strncmp(*output, "rx", MAX_NAME_LENGTH) == 0) {
                rxInput = module;
                goto rxInputFound;
            }
        }
    }
rxInputFound:
    if ((rxInput == nullptr) || (rxInput->type != MODULE_TYPE_CONJUNCTION)) {
        return -1;
    }
    ScuHashMap* cycleLengths = scu_hash_map_new(
        SCU_SIZEOF(char[MAX_NAME_LENGTH]),
        SCU_SIZEOF(isize),
        module_name_hash,
        module_name_equal,
        scu_equal_isize
    );
    if (cycleLengths == nullptr) {
        return -1;
    }
    ScuQueue* queue = scu_queue_new(SCU_SIZEOF(Pulse));
    if (queue == nullptr) {
        scu_hash_map_free(cycleLengths);
        return -1;
    }
    for (
        isize i = 1;
        scu_hash_map_count(cycleLengths) < scu_hash_map_count(rxInput->inputs);
        i++
    ) {
        ScuError error = scu_queue_enqueue(
            queue,
            &(Pulse) {
                .src = "button",
                .dest = "broadcaster",
                .isHigh = false
            }
        );
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        Pulse pulse;
        while (scu_queue_try_dequeue(queue, &pulse)) {
            if (
                pulse.isHigh
                    && (scu_strncmp(
                        pulse.dest,
                        rxInput->name,
                        MAX_NAME_LENGTH
                    ) == 0)
            ) {
                error = scu_hash_map_try_add(cycleLengths, pulse.src, &i);
                if (
                    (error != SCU_ERROR_NONE)
                        && (error != SCU_ERROR_ALREADY_PRESENT)
                ) {
                    goto fail;
                }
            }
            Module* module;
            if (!scu_hash_map_try_get(modules, pulse.dest, &module)) {
                continue;
            }
            switch (module->type) {
                case MODULE_TYPE_FLIP_FLOP: {
                    if (!pulse.isHigh) {
                        module->isOn = !module->isOn;
                        const char (*output)[MAX_NAME_LENGTH];
                        SCU_LIST_FOREACH(output, module->outputs) {
                            error = scu_queue_enqueue(
                                queue,
                                &(Pulse) {
                                    .src = module->name,
                                    .dest = *output,
                                    .isHigh = module->isOn
                                }
                            );
                            if (error != SCU_ERROR_NONE) {
                                goto fail;
                            }
                        }
                    }
                    break;
                }
                case MODULE_TYPE_CONJUNCTION: {
                    error = scu_hash_map_set(
                        module->inputs,
                        pulse.src,
                        &pulse.isHigh
                    );
                    if (error != SCU_ERROR_NONE) {
                        goto fail;
                    }
                    bool allHigh = true;
                    ScuHashMapEntry inputEntry;
                    SCU_HASH_MAP_FOREACH(inputEntry, module->inputs) {
                        bool isHigh = *(bool*) inputEntry.value;
                        if (!isHigh) {
                            allHigh = false;
                            break;
                        }
                    }
                    const char (*output)[MAX_NAME_LENGTH];
                    SCU_LIST_FOREACH(output, module->outputs) {
                        error = scu_queue_enqueue(
                            queue,
                            &(Pulse) {
                                .src = module->name,
                                .dest = *output,
                                .isHigh = !allHigh
                            }
                        );
                        if (error != SCU_ERROR_NONE) {
                            goto fail;
                        }
                    }
                    break;
                }
                case MODULE_TYPE_BROADCASTER: {
                    const char (*output)[MAX_NAME_LENGTH];
                    SCU_LIST_FOREACH(output, module->outputs) {
                        error = scu_queue_enqueue(
                            queue,
                            &(Pulse) {
                                .src = module->name,
                                .dest = *output,
                                .isHigh = pulse.isHigh
                            }
                        );
                        if (error != SCU_ERROR_NONE) {
                            goto fail;
                        }
                    }
                    break;
                }
                default:
                    SCU_UNREACHABLE();
            }
        }
    }
    isize minButtonPresses = 1;
    ScuHashMapEntry cycleEntry;
    SCU_HASH_MAP_FOREACH(cycleEntry, cycleLengths) {
        isize cycleLength = *(isize*) cycleEntry.value;
        minButtonPresses = lcm(minButtonPresses, cycleLength);
    }
    scu_queue_free(queue);
    scu_hash_map_free(cycleLengths);
    return minButtonPresses;
fail:
    scu_queue_free(queue);
    scu_hash_map_free(cycleLengths);
    return -1;
}

int main() {
    ScuHashMap* modules;
    ScuError error = modules_parse(&modules);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    isize productOfPulses = modules_product_of_pulses(modules);
    isize minButtonPresses = modules_min_button_presses(modules);
    scu_printf(
        "After simulating %" ISIZE_PRID " button presses, the product of the "
            "number of low and high pulses is %" ISIZE_PRID ".\n",
        BUTTON_PRESSES,
        productOfPulses
    );
    scu_printf(
        "The minimum number of button presses required to deliver a single low "
            "pulse to the module named \"rx\" is %" ISIZE_PRID ".\n",
        minButtonPresses
    );
    modules_free(modules);
    return EXIT_SUCCESS;
}