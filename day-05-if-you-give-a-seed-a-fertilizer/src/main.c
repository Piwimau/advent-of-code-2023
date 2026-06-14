#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/array.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/math.h>
#include <scu/memory.h>
#include <scu/stack.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/**
 * @brief Represents a mapping that translates a range of source values to a
 * range of destination values.
 */
typedef struct Mapping {

    /** @brief The first value of the destination range. */
    i64 dest;

    /** @brief The first value of the source range. */
    i64 src;

    /** @brief The length of the range. */
    i64 length;

} Mapping;

/**
 * @brief Represents a map containing zero or more mappings to translate ranges
 * of source values to ranges of destination values.
 */
typedef struct Map {

    /** @brief The list of mappings in the map. */
    Mapping* mappings;

} Map;

/** @brief Headers used to identify different maps while parsing the input. */
static const char* const MAP_HEADERS[] = {
    "seed-to-soil map:\n",
    "soil-to-fertilizer map:\n",
    "fertilizer-to-water map:\n",
    "water-to-light map:\n",
    "light-to-temperature map:\n",
    "temperature-to-humidity map:\n",
    "humidity-to-location map:\n"
};

/**
 * @brief Represents an almanac containing seeds (or ranges of seeds) and maps
 * to translate those seeds to their corresponding locations.
 */
typedef struct Almanac {

    /** @brief The list of seeds. */
    i64* seeds;

    /** @brief The maps in the almanac. */
    Map maps[SCU_COUNTOF(MAP_HEADERS)];

} Almanac;

/** @brief Represents a range of seeds. */
typedef struct Range {

    /** @brief The first value of the seed range. */
    i64 src;

    /** @brief The length of the seed range. */
    i64 length;

} Range;

/**
 * @brief Deallocates all resources associated with a specified almanac.
 *
 * @note If `almanac` is a `nullptr`, this function does nothing.
 *
 * Note that this function does not deallocate the `almanac` itself.
 *
 * @param[in, out] almanac The almanac to deallocate all resources of.
 */
static void almanac_free(Almanac* almanac) {
    if (almanac != nullptr) {
        scu_list_free(almanac->seeds);
        almanac->seeds = nullptr;
        Map* map;
        SCU_ARRAY_FOREACH(map, almanac->maps) {
            scu_list_free(map->mappings);
            map->mappings = nullptr;
        }
    }
}

/**
 * @brief Parses an almanac from the standard input stream.
 *
 * The input lines must have the following format:
 *
 * ```plaintext
 * seeds: <Seeds>
 *
 * seed-to-soil map:
 * <Mappings>
 *
 * soil-to-fertilizer map:
 * <Mappings>
 *
 * fertilizer-to-water map:
 * <Mappings>
 *
 * water-to-light map:
 * <Mappings>
 *
 * light-to-temperature map:
 * <Mappings>
 *
 * temperature-to-humidity map:
 * <Mappings>
 *
 * humidity-to-location map:
 * <Mappings>
 * ```
 *
 * Here, &lt;Seeds&gt; is a list of positive integers (separated by one or more
 * spaces each). &lt;Mappings&gt; represents a list of zero or more lines, each
 * containing three positive integers separated by spaces, representing the
 * destination, source, and length of a mapping, respectively. The seeds and
 * maps should appear in the order shown above, and each must appear exactly
 * once. Empty lines may be used to separate sections for readability and are
 * ignored during parsing. All lines must end with a newline, regardless of the
 * actual content.
 *
 * @param[out] almanac The parsed almanac on success, otherwise the default.
 * @return `SCU_ERROR_NONE` on success, or an appropriate error code on failure.
 */
static ScuError almanac_parse(Almanac* almanac) {
    SCU_ASSERT(almanac != nullptr);
    *almanac = (Almanac) { };
    ScuError error;
    char* line = nullptr;
    isize size = 0;
    Map* currentMap = nullptr;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        if ((line[0] >= '0') && (line[0] <= '9')) {
            if (currentMap == nullptr) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            Mapping mapping;
            isize read = scu_sscanf(
                line,
                "%" I64_SCND " %" I64_SCND " %" I64_SCND "\n",
                &mapping.dest,
                &mapping.src,
                &mapping.length
            );
            if (read != 3) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            error = scu_list_add(&currentMap->mappings, &mapping);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
        }
        else if (line[0] == '\n') {
            // Ignore empty lines.
            continue;
        }
        else if (scu_str_starts_with(line, "seeds: ")) {
            // There must be only one seeds line, so check if we already parsed
            // one earlier and fail if so.
            if (almanac->seeds != nullptr) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
            almanac->seeds = scu_list_new(SCU_SIZEOF(i64));
            if (almanac->seeds == nullptr) {
                error = SCU_ERROR_OUT_OF_MEMORY;
                goto fail;
            }
            const char* temp = line;
            temp += scu_strlen("seeds: ");
            i64 seed;
            isize read;
            while (scu_sscanf(temp, "%" I64_SCND "%" ISIZE_SCNN, &seed, &read) == 1) {
                error = scu_list_add(&almanac->seeds, &seed);
                if (error != SCU_ERROR_NONE) {
                    goto fail;
                }
                temp += read;
            }
        }
        else {
            bool matchedMap = false;
            for (isize i = 0; i < SCU_COUNTOF(MAP_HEADERS); i++) {
                if (scu_strcmp(line, MAP_HEADERS[i]) == 0) {
                    matchedMap = true;
                    // There must be only one of each map, so check if we
                    // already parsed one earlier and fail if so.
                    if (almanac->maps[i].mappings != nullptr) {
                        error = SCU_ERROR_INVALID_FORMAT;
                        goto fail;
                    }
                    almanac->maps[i].mappings = scu_list_new(
                        SCU_SIZEOF(Mapping)
                    );
                    if (almanac->maps[i].mappings == nullptr) {
                        error = SCU_ERROR_OUT_OF_MEMORY;
                        goto fail;
                    }
                    currentMap = &almanac->maps[i];
                    break;
                }
            }
            if (!matchedMap) {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
        }
    }
fail:
    scu_free(line);
    if (error == SCU_ERROR_END_OF_FILE) {
        error = SCU_ERROR_NONE;
        // Check that we actually parsed the seeds and all maps.
        if (almanac->seeds == nullptr) {
            error = SCU_ERROR_INVALID_FORMAT;
        }
        else {
            const Map* map;
            SCU_ARRAY_FOREACH(map, almanac->maps) {
                if (map->mappings == nullptr) {
                    error = SCU_ERROR_INVALID_FORMAT;
                    break;
                }
            }
        }
    }
    else {
        almanac_free(almanac);
        *almanac = (Almanac) { };
    }
    return error;
}

/**
 * @brief Returns the lowest location of any single seed after translating it
 * using all maps in the containing almanac.
 *
 * @warning This function assumes that the almanac contains at least one seed.
 *
 * @param[in] almanac The almanac containing the seeds and maps.
 * @return The lowest location of any single seed in the specified almanac.
 */
static i64 almanac_lowest_location_single(const Almanac* almanac) {
    SCU_ASSERT(almanac != nullptr);
    SCU_ASSERT(scu_list_count(almanac->seeds) > 0);
    i64 lowestLocation = I64_MAX;
    const i64* seed;
    SCU_LIST_FOREACH(seed, almanac->seeds) {
        i64 dest = *seed;
        const Map* map;
        SCU_ARRAY_FOREACH(map, almanac->maps) {
            const Mapping* mapping;
            SCU_LIST_FOREACH(mapping, map->mappings) {
                if (
                    (dest >= mapping->src)
                        && (dest < (mapping->src + mapping->length))
                ) {
                    dest = mapping->dest + dest - mapping->src;
                    break;
                }
            }
        }
        lowestLocation = SCU_MIN(lowestLocation, dest);
    }
    return lowestLocation;
}

/**
 * @brief Returns the lowest location of any seed range after translating it
 * using all maps in the containing almanac.
 *
 * @warning This function assumes that the almanac contains at least one seed
 * range (i.e., an even number of seeds greater than zero).
 *
 * @param[in] almanac The almanac containing the seed ranges and maps.
 * @return The lowest location of any seed range in the specified almanac.
 */
static i64 almanac_lowest_location_ranges(const Almanac* almanac) {
    SCU_ASSERT(almanac != nullptr);
    isize count = scu_list_count(almanac->seeds);
    SCU_ASSERT((count > 0) && ((count % 2) == 0));
    i64 lowestLocation = I64_MAX;
    ScuStack* oldRanges = scu_stack_new(SCU_SIZEOF(Range));
    ScuStack* newRanges = scu_stack_new(SCU_SIZEOF(Range));
    for (isize i = 0; i < count; i += 2) {
        scu_stack_clear(oldRanges);
        Range oldRange = {
            .src = almanac->seeds[i],
            .length = almanac->seeds[i + 1]
        };
        scu_stack_push(oldRanges, &oldRange);
        const Map* map;
        SCU_ARRAY_FOREACH(map, almanac->maps) {
            scu_stack_clear(newRanges);
            while (scu_stack_try_pop(oldRanges, &oldRange)) {
                i64 oldStart = oldRange.src;
                i64 oldEnd = oldRange.src + oldRange.length - 1;
                bool matchedRange = false;
                const Mapping* mapping;
                SCU_LIST_FOREACH(mapping, map->mappings) {
                    i64 overlapStart = SCU_MAX(oldStart, mapping->src);
                    i64 overlapEnd = SCU_MIN(
                        oldEnd,
                        mapping->src + mapping->length - 1
                    );
                    if (overlapStart < overlapEnd) {
                        matchedRange = true;
                        if (oldStart < overlapStart) {
                            Range before = {
                                .src = oldStart,
                                .length = overlapStart - oldStart
                            };
                            scu_stack_push(oldRanges, &before);
                        }
                        Range translated = {
                            .src = overlapStart + mapping->dest - mapping->src,
                            .length = overlapEnd - overlapStart
                        };
                        scu_stack_push(newRanges, &translated);
                        if (oldEnd > overlapEnd) {
                            Range after = {
                                .src = overlapEnd,
                                .length = oldEnd - overlapEnd
                            };
                            scu_stack_push(oldRanges, &after);
                        }
                        break;
                    }
                }
                if (!matchedRange) {
                    scu_stack_push(newRanges, &oldRange);
                }
            }
            ScuStack* temp = oldRanges;
            oldRanges = newRanges;
            newRanges = temp;
        }
        Range* range;
        SCU_STACK_FOREACH(range, oldRanges) {
            lowestLocation = SCU_MIN(lowestLocation, range->src);
        }
    }
    scu_stack_free(newRanges);
    scu_stack_free(oldRanges);
    return lowestLocation;
}

int main() {
    ScuError error;
    Almanac almanac;
    if ((error = almanac_parse(&almanac)) != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    i64 lowestLocationSingle = almanac_lowest_location_single(&almanac);
    i64 lowestLocationRanges = almanac_lowest_location_ranges(&almanac);
    scu_printf(
        "The lowest location of any single seed is %" I64_PRID ".\n",
        lowestLocationSingle
    );
    scu_printf(
        "The lowest location of the seed ranges is %" I64_PRID ".\n",
        lowestLocationRanges
    );
    almanac_free(&almanac);
    return EXIT_SUCCESS;
}