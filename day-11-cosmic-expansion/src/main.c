#define SCU_SHORT_ALIASES

#include <scu/alloc.h>
#include <scu/assert.h>
#include <scu/io.h>
#include <scu/list.h>
#include <scu/math.h>
#include <scu/memory.h>
#include <scu/string.h>
#include <scu/types.h>
#include <stdlib.h>

/** @brief Represents a pixel in an image of the universe. */
typedef enum Pixel {
    PIXEL_EMPTY,
    PIXEL_GALAXY
} Pixel;

/** @brief Represents an image of the universe. */
typedef struct Image {

    /**
     * @brief The pixels of the image.
     *
     * @note This is actually a two-dimensional array of `width * height`
     * pixels, stored as a one-dimensional array in row-major order.
     */
    Pixel* pixels;

    /** @brief The width of the image. */
    isize width;

    /** @brief The height of the image. */
    isize height;

    /**
     * @brief An array of `height` values indicating whether each row is empty.
     */
    bool* isEmptyRow;

    /**
     * @brief An array of `width` values indicating whether each column is
     * empty.
     */
    bool* isEmptyColumn;

} Image;

/** @brief Represents a two-dimensional position. */
typedef struct Position {

    /** @brief The x-coordinate of the position. */
    isize x;

    /** @brief The y-coordinate of the position. */
    isize y;

} Position;

/**
 * @brief Parses a pixel from a specified character.
 *
 * The specified character must be a '.' (for `PIXEL_EMPTY`) or a '#' (for
 * `PIXEL_GALAXY`).
 *
 * @param[in]  c     The character to parse a pixel from.
 * @param[out] pixel The parsed pixel on success, otherwise unmodified.
 * @return `true` if a pixel was successfully parsed, otherwise `false`.
 */
static inline bool pixel_parse(char c, Pixel* pixel) {
    SCU_ASSERT(pixel != nullptr);
    switch (c) {
        case '.':
            *pixel = PIXEL_EMPTY;
            return true;
        case '#':
            *pixel = PIXEL_GALAXY;
            return true;
        default:
            return false;
    }
}

/**
 * @brief Returns the distance between two specified positions.
 *
 * @note Here, the distance is defined as the sum of the absolute differences of
 * the coordinates of the positions (i.e., the Manhattan distance).
 *
 * @param[in] a The first position.
 * @param[in] b The second position.
 * @return The distance between `a` and `b`.
 */
static inline isize position_distance(Position a, Position b) {
    return SCU_ABS(a.x - b.x) + SCU_ABS(a.y - b.y);
}

/**
 * @brief Deallocates all resources associated with a specified image.
 *
 * @note If `image` is a `nullptr`, this function does nothing.
 *
 * Note that this function does not deallocate `image` itself.
 *
 * @warning The behavior is undefined if `image` is used after it has been
 * deallocated.
 *
 * @param[in, out] image The image to deallocate all resources of.
 */
static void image_free(Image* image) {
    if (image != nullptr) {
        scu_list_free(image->pixels);
        image->pixels = nullptr;
        scu_list_free(image->isEmptyRow);
        image->isEmptyRow = nullptr;
        scu_list_free(image->isEmptyColumn);
        image->isEmptyColumn = nullptr;
    }
}

/**
 * @brief Parses an image from the standard input stream.
 *
 * The input is expected to consist of lines of equal length, each containing
 * a sequence of one or more characters representing pixels, followed by an
 * optional terminating newline. For more information about the recognized
 * characters, see `pixel_parse()`.
 *
 * An example for a valid input might be the following:
 *
 * ```plaintext
 * ...#......
 * .......#..
 * #.........
 * ..........
 * ......#...
 * .#........
 * .........#
 * ..........
 * .......#..
 * #...#.....
 * ```
 *
 * @param[out] image The parsed image on success, otherwise unspecified.
 * @return `SCU_ERROR_NONE` on success, or an appropriate error code on failure.
 */
static ScuError image_parse(Image* image) {
    SCU_ASSERT(image != nullptr);
    ScuError error = SCU_ERROR_NONE;
    *image = (Image) { };
    image->pixels = scu_list_new(SCU_SIZEOF(Pixel));
    if (image->pixels == nullptr) {
        error = SCU_ERROR_OUT_OF_MEMORY;
        goto imageAllocFailed;
    }
    image->isEmptyRow = scu_list_new(SCU_SIZEOF(bool));
    if (image->isEmptyRow == nullptr) {
        error = SCU_ERROR_OUT_OF_MEMORY;
        goto imageAllocFailed;
    }
    image->isEmptyColumn = scu_list_new(SCU_SIZEOF(bool));
    if (image->isEmptyColumn == nullptr) {
        error = SCU_ERROR_OUT_OF_MEMORY;
        goto imageAllocFailed;
    }
    bool foundWidth = false;
    char* line = nullptr;
    isize size = 0;
    while ((error = scu_readln(&line, &size)) == SCU_ERROR_NONE) {
        // We initially assume that each row is empty. If we later encounter a
        // galaxy in the row, we will set the corresponding value to `false`.
        error = scu_list_add(&image->isEmptyRow, &(bool) { true });
        if (error != SCU_ERROR_NONE) {
            goto fail;
        }
        isize newlineIndex = scu_str_index_of(line, '\n');
        if (newlineIndex != -1) {
            line[newlineIndex] = '\0';
        }
        isize width = scu_strlen(line);
        if (width == 0) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        if (!foundWidth) {
            image->width = width;
            foundWidth = true;
            error = scu_list_ensure_capacity(
                &image->isEmptyColumn,
                image->width
            );
            if (error != SCU_ERROR_NONE) {
                goto fail;
            }
            for (isize i = 0; i < image->width; i++) {
                // Just like with the rows, we initially assume that each column
                // is empty. If we later encounter a galaxy in the column, we
                // will set the corresponding value to `false`.
                error = scu_list_add(&image->isEmptyColumn, &(bool) { true });
                if (error != SCU_ERROR_NONE) {
                    goto fail;
                }
            }
        }
        else if (image->width != width) {
            error = SCU_ERROR_INVALID_FORMAT;
            goto fail;
        }
        for (isize i = 0; line[i] != '\0'; i++) {
            Pixel pixel;
            if (pixel_parse(line[i], &pixel)) {
                error = scu_list_add(&image->pixels, &pixel);
                if (error != SCU_ERROR_NONE) {
                    goto fail;
                }
                if (pixel == PIXEL_GALAXY) {
                    image->isEmptyRow[image->height] = false;
                    image->isEmptyColumn[i] = false;
                }
            }
            else {
                error = SCU_ERROR_INVALID_FORMAT;
                goto fail;
            }
        }
        image->height++;
    }
    if (error != SCU_ERROR_END_OF_FILE) {
        goto fail;
    }
    if (!foundWidth) {
        error = SCU_ERROR_INVALID_FORMAT;
        goto fail;
    }
    scu_free(line);
    return SCU_ERROR_NONE;
fail:
    scu_free(line);
imageAllocFailed:
    image_free(image);
    return error;
}

/**
 * @brief Returns the sum of the shortest path lengths between every pair of
 * galaxies in a specified image.
 *
 * @note The image is conceptually expanded by the specified expansion factor
 * before examining it. During this expansion, each row or column of empty
 * pixels (i.e., any row or column not containing any galaxies) is duplicated
 * `expansionFactor` times.
 *
 * @param[in] image           The image to examine.
 * @param[in] expansionFactor The factor by which to expand the image.
 * @return The sum of the shortest path lengths between every pair of galaxies
 * in the expanded image, or `-1` if an error occurred.
 */
static isize image_sum_of_shortest_path_lengths(
    const Image* image,
    i32 expansionFactor
) {
    SCU_ASSERT(image != nullptr);
    SCU_ASSERT(expansionFactor > 0);
    Position* galaxies = scu_list_new(SCU_SIZEOF(Position));
    if (galaxies == nullptr) {
        return -1;
    }
    for (isize y = 0; y < image->height; y++) {
        for (isize x = 0; x < image->width; x++) {
            if (image->pixels[y * image->width + x] == PIXEL_GALAXY) {
                // Instead of actually creating the expanded image, we can
                // simply calculate the actual positions of the galaxies by
                // counting how many empty rows and columns are before them, and
                // then adding the corresponding offsets to their coordinates.
                isize emptyRowsBefore = 0;
                for (isize i = 0; i < y; i++) {
                    if (image->isEmptyRow[i]) {
                        emptyRowsBefore++;
                    }
                }
                isize emptyColumnsBefore = 0;
                for (isize i = 0; i < x; i++) {
                    if (image->isEmptyColumn[i]) {
                        emptyColumnsBefore++;
                    }
                }
                Position position = {
                    .x = x + emptyColumnsBefore * (expansionFactor - 1),
                    .y = y + emptyRowsBefore * (expansionFactor - 1)
                };
                if (scu_list_add(&galaxies, &position) != SCU_ERROR_NONE) {
                    scu_list_free(galaxies);
                    return -1;
                }
            }
        }
    }
    isize sumOfDistances = 0;
    for (isize i = 0; i < scu_list_count(galaxies); i++) {
        for (isize j = i + 1; j < scu_list_count(galaxies); j++) {
            sumOfDistances += position_distance(galaxies[i], galaxies[j]);
        }
    }
    scu_list_free(galaxies);
    return sumOfDistances;
}

int main() {        
    Image image;
    ScuError error = image_parse(&image);
    if (error != SCU_ERROR_NONE) {
        scu_fprintf(
            SCU_STDERR,
            "An error occurred while reading the input file (code %d).\n",
            error
        );
        return EXIT_FAILURE;
    }
    i64 sumTwo = image_sum_of_shortest_path_lengths(&image, 2);
    i64 sumMillion = image_sum_of_shortest_path_lengths(&image, 1'000'000);
    scu_printf(
        "Using an expansion factor of 2, the sum of all shortest path lengths "
            "is %" I64_PRID ".\n",
        sumTwo
    );
    scu_printf(
        "Using an expansion factor of 1,000,000, the sum of all shortest path "
            "lengths is %" I64_PRID ".\n",
        sumMillion
    );
    image_free(&image);
    return EXIT_SUCCESS;
}