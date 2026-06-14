#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/types.h>
#include <stdlib.h>
#include <tgmath.h>

/** @brief Represents a boat race. */
typedef struct Race {

    /** @brief The duration of the race. */
    i64 time;

    /** @brief The record distance of the race. */
    i64 distance;

} Race;

/**
 * @brief Parses races from the standard input stream.
 *
 * The input must consist of two lines in the following format:
 *
 * ```plaintext
 * Time: <Times>
 * Distance: <Distances>
 * ```
 *
 * &lt;Times&gt; is a sequence of positive integers (separated by one or more
 * spaces each) representing the durations of the races in milliseconds.
 * &lt;Distances&gt; is also a sequence of positive integers (separated by one
 * or more spaces each) representing the record distances of the races in
 * millimeters. There must be a record distance for each duration.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * Time:      7  15   30
 * Distance:  9  40  200
 * ```
 *
 * Note that the durations and distances need not be aligned as nicely as in the
 * example shown above. The only requirement is that they are separated by one
 * or more spaces.
 *
 * @param[out] races A list of the parsed races, or `nullptr` on failure.
 * @return `SCU_ERROR_NONE` on success, or an appropriate error code on failure.
 */
static ScuError parse_races(Race** races) {
    SCU_ASSERT(races != nullptr);
    *races = scu_list_new(SCU_SIZEOF(Race));
    if (*races == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    ScuError error = scu_readln(&line, &size);
    if (error != SCU_ERROR_NONE) {
        goto fail;
    }
    isize read;
    const char* temp = line;
    if (scu_sscanf(temp, "Time:%" ISIZE_SCNN, &read) != 0) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    temp += read;
    i64 time;
    while (scu_sscanf(temp, "%" I64_SCND "%" ISIZE_SCNN, &time, &read) == 1) {
        Race race = { .time = time };
        error = scu_list_add(races, &race);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        temp += read;
    }
    error = scu_readln(&line, &size);
    if (error != SCU_ERROR_NONE) {
        goto fail;
    }
    temp = line;
    if (scu_sscanf(temp, "Distance:%" ISIZE_SCNN, &read) != 0) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    temp += read;
    Race* race;
    SCU_LIST_FOREACH(race, *races) {
        if (scu_sscanf(temp, "%" I64_SCND "%" ISIZE_SCNN, &race->distance, &read) != 1) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        temp += read;
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    scu_list_free(*races);
    *races = nullptr;
    return error;
}

/**
 * @brief Returns the number of possibilities to win a specified race.
 * 
 * @param[in] race The race to examine.
 * @return The number of possibilities to win the specified race.
 */
static inline i64 race_win_possibilities(const Race* race) {
    SCU_ASSERT(race != nullptr);
    f64 time = (f64) race->time;
    f64 d = time * time - 4.0 * (f64) race->distance;
    i64 minTime = (i64) floor((time - sqrt(d)) / 2.0) + 1;
    i64 maxTime = (i64) ceil((time + sqrt(d)) / 2.0) - 1;
    return maxTime - minTime + 1;
}

/**
 * @brief Returns the product of the number of possibilities to win each race
 * in a specified list of races.
 * 
 * @warning This function assumes that the specified list of races is not empty.
 *
 * @param[in] races The list of races to examine.
 * @return The product of the number of possibilities to win each race in the
 * specified list of races.
 */
static i64 product_of_win_possibilities_individual(const Race* races) {
    SCU_ASSERT(races != nullptr);
    SCU_ASSERT(scu_list_count(races) > 0);
    i64 product = 1;
    const Race* race;
    SCU_LIST_FOREACH(race, races) {
        product *= race_win_possibilities(race);
    }
    return product;
}

/**
 * @brief Returns the number of possibilities to win a single large race that
 * combines all races in a specified list of races.
 *
 * @warning This function assumes that the specified list of races is not empty.
 *
 * @param[in] races The list of races to examine.
 * @return The number of possibilities to win the single large race that
 * combines all races in the specified list of races, or -1 on failure.
 */
static i64 win_possibilities_single_combined(const Race* races) {
    SCU_ASSERT(races != nullptr);
    SCU_ASSERT(scu_list_count(races) > 0);
    char* temp = nullptr;
    isize size = 0;
    const Race* race;
    SCU_LIST_FOREACH(race, races) {
        ScuError error = scu_rasnprintf(&temp, &size, "%" I64_PRID, race->time);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
    }
    i64 time;
    if (scu_sscanf(temp, "%" I64_SCND, &time) != 1) {
        goto fail;
    }
    temp[0] = '\0';
    SCU_LIST_FOREACH(race, races) {
        ScuError error = scu_rasnprintf(
            &temp,
            &size,
            "%" I64_PRID,
            race->distance
        );
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
    }
    i64 distance;
    if (scu_sscanf(temp, "%" I64_SCND, &distance) != 1) {
        goto fail;
    }
    scu_free(temp);
    Race singleRace = { .time = time, .distance = distance };
    return race_win_possibilities(&singleRace);
fail:
    scu_free(temp);
    return -1;
}

int main() {
    Race* races;
    ScuError error = parse_races(&races);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    i64 productIndividual = product_of_win_possibilities_individual(races);
    i64 singleCombined = win_possibilities_single_combined(races);
    scu_printf(
        "The product of the number of possibilities to win the individual "
            "races is %" I64_PRID".\n",
        productIndividual
    );
    scu_printf(
        "There are %" I64_PRID " possibilities to win the single combined "
            "race.\n",
        singleCombined
    );
    scu_list_free(races);
    return EXIT_SUCCESS;
}