#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/array.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/memory.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a digit. */
typedef struct Digit {

    /** @brief The token of the digit, e.g., "one" for the digit one. */
    const char* token;

    /** @brief The length of the token, e.g., 3 for the digit one. */
    isize length;

    /** @brief The value of the digit, e.g., 1 for the digit one. */
    i32 value;

} Digit;

/** @brief The digits to find in the second part of the puzzle. */
static const Digit DIGITS[] = {
    { .token = "one", .length = SCU_SIZEOF("one") - 1, .value = 1 },
    { .token = "two", .length = SCU_SIZEOF("two") - 1, .value = 2 },
    { .token = "three", .length = SCU_SIZEOF("three") - 1, .value = 3 },
    { .token = "four", .length = SCU_SIZEOF("four") - 1, .value = 4 },
    { .token = "five", .length = SCU_SIZEOF("five") - 1, .value = 5 },
    { .token = "six", .length = SCU_SIZEOF("six") - 1, .value = 6 },
    { .token = "seven", .length = SCU_SIZEOF("seven") - 1, .value = 7 },
    { .token = "eight", .length = SCU_SIZEOF("eight") - 1, .value = 8 },
    { .token = "nine", .length = SCU_SIZEOF("nine") - 1, .value = 9 }
};

/**
 * @brief Finds the numeric and alphanumeric calibration value in a
 * null-terminated byte string.
 *
 * @note The numeric calibration value is formed by combining the first and last
 * numeric digit (1 to 9) found in `s`. The alphanumeric calibration value is
 * formed by combining the first and last alphanumeric digit (1 to 9 and "one"
 * to "nine") found in `s`.
 *
 * @warning The behavior is undefined if `s` is not a pointer to a
 * null-terminated byte string.
 *
 * @param[in]  s                     The null-terminated byte string to search
 *                                   for calibration values.
 * @param[out] calibrationValue      The numeric calibration value, or -1 on
 *                                   failure.
 * @param[out] alnumCalibrationValue The alphanumeric calibration value, or -1
 *                                   on failure.
 * @return `true` if the two calibration values were found, otherwise `false`.
 */
static inline bool find_calibration_values(
    const char* restrict s,
    i32* restrict calibrationValue,
    i32* restrict alnumCalibrationValue
) {
    SCU_ASSERT(s != nullptr);
    SCU_ASSERT(calibrationValue != nullptr);
    SCU_ASSERT(alnumCalibrationValue != nullptr);
    i32 firstDigit = -1;
    i32 lastDigit = -1;
    i32 firstAlnumDigit = -1;
    i32 lastAlnumDigit = -1;
    for (isize i = 0; s[i] != '\0'; i++) {
        if ((s[i] >= '1') && (s[i] <= '9')) {
            i32 digit = s[i] - '0';
            if (firstDigit == -1) {
                firstDigit = digit;
            }
            if (firstAlnumDigit == -1) {
                firstAlnumDigit = digit;
            }
            lastDigit = digit;
            lastAlnumDigit = digit;
        }
        else {
            const Digit* digit;
            SCU_ARRAY_FOREACH(digit, DIGITS) {
                if (scu_strncmp(s + i, digit->token, digit->length) == 0) {
                    if (firstAlnumDigit == -1) {
                        firstAlnumDigit = digit->value;
                    }
                    lastAlnumDigit = digit->value;
                    break;
                }
            }
        }
    }
    if (firstDigit == -1) {
        *calibrationValue = -1;
        *alnumCalibrationValue = -1;
        return false;
    }
    *calibrationValue = firstDigit * 10 + lastDigit;
    *alnumCalibrationValue = firstAlnumDigit * 10 + lastAlnumDigit;
    return true;
}

int main() {
    ScuError error = SCU_ERROR_NONE;
    char* line = nullptr;
    isize size = 0;
    i32 sumOfCalibrationValues = 0;
    i32 sumOfAlnumCalibrationValues = 0;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        i32 calibrationValue = -1;
        i32 alnumCalibrationValue = -1;
        bool foundCalibrationValues = find_calibration_values(
            line,
            &calibrationValue,
            &alnumCalibrationValue
        );
        if (!foundCalibrationValues) {
            // Replace the newline (if present) to avoid an ugly line break.
            isize newlineIndex = scu_str_index_of(line, '\n');
            if (newlineIndex != -1) {
                line[newlineIndex] = '\0';
            }
            scu_fprintf(
                SCU_STDERR,
                "The line '%s' does not contain both calibration values.\n",
                line
            );
            scu_free(line);
            return EXIT_FAILURE;
        }
        sumOfCalibrationValues += calibrationValue;
        sumOfAlnumCalibrationValues += alnumCalibrationValue;
    }
    scu_free(line);
    if (error != SCU_ERROR_END_OF_FILE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    scu_printf(
        "The sum of the numeric calibration values is %" I32_PRID ".\n",
        sumOfCalibrationValues
    );
    scu_printf(
        "The sum of the alphanumeric calibration values is %" I32_PRID ".\n",
        sumOfAlnumCalibrationValues
    );
    return EXIT_SUCCESS;
}