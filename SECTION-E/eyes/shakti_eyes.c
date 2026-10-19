#include "shakti_senses.h"
#include <limits.h>
#include <string.h>

static int white(unsigned char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v';
}

static int number(const unsigned char *file, size_t bytes, size_t *at, unsigned int *out)
{
    unsigned int value = 0U;
    size_t start;
    for (;;) {
        while (*at < bytes && white(file[*at])) ++*at;
        if (*at >= bytes || file[*at] != '#') break;
        while (*at < bytes && file[*at] != '\n') ++*at;
    }
    start = *at;
    while (*at < bytes && file[*at] >= '0' && file[*at] <= '9') {
        unsigned int digit = file[*at] - '0';
        if (value > (UINT_MAX - digit) / 10U) return 0;
        value = value * 10U + digit; ++*at;
    }
    if (*at == start || *at >= bytes || !white(file[*at])) return 0;
    *out = value;
    return 1;
}

int shakti_eyes_pnm(const unsigned char *file, size_t bytes,
    unsigned char *rgb, size_t capacity, shakti_rgb_image_t *image)
{
    unsigned int width, height, maximum, channels;
    size_t at = 2U, count, i;
    shakti_rgb_image_t candidate;
    uintptr_t a, b;
    if (!file || !rgb || !image || bytes < 4U || file[0] != 'P' ||
        (file[1] != '5' && file[1] != '6') || !white(file[2])) return 0;
    channels = file[1] == '6' ? 3U : 1U;
    if (!number(file, bytes, &at, &width) || !number(file, bytes, &at, &height) ||
        !number(file, bytes, &at, &maximum) || !width || !height || maximum != 255U ||
        (uint64_t)width * height > SIZE_MAX / 3U) return 0;
    count = (size_t)width * height;
    if (file[at++] == '\r' && at < bytes && file[at] == '\n' &&
        bytes - at != count * channels) ++at;
    if (capacity < count * 3U || bytes - at != count * channels) return 0;
    a = (uintptr_t)file; b = (uintptr_t)rgb;
    if (bytes > UINTPTR_MAX - a || count * 3U > UINTPTR_MAX - b ||
        (a < b + count * 3U && b < a + bytes)) return 0;
    if (channels == 3U) memcpy(rgb, file + at, count * 3U);
    else for (i = 0U; i < count; ++i)
        rgb[i * 3U] = rgb[i * 3U + 1U] = rgb[i * 3U + 2U] = file[at + i];
    candidate.width = width; candidate.height = height;
    candidate.rgb = rgb; candidate.bytes = count * 3U;
    *image = candidate;
    return 1;
}

static int image_valid(const shakti_rgb_image_t *image)
{
    return image && image->rgb && image->width && image->height &&
        (uint64_t)image->width * image->height <= SIZE_MAX / 3U &&
        image->bytes == (size_t)image->width * image->height * 3U;
}

static int ink(const shakti_rgb_image_t *image, unsigned int x, unsigned int y)
{
    const unsigned char *p = image->rgb + ((size_t)y * image->width + x) * 3U;
    return 77U * p[0] + 150U * p[1] + 29U * p[2] < 128U * 256U;
}

static int same_cell(const shakti_rgb_image_t *a, unsigned int ax, unsigned int ay,
    const shakti_rgb_image_t *b, unsigned int bx, unsigned int by,
    unsigned int width, unsigned int height)
{
    unsigned int x, y;
    for (y = 0U; y < height; ++y) for (x = 0U; x < width; ++x)
        if (ink(a, ax + x, ay + y) != ink(b, bx + x, by + y)) return 0;
    return 1;
}

int shakti_eyes_ocr(const shakti_rgb_image_t *image, const shakti_rgb_image_t *atlas,
    const char *alphabet, unsigned int cw, unsigned int ch, shakti_ocr_result_t *result)
{
    size_t glyphs, i, j, used = 0U, last_nonempty = 0U;
    unsigned int row, col, rows, columns;
    if (!result) return 0;
    memset(result, 0, sizeof(*result));
    if (!image_valid(image) || !image_valid(atlas) || !alphabet ||
        !(glyphs = strlen(alphabet)) || glyphs > 95U || !cw || !ch || cw > 128U || ch > 128U ||
        atlas->height != ch || (uint64_t)glyphs * cw != atlas->width ||
        image->width % cw || image->height % ch || alphabet[0] != ' ') return 0;
    for (i = 0U; i < glyphs; ++i) {
        if ((unsigned char)alphabet[i] < 32U || (unsigned char)alphabet[i] > 126U) return 0;
        for (j = 0U; j < i; ++j)
            if (alphabet[i] == alphabet[j] ||
                same_cell(atlas, (unsigned int)i * cw, 0U, atlas, (unsigned int)j * cw, 0U, cw, ch)) return 0;
    }
    /* The declared space cell must contain no ink. */
    for (row = 0U; row < ch; ++row) for (col = 0U; col < cw; ++col)
        if (ink(atlas, col, row)) return 0;
    rows = image->height / ch; columns = image->width / cw;
    for (row = 0U; row < rows; ++row) {
        size_t row_start = used, row_end = used;
        for (col = 0U; col < columns; ++col) {
            size_t matched = glyphs;
            for (i = 0U; i < glyphs; ++i)
                if (same_cell(image, col * cw, row * ch, atlas, (unsigned int)i * cw, 0U, cw, ch)) {
                    matched = i; break;
                }
            if (matched == glyphs || used + 2U >= sizeof(result->text)) goto refuse;
            result->text[used++] = alphabet[matched];
            if (matched) {
                shakti_ocr_character_t *character;
                if (result->count == SHAKTI_OCR_CHARACTERS) goto refuse;
                character = &result->characters[result->count++];
                character->character = (unsigned char)alphabet[matched];
                character->x = col * cw; character->y = row * ch;
                character->width = cw; character->height = ch;
                character->row = row; character->column = col;
                row_end = used;
            }
        }
        used = row_end; /* Trim trailing spaces, retain declared leading spaces. */
        if (used != row_start) { ++result->lines; last_nonempty = used; }
        if (used + 1U >= sizeof(result->text)) goto refuse;
        result->text[used++] = '\n';
    }
    result->text[last_nonempty] = 0;
    result->width = image->width; result->height = image->height;
    result->source_hash = shakti_media_hash(image->rgb, image->bytes);
    return 1;
refuse:
    memset(result, 0, sizeof(*result));
    return 0;
}
