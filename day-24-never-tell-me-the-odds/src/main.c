#define SCU_SHORT_ALIASES

#include <math.h>
#include <scu/alloc.h>
#include <scu/array.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/math.h>
#include <scu/memory.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a three-dimensional vector. */
typedef struct Vec3 {

    /** @brief The x-component of the vector. */
    f64 x;

    /** @brief The y-component of the vector. */
    f64 y;

    /** @brief The z-component of the vector. */
    f64 z;

} Vec3;

/** @brief Represents a hailstone. */
typedef struct Hailstone {

    /** @brief The position of the hailstone. */
    Vec3 position;

    /** @brief The velocity of the hailstone. */
    Vec3 velocity;

} Hailstone;

/** @brief The minimum coordinate value for the test area. */
static constexpr f64 TEST_AREA_MIN = 200'000'000'000'000.0;

/** @brief The maximum coordinate value for the test area. */
static constexpr f64 TEST_AREA_MAX = 400'000'000'000'000.0;

/**
 * @brief Parses a hailstone from a specified line of text.
 *
 * The line must have the format `x, y, z @ vx, vy, vz`, where `x`, `y`, and `z`
 * represent the hailstone's position, and `vx`, `vy`, and `vz` represent the
 * hailstone's velocity. The components of the position and velocity must be
 * valid 64-bit floating point numbers in order to be parsed successfully.
 *
 * An example for a valid line might be the following:
 *
 * ```plaintext
 * 19, 13, 30 @ -2, 1, -2
 * ```
 *
 * @param[in]  line      The line of text to parse.
 * @param[out] hailstone The parsed hailstone on success, otherwise unspecified.
 * @return `true` if a hailstone was successfully parsed, otherwise `false`.
 */
static inline bool hailstone_parse(
    const char* restrict line,
    Hailstone* restrict hailstone
) {
    SCU_ASSERT(line != nullptr);
    SCU_ASSERT(hailstone != nullptr);
    return scu_sscanf(
        line,
        "%" F64_SCNF ", %" F64_SCNF ", %" F64_SCNF " @ %" F64_SCNF ", %"
            F64_SCNF ", %" F64_SCNF,
        &hailstone->position.x,
        &hailstone->position.y,
        &hailstone->position.z,
        &hailstone->velocity.x,
        &hailstone->velocity.y,
        &hailstone->velocity.z
    ) == 6;
}

/**
 * @brief Deallocates a specified list of hailstones.
 *
 * @note If `hailstones` is a `nullptr`, this function does nothing.
 *
 * @warning The behavior is undefined if `hailstones` is used after it has been
 * deallocated.
 *
 * @param[in, out] hailstones The list of hailstones to deallocate.
 */
static inline void hailstones_free(Hailstone* hailstones) {
    if (hailstones != nullptr) {
        scu_list_free(hailstones);
    }
}

/**
 * @brief Parses a list of hailstones from the standard input stream.
 *
 * The input must consist of zero or more lines of text, each representing a
 * hailstone as described in the documentation of `hailstone_parse()`. The
 * individual lines must be separated with a newline character.
 *
 * @param[out] hailstones The parsed list of hailstones on success, otherwise a
 *                        `nullptr`.
 * @return `SCU_ERROR_NONE` on success, otherwise an appropriate error code.
 */
static ScuError hailstones_parse(Hailstone** hailstones) {
    SCU_ASSERT(hailstones != nullptr);
    *hailstones = scu_list_new(SCU_SIZEOF(Hailstone));
    if (*hailstones == nullptr) {
        return SCU_ERROR_OUT_OF_MEMORY;
    }
    char* line = nullptr;
    isize size = 0;
    ScuError error;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        Hailstone hailstone;
        if (hailstone_parse(line, &hailstone)) {
            error = scu_list_add(hailstones, &hailstone);
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
        }
        else {
            error = SCU_ERROR_INVALID_FORMAT;
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
    hailstones_free(*hailstones);
    *hailstones = nullptr;
    return error;
}

/**
 * @brief Counts the number of intersections between the paths of a specified
 * list of hailstones.
 *
 * @note The paths of two hailstones are considered to intersect if there exist
 * two points in time at which the two hailstones occupy the same position. Only
 * intersections that happen within the test area are counted, where the test
 * area is defined as the set of all points with an X and Y coordinate of at
 * least `TEST_AREA_MIN` and at most `TEST_AREA_MAX`.
 *
 * @param[in] hailstones The list of hailstones for the computation.
 * @return The number of intersections between the paths of the specified list
 * of hailstones.
 */
static isize hailstones_intersections(const Hailstone* hailstones) {
    SCU_ASSERT(hailstones != nullptr);
    isize count = 0;
    for (isize i = 0; i < scu_list_count(hailstones); i++) {
        for (isize j = i + 1; j < scu_list_count(hailstones); j++) {
            Hailstone a = hailstones[i];
            Hailstone b = hailstones[j];
            f64 d = b.velocity.x * a.velocity.y - a.velocity.x * b.velocity.y;
            if (d == 0.0) {
                continue;
            }
            f64 dx = b.position.x - a.position.x;
            f64 dy = b.position.y - a.position.y;
            f64 tA = (dx * -b.velocity.y + b.velocity.x * dy) / d;
            f64 tB = (a.velocity.x * dy - a.velocity.y * dx) / d;
            if ((tA < 0.0) || (tB < 0.0)) {
                continue;
            }
            f64 x = a.position.x + a.velocity.x * tA;
            f64 y = a.position.y + a.velocity.y * tA;
            if (
                (x >= TEST_AREA_MIN) && (x <= TEST_AREA_MAX)
                    && (y >= TEST_AREA_MIN) && (y <= TEST_AREA_MAX)
            ) {
                count++;
            }
        }
    }
    return count;
}

/**
 * @brief Calculates the sum of the coordinates of the initial position of a
 * perfectly thrown rock, such that it collides with all hailstones in a
 * specified list.
 *
 * @param[in] hailstones The list of hailstones for the computation.
 * @return The sum of the coordinates of the initial position of the perfectly
 * thrown rock.
 */
static f64 hailstones_sum_of_perfect_throw(const Hailstone* hailstones) {
    SCU_ASSERT(hailstones != nullptr);
    f64 m[6][7] = { };
    for (isize k = 0; k < 2; k++) {
        f64 px0 = hailstones[0].position.x;
        f64 py0 = hailstones[0].position.y;
        f64 pz0 = hailstones[0].position.z;
        f64 vx0 = hailstones[0].velocity.x;
        f64 vy0 = hailstones[0].velocity.y;
        f64 vz0 = hailstones[0].velocity.z;
        f64 px1 = hailstones[k + 1].position.x;
        f64 py1 = hailstones[k + 1].position.y;
        f64 pz1 = hailstones[k + 1].position.z;
        f64 vx1 = hailstones[k + 1].velocity.x;
        f64 vy1 = hailstones[k + 1].velocity.y;
        f64 vz1 = hailstones[k + 1].velocity.z;
        m[k][0] = vy0 - vy1;
        m[k][1] = vx1 - vx0;
        m[k][2] = 0.0;
        m[k][3] = py1 - py0;
        m[k][4] = px0 - px1;
        m[k][5] = 0.0;
        m[k][6] = px0 * vy0 - py0 * vx0 - px1 * vy1 + py1 * vx1;
        m[k + 2][0] = vz0 - vz1;
        m[k + 2][1] = 0.0;
        m[k + 2][2] = vx1 - vx0;
        m[k + 2][3] = pz1 - pz0;
        m[k + 2][4] = 0.0;
        m[k + 2][5] = px0 - px1;
        m[k + 2][6] = px0 * vz0 - pz0 * vx0 - px1 * vz1 + pz1 * vx1;
        m[k + 4][0] = 0.0;
        m[k + 4][1] = vz0 - vz1;
        m[k + 4][2] = vy1 - vy0;
        m[k + 4][3] = 0.0;
        m[k + 4][4] = pz1 - pz0;
        m[k + 4][5] = py0 - py1;
        m[k + 4][6] = py0 * vz0 - pz0 * vy0 - py1 * vz1 + pz1 * vy1;
    }
    for (isize col = 0; col < SCU_COUNTOF(m); col++) {
        isize pivot = col;
        for (isize row = col + 1; row < SCU_COUNTOF(m); row++) {
            if (SCU_ABS(m[row][col]) > SCU_ABS(m[pivot][col])) {
                pivot = row;
            }
        }
        if (pivot != col) {
            for (isize c = 0; c < SCU_COUNTOF(m[0]); c++) {
                f64 tmp = m[col][c];
                m[col][c] = m[pivot][c];
                m[pivot][c] = tmp;
            }
        }
        SCU_ASSERT(m[col][col] != 0.0);
        for (isize row = 0; row < SCU_COUNTOF(m); row++) {
            if (row != col) {
                f64 factor = m[row][col] / m[col][col];
                for (isize c = col; c < SCU_COUNTOF(m[0]); c++) {
                    m[row][c] -= factor * m[col][c];
                }
            }
        }
    }
    f64 x = round(m[0][6] / m[0][0]);
    f64 y = round(m[1][6] / m[1][1]);
    f64 z = round(m[2][6] / m[2][2]);
    return x + y + z;
}

int main() {
    Hailstone* hailstones;
    ScuError error = hailstones_parse(&hailstones);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    isize intersections = hailstones_intersections(hailstones);
    f64 sum = hailstones_sum_of_perfect_throw(hailstones);
    scu_printf(
        "The paths intersect %" ISIZE_PRID " times within the test area.\n",
        intersections
    );
    scu_printf(
        "The sum of the coordinates of the initial position of the perfectly "
            "thrown rock is %.0" F64_PRIF ".\n",
        sum
    );
    hailstones_free(hailstones);
    return EXIT_SUCCESS;
}