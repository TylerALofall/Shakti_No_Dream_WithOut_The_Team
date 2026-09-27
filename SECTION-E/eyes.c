#include "eyes.h"

#include <stdio.h>
#include <string.h>

/*
 * Clean font: the 5x7 letterforms live in an 8x8 cell (1 column left,
 * 1 row bottom as spacing). Identity is the bitmap; color comes later.
 */

typedef struct {
    unsigned char character;
    const char *rows[7];
} eyes_font_entry_t;

static const eyes_font_entry_t EYES_FONT[] = {
    { 'A', { "01110", "10001", "10001", "11111", "10001", "10001", "10001" } },
    { 'B', { "11110", "10001", "10001", "11110", "10001", "10001", "11110" } },
    { 'C', { "01111", "10000", "10000", "10000", "10000", "10000", "01111" } },
    { 'D', { "11110", "10001", "10001", "10001", "10001", "10001", "11110" } },
    { 'E', { "11111", "10000", "10000", "11110", "10000", "10000", "11111" } },
    { 'F', { "11111", "10000", "10000", "11110", "10000", "10000", "10000" } },
    { 'G', { "01111", "10000", "10000", "10111", "10001", "10001", "01111" } },
    { 'H', { "10001", "10001", "10001", "11111", "10001", "10001", "10001" } },
    { 'I', { "11111", "00100", "00100", "00100", "00100", "00100", "11111" } },
    { 'J', { "00111", "00010", "00010", "00010", "10010", "10010", "01100" } },
    { 'K', { "10001", "10010", "10100", "11000", "10100", "10010", "10001" } },
    { 'L', { "10000", "10000", "10000", "10000", "10000", "10000", "11111" } },
    { 'M', { "10001", "11011", "10101", "10101", "10001", "10001", "10001" } },
    { 'N', { "10001", "11001", "10101", "10011", "10001", "10001", "10001" } },
    { 'O', { "01110", "10001", "10001", "10001", "10001", "10001", "01110" } },
    { 'P', { "11110", "10001", "10001", "11110", "10000", "10000", "10000" } },
    { 'Q', { "01110", "10001", "10001", "10001", "10101", "10010", "01101" } },
    { 'R', { "11110", "10001", "10001", "11110", "10100", "10010", "10001" } },
    { 'S', { "01111", "10000", "10000", "01110", "00001", "00001", "11110" } },
    { 'T', { "11111", "00100", "00100", "00100", "00100", "00100", "00100" } },
    { 'U', { "10001", "10001", "10001", "10001", "10001", "10001", "01110" } },
    { 'V', { "10001", "10001", "10001", "10001", "10001", "01010", "00100" } },
    { 'W', { "10001", "10001", "10001", "10101", "10101", "10101", "01010" } },
    { 'X', { "10001", "10001", "01010", "00100", "01010", "10001", "10001" } },
    { 'Y', { "10001", "10001", "01010", "00100", "00100", "00100", "00100" } },
    { 'Z', { "11111", "00001", "00010", "00100", "01000", "10000", "11111" } },
    { 'a', { "00000", "00000", "01110", "00001", "01111", "10001", "01111" } },
    { 'b', { "10000", "10000", "10110", "11001", "10001", "10001", "11110" } },
    { 'c', { "00000", "00000", "01111", "10000", "10000", "10000", "01111" } },
    { 'd', { "00001", "00001", "01101", "10011", "10001", "10001", "01111" } },
    { 'e', { "00000", "00000", "01110", "10001", "11111", "10000", "01111" } },
    { 'f', { "00110", "01001", "01000", "11100", "01000", "01000", "01000" } },
    { 'g', { "00000", "01111", "10001", "10001", "01111", "00001", "11110" } },
    { 'h', { "10000", "10000", "10110", "11001", "10001", "10001", "10001" } },
    { 'i', { "00100", "00000", "01100", "00100", "00100", "00100", "01110" } },
    { 'j', { "00010", "00000", "00110", "00010", "00010", "10010", "01100" } },
    { 'k', { "10000", "10000", "10010", "10100", "11000", "10100", "10010" } },
    { 'l', { "01100", "00100", "00100", "00100", "00100", "00100", "01110" } },
    { 'm', { "00000", "00000", "11010", "10101", "10101", "10101", "10101" } },
    { 'n', { "00000", "00000", "10110", "11001", "10001", "10001", "10001" } },
    { 'o', { "00000", "00000", "01110", "10001", "10001", "10001", "01110" } },
    { 'p', { "00000", "11110", "10001", "10001", "11110", "10000", "10000" } },
    { 'q', { "00000", "01111", "10001", "10001", "01111", "00001", "00001" } },
    { 'r', { "00000", "00000", "10110", "11001", "10000", "10000", "10000" } },
    { 's', { "00000", "00000", "01111", "10000", "01110", "00001", "11110" } },
    { 't', { "01000", "01000", "11100", "01000", "01000", "01001", "00110" } },
    { 'u', { "00000", "00000", "10001", "10001", "10001", "10011", "01101" } },
    { 'v', { "00000", "00000", "10001", "10001", "10001", "01010", "00100" } },
    { 'w', { "00000", "00000", "10001", "10001", "10101", "10101", "01010" } },
    { 'x', { "00000", "00000", "10001", "01010", "00100", "01010", "10001" } },
    { 'y', { "00000", "10001", "10001", "10001", "01111", "00001", "11110" } },
    { 'z', { "00000", "00000", "11111", "00010", "00100", "01000", "11111" } },
    { '0', { "01110", "10001", "10011", "10101", "11001", "10001", "01110" } },
    { '1', { "00100", "01100", "00100", "00100", "00100", "00100", "01110" } },
    { '2', { "01110", "10001", "00001", "00010", "00100", "01000", "11111" } },
    { '3', { "11110", "00001", "00001", "01110", "00001", "00001", "11110" } },
    { '4', { "00010", "00110", "01010", "10010", "11111", "00010", "00010" } },
    { '5', { "11111", "10000", "10000", "11110", "00001", "00001", "11110" } },
    { '6', { "01110", "10000", "10000", "11110", "10001", "10001", "01110" } },
    { '7', { "11111", "00001", "00010", "00100", "01000", "01000", "01000" } },
    { '8', { "01110", "10001", "10001", "01110", "10001", "10001", "01110" } },
    { '9', { "01110", "10001", "10001", "01111", "00001", "00001", "01110" } },
    { ' ', { "00000", "00000", "00000", "00000", "00000", "00000", "00000" } },
    { '.', { "00000", "00000", "00000", "00000", "00000", "00110", "00110" } },
    { ',', { "00000", "00000", "00000", "00000", "00110", "00110", "00100" } },
    { '+', { "00000", "00100", "00100", "11111", "00100", "00100", "00000" } },
    { '-', { "00000", "00000", "00000", "11111", "00000", "00000", "00000" } },
    { '=', { "00000", "00000", "11111", "00000", "11111", "00000", "00000" } },
    { '(', { "00010", "00100", "01000", "01000", "01000", "00100", "00010" } },
    { ')', { "01000", "00100", "00010", "00010", "00010", "00100", "01000" } },
    { '?', { "01110", "10001", "00001", "00010", "00100", "00000", "00100" } },
    { '!', { "00100", "00100", "00100", "00100", "00100", "00000", "00100" } },
    { '/', { "00001", "00010", "00010", "00100", "01000", "01000", "10000" } },
    { ':', { "00000", "00110", "00110", "00000", "00110", "00110", "00000" } },
};

#define EYES_FONT_COUNT (sizeof(EYES_FONT) / sizeof(EYES_FONT[0]))

static const eyes_font_entry_t *find_glyph(unsigned char character)
{
    size_t index;

    for (index = 0U; index < EYES_FONT_COUNT; ++index) {
        if (EYES_FONT[index].character == character) {
            return &EYES_FONT[index];
        }
    }

    return NULL;
}

static int dimensions_valid(unsigned int width, unsigned int height)
{
    return width > 0U &&
           height > 0U &&
           (unsigned long)width * (unsigned long)height <= EYES_MAX_PIXELS;
}

static unsigned int luma_of(
    const unsigned char *rgba,
    unsigned long pixel_index
)
{
    unsigned long base;

    base = pixel_index * 4UL;

    return (77U * (unsigned int)rgba[base] +
            150U * (unsigned int)rgba[base + 1UL] +
            29U * (unsigned int)rgba[base + 2UL] + 128U) / 256U;
}

unsigned int eyes_cells_x(unsigned int width)
{
    return width / 8U;
}

unsigned int eyes_cells_y(unsigned int height)
{
    return height / 8U;
}

int eyes_render_glyph(unsigned char character, char bitmap[8][9])
{
    const eyes_font_entry_t *entry;
    size_t row;
    size_t column;

    if (bitmap == NULL) {
        return 0;
    }

    entry = find_glyph(character);

    if (entry == NULL) {
        return 0;
    }

    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            bitmap[row][column] = '.';
        }

        bitmap[row][8] = '\0';
    }

    for (row = 0U; row < 7U; ++row) {
        for (column = 0U; column < 5U; ++column) {
            if (entry->rows[row][column] == '1') {
                bitmap[row][column + 1U] = '#';
            }
        }
    }

    return 1;
}

int eyes_text_supported(const char *text)
{
    const unsigned char *cursor;

    if (text == NULL) {
        return 0;
    }

    cursor = (const unsigned char *)text;

    while (*cursor != '\0') {
        if (find_glyph(*cursor) == NULL) {
            return 0;
        }

        cursor++;
    }

    return 1;
}

static void fill_white(
    unsigned char *rgba,
    unsigned long pixel_count
)
{
    unsigned long index;

    for (index = 0UL; index < pixel_count; ++index) {
        rgba[index * 4UL] = 255U;
        rgba[index * 4UL + 1UL] = 255U;
        rgba[index * 4UL + 2UL] = 255U;
        rgba[index * 4UL + 3UL] = 255U;
    }
}

static void stamp_glyph(
    unsigned char *rgba,
    unsigned int width,
    unsigned int height,
    unsigned int origin_x,
    unsigned int origin_y,
    unsigned char character
)
{
    char bitmap[8][9];
    unsigned int row;
    unsigned int column;

    if (!eyes_render_glyph(character, bitmap)) {
        return;
    }

    for (row = 0U; row < 8U; ++row) {
        for (column = 0U; column < 8U; ++column) {
            unsigned int x;
            unsigned int y;
            unsigned long index;

            x = origin_x + column;
            y = origin_y + row;

            if (x >= width || y >= height) {
                continue;
            }

            if (bitmap[row][column] != '#') {
                continue;
            }

            index = ((unsigned long)y * (unsigned long)width +
                     (unsigned long)x) * 4UL;
            rgba[index] = 0U;
            rgba[index + 1UL] = 0U;
            rgba[index + 2UL] = 0U;
            rgba[index + 3UL] = 255U;
        }
    }
}

static void stamp_line(
    unsigned char *rgba,
    unsigned int width,
    unsigned int height,
    const char *line,
    unsigned int origin_y
)
{
    size_t length;
    size_t index;
    unsigned int origin_x;

    length = strlen(line);

    if (length == 0U) {
        return;
    }

    if ((unsigned long)length * 8UL >= (unsigned long)width) {
        origin_x = 0U;
    } else {
        origin_x = (width - (unsigned int)length * 8U) / 2U;
    }

    for (index = 0U; index < length; ++index) {
        stamp_glyph(
            rgba,
            width,
            height,
            origin_x + (unsigned int)index * 8U,
            origin_y,
            (unsigned char)line[index]
        );
    }
}

static void generate_color_pattern(
    unsigned char *rgba,
    unsigned int width,
    unsigned int height,
    unsigned long seed
)
{
    unsigned int x;
    unsigned int y;

    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            unsigned long index;
            unsigned int band;

            index = ((unsigned long)y * (unsigned long)width +
                     (unsigned long)x) * 4UL;
            band = (unsigned int)(
                ((unsigned long)x + seed) %
                (((unsigned long)width + 2UL) / 3UL)
            );

            rgba[index] = band == 0U ? 255U : 0U;
            rgba[index + 1UL] = band == 1U ? 255U : 0U;
            rgba[index + 2UL] = band == 2U ? 255U : 0U;
            rgba[index + 3UL] = 255U;
        }
    }
}

int eyes_load_document(
    unsigned int kind,
    unsigned int width,
    unsigned int height,
    const eyes_text_page_t *text,
    unsigned long seed,
    unsigned char *rgba,
    unsigned long rgba_capacity
)
{
    unsigned long pixel_count;

    if (rgba == NULL || !dimensions_valid(width, height)) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;

    if (rgba_capacity < pixel_count * 4UL) {
        return 0;
    }

    fill_white(rgba, pixel_count);

    if (kind == 0U) {
        if (text == NULL ||
            !eyes_text_supported(text->line1) ||
            !eyes_text_supported(text->line2) ||
            !eyes_text_supported(text->line3)) {
            return 0;
        }

        stamp_line(rgba, width, height, text->line1, 0U);
        stamp_line(rgba, width, height, text->line2, 8U);
        stamp_line(rgba, width, height, text->line3, 16U);
    } else if (kind == 1U) {
        generate_color_pattern(rgba, width, height, seed);
    } else {
        return 0;
    }

    return 1;
}

/*
 * A pull bit is 1 only when the pixel is a full-strength primary on this
 * channel (this channel saturated, the other two zero). A real black is
 * three channels mixed, so no channel reads saturated and the color pull
 * does not see black text -- the old 3-color-printer fact.
 */
static unsigned int channel_saturated(
    const unsigned char *rgba,
    unsigned long pixel_index,
    unsigned int channel
)
{
    unsigned long base;
    unsigned int r;
    unsigned int g;
    unsigned int b;

    base = pixel_index * 4UL;
    r = (unsigned int)rgba[base];
    g = (unsigned int)rgba[base + 1UL];
    b = (unsigned int)rgba[base + 2UL];

    if (channel == 0U) {
        return r == 255U && g == 0U && b == 0U;
    }

    if (channel == 1U) {
        return r == 0U && g == 255U && b == 0U;
    }

    return r == 0U && g == 0U && b == 255U;
}

int eyes_pull_color(
    const unsigned char *rgba,
    unsigned int width,
    unsigned int height,
    char *bits,
    unsigned long bits_capacity
)
{
    unsigned long pixel_count;
    unsigned long index;

    if (rgba == NULL || bits == NULL ||
        !dimensions_valid(width, height)) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;

    if (bits_capacity < pixel_count * 3UL + 1UL) {
        return 0;
    }

    for (index = 0UL; index < pixel_count; ++index) {
        unsigned int channel;

        for (channel = 0U; channel < 3U; ++channel) {
            bits[index * 3UL + (unsigned long)channel] =
                channel_saturated(rgba, index, channel) ? '1' : '0';
        }
    }

    bits[pixel_count * 3UL] = '\0';

    return 1;
}

int eyes_pull_mono(
    const unsigned char *rgba,
    unsigned int width,
    unsigned int height,
    char *bits,
    unsigned long bits_capacity
)
{
    unsigned long pixel_count;
    unsigned long index;

    if (rgba == NULL || bits == NULL ||
        !dimensions_valid(width, height)) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;

    if (bits_capacity < pixel_count + 1UL) {
        return 0;
    }

    for (index = 0UL; index < pixel_count; ++index) {
        bits[index] = luma_of(rgba, index) >= 128U ? '0' : '1';
    }

    bits[pixel_count] = '\0';

    return 1;
}

int eyes_reconstruct_color(
    const char *bits,
    unsigned int width,
    unsigned int height,
    unsigned char *rgba,
    unsigned long rgba_capacity
)
{
    unsigned long pixel_count;
    unsigned long index;

    if (bits == NULL || rgba == NULL ||
        !dimensions_valid(width, height)) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;

    if (rgba_capacity < pixel_count * 4UL) {
        return 0;
    }

    if (strlen(bits) != pixel_count * 3UL) {
        return 0;
    }

    for (index = 0UL; index < pixel_count; ++index) {
        char r;
        char g;
        char b;

        r = bits[index * 3UL];
        g = bits[index * 3UL + 1UL];
        b = bits[index * 3UL + 2UL];

        if ((r != '0' && r != '1') ||
            (g != '0' && g != '1') ||
            (b != '0' && b != '1')) {
            return 0;
        }

        rgba[index * 4UL] = r == '1' ? 255U : 0U;
        rgba[index * 4UL + 1UL] = g == '1' ? 255U : 0U;
        rgba[index * 4UL + 2UL] = b == '1' ? 255U : 0U;
        rgba[index * 4UL + 3UL] = 255U;
    }

    return 1;
}

int eyes_reconstruct_mono(
    const char *bits,
    unsigned int width,
    unsigned int height,
    unsigned char *rgba,
    unsigned long rgba_capacity
)
{
    unsigned long pixel_count;
    unsigned long index;

    if (bits == NULL || rgba == NULL ||
        !dimensions_valid(width, height)) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;

    if (rgba_capacity < pixel_count * 4UL) {
        return 0;
    }

    if (strlen(bits) != pixel_count) {
        return 0;
    }

    for (index = 0UL; index < pixel_count; ++index) {
        unsigned char value;

        if (bits[index] != '0' && bits[index] != '1') {
            return 0;
        }

        value = bits[index] == '1' ? 0U : 255U;
        rgba[index * 4UL] = value;
        rgba[index * 4UL + 1UL] = value;
        rgba[index * 4UL + 2UL] = value;
        rgba[index * 4UL + 3UL] = 255U;
    }

    return 1;
}

unsigned long eyes_diff(
    const unsigned char *original,
    const unsigned char *reconstructed,
    unsigned int width,
    unsigned int height,
    int color
)
{
    unsigned long pixel_count;
    unsigned long index;
    unsigned long drift;

    if (original == NULL || reconstructed == NULL ||
        !dimensions_valid(width, height)) {
        return (unsigned long)-1;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;
    drift = 0UL;

    for (index = 0UL; index < pixel_count; ++index) {
        if (color) {
            /* Per-pixel: count every pixel that did not come back. */
            if (memcmp(original + index * 4UL,
                       reconstructed + index * 4UL, 4UL) != 0) {
                ++drift;
            }
        } else {
            unsigned int a;
            unsigned int b;

            a = luma_of(original, index) >= 128U ? 0U : 1U;
            b = luma_of(reconstructed, index) >= 128U ? 0U : 1U;

            if (a != b) {
                ++drift;
            }
        }
    }

    return drift;
}

unsigned long eyes_recognize_text(
    const unsigned char *mono_rgba,
    unsigned int width,
    unsigned int height,
    eyes_recog_t *out,
    unsigned long out_capacity
)
{
    unsigned int cells_x;
    unsigned int cells_y;
    unsigned int cell_row;
    unsigned int cell_col;
    unsigned long found;

    if (mono_rgba == NULL || out == NULL ||
        !dimensions_valid(width, height)) {
        return 0UL;
    }

    cells_x = eyes_cells_x(width);
    cells_y = eyes_cells_y(height);
    found = 0UL;

    for (cell_row = 0U; cell_row < cells_y; ++cell_row) {
        for (cell_col = 0U; cell_col < cells_x; ++cell_col) {
            size_t index;
            int matched;
            char candidate;

            matched = 0;
            candidate = '\0';

            for (index = 0U; index < EYES_FONT_COUNT; ++index) {
                char bitmap[8][9];
                unsigned int row;
                unsigned int column;
                int exact;

                if (!eyes_render_glyph(EYES_FONT[index].character, bitmap)) {
                    continue;
                }

                exact = 1;

                for (row = 0U; exact && row < 8U; ++row) {
                    for (column = 0U; column < 8U; ++column) {
                        unsigned long pixel;
                        unsigned int ink;
                        unsigned int glyph_ink;

                        pixel = ((unsigned long)(cell_row * 8U + row) *
                                 (unsigned long)width +
                                 (unsigned long)(cell_col * 8U + column));
                        ink = luma_of(mono_rgba, pixel) >= 128U ? 0U : 1U;
                        glyph_ink = bitmap[row][column] == '#' ? 1U : 0U;

                        if (ink != glyph_ink) {
                            exact = 0;
                            break;
                        }
                    }
                }

                if (exact) {
                    matched = 1;
                    candidate = (char)EYES_FONT[index].character;
                    break;
                }
            }

            if (matched && candidate != ' ') {
                if ((unsigned long)found < out_capacity) {
                    out[found].character = candidate;
                    out[found].cell_col = cell_col;
                    out[found].cell_row = cell_row;
                }

                ++found;
            }
        }
    }

    return found;
}

int eyes_render_clean_page(
    unsigned int width,
    unsigned int height,
    const eyes_recog_t *characters,
    unsigned long character_count,
    unsigned char *rgba,
    unsigned long rgba_capacity
)
{
    unsigned long pixel_count;
    unsigned long index;

    if (rgba == NULL || !dimensions_valid(width, height)) {
        return 0;
    }

    if (characters == NULL && character_count > 0UL) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;

    if (rgba_capacity < pixel_count * 4UL) {
        return 0;
    }

    fill_white(rgba, pixel_count);

    for (index = 0UL; index < character_count; ++index) {
        stamp_glyph(
            rgba,
            width,
            height,
            characters[index].cell_col * 8U,
            characters[index].cell_row * 8U,
            (unsigned char)characters[index].character
        );
    }

    return 1;
}

int eyes_write_frame(
    const char *path,
    unsigned int width,
    unsigned int height,
    int color,
    const char *bits
)
{
    FILE *file;
    unsigned long pixel_count;
    unsigned long bits_per_pixel;
    unsigned long row;
    unsigned long column;
    int success;

    if (path == NULL || bits == NULL ||
        !dimensions_valid(width, height)) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;
    bits_per_pixel = color ? 3UL : 1UL;

    if (strlen(bits) != pixel_count * bits_per_pixel) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success =
        fprintf(file, "EYES_FRAME_V1\n") > 0 &&
        fprintf(file, "WIDTH=%u\n", width) > 0 &&
        fprintf(file, "HEIGHT=%u\n", height) > 0 &&
        fprintf(file, "KIND=%s\n", color ? "color" : "mono") > 0 &&
        fprintf(file, "TEXT=%s\n", bits) > 0 &&
        fprintf(file, "GRID\n") > 0;

    for (row = 0UL; success && row < (unsigned long)height; ++row) {
        for (column = 0UL; column < (unsigned long)width; ++column) {
            unsigned long base;
            int written;

            base = (row * (unsigned long)width + column) * bits_per_pixel;

            if (color) {
                written = fprintf(
                    file,
                    "%c%c%c ",
                    bits[base],
                    bits[base + 1UL],
                    bits[base + 2UL]
                );
            } else {
                written = fprintf(file, "%c", bits[base]);
            }

            if (written < 0) {
                success = 0;
                break;
            }
        }

        if (success && fputc('\n', file) == EOF) {
            success = 0;
        }
    }

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)remove(path);
        return 0;
    }

    return 1;
}

int eyes_write_recon(
    const char *path,
    unsigned int width,
    unsigned int height,
    int color,
    const unsigned char *rgba
)
{
    FILE *file;
    unsigned long row;
    unsigned long column;
    int success;
    unsigned int plane;
    unsigned int plane_count;

    if (path == NULL || rgba == NULL ||
        !dimensions_valid(width, height)) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success =
        fprintf(file, "EYES_RECON_V1\n") > 0 &&
        fprintf(file, "WIDTH=%u\n", width) > 0 &&
        fprintf(file, "HEIGHT=%u\n", height) > 0 &&
        fprintf(file, "KIND=%s\n", color ? "color" : "mono") > 0;

    plane_count = color ? 4U : 1U;

    for (plane = 0U; success && plane < plane_count; ++plane) {
        const char *name;

        if (color) {
            name = plane == 0U ? "R" :
                   plane == 1U ? "G" :
                   plane == 2U ? "B" : "LUMA";
        } else {
            name = "INK";
        }

        if (fprintf(file, "PLANE=%s\n", name) < 0) {
            success = 0;
            break;
        }

        for (row = 0UL; success && row < (unsigned long)height; ++row) {
            for (column = 0UL; column < (unsigned long)width; ++column) {
                unsigned long index;
                int ink;

                index = (row * (unsigned long)width + column) * 4UL;

                if (plane == 3U) {
                    ink = rgba[index] < 128U ||
                          rgba[index + 1UL] < 128U ||
                          rgba[index + 2UL] < 128U;
                } else if (color) {
                    ink = rgba[index + (unsigned long)plane] >= 128U;
                } else {
                    ink = rgba[index] < 128U;
                }

                if (fputc(ink ? '#' : '.', file) == EOF) {
                    success = 0;
                    break;
                }
            }

            if (success && fputc('\n', file) == EOF) {
                success = 0;
            }
        }
    }

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)remove(path);
        return 0;
    }

    return 1;
}

unsigned long eyes_run_cycles(
    const unsigned char *original,
    unsigned int width,
    unsigned int height,
    int color,
    unsigned char *work_a,
    unsigned char *work_b,
    char *bits_a,
    char *bits_b,
    unsigned long buffer_capacity
)
{
    unsigned long pixel_count;
    unsigned long bits_per_pixel;
    unsigned long total_drift;
    unsigned int iteration;

    if (original == NULL || work_a == NULL || work_b == NULL ||
        bits_a == NULL || bits_b == NULL ||
        !dimensions_valid(width, height)) {
        return (unsigned long)-1;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;
    bits_per_pixel = color ? 3UL : 1UL;

    if (buffer_capacity < pixel_count * 4UL ||
        buffer_capacity < pixel_count * bits_per_pixel + 1UL) {
        return (unsigned long)-1;
    }

    total_drift = 0UL;

    if (color) {
        if (!eyes_pull_color(original, width, height, bits_a,
                             buffer_capacity)) {
            return (unsigned long)-1;
        }
    } else {
        if (!eyes_pull_mono(original, width, height, bits_a,
                            buffer_capacity)) {
            return (unsigned long)-1;
        }
    }

    for (iteration = 0U; iteration < 20U; ++iteration) {
        int ok;

        if (color) {
            ok = eyes_reconstruct_color(bits_a, width, height, work_a,
                                        buffer_capacity) &&
                 eyes_pull_color(work_a, width, height, bits_b,
                                 buffer_capacity);
        } else {
            ok = eyes_reconstruct_mono(bits_a, width, height, work_a,
                                       buffer_capacity) &&
                 eyes_pull_mono(work_a, width, height, bits_b,
                                buffer_capacity);
        }

        if (!ok) {
            return (unsigned long)-1;
        }

        if (strcmp(bits_a, bits_b) != 0) {
            unsigned long index;

            for (index = 0UL; bits_a[index] != '\0'; ++index) {
                if (bits_a[index] != bits_b[index]) {
                    ++total_drift;
                }
            }
        }

        memcpy(work_b, work_a, pixel_count * 4UL);
        strcpy(bits_a, bits_b);
    }

    return total_drift;
}
