#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/array.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/memory.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents an operation for modifying the lens configuration. */
typedef enum Operation {
    OPERATION_ADD_OR_REPLACE,
    OPERATION_REMOVE
} Operation;

/** @brief Represents a single step in the initialization sequence. */
typedef struct Step {

    /**
     * @brief The label of the lens to add, replace or remove.
     *
     * @note This is a dynamically allocated, null-terminated byte string.
     */
    char* label;

    /** @brief The operation to perform on the lens configuration. */
    Operation operation;

    /**
     * @brief The focal length of the lens to add, or `-1` if `operation` is
     * `OPERATION_REMOVE`.
     */
    isize focalLength;

} Step;

/**
 * @brief Deallocates a specified step.
 *
 * @note If `step` is a `nullptr`, this function does nothing.
 *
 * This function only deallocates any resources associated with the step, but
 * not the step itself. The caller is responsible for deallocating the step
 * manually if it was dynamically allocated.
 *
 * @warning The behavior is undefined if `step` is used after it has been
 * deallocated.
 *
 * @param[in, out] step The step to deallocate.
 */
static inline void step_free(Step* step) {
    if (step != nullptr) {
        scu_free(step->label);
        step->label = nullptr;
    }
}

/**
 * @brief Parses a step from a specified line of text.
 *
 * The line of text is expected to begin with one of the following two formats:
 *
 * ```plaintext
 * <label>=<focal length>,...
 * <label>-,...
 * ```
 *
 * In the first case, the step represents an `OPERATION_ADD_OR_REPLACE`
 * operation, and the focal length of the lens to add or replace is given by
 * `<focal length>`, which must be a positive integer between `1` and `9`
 * (inclusive). In the second case, the step represents an `OPERATION_REMOVE`
 * operation, and no focal length must be specified. `<label>` is the label of
 * the lens, which must be a non-empty ASCII string that does not contain any of
 * the characters `=`, `-` or `,`.
 *
 * If a comma is present in the line of text, it must immediately follow the
 * focal length in the first format, or the `-` character in the second format.
 * In this case, `*line` is updated to point to the character immediately
 * following the comma on success, or left unchanged on failure. If no comma is
 * present, `*line` is updated to point to the terminating null byte at the end
 * of the line on success, or left unchanged on failure.
 *
 * @param[in, out] line A pointer to the line of text to parse.
 * @param[out]     step The parsed step on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` if a step was successfully parsed, otherwise an
 * appropriate error code.
 */
static inline ScuError step_parse(
    const char* restrict* restrict line,
    Step* restrict step
) {
    SCU_ASSERT((line != nullptr) && (*line != nullptr));
    SCU_ASSERT(step != nullptr);
    isize endIndex = scu_str_index_of(*line, ',');
    if (endIndex == -1) {
        endIndex = scu_strlen(*line);
    }
    isize operationIndex = scu_str_index_of_any(*line, "=-");
    if (
        (operationIndex == -1)
            || ((operationIndex != (endIndex - 2))
                && (operationIndex != (endIndex - 1)))
    ) {
        return SCU_ERROR_INVALID_FORMAT;
    }
    step->label = scu_strndup(*line, operationIndex);
    if (step->label == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    if ((*line)[operationIndex] == '=') {
        step->operation = OPERATION_ADD_OR_REPLACE;
        char c = (*line)[operationIndex + 1];
        if ((c < '1') || (c > '9')) {
            step_free(step);
            return SCU_ERROR_INVALID_FORMAT;
        }
        step->focalLength = c - '0';
    }
    else {
        step->operation = OPERATION_REMOVE;
        step->focalLength = -1;
    }
    *line += endIndex + (((*line)[endIndex] == ',') ? 1 : 0);
    return SCU_ERROR_NONE;
}

/**
 * @brief Deallocates a specified list of steps.
 *
 * @note If `steps` is a `nullptr`, this function does nothing.
 *
 * @warning The behavior is undefined if `steps` is used after it has been
 * deallocated.
 *
 * @param[in, out] steps The list of steps to deallocate.
 */
static inline void steps_free(Step* steps) {
    if (steps != nullptr) {
        Step* step;
        SCU_LIST_FOREACH(step, steps) {
            step_free(step);
        }
        scu_list_free(steps);
    }
}

/**
 * @brief Parses a list of steps from the standard input stream.
 *
 * The input must consist of zero or more comma-separated steps, where each step
 * is given in the format described in `step_parse()`.
 *
 * @param[out] steps The list of steps on success, otherwise a `nullptr`.
 * @return `SCU_ERROR_NONE` if a list of steps was successfully parsed,
 * otherwise an appropriate error code.
 */
static ScuError steps_parse(Step** steps) {
    SCU_ASSERT(steps != nullptr);
    *steps = scu_list_new(SCU_SIZEOF(Step));
    if (*steps == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    ScuError error = scu_readall(&line, &size);
    if (error != SCU_ERROR_NONE) {
        goto fail;
    }
    const char* temp = line;
    Step step;
    while ((error = step_parse(&temp, &step)) == SCU_ERROR_NONE) {
        error = scu_list_add(steps, &step);
        if (error != SCU_ERROR_NONE) {
            // If we fail to add the step to the list, we need to deallocate it
            // manually here since it won't be caught by the cleanup code at the
            // end of this function.
            step_free(&step);
            goto fail;
        }
    }
    if (*temp != '\0') {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
    steps_free(*steps);
    *steps = nullptr;
    return error;
}

/**
 * @brief Returns the hash of a specified null-terminated byte string according
 * to the `HASH` algorithm.
 *
 * @param[in] s The null-terminated byte string to hash.
 * @return The hash of the specified null-terminated byte string.
 */
static inline isize hash(const char* s) {
    SCU_ASSERT(s != nullptr);
    isize hash = 0;
    for (const char* c = s; *c != '\0'; c++) {
        hash = (hash + *c) * 17 % 256;
    }
    return hash;
}

/**
 * @brief Returns the sum of the hashes of a specified list of steps.
 *
 * @param[in] steps The list of steps for the calculation.
 * @return The sum of the hashes of the specified list of steps, or `-1` if an
 * error occurred.
 */
static isize steps_sum_of_hashes(const Step* steps) {
    SCU_ASSERT(steps != nullptr);
    char* buffer = nullptr;
    isize size = 0;
    isize sumOfHashes = 0;
    const Step* step;
    SCU_LIST_FOREACH(step, steps) {
        ScuError error = scu_rsnprintf(
            &buffer,
            &size,
            "%s%c%c",
            step->label,
            (step->operation == OPERATION_ADD_OR_REPLACE) ? '=' : '-',
            (step->operation == OPERATION_ADD_OR_REPLACE)
                ? ('0' + step->focalLength)
                : '\0'
        );
        if (error != SCU_ERROR_NONE) {
            sumOfHashes = -1;
            goto end;
        }
        sumOfHashes += hash(buffer);
    }
end:
    scu_free(buffer);
    return sumOfHashes;
}

/**
 * @brief Returns the focusing power of a specified list of steps.
 *
 * @param[in] steps The list of steps for the calculation.
 * @return The focusing power of the specified list of steps, or `-1` if an
 * error occurred.
 */
static isize steps_focusing_power(const Step* steps) {
    SCU_ASSERT(steps != nullptr);
    Step* boxes[256] = { };
    isize focusingPower = 0;
    const Step* step;
    SCU_LIST_FOREACH(step, steps) {
        isize index = hash(step->label);
        if (step->operation == OPERATION_ADD_OR_REPLACE) {
            if (boxes[index] == nullptr) {
                boxes[index] = scu_list_new(SCU_SIZEOF(Step));
                if (boxes[index] == nullptr) {
                    focusingPower = -1;
                    goto end;
                }
            }
            bool found = false;
            for (isize i = 0; i < scu_list_count(boxes[index]); i++) {
                if (scu_strcmp(boxes[index][i].label, step->label) == 0) {
                    boxes[index][i].focalLength = step->focalLength;
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (scu_list_add(&boxes[index], step) != SCU_ERROR_NONE) {
                    focusingPower = -1;
                    goto end;
                }
            }
        }
        else if (boxes[index] != nullptr) {
            for (isize i = 0; i < scu_list_count(boxes[index]); i++) {
                if (scu_strcmp(boxes[index][i].label, step->label) == 0) {
                    scu_list_remove_at(boxes[index], i);
                    break;
                }
            }
        }
    }
    for (isize i = 0; i < SCU_COUNTOF(boxes); i++) {
        if (boxes[i] != nullptr) {
            for (isize j = 0; j < scu_list_count(boxes[i]); j++) {
                focusingPower += (i + 1) * (j + 1) * boxes[i][j].focalLength;
            }
        }
    }
end:
    Step** box;
    SCU_ARRAY_FOREACH(box, boxes) {
        scu_list_free(*box);
    }
    return focusingPower;
}

int main() {
    Step* steps;
    ScuError error = steps_parse(&steps);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    isize sumOfHashes = steps_sum_of_hashes(steps);
    isize focusingPower = steps_focusing_power(steps);
    scu_printf("The sum of the hashes is %" ISIZE_PRID ".\n", sumOfHashes);
    scu_printf("The focusing power is %" ISIZE_PRID ".\n", focusingPower);
    steps_free(steps);
    return EXIT_SUCCESS;
}