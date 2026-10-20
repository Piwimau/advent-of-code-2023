#define SCU_SHORT_ALIASES

#include <scu/scu.h>
#include <stdlib.h>

/** @brief Represents a history of environmental values. */
typedef struct History {

    /** @brief The values in the history. */
    i32* values;

} History;

/** @brief Represents a report of histories. */
typedef struct Report {

    /** @brief The histories in the report. */
    History* histories;

} Report;

/** @brief Represents an extrapolation of a history or report. */
typedef struct Extrapolation {

    /** @brief The previous extrapolated value. */
    i32 prev;

    /** @brief The next extrapolated value. */
    i32 next;

} Extrapolation;

/**
 * @brief Parses a history from a specified line of text.
 *
 * The line of text must consist of one or more integer values separated by
 * spaces. It may optionally contain a terminating newline, but that is not
 * strictly required.
 *
 * An example for a valid line might be the following:
 *
 * ```plaintext
 * 0 3 6 9 12 15
 * ```
 *
 * @warning The behavior is undefined if `line` is not a pointer to a
 * null-terminated byte string.
 *
 * @param[in]  line    The line of text to parse a history from.
 * @param[out] history The parsed history on success, or a zero-initialized
 *                     history on failure.
 * @return `SCU_ERROR_NONE` on success, or an appropriate error code on failure.
 */
static ScuError history_parse(
    const char* restrict line,
    History* restrict history
) {
    SCU_ASSERT(line);
    SCU_ASSERT(history);
    history->values = scu_list_new(i32);
    if (!history->values) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    ScuError error = SCU_ERROR_NONE;
    i32 value;
    isize read;
    while (scu_sscanf(line, "%" I32_SCND "%" ISIZE_SCNN, &value, &read) == 1) {
        error = scu_list_add(&history->values, &value);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        line += read;
    }
    if (
        (scu_list_count(history->values) == 0)
            || ((*line != '\n') && (*line != '\0'))
    ) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    return SCU_ERROR_NONE;
fail:
    scu_list_delete(history->values);
    history->values = nullptr;
    return error;
}

/**
 * @brief Determines whether a specified history contains any non-zero values.
 *
 * @param[in] history The history to examine.
 * @return `true` if `history` contains at least one non-zero value, otherwise
 * `false`.
 */
static inline bool history_contains_non_zero(const History* history) {
    SCU_ASSERT(history);
    i32* value;
    SCU_LIST_FOREACH(value, history->values) {
        if (*value != 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Replaces all values in a specified history with the values from
 * another one.
 * 
 * @note The `dest` history is cleared before copying the values from `src`.
 *
 * @param[out] dest The history to replace all values of.
 * @param[in]  src  The history to copy the values from.
 */
static inline void history_replace_with(
    History* restrict dest,
    const History* restrict src
) {
    SCU_ASSERT(dest);
    SCU_ASSERT(src);
    scu_list_clear(dest->values);
    i32* value;
    SCU_LIST_FOREACH(value, src->values) {
        scu_list_add(&dest->values, value);
    }
}

/**
 * @brief Calculates the differences between every pair of consecutive values in
 * a specified history and stores them in another one.
 *
 * @note The `dest` history is cleared before adding the calculated differences
 * from `src`.
 * 
 * @warning The behavior is undefined if `src` contains fewer than two values.
 *
 * @param[out] dest The history to add the calculated differences to.
 * @param[in]  src  The history to calculate the differences of.
 */
static inline void history_calc_differences(
    History* restrict dest,
    const History* restrict src
) {
    SCU_ASSERT(dest);
    SCU_ASSERT(src);
    isize count = scu_list_count(src->values);
    SCU_ASSERT(count > 1);
    scu_list_clear(dest->values);
    for (isize i = 0; i < (count - 1); i++) {
        i32 difference = src->values[i + 1] - src->values[i];
        scu_list_add(&dest->values, &difference);
    }
}

/**
 * @brief Returns the first value in a specified history.
 *
 * @warning The behavior is undefined if `history` contains no values.
 *
 * @param[in] history The history to retrieve the first value from.
 * @return The first value in `history`.
 */
static inline i32 history_first_value(const History* history) {
    SCU_ASSERT(history);
    SCU_ASSERT(scu_list_count(history->values) > 0);
    return history->values[0];
}

/**
 * @brief Returns the last value in a specified history.
 *
 * @warning The behavior is undefined if `history` contains no values.
 *
 * @param[in] history The history to retrieve the last value from.
 * @return The last value in `history`.
 */
static inline i32 history_last_value(const History* history) {
    SCU_ASSERT(history);
    isize count = scu_list_count(history->values);
    SCU_ASSERT(count > 0);
    return history->values[count - 1];
}

/**
 * @brief Swaps the contents of two specified histories.
 *
 * @param[in] a The first history.
 * @param[in] b The second history.
 */
static inline void history_swap(History* restrict a, History* restrict b) {
    SCU_ASSERT(a);
    SCU_ASSERT(b);
    History temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * @brief Deallocates all resources associated with a specified history.
 *
 * @note If `history` is a `nullptr`, this function does nothing.
 *
 * Note that this function does not deallocate `history` itself.
 *
 * @warning The behavior is undefined if `history` is used after it has been
 * deallocated.
 *
 * @param[in, out] history The history to deallocate all resources of.
 */
static void history_delete(History* history) {
    if (history) {
        scu_list_delete(history->values);
        history->values = nullptr;
    }
}

/**
 * @brief Deallocates all resources associated with a specified report.
 *
 * @note If `report` is a `nullptr`, this function does nothing.
 *
 * Note that this function does not deallocate `report` itself.
 *
 * @warning The behavior is undefined if `report` is used after it has been
 * deallocated.
 *
 * @param[in, out] report The report to deallocate all resources of.
 */
static void report_delete(Report* report) {
    if (report) {
        History* history;
        SCU_LIST_FOREACH(history, report->histories) {
            history_delete(history);
        }
        scu_list_delete(report->histories);
        report->histories = nullptr;
    }
}

/**
 * @brief Parses a report from the standard input stream.
 *
 * The input must consist of zero or more lines, each representing a history of
 * environmental values. For details on the expected format of each line, see
 * `history_parse()`.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * 0 3 6 9 12 15
 * 1 3 6 10 15 21
 * 10 13 16 21 30 45
 * ```
 *
 * @param[out] report The parsed report on success, or a zero-initialized report
 *                    on failure.
 * @return `SCU_ERROR_NONE` on success, or an appropriate error code on failure.
 */
static ScuError report_parse(Report* report) {
    SCU_ASSERT(report);
    report->histories = scu_list_new(History);
    if (!report->histories) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    ScuError error;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        History history;
        error = history_parse(line, &history);
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        error = scu_list_add(&report->histories, &history);
        if (error != SCU_ERROR_NONE) {
            history_delete(&history);
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
    report_delete(report);
    return error;
}

/**
 * @brief Extrapolates a specified report by calculating the sum of the next and
 * previous extrapolated values for each of its histories.
 *
 * @param[in] report The report to extrapolate.
 * @return The sum of the next and previous extrapolated values for each of its
 * histories.
 */
static Extrapolation report_extrapolate(const Report* report) {
    SCU_ASSERT(report);
    History oldHistory = { .values = scu_list_new(i32) };
    if (!oldHistory.values) {
        goto oldHistoryAllocFail;
    }
    History newHistory = { .values = scu_list_new(i32) };
    if (!newHistory.values) {
        goto newHistoryAllocFail;
    }
    ScuStack* extrapolations = scu_stack_new(Extrapolation);
    if (!extrapolations) {
        goto extrapolationsAllocFail;
    }
    Extrapolation result = { };
    const History* history;
    SCU_LIST_FOREACH(history, report->histories) {
        history_replace_with(&oldHistory, history);
        while (history_contains_non_zero(&oldHistory)) {
            Extrapolation extrapolation = {
                .prev = history_first_value(&oldHistory),
                .next = history_last_value(&oldHistory)
            };
            scu_stack_push(extrapolations, &extrapolation);
            history_calc_differences(&newHistory, &oldHistory);
            history_swap(&oldHistory, &newHistory);
        }
        i32 prev = 0;
        i32 next = 0;
        Extrapolation extrapolation;
        while (scu_stack_try_pop(extrapolations, &extrapolation)) {
            prev = extrapolation.prev - prev;
            next += extrapolation.next;
        }
        result.prev += prev;
        result.next += next;
    }
    scu_stack_delete(extrapolations);
    history_delete(&newHistory);
    history_delete(&oldHistory);
    return result;
extrapolationsAllocFail:
    history_delete(&newHistory);
newHistoryAllocFail:
    history_delete(&oldHistory);
oldHistoryAllocFail:
    return (Extrapolation) { .prev = -1, .next = -1 };
}

int main() {
    Report report;
    ScuError error = report_parse(&report);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    Extrapolation extrapolation = report_extrapolate(&report);
    scu_printf(
        "The sum of the next values is %" I32_PRID ".\n",
        extrapolation.next
    );
    scu_printf(
        "The sum of the previous values is %" I32_PRID ".\n",
        extrapolation.prev
    );
    report_delete(&report);
    return EXIT_SUCCESS;
}