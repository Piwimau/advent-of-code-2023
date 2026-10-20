#define SCU_SHORT_ALIASES

#include <scu/scu.h>
#include <stdlib.h>

/** @brief Represents the condition of a spring. */
typedef enum Condition {
    CONDITION_UNKNOWN,
    CONDITION_OPERATIONAL,
    CONDITION_DAMAGED
} Condition;

/** @brief Represents a record of spring conditions. */
typedef struct Record {

    /** @brief An array of spring conditions. */
    Condition* conditions;

    /** @brief An array containing the sizes of groups of damaged springs. */
    int32_t* groupSizes;

} Record;

/**
 * @brief Represents a state in the search for possible arrangements of
 * operational and damaged springs for a record.
 */
typedef struct State {

    /** @brief The index of the current condition in the record. */
    isize conditionIndex;

    /** @brief The index of the current group size in the record. */
    isize groupSizeIndex;

    /** @brief The current size of the damaged group being built. */
    isize currentGroupSize;

} State;

/** @brief The number of copies to create when unfolding a record. */
static constexpr int32_t UNFOLD_COPIES = 5;

/**
 * @brief Parses a condition from a specified character.
 *
 * The character must be either `?` (for `CONDITION_UNKNOWN`), `.` (for
 * `CONDITION_OPERATIONAL`), or `#` (for `CONDITION_DAMAGED`).
 *
 * @param[in]  c         The character to parse.
 * @param[out] condition The parsed condition on success, otherwise unspecified.
 * @return `true` if a condition was successfully parsed, otherwise `false`.
 */
static inline bool condition_parse(char c, Condition* condition) {
    SCU_ASSERT(condition);
    switch (c) {
        case '?':
            *condition = CONDITION_UNKNOWN;
            return true;
        case '.':
            *condition = CONDITION_OPERATIONAL;
            return true;
        case '#':
            *condition = CONDITION_DAMAGED;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Deallocates a specified record.
 *
 * @note If `record` is a `nullptr`, this function does nothing.
 *
 * Note that this function only deallocates any resources associated with the
 * record, but not the record itself. If the record was dynamically allocated,
 * it is the caller's responsibility to deallocate it manually.
 *
 * @warning The behavior is undefined if `record` is used after it has been
 * deallocated.
 *
 * @param[in, out] record The record to deallocate.
 */
static inline void record_delete(Record* record) {
    if (record) {
        scu_list_delete(record->conditions);
        record->conditions = nullptr;
        scu_list_delete(record->groupSizes);
        record->groupSizes = nullptr;
    }
}

/**
 * @brief Parses a record from a specified line of text.
 *
 * The line of text must have the following format:
 *
 * ```plaintext
 * <conditions> <damaged group sizes>
 * ```
 *
 * Here, &lt;conditions&gt; is a sequence of zero or more characters
 * representing spring conditions, each of which must be either `?`, `.`, or
 * `#`. It must be followed by a single space character and &lt;damaged group
 * sizes&gt;, a comma-separated list of zero or more integers (greater than or
 * equal to one) representing the sizes of groups of damaged springs.
 *
 * An example for a valid line of text might be the following:
 *
 * ```plaintext
 * "???.### 1,1,3"
 * ```
 *
 * @param[in]  line   The line of text to parse.
 * @param[out] record The parsed record on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if the record was successfully parsed, otherwise an
 * appropriate error code.
 */
static inline ScuError record_parse(
    const char* restrict line,
    Record* restrict record
) {
    SCU_ASSERT(line);
    SCU_ASSERT(record);
    ScuError error = SCU_ERROR_NONE;
    record->conditions = scu_list_new(Condition);
    if (!record->conditions) {
        error = SCU_ERROR_OUT_OF_MEMORY;
        goto fail;
    }
    record->groupSizes = scu_list_new(int32_t);
    if (!record->groupSizes) {
        error = SCU_ERROR_OUT_OF_MEMORY;
        goto fail;
    }
    Condition condition;
    while (condition_parse(*line, &condition)) {
        error = scu_list_add(&record->conditions, &condition);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        line++;
    }
    if (*line != ' ') {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    line++;
    int32_t size;
    isize read = 0;
    while (scu_sscanf(line, "%" I32_SCND "%" ISIZE_SCNN, &size, &read) == 1) {
        if (size < 1) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        error = scu_list_add(&record->groupSizes, &size);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        line += read;
        if (*line == ',') {
            line++;
        }
    }
    if (*line != '\0') {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    return SCU_ERROR_NONE;
fail:
    record_delete(record);
    return error;
}

/**
 * @brief Recursively counts the number of possible arrangements of operational
 * and damaged springs for a specified record, given a specified state and a
 * cache of previously computed results.
 *
 * @param[in]      record The record for which to count the possible
 *                        arrangements.
 * @param[in]      state  The current state of the search.
 * @param[in, out] cache  A cache of previously computed results.
 * @return The number of possible arrangements for the current state, or `-1` if
 * an error occurred.
 */
static inline i64 record_count_possible_arrangements(
    const Record* record,
    State state,
    ScuHashMap* cache
) {
    SCU_ASSERT(record);
    SCU_ASSERT(cache);
    const Condition* conditions = record->conditions;
    const int32_t* groupSizes = record->groupSizes;
    isize conditionCount = scu_list_count(conditions);
    isize groupSizeCount = scu_list_count(groupSizes);
    if (state.conditionIndex == conditionCount) {
        if (
            (state.groupSizeIndex == groupSizeCount)
                && (state.currentGroupSize == 0)
        ) {
            return 1;
        }
        if (
            (state.groupSizeIndex == (groupSizeCount - 1))
                && (state.currentGroupSize == groupSizes[state.groupSizeIndex])
        ) {
            return 1;
        }
        return 0;
    }
    i64* cachedArrangements;
    if (scu_hash_map_try_get(cache, &state, &cachedArrangements)) {
        return *cachedArrangements;
    }
    Condition condition = conditions[state.conditionIndex];
    i64 arrangements = 0;
    if (
        (condition == CONDITION_OPERATIONAL) || (condition == CONDITION_UNKNOWN)
    ) {
        if (state.currentGroupSize == 0) {
            arrangements += record_count_possible_arrangements(
                record,
                (State) {
                    .conditionIndex = state.conditionIndex + 1,
                    .groupSizeIndex = state.groupSizeIndex,
                    .currentGroupSize = 0
                },
                cache
            );
        }
        else if (
            (state.groupSizeIndex < groupSizeCount)
                && (state.currentGroupSize == groupSizes[state.groupSizeIndex])
        ) {
            arrangements += record_count_possible_arrangements(
                record,
                (State) {
                    .conditionIndex = state.conditionIndex + 1,
                    .groupSizeIndex = state.groupSizeIndex + 1,
                    .currentGroupSize = 0
                },
                cache
            );
        }
    }
    if (
        (condition == CONDITION_DAMAGED) || (condition == CONDITION_UNKNOWN)
    ) {
        arrangements += record_count_possible_arrangements(
            record,
            (State) {
                .conditionIndex = state.conditionIndex + 1,
                .groupSizeIndex = state.groupSizeIndex,
                .currentGroupSize = state.currentGroupSize + 1
            },
            cache
        );
    }
    if (scu_hash_map_add(cache, &state, &arrangements) != SCU_ERROR_NONE) {
        return -1;
    }
    return arrangements;
}

/**
 * @brief Unfolds a specified record by appending multiple copies of its
 * conditions and damaged group sizes.
 *
 * @param[in, out] record The record to unfold.
 * @return `true` if the record was successfully unfolded, otherwise `false`.
 */
static bool record_unfold(Record* record) {
    SCU_ASSERT(record);
    isize originalConditions = scu_list_count(record->conditions);
    ScuError error = scu_list_ensure_capacity(
        &record->conditions,
        originalConditions * UNFOLD_COPIES
    );
    if (error != SCU_ERROR_NONE) {
        return false;
    }
    isize originalGroupSizes = scu_list_count(record->groupSizes);
    error = scu_list_ensure_capacity(
        &record->groupSizes,
        originalGroupSizes * UNFOLD_COPIES
    );
    if (error != SCU_ERROR_NONE) {
        return false;
    }
    for (i32 i = 0; i < (UNFOLD_COPIES - 1); i++) {
        error = scu_list_add(
            &record->conditions,
            &(Condition) { CONDITION_UNKNOWN }
        );
        if (error != SCU_ERROR_NONE) {
            return false;
        }
        for (isize j = 0; j < originalConditions; j++) {
            // Copy into a temporary, as the internal array may be reallocated.
            Condition condition = record->conditions[j];
            error = scu_list_add(&record->conditions, &condition);
            if (error != SCU_ERROR_NONE) {
                return false;
            }
        }
        for (isize j = 0; j < originalGroupSizes; j++) {
            // Copy into a temporary, as the internal array may be reallocated.
            int32_t groupSize = record->groupSizes[j];
            error = scu_list_add(&record->groupSizes, &groupSize);
            if (error != SCU_ERROR_NONE) {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Deallocates a specified list of records.
 *
 * @note If `records` is a `nullptr`, this function does nothing.
 *
 * @warning The behavior is undefined if `records` is used after it has been
 * deallocated.
 *
 * @param[in, out] records The list of records to deallocate.
 */
static inline void records_delete(Record* records) {
    if (records) {
        Record* record;
        SCU_LIST_FOREACH(record, records) {
            record_delete(record);
        }
        scu_list_delete(records);
    }
}

/**
 * @brief Parses the records from the standard input stream.
 *
 * The input must consist of zero or more lines of text, each of which must have
 * a specific format. See `record_parse()` for more details.
 *
 * @param[out] records The parsed records on success, otherwise a `nullptr`.
 * @return `SCU_ERROR_NONE` if the records were successfully parsed, otherwise
 * an appropriate error code.
 */
static ScuError records_parse(Record** records) {
    SCU_ASSERT(records);
    *records = scu_list_new(Record);
    if (!*records) {
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
        Record record;
        error = record_parse(line, &record);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        error = scu_list_add(records, &record);
        if (error != SCU_ERROR_NONE) {
            record_delete(&record);
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
    records_delete(*records);
    *records = nullptr;
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
    SCU_ASSERT(value);
    const State* state = value;
    usize hash = 0;
    hash = scu_hash_combine(hash, scu_hash_isize(&state->conditionIndex));
    hash = scu_hash_combine(hash, scu_hash_isize(&state->groupSizeIndex));
    hash = scu_hash_combine(hash, scu_hash_isize(&state->currentGroupSize));
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
    SCU_ASSERT(a);
    SCU_ASSERT(b);
    const State* l = a;
    const State* r = b;
    return (l->conditionIndex == r->conditionIndex)
        && (l->groupSizeIndex == r->groupSizeIndex)
        && (l->currentGroupSize == r->currentGroupSize);
}

/**
 * @brief Returns the sum of the number of possible arrangements of operational
 * and damaged springs of each record in a specified list of records.
 *
 * @param[in] records The list of records for the calculation.
 * @return The sum of the number of possible arrangements of all records, or
 * `-1` if an error occurred.
 */
static i64 records_sum_of_possible_arrangements(const Record* records) {
    SCU_ASSERT(records);
    ScuHashMap* cache = scu_hash_map_new(
        State,
        i64,
        state_hash,
        state_equal,
        scu_equal_i64
    );
    if (!cache) {
        return -1;
    }
    i64 totalArrangements = 0;
    const Record* record;
    SCU_LIST_FOREACH(record, records) {
        i64 arrangements = record_count_possible_arrangements(
            record,
            (State) { },
            cache
        );
        if (arrangements == -1) {
            totalArrangements = -1;
            break;
        }
        totalArrangements += arrangements;
        scu_hash_map_clear(cache);
    }
    scu_hash_map_delete(cache);
    return totalArrangements;
}

/**
 * @brief Unfolds a specified list of records.
 *
 * @param[in, out] records The list of records to unfold.
 * @return `true` if the records were successfully unfolded, otherwise `false`.
 */
static bool records_unfold(Record* records) {
    SCU_ASSERT(records);
    Record* record;
    SCU_LIST_FOREACH(record, records) {
        if (!record_unfold(record)) {
            return false;
        }
    }
    return true;
}

int main() {
    Record* records;
    ScuError error = records_parse(&records);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    i64 sumFolded = records_sum_of_possible_arrangements(records);
    scu_printf(
        "The sum of the possible arrangements of the folded records is %"
            I64_PRID ".\n",
        sumFolded
    );
    if (!records_unfold(records)) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while unfolding the records.\n"
        );
        records_delete(records);
        return EXIT_FAILURE;
    }
    i64 sumUnfolded = records_sum_of_possible_arrangements(records);
    scu_printf(
        "The sum of the possible arrangements of the unfolded records is %"
            I64_PRID ".\n",
        sumUnfolded
    );
    records_delete(records);
    return EXIT_SUCCESS;
}