#include "eyes.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * binary_deposit: the pixel deposit, in 1s and 0s, that Tyler can read.
 *
 * Two real files on disk are the documents:
 *   binary/page1_picture.txt  (small palette-letter picture)
 *   binary/page2_text.txt     (small text printed onto a page with the
 *                              repository's fixed 5x7 font)
 *
 * Every pixel location of each page is deposited into its own dated
 * text file, one file per page number, with 5 binary marks per line:
 * R, G, B, A as 8 bits each, then the single mono ink bit.
 *
 * The rebuild is earned: the deposit file is read back from disk and the
 * page is rebuilt only from the marks in the file. The counts printed are
 * exactly what the pixel-for-pixel comparison measures — nothing adjusts
 * them. The collected output is one dated 2-page Letter-size PDF.
 *
 * Locked core eyes.c / eyes.h is linked unchanged. Plain C99, no
 * libraries, no heap, no subprocess.
 */

#define BD_DIR "binary"
#define BD_PAGE_COUNT 2U
#define BD_WIDTH_MAX 64U
#define BD_HEIGHT_MAX 24U
#define BD_RGBA_CAPACITY \
    ((unsigned long)BD_WIDTH_MAX * (unsigned long)BD_HEIGHT_MAX * 4UL)
#define BD_BITS_CAPACITY \
    ((unsigned long)BD_WIDTH_MAX * (unsigned long)BD_HEIGHT_MAX + 1UL)
#define BD_LINE_CAPACITY 256UL
#define BD_PATH_CAPACITY 256UL
#define BD_CONTENT_CAPACITY 8192UL
#define BD_IMAGE_CAPACITY \
    ((unsigned long)BD_WIDTH_MAX * (unsigned long)BD_HEIGHT_MAX * 3UL)
#define BD_TEXT_PAGE_WIDTH 64U
#define BD_TEXT_PAGE_HEIGHT 24U
#define BD_PDF_SCALE 6U
/* PDF objects: catalog, pages, font; per page 3 images + content + page. */
#define BD_PDF_BASE_OBJECTS 3U
#define BD_PDF_PAGE_OBJECTS 5U
#define BD_PDF_FIRST(page_index) \
    (BD_PDF_BASE_OBJECTS + 1U + (page_index) * BD_PDF_PAGE_OBJECTS)
#define BD_PDF_MAX_OBJECTS \
    (BD_PDF_BASE_OBJECTS + BD_PAGE_COUNT * BD_PDF_PAGE_OBJECTS)

/* Section 1: fixed buffers, no heap. */
static unsigned char g_original[BD_PAGE_COUNT][BD_RGBA_CAPACITY];
static unsigned char g_rebuilt[BD_PAGE_COUNT][BD_RGBA_CAPACITY];
static unsigned char g_bitonly[BD_PAGE_COUNT][BD_RGBA_CAPACITY];
static char g_bits[BD_BITS_CAPACITY];
static char g_bits_back[BD_BITS_CAPACITY];
static char g_line[BD_LINE_CAPACITY];
static char g_escaped[BD_LINE_CAPACITY * 2UL];
static char g_content[BD_CONTENT_CAPACITY];
static unsigned char g_image[BD_IMAGE_CAPACITY];
static long g_object_offsets[BD_PDF_MAX_OBJECTS + 1U];

static unsigned int g_width[BD_PAGE_COUNT];
static unsigned int g_height[BD_PAGE_COUNT];
static unsigned long g_cannot[BD_PAGE_COUNT];
static unsigned long g_bit_loses[BD_PAGE_COUNT];
static char g_deposit_name[BD_PAGE_COUNT][BD_PATH_CAPACITY];

/* Section 2: small helpers. */
static int text_append__deposit_h(
    char *buffer,
    unsigned long capacity,
    unsigned long *used,
    const char *format,
    ...
)
{
    va_list arguments;
    int written;

    if (buffer == NULL || used == NULL || format == NULL ||
        *used >= capacity) {
        return 0;
    }

    va_start(arguments, format);
    written = vsnprintf(buffer + *used, capacity - *used, format, arguments);
    va_end(arguments);

    if (written < 0 || (unsigned long)written >= capacity - *used) {
        return 0;
    }

    *used += (unsigned long)written;
    return 1;
}

static int date_today__deposit_h(char *out, unsigned long capacity)
{
    time_t now;
    struct tm *local;
    int written;

    if (out == NULL || capacity == 0UL) {
        return 0;
    }

    now = time(NULL);

    if (now == (time_t)-1) {
        return 0;
    }

    local = localtime(&now);

    if (local == NULL) {
        return 0;
    }

    written = snprintf(
        out,
        capacity,
        "%04d-%02d-%02d",
        local->tm_year + 1900,
        local->tm_mon + 1,
        local->tm_mday
    );

    return written > 0 && (unsigned long)written < capacity;
}

static void strip_line_end__deposit_h(char *line)
{
    unsigned long length;

    length = (unsigned long)strlen(line);

    while (length > 0UL &&
           (line[length - 1UL] == '\n' || line[length - 1UL] == '\r')) {
        line[length - 1UL] = '\0';
        --length;
    }
}

/* Section 3: the two input documents, read from real files. */
static int palette_rgb(
    char letter,
    unsigned char *red,
    unsigned char *green,
    unsigned char *blue
)
{
    switch (letter) {
    case 'W': *red = 255U; *green = 255U; *blue = 255U; return 1;
    case 'K': *red = 0U;   *green = 0U;   *blue = 0U;   return 1;
    case 'R': *red = 255U; *green = 0U;   *blue = 0U;   return 1;
    case 'G': *red = 0U;   *green = 255U; *blue = 0U;   return 1;
    case 'B': *red = 0U;   *green = 0U;   *blue = 255U; return 1;
    case 'Y': *red = 255U; *green = 255U; *blue = 0U;   return 1;
    case 'C': *red = 0U;   *green = 255U; *blue = 255U; return 1;
    case 'M': *red = 255U; *green = 0U;   *blue = 255U; return 1;
    case 'D': *red = 128U; *green = 128U; *blue = 128U; return 1;
    default:  return 0;
    }
}

static int read_picture_file(
    const char *path,
    unsigned char *rgba,
    unsigned int *width_out,
    unsigned int *height_out
)
{
    FILE *file;
    unsigned int width;
    unsigned int height;
    unsigned int row;

    file = fopen(path, "r");

    if (file == NULL) {
        printf("FAIL: cannot open %s\n", path);
        return 0;
    }

    width = 0U;
    height = 0U;

    if (fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        (strip_line_end__deposit_h(g_line), strcmp(g_line, "BINARY_PICTURE_V1")) != 0 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        sscanf(g_line, "WIDTH=%u", &width) != 1 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        sscanf(g_line, "HEIGHT=%u", &height) != 1 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        strncmp(g_line, "PALETTE", 7UL) != 0 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        (strip_line_end__deposit_h(g_line), strcmp(g_line, "ROWS")) != 0 ||
        width == 0U || width > BD_WIDTH_MAX ||
        height == 0U || height > BD_HEIGHT_MAX) {
        printf("FAIL: bad picture header in %s\n", path);
        fclose(file);
        return 0;
    }

    for (row = 0U; row < height; ++row) {
        unsigned int column;

        if (fgets(g_line, (int)sizeof(g_line), file) == NULL) {
            printf("FAIL: %s row %u missing\n", path, row);
            fclose(file);
            return 0;
        }

        strip_line_end__deposit_h(g_line);

        if ((unsigned long)strlen(g_line) != (unsigned long)width) {
            printf("FAIL: %s row %u is not %u letters\n", path, row, width);
            fclose(file);
            return 0;
        }

        for (column = 0U; column < width; ++column) {
            unsigned long base;
            unsigned char red;
            unsigned char green;
            unsigned char blue;

            if (!palette_rgb(g_line[column], &red, &green, &blue)) {
                printf(
                    "FAIL: %s row %u letter %c unknown\n",
                    path, row, g_line[column]
                );
                fclose(file);
                return 0;
            }

            base = ((unsigned long)row * (unsigned long)width +
                    (unsigned long)column) * 4UL;
            rgba[base] = red;
            rgba[base + 1UL] = green;
            rgba[base + 2UL] = blue;
            rgba[base + 3UL] = 255U;
        }
    }

    fclose(file);
    *width_out = width;
    *height_out = height;
    return 1;
}

static int read_text_file(const char *path, eyes_text_page_t *text)
{
    FILE *file;
    char *lines[3];
    unsigned int index;

    file = fopen(path, "r");

    if (file == NULL) {
        printf("FAIL: cannot open %s\n", path);
        return 0;
    }

    memset(text, 0, sizeof(*text));
    lines[0] = text->line1;
    lines[1] = text->line2;
    lines[2] = text->line3;

    for (index = 0U; index < 3U; ++index) {
        if (fgets(g_line, (int)sizeof(g_line), file) == NULL) {
            break;
        }

        strip_line_end__deposit_h(g_line);

        if ((unsigned long)strlen(g_line) >= EYES_TEXT_MAX) {
            printf("FAIL: %s line %u too long\n", path, index + 1U);
            fclose(file);
            return 0;
        }

        strcpy(lines[index], g_line);
    }

    fclose(file);
    return 1;
}

/* Section 4: the deposit — 5 binary marks for every pixel location. */
static int write_octet(FILE *file, unsigned char value)
{
    unsigned int bit;

    for (bit = 8U; bit > 0U; --bit) {
        int mark;

        mark = ((value >> (bit - 1U)) & 1U) ? '1' : '0';

        if (fputc(mark, file) == EOF) {
            return 0;
        }
    }

    return 1;
}

static int write_deposit(
    const char *path,
    const char *date,
    unsigned int page_number,
    unsigned int width,
    unsigned int height,
    const unsigned char *rgba,
    const char *bits
)
{
    FILE *file;
    unsigned long pixel_count;
    unsigned long index;
    int success;

    file = fopen(path, "w");

    if (file == NULL) {
        printf("FAIL: cannot write %s\n", path);
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;

    success =
        fprintf(file, "BINARY_DEPOSIT_V1\n") > 0 &&
        fprintf(file, "DATE=%s\n", date) > 0 &&
        fprintf(file, "PAGE=%u\n", page_number) > 0 &&
        fprintf(file, "WIDTH=%u\n", width) > 0 &&
        fprintf(file, "HEIGHT=%u\n", height) > 0 &&
        fprintf(file, "MARKS=R G B A BIT\n") > 0;

    for (index = 0UL; success && index < pixel_count; ++index) {
        unsigned long base;
        unsigned int mark;

        base = index * 4UL;

        success = fprintf(
            file,
            "X=%02lu Y=%02lu ",
            index % (unsigned long)width,
            index / (unsigned long)width
        ) > 0;

        for (mark = 0U; success && mark < 4U; ++mark) {
            success = write_octet(file, rgba[base + (unsigned long)mark]) &&
                fputc(' ', file) != EOF;
        }

        success = success &&
            fputc(bits[index], file) != EOF &&
            fputc('\n', file) != EOF;
    }

    success = success && fprintf(file, "END\n") > 0 && fflush(file) == 0;

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        printf("FAIL: writing %s\n", path);
    }

    return success;
}

static int octet_value(const char *marks, unsigned char *value_out)
{
    unsigned int value;
    unsigned int bit;

    if ((unsigned long)strlen(marks) != 8UL) {
        return 0;
    }

    value = 0U;

    for (bit = 0U; bit < 8U; ++bit) {
        if (marks[bit] != '0' && marks[bit] != '1') {
            return 0;
        }

        value = (value << 1U) | (unsigned int)(marks[bit] - '0');
    }

    *value_out = (unsigned char)value;
    return 1;
}

/* Rebuild the page only from what the deposit file on disk says. */
static int read_deposit(
    const char *path,
    unsigned int page_number,
    unsigned int width,
    unsigned int height,
    unsigned char *rgba,
    char *bits
)
{
    FILE *file;
    unsigned long pixel_count;
    unsigned long index;
    unsigned int header_page;
    unsigned int header_width;
    unsigned int header_height;

    file = fopen(path, "r");

    if (file == NULL) {
        printf("FAIL: cannot read back %s\n", path);
        return 0;
    }

    header_page = 0U;
    header_width = 0U;
    header_height = 0U;

    if (fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        (strip_line_end__deposit_h(g_line), strcmp(g_line, "BINARY_DEPOSIT_V1")) != 0 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        strncmp(g_line, "DATE=", 5UL) != 0 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        sscanf(g_line, "PAGE=%u", &header_page) != 1 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        sscanf(g_line, "WIDTH=%u", &header_width) != 1 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        sscanf(g_line, "HEIGHT=%u", &header_height) != 1 ||
        fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        (strip_line_end__deposit_h(g_line), strcmp(g_line, "MARKS=R G B A BIT")) != 0 ||
        header_page != page_number ||
        header_width != width ||
        header_height != height) {
        printf("FAIL: bad deposit header in %s\n", path);
        fclose(file);
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;

    for (index = 0UL; index < pixel_count; ++index) {
        unsigned long x;
        unsigned long y;
        char red_marks[9];
        char green_marks[9];
        char blue_marks[9];
        char alpha_marks[9];
        char bit_mark[2];
        unsigned long base;

        if (fgets(g_line, (int)sizeof(g_line), file) == NULL ||
            sscanf(
                g_line,
                "X=%lu Y=%lu %8[01] %8[01] %8[01] %8[01] %1[01]",
                &x, &y, red_marks, green_marks, blue_marks,
                alpha_marks, bit_mark
            ) != 7 ||
            x != index % (unsigned long)width ||
            y != index / (unsigned long)width) {
            printf("FAIL: %s pixel line %lu is wrong\n", path, index);
            fclose(file);
            return 0;
        }

        base = index * 4UL;

        if (!octet_value(red_marks, &rgba[base]) ||
            !octet_value(green_marks, &rgba[base + 1UL]) ||
            !octet_value(blue_marks, &rgba[base + 2UL]) ||
            !octet_value(alpha_marks, &rgba[base + 3UL])) {
            printf("FAIL: %s marks at pixel %lu are wrong\n", path, index);
            fclose(file);
            return 0;
        }

        bits[index] = bit_mark[0];
    }

    bits[pixel_count] = '\0';

    if (fgets(g_line, (int)sizeof(g_line), file) == NULL ||
        (strip_line_end__deposit_h(g_line), strcmp(g_line, "END")) != 0) {
        printf("FAIL: %s END line missing\n", path);
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

/* Section 5: plain C99 PDF writer (uncompressed, no libraries). */
static int pdf_begin_object__deposit_h(FILE *file, unsigned int number)
{
    long offset;

    if (file == NULL || number == 0U || number > BD_PDF_MAX_OBJECTS) {
        return 0;
    }

    offset = ftell(file);

    if (offset < 0L) {
        return 0;
    }

    g_object_offsets[number] = offset;
    return fprintf(file, "%u 0 obj\n", number) > 0;
}

static int pdf_escape__deposit_h(const char *in, char *out, unsigned long capacity)
{
    unsigned long used;
    unsigned long index;

    if (in == NULL || out == NULL || capacity == 0UL) {
        return 0;
    }

    used = 0UL;

    for (index = 0UL; in[index] != '\0'; ++index) {
        char character;

        character = in[index];

        if (character == '(' || character == ')' || character == '\\') {
            if (used + 2UL >= capacity) {
                return 0;
            }

            out[used] = '\\';
            ++used;
        } else if (used + 1UL >= capacity) {
            return 0;
        }

        out[used] = character;
        ++used;
    }

    out[used] = '\0';
    return 1;
}

static int pdf_write_image_object__deposit_h(
    FILE *file,
    unsigned int number,
    const unsigned char *rgba,
    unsigned int width,
    unsigned int height
)
{
    unsigned long pixel_count;
    unsigned long index;
    unsigned long length;

    if (rgba == NULL) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;
    length = pixel_count * 3UL;

    if (length > sizeof(g_image)) {
        return 0;
    }

    for (index = 0UL; index < pixel_count; ++index) {
        g_image[index * 3UL] = rgba[index * 4UL];
        g_image[index * 3UL + 1UL] = rgba[index * 4UL + 1UL];
        g_image[index * 3UL + 2UL] = rgba[index * 4UL + 2UL];
    }

    if (!pdf_begin_object__deposit_h(file, number)) {
        return 0;
    }

    if (fprintf(
            file,
            "<< /Type /XObject /Subtype /Image /Width %u /Height %u "
            "/ColorSpace /DeviceRGB /BitsPerComponent 8 /Length %lu >>\n"
            "stream\n",
            width,
            height,
            length
        ) <= 0) {
        return 0;
    }

    if (fwrite(g_image, 1U, length, file) != length) {
        return 0;
    }

    return fprintf(file, "\nendstream\nendobj\n") > 0;
}

static int pdf_write_stream_object__deposit_h(
    FILE *file,
    unsigned int number,
    const char *content
)
{
    unsigned long length;

    if (content == NULL || !pdf_begin_object__deposit_h(file, number)) {
        return 0;
    }

    length = (unsigned long)strlen(content);

    if (fprintf(file, "<< /Length %lu >>\nstream\n", length) <= 0) {
        return 0;
    }

    if (fwrite(content, 1U, length, file) != length) {
        return 0;
    }

    return fprintf(file, "\nendstream\nendobj\n") > 0;
}

static int content_caption__deposit_h(unsigned long *used, int y, const char *caption)
{
    return pdf_escape__deposit_h(caption, g_escaped, sizeof(g_escaped)) &&
        text_append__deposit_h(
            g_content,
            sizeof(g_content),
            used,
            "BT /F1 9 Tf 72 %d Td (%s) Tj ET\n",
            y,
            g_escaped
        );
}

static int content_image__deposit_h(
    unsigned long *used,
    int bottom,
    unsigned int width,
    unsigned int height,
    unsigned int local_index
)
{
    return text_append__deposit_h(
        g_content,
        sizeof(g_content),
        used,
        "q %u 0 0 %u 72 %d cm /Im%u Do Q\n",
        width * BD_PDF_SCALE,
        height * BD_PDF_SCALE,
        bottom,
        local_index
    );
}

/* Two normal Letter-size pages, one per document page number. */
static int write_pdf__deposit_h(const char *path)
{
    FILE *file;
    unsigned int page_index;
    unsigned int object_number;
    long xref_offset;
    int success;

    file = fopen(path, "wb");

    if (file == NULL) {
        printf("FAIL: cannot write %s\n", path);
        return 0;
    }

    success = fprintf(file, "%%PDF-1.4\n%%\342\343\317\323\n") > 0;

    success = success &&
        pdf_begin_object__deposit_h(file, 1U) &&
        fprintf(file, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n") > 0;

    if (success) {
        success = pdf_begin_object__deposit_h(file, 2U) &&
            fprintf(
                file,
                "<< /Type /Pages /Count %u /Kids [",
                BD_PAGE_COUNT
            ) > 0;

        for (page_index = 0U;
             success && page_index < BD_PAGE_COUNT;
             ++page_index) {
            success = fprintf(
                file, " %u 0 R", BD_PDF_FIRST(page_index) + 4U
            ) > 0;
        }

        success = success && fprintf(file, " ] >>\nendobj\n") > 0;
    }

    success = success &&
        pdf_begin_object__deposit_h(file, 3U) &&
        fprintf(
            file,
            "<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\n"
            "endobj\n"
        ) > 0;

    for (page_index = 0U;
         success && page_index < BD_PAGE_COUNT;
         ++page_index) {
        unsigned long used;
        unsigned int base;
        unsigned int width;
        unsigned int height;
        int y;
        int bottom;

        base = BD_PDF_FIRST(page_index);
        width = g_width[page_index];
        height = g_height[page_index];

        success =
            pdf_write_image_object__deposit_h(
                file, base, g_original[page_index], width, height
            ) &&
            pdf_write_image_object__deposit_h(
                file, base + 1U, g_rebuilt[page_index], width, height
            ) &&
            pdf_write_image_object__deposit_h(
                file, base + 2U, g_bitonly[page_index], width, height
            );

        if (!success) {
            break;
        }

        used = 0UL;
        g_content[0] = '\0';

        success = text_append__deposit_h(
            g_content,
            sizeof(g_content),
            &used,
            "BT /F1 12 Tf 72 745 Td (PAGE %u  %ux%u) Tj ET\n",
            page_index + 1U,
            width,
            height
        );

        y = 720;
        bottom = y - 8 - (int)(height * BD_PDF_SCALE);
        success = success &&
            content_caption__deposit_h(&used, y, "ORIGINAL") &&
            content_image__deposit_h(&used, bottom, width, height, 1U);

        y = bottom - 22;
        bottom = y - 8 - (int)(height * BD_PDF_SCALE);
        success = success &&
            content_caption__deposit_h(&used, y, "REBUILT FROM THE DEPOSIT FILE") &&
            content_image__deposit_h(&used, bottom, width, height, 2U);

        y = bottom - 22;
        bottom = y - 8 - (int)(height * BD_PDF_SCALE);
        success = success &&
            content_caption__deposit_h(&used, y, "BIT MARK ONLY") &&
            content_image__deposit_h(&used, bottom, width, height, 3U);

        y = bottom - 26;
        success = success && text_append__deposit_h(
            g_content, sizeof(g_content), &used,
            "BT /F1 9 Tf 72 %d Td (DEPOSIT %s) Tj ET\n",
            y, g_deposit_name[page_index]
        );

        y -= 14;
        success = success && text_append__deposit_h(
            g_content, sizeof(g_content), &used,
            "BT /F1 9 Tf 72 %d Td "
            "(PIXELS %lu  5 MARKS EACH  R G B A BIT) Tj ET\n",
            y,
            (unsigned long)width * (unsigned long)height
        );

        y -= 14;
        success = success && text_append__deposit_h(
            g_content, sizeof(g_content), &used,
            "BT /F1 9 Tf 72 %d Td "
            "(CANNOT REBUILD FROM DEPOSIT %lu PIXELS) Tj ET\n",
            y, g_cannot[page_index]
        );

        y -= 14;
        success = success && text_append__deposit_h(
            g_content, sizeof(g_content), &used,
            "BT /F1 9 Tf 72 %d Td "
            "(BIT MARK ALONE LOSES %lu PIXELS) Tj ET\n",
            y, g_bit_loses[page_index]
        );

        success = success &&
            pdf_write_stream_object__deposit_h(file, base + 3U, g_content) &&
            pdf_begin_object__deposit_h(file, base + 4U) &&
            fprintf(
                file,
                "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
                "/Resources << /Font << /F1 3 0 R >> /XObject << "
                "/Im1 %u 0 R /Im2 %u 0 R /Im3 %u 0 R >> >> "
                "/Contents %u 0 R >>\nendobj\n",
                base,
                base + 1U,
                base + 2U,
                base + 3U
            ) > 0;
    }

    xref_offset = success ? ftell(file) : -1L;
    success = success && xref_offset >= 0L &&
        fprintf(file, "xref\n0 %u\n", BD_PDF_MAX_OBJECTS + 1U) > 0 &&
        fprintf(file, "0000000000 65535 f \n") > 0;

    for (object_number = 1U;
         success && object_number <= BD_PDF_MAX_OBJECTS;
         ++object_number) {
        success = fprintf(
            file,
            "%010lu 00000 n \n",
            (unsigned long)g_object_offsets[object_number]
        ) > 0;
    }

    success = success &&
        fprintf(
            file,
            "trailer\n<< /Size %u /Root 1 0 R >>\nstartxref\n%lu\n%%%%EOF\n",
            BD_PDF_MAX_OBJECTS + 1U,
            (unsigned long)xref_offset
        ) > 0;

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        printf("FAIL: writing %s\n", path);
        (void)remove(path);
        return 0;
    }

    return 1;
}

/* Section 6: run — deposit, read back, rebuild, count, collect. */
int main(void)
{
    char date[16];
    char path[BD_PATH_CAPACITY];
    eyes_text_page_t text;
    unsigned int page_index;

    if (!date_today__deposit_h(date, sizeof(date))) {
        printf("FAIL: could not read today's date\n");
        return 1;
    }

    if (!read_picture_file(
            BD_DIR "/page1_picture.txt",
            g_original[0],
            &g_width[0],
            &g_height[0]
        )) {
        return 1;
    }

    if (!read_text_file(BD_DIR "/page2_text.txt", &text)) {
        return 1;
    }

    g_width[1] = BD_TEXT_PAGE_WIDTH;
    g_height[1] = BD_TEXT_PAGE_HEIGHT;

    if (!eyes_load_document(
            0U,
            g_width[1],
            g_height[1],
            &text,
            0UL,
            g_original[1],
            sizeof(g_original[1])
        )) {
        printf("FAIL: could not print page2_text.txt onto the page\n");
        return 1;
    }

    for (page_index = 0U; page_index < BD_PAGE_COUNT; ++page_index) {
        unsigned int page_number;

        page_number = page_index + 1U;

        if (!eyes_pull_mono(
                g_original[page_index],
                g_width[page_index],
                g_height[page_index],
                g_bits,
                sizeof(g_bits)
            )) {
            printf("FAIL: mono pull page %u\n", page_number);
            return 1;
        }

        if (snprintf(
                g_deposit_name[page_index],
                sizeof(g_deposit_name[page_index]),
                "%s-page%u_deposit.txt",
                date,
                page_number
            ) >= (int)sizeof(g_deposit_name[page_index]) ||
            snprintf(
                path, sizeof(path), "%s/%s",
                BD_DIR, g_deposit_name[page_index]
            ) >= (int)sizeof(path)) {
            printf("FAIL: deposit path page %u\n", page_number);
            return 1;
        }

        if (!write_deposit(
                path,
                date,
                page_number,
                g_width[page_index],
                g_height[page_index],
                g_original[page_index],
                g_bits
            )) {
            return 1;
        }

        if (!read_deposit(
                path,
                page_number,
                g_width[page_index],
                g_height[page_index],
                g_rebuilt[page_index],
                g_bits_back
            )) {
            return 1;
        }

        if (!eyes_reconstruct_mono(
                g_bits_back,
                g_width[page_index],
                g_height[page_index],
                g_bitonly[page_index],
                sizeof(g_bitonly[page_index])
            )) {
            printf("FAIL: bit mark rebuild page %u\n", page_number);
            return 1;
        }

        g_cannot[page_index] = eyes_diff(
            g_original[page_index],
            g_rebuilt[page_index],
            g_width[page_index],
            g_height[page_index],
            1
        );

        g_bit_loses[page_index] = eyes_diff(
            g_original[page_index],
            g_bitonly[page_index],
            g_width[page_index],
            g_height[page_index],
            1
        );

        printf(
            "PAGE %u  %ux%u  DEPOSIT %s/%s\n",
            page_number,
            g_width[page_index],
            g_height[page_index],
            BD_DIR,
            g_deposit_name[page_index]
        );
        printf(
            "PAGE %u  PIXELS %lu  5 MARKS EACH\n",
            page_number,
            (unsigned long)g_width[page_index] *
            (unsigned long)g_height[page_index]
        );
        printf(
            "PAGE %u  CANNOT REBUILD FROM DEPOSIT %lu PIXELS\n",
            page_number,
            g_cannot[page_index]
        );
        printf(
            "PAGE %u  BIT MARK ALONE LOSES %lu PIXELS\n",
            page_number,
            g_bit_loses[page_index]
        );
    }

    if (snprintf(
            path, sizeof(path), "%s/%s-binary_deposit.pdf", BD_DIR, date
        ) >= (int)sizeof(path)) {
        printf("FAIL: pdf path\n");
        return 1;
    }

    if (!write_pdf__deposit_h(path)) {
        return 1;
    }

    printf("COLLECTED %s\n", path);
    return 0;
}
