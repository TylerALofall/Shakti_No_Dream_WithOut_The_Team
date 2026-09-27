#include "eyes_xml.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define EYES_LOOP_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define EYES_LOOP_MKDIR(path) mkdir((path), 0777)
#endif

/*
 * eyes_loop_rebuild: run the exact rebuild in a loop, 20 times.
 *
 * The chain is the proven-exact path: mono pull -> mono reconstruct, and
 * each iteration's rebuilt page feeds the next iteration. Every iteration
 * gets its own sub dir under eyes/output/loop/ storing the rebuilt pages
 * in color and black and white (.tan via the locked eyes_write_recon),
 * plus its own hash (FNV-1a 64, the repository's hash family) and log.
 *
 * The 20th iteration is assured on point: per-page RGBA drift against the
 * original must be 0 and the iteration hash must equal iteration 1's hash.
 * The collected output is one picture-and-text PDF document named
 * YYYY-MM-DD-<tag>.pdf, written by a plain C99 writer with no libraries.
 * The locked core (eyes.c / eyes.h) is untouched; the clock is read only
 * for the dated output filename, never inside the collection itself.
 */

#define EYES_LOOP_ITERATIONS 20U
#define EYES_LOOP_DIR EYES_XML_OUTPUT_DIR "/loop"
#define EYES_LOOP_DEFAULT_TAG "eyes_loop"
#define EYES_LOOP_FNV_INIT 0xCBF29CE484222325ULL
#define EYES_LOOP_FNV_PRIME 0x100000001B3ULL
#define EYES_LOOP_BLOCK_CAPACITY 2048UL
#define EYES_LOOP_COLLECTED_CAPACITY 40960UL
#define EYES_LOOP_CONTENT_CAPACITY 16384UL
#define EYES_LOOP_LINE_CAPACITY 256UL
#define EYES_LOOP_PDF_SCALE 6U
#define EYES_LOOP_PDF_TEXT_LINES 55U
#define EYES_LOOP_PDF_MAX_OBJECTS 96U
/* PDF object layout: catalog, pages, font; then per picture page 3 images
 * + content + page; then per text page content + page. */
#define EYES_LOOP_PDF_BASE_OBJECTS 3U
#define EYES_LOOP_PDF_PICTURE_OBJECTS 5U
#define EYES_LOOP_PDF_TEXT_OBJECTS 2U
#define EYES_LOOP_PDF_PICTURE_FIRST(page_index) \
    (EYES_LOOP_PDF_BASE_OBJECTS + 1U + \
     (page_index) * EYES_LOOP_PDF_PICTURE_OBJECTS)
#define EYES_LOOP_PDF_TEXT_FIRST(text_index) \
    (EYES_LOOP_PDF_PICTURE_FIRST(EYES_XML_PAGE_COUNT) + \
     (text_index) * EYES_LOOP_PDF_TEXT_OBJECTS)
#define EYES_LOOP_IMAGE_CAPACITY \
    ((unsigned long)EYES_XML_WIDTH_MAX * (unsigned long)EYES_XML_HEIGHT_MAX * 3UL)

/* Section 1: fixed buffers, no heap. */
const eyes_xml_page_spec_t g_eyes_xml_document_pages[EYES_XML_PAGE_COUNT] =
    EYES_XML_DOCUMENT_PAGES_INIT;

static unsigned char g_original[EYES_XML_PAGE_COUNT][EYES_XML_RGBA_CAPACITY];
static unsigned char g_current[EYES_XML_PAGE_COUNT][EYES_XML_RGBA_CAPACITY];
static unsigned char g_rebuilt[EYES_XML_RGBA_CAPACITY];
static char g_bits[EYES_XML_PAGE_BITS_CAPACITY];
static char g_block[EYES_LOOP_BLOCK_CAPACITY];
static char g_collected[EYES_LOOP_COLLECTED_CAPACITY];
static char g_content[EYES_LOOP_CONTENT_CAPACITY];
static char g_line[EYES_LOOP_LINE_CAPACITY];
static char g_escaped[EYES_LOOP_LINE_CAPACITY * 2UL];
static unsigned char g_image[EYES_LOOP_IMAGE_CAPACITY];
static long g_object_offsets[EYES_LOOP_PDF_MAX_OBJECTS + 1U];

/* Section 2: hash and text helpers. */
static unsigned long long fnv1a64__loop_rebuild_h(
    unsigned long long hash,
    const unsigned char *data,
    unsigned long length
)
{
    unsigned long index;

    for (index = 0UL; index < length; ++index) {
        hash ^= (unsigned long long)data[index];
        hash *= EYES_LOOP_FNV_PRIME;
    }

    return hash;
}

static int text_append__loop_rebuild_h(
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

static int date_today__loop_rebuild_h(char *out, unsigned long capacity)
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

static int ensure_dir(const char *path)
{
    if (EYES_LOOP_MKDIR(path) == 0) {
        return 1;
    }

    return errno == EEXIST;
}

static int write_text_file__loop_rebuild_h(const char *path, const char *text)
{
    FILE *file;
    int success;

    if (path == NULL || text == NULL) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success = fputs(text, file) >= 0 && fflush(file) == 0;

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

/* Section 3: plain C99 PDF writer (uncompressed, no libraries). */
static int pdf_begin_object__loop_rebuild_h(FILE *file, unsigned int number)
{
    long offset;

    if (file == NULL || number == 0U ||
        number > EYES_LOOP_PDF_MAX_OBJECTS) {
        return 0;
    }

    offset = ftell(file);

    if (offset < 0L) {
        return 0;
    }

    g_object_offsets[number] = offset;
    return fprintf(file, "%u 0 obj\n", number) > 0;
}

static int pdf_escape__loop_rebuild_h(const char *in, char *out, unsigned long capacity)
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

/*
 * Emit one image XObject. color != 0 writes DeviceRGB (3 bytes per pixel
 * from the RGBA buffer); color == 0 writes DeviceGray from the red
 * channel, the same channel eyes_write_recon uses for the INK plane.
 */
static int pdf_write_image_object__loop_rebuild_h(
    FILE *file,
    unsigned int number,
    const unsigned char *rgba,
    unsigned int width,
    unsigned int height,
    int color
)
{
    unsigned long pixel_count;
    unsigned long index;
    unsigned long length;

    if (rgba == NULL) {
        return 0;
    }

    pixel_count = (unsigned long)width * (unsigned long)height;
    length = pixel_count * (color ? 3UL : 1UL);

    if (length > sizeof(g_image)) {
        return 0;
    }

    for (index = 0UL; index < pixel_count; ++index) {
        if (color) {
            g_image[index * 3UL] = rgba[index * 4UL];
            g_image[index * 3UL + 1UL] = rgba[index * 4UL + 1UL];
            g_image[index * 3UL + 2UL] = rgba[index * 4UL + 2UL];
        } else {
            g_image[index] = rgba[index * 4UL];
        }
    }

    if (!pdf_begin_object__loop_rebuild_h(file, number)) {
        return 0;
    }

    if (fprintf(
            file,
            "<< /Type /XObject /Subtype /Image /Width %u /Height %u "
            "/ColorSpace %s /BitsPerComponent 8 /Length %lu >>\nstream\n",
            width,
            height,
            color ? "/DeviceRGB" : "/DeviceGray",
            length
        ) <= 0) {
        return 0;
    }

    if (fwrite(g_image, 1U, length, file) != length) {
        return 0;
    }

    return fprintf(file, "\nendstream\nendobj\n") > 0;
}

static int pdf_write_stream_object__loop_rebuild_h(
    FILE *file,
    unsigned int number,
    const char *content
)
{
    unsigned long length;

    if (content == NULL || !pdf_begin_object__loop_rebuild_h(file, number)) {
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

static int content_caption__loop_rebuild_h(
    unsigned long *used,
    int y,
    const char *caption
)
{
    return text_append__loop_rebuild_h(
        g_content,
        sizeof(g_content),
        used,
        "BT /F1 9 Tf 72 %d Td (%s) Tj ET\n",
        y,
        caption
    );
}

static int content_image__loop_rebuild_h(
    unsigned long *used,
    int bottom,
    unsigned int width,
    unsigned int height,
    unsigned int local_index
)
{
    return text_append__loop_rebuild_h(
        g_content,
        sizeof(g_content),
        used,
        "q %u 0 0 %u 72 %d cm /Im%u Do Q\n",
        width * EYES_LOOP_PDF_SCALE,
        height * EYES_LOOP_PDF_SCALE,
        bottom,
        local_index
    );
}

static unsigned int pdf_count_lines(const char *text)
{
    unsigned int lines;
    unsigned long index;

    lines = 0U;

    for (index = 0UL; text[index] != '\0'; ++index) {
        if (text[index] == '\n') {
            ++lines;
        }
    }

    if (index > 0UL && text[index - 1UL] != '\n') {
        ++lines;
    }

    return lines;
}

/*
 * Collected document: one picture page per document page (original in
 * color, iteration-20 rebuild in color, iteration-20 rebuild in black
 * and white), then the full run log as text pages.
 */
static int write_pdf__loop_rebuild_h(const char *path, const char *log_text)
{
    FILE *file;
    unsigned int text_lines;
    unsigned int text_pages;
    unsigned int total_objects;
    unsigned int page_index;
    unsigned int text_index;
    unsigned int object_number;
    long xref_offset;
    const char *cursor;
    int success;

    if (path == NULL || log_text == NULL) {
        return 0;
    }

    text_lines = pdf_count_lines(log_text);
    text_pages = (text_lines + EYES_LOOP_PDF_TEXT_LINES - 1U) /
                 EYES_LOOP_PDF_TEXT_LINES;

    if (text_pages == 0U) {
        text_pages = 1U;
    }

    total_objects = EYES_LOOP_PDF_BASE_OBJECTS +
                    EYES_XML_PAGE_COUNT * EYES_LOOP_PDF_PICTURE_OBJECTS +
                    text_pages * EYES_LOOP_PDF_TEXT_OBJECTS;

    if (total_objects > EYES_LOOP_PDF_MAX_OBJECTS) {
        return 0;
    }

    file = fopen(path, "wb");

    if (file == NULL) {
        return 0;
    }

    success = fprintf(file, "%%PDF-1.4\n%%\342\343\317\323\n") > 0;

    /* Object 1: catalog. Object 2: page tree. Object 3: Courier font. */
    success = success &&
        pdf_begin_object__loop_rebuild_h(file, 1U) &&
        fprintf(file, "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n") > 0;

    if (success) {
        success = pdf_begin_object__loop_rebuild_h(file, 2U) &&
            fprintf(
                file,
                "<< /Type /Pages /Count %u /Kids [",
                EYES_XML_PAGE_COUNT + text_pages
            ) > 0;

        for (page_index = 0U;
             success && page_index < EYES_XML_PAGE_COUNT;
             ++page_index) {
            success = fprintf(
                file,
                " %u 0 R",
                EYES_LOOP_PDF_PICTURE_FIRST(page_index) + 4U
            ) > 0;
        }

        for (text_index = 0U;
             success && text_index < text_pages;
             ++text_index) {
            success = fprintf(
                file,
                " %u 0 R",
                EYES_LOOP_PDF_TEXT_FIRST(text_index) + 1U
            ) > 0;
        }

        success = success && fprintf(file, " ] >>\nendobj\n") > 0;
    }

    success = success &&
        pdf_begin_object__loop_rebuild_h(file, 3U) &&
        fprintf(
            file,
            "<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>\n"
            "endobj\n"
        ) > 0;

    /* Picture pages, then their content stream and page object. */
    for (page_index = 0U;
         success && page_index < EYES_XML_PAGE_COUNT;
         ++page_index) {
        const eyes_xml_page_spec_t *page;
        unsigned long used;
        unsigned int base;
        int y;
        int bottom;

        page = &g_eyes_xml_document_pages[page_index];
        base = EYES_LOOP_PDF_PICTURE_FIRST(page_index);

        success =
            pdf_write_image_object__loop_rebuild_h(
                file, base, g_original[page_index],
                page->width, page->height, 1
            ) &&
            pdf_write_image_object__loop_rebuild_h(
                file, base + 1U, g_current[page_index],
                page->width, page->height, 1
            ) &&
            pdf_write_image_object__loop_rebuild_h(
                file, base + 2U, g_current[page_index],
                page->width, page->height, 0
            );

        if (!success) {
            break;
        }

        used = 0UL;
        g_content[0] = '\0';

        success = text_append__loop_rebuild_h(
            g_content,
            sizeof(g_content),
            &used,
            "BT /F1 12 Tf 72 745 Td (PAGE %u  %ux%u) Tj ET\n",
            page->number,
            page->width,
            page->height
        );

        y = 720;
        bottom = y - 8 - (int)(page->height * EYES_LOOP_PDF_SCALE);
        success = success &&
            content_caption__loop_rebuild_h(&used, y, "ORIGINAL  COLOR") &&
            content_image__loop_rebuild_h(&used, bottom, page->width, page->height, 1U);

        y = bottom - 22;
        bottom = y - 8 - (int)(page->height * EYES_LOOP_PDF_SCALE);
        success = success &&
            content_caption__loop_rebuild_h(&used, y, "ITERATION 20 REBUILD  COLOR") &&
            content_image__loop_rebuild_h(&used, bottom, page->width, page->height, 2U);

        y = bottom - 22;
        bottom = y - 8 - (int)(page->height * EYES_LOOP_PDF_SCALE);
        success = success &&
            content_caption__loop_rebuild_h(
                &used, y, "ITERATION 20 REBUILD  BLACK AND WHITE"
            ) &&
            content_image__loop_rebuild_h(&used, bottom, page->width, page->height, 3U);

        success = success &&
            pdf_write_stream_object__loop_rebuild_h(file, base + 3U, g_content) &&
            pdf_begin_object__loop_rebuild_h(file, base + 4U) &&
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

    /* Text pages: the collected run log in Courier. */
    cursor = log_text;

    for (text_index = 0U;
         success && text_index < text_pages;
         ++text_index) {
        unsigned long used;
        unsigned int line_count;
        unsigned int base;

        base = EYES_LOOP_PDF_TEXT_FIRST(text_index);
        used = 0UL;
        g_content[0] = '\0';

        success = text_append__loop_rebuild_h(
            g_content,
            sizeof(g_content),
            &used,
            "BT /F1 9 Tf 11 TL 72 751 Td\n"
        );

        line_count = 0U;

        while (success && *cursor != '\0' &&
               line_count < EYES_LOOP_PDF_TEXT_LINES) {
            unsigned long length;

            length = 0UL;

            while (cursor[length] != '\0' && cursor[length] != '\n' &&
                   length + 1UL < sizeof(g_line)) {
                g_line[length] = cursor[length];
                ++length;
            }

            g_line[length] = '\0';
            cursor += length;

            if (*cursor == '\n') {
                ++cursor;
            }

            success = pdf_escape__loop_rebuild_h(g_line, g_escaped, sizeof(g_escaped)) &&
                text_append__loop_rebuild_h(
                    g_content,
                    sizeof(g_content),
                    &used,
                    "T* (%s) Tj\n",
                    g_escaped
                );
            ++line_count;
        }

        success = success &&
            text_append__loop_rebuild_h(g_content, sizeof(g_content), &used, "ET\n") &&
            pdf_write_stream_object__loop_rebuild_h(file, base, g_content) &&
            pdf_begin_object__loop_rebuild_h(file, base + 1U) &&
            fprintf(
                file,
                "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
                "/Resources << /Font << /F1 3 0 R >> >> "
                "/Contents %u 0 R >>\nendobj\n",
                base
            ) > 0;
    }

    /* Cross-reference table and trailer. */
    xref_offset = success ? ftell(file) : -1L;
    success = success && xref_offset >= 0L &&
        fprintf(file, "xref\n0 %u\n", total_objects + 1U) > 0 &&
        fprintf(file, "0000000000 65535 f \n") > 0;

    for (object_number = 1U;
         success && object_number <= total_objects;
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
            total_objects + 1U,
            (unsigned long)xref_offset
        ) > 0;

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

/* Section 4: the 20-iteration rebuild loop. */
#define EYES_LOOP_REBUILD_MAIN main
int EYES_LOOP_REBUILD_MAIN(int argc, char **argv)
{
    const char *tag;
    char date[16];
    char path[EYES_XML_PATH_CAPACITY];
    unsigned long collected_used;
    unsigned long long iteration1_hash;
    unsigned int iteration;
    unsigned int page_index;
    int all_exact;

    if (argc > 2) {
        printf("FAIL: usage: eyes_loop_rebuild [tag]\n");
        return 1;
    }

    tag = argc == 2 && argv[1][0] != '\0' ? argv[1] : EYES_LOOP_DEFAULT_TAG;

    if (!date_today__loop_rebuild_h(date, sizeof(date))) {
        printf("FAIL: could not read today's date\n");
        return 1;
    }

    if (!ensure_dir(EYES_XML_OUTPUT_DIR) || !ensure_dir(EYES_LOOP_DIR)) {
        printf("FAIL: could not create %s\n", EYES_LOOP_DIR);
        return 1;
    }

    for (page_index = 0U; page_index < EYES_XML_PAGE_COUNT; ++page_index) {
        const eyes_xml_page_spec_t *page;

        page = &g_eyes_xml_document_pages[page_index];

        if (!eyes_load_document(
                page->source_kind,
                page->width,
                page->height,
                &page->text,
                page->seed,
                g_original[page_index],
                sizeof(g_original[page_index])
            )) {
            printf("FAIL: load page %u\n", page->number);
            return 1;
        }

        memcpy(
            g_current[page_index],
            g_original[page_index],
            sizeof(g_original[page_index])
        );
    }

    collected_used = 0UL;
    g_collected[0] = '\0';

    if (!text_append__loop_rebuild_h(
            g_collected,
            sizeof(g_collected),
            &collected_used,
            "EYES_LOOP_V1  %u ITERATIONS  DATE %s  TAG %s\n"
            "chain: mono pull -> exact rebuild feeds the next iteration\n\n",
            EYES_LOOP_ITERATIONS,
            date,
            tag
        )) {
        printf("FAIL: collected log capacity\n");
        return 1;
    }

    iteration1_hash = 0ULL;
    all_exact = 1;

    for (iteration = 1U; iteration <= EYES_LOOP_ITERATIONS; ++iteration) {
        unsigned long long iteration_hash;
        unsigned long block_used;
        unsigned long hash_used;
        char hash_text[512];
        int match;

        if (snprintf(
                path, sizeof(path), "%s/iter_%02u", EYES_LOOP_DIR, iteration
            ) >= (int)sizeof(path) ||
            !ensure_dir(path)) {
            printf("FAIL: could not create iteration %u dir\n", iteration);
            return 1;
        }

        iteration_hash = EYES_LOOP_FNV_INIT;
        block_used = 0UL;
        g_block[0] = '\0';
        hash_used = 0UL;
        hash_text[0] = '\0';

        if (!text_append__loop_rebuild_h(
                g_block, sizeof(g_block), &block_used,
                "ITERATION %02u  DATE %s\n", iteration, date
            ) ||
            !text_append__loop_rebuild_h(
                hash_text, sizeof(hash_text), &hash_used,
                "EYES_LOOP_HASH_V1\nITERATION=%02u\n", iteration
            )) {
            printf("FAIL: log capacity iteration %u\n", iteration);
            return 1;
        }

        for (page_index = 0U;
             page_index < EYES_XML_PAGE_COUNT;
             ++page_index) {
            const eyes_xml_page_spec_t *page;
            unsigned long long page_hash;
            unsigned long rgba_length;
            unsigned long drift;

            page = &g_eyes_xml_document_pages[page_index];
            rgba_length = (unsigned long)page->width *
                          (unsigned long)page->height * 4UL;

            if (!eyes_pull_mono(
                    g_current[page_index],
                    page->width,
                    page->height,
                    g_bits,
                    sizeof(g_bits)
                ) ||
                !eyes_reconstruct_mono(
                    g_bits,
                    page->width,
                    page->height,
                    g_rebuilt,
                    sizeof(g_rebuilt)
                )) {
                printf(
                    "FAIL: rebuild page %u iteration %u\n",
                    page->number,
                    iteration
                );
                return 1;
            }

            drift = eyes_diff(
                g_original[page_index],
                g_rebuilt,
                page->width,
                page->height,
                1
            );

            if (drift != 0UL) {
                all_exact = 0;
            }

            if (snprintf(
                    path, sizeof(path), "%s/iter_%02u/page%u_color.tan",
                    EYES_LOOP_DIR, iteration, page->number
                ) >= (int)sizeof(path) ||
                !eyes_write_recon(
                    path, page->width, page->height, 1, g_rebuilt
                ) ||
                snprintf(
                    path, sizeof(path), "%s/iter_%02u/page%u_bw.tan",
                    EYES_LOOP_DIR, iteration, page->number
                ) >= (int)sizeof(path) ||
                !eyes_write_recon(
                    path, page->width, page->height, 0, g_rebuilt
                )) {
                printf(
                    "FAIL: write page %u iteration %u\n",
                    page->number,
                    iteration
                );
                return 1;
            }

            page_hash = fnv1a64__loop_rebuild_h(EYES_LOOP_FNV_INIT, g_rebuilt, rgba_length);
            iteration_hash = fnv1a64__loop_rebuild_h(iteration_hash, g_rebuilt, rgba_length);
            memcpy(g_current[page_index], g_rebuilt, rgba_length);

            if (!text_append__loop_rebuild_h(
                    g_block, sizeof(g_block), &block_used,
                    "page %u: %ux%u drift_vs_original=%lu "
                    "hash=fnv1a64__loop_rebuild_h:%016llX\n",
                    page->number,
                    page->width,
                    page->height,
                    drift,
                    page_hash
                ) ||
                !text_append__loop_rebuild_h(
                    hash_text, sizeof(hash_text), &hash_used,
                    "PAGE_%u=fnv1a64__loop_rebuild_h:%016llX\n", page->number, page_hash
                )) {
                printf("FAIL: log capacity iteration %u\n", iteration);
                return 1;
            }
        }

        if (iteration == 1U) {
            iteration1_hash = iteration_hash;
        }

        match = iteration_hash == iteration1_hash;

        if (!match) {
            all_exact = 0;
        }

        if (!text_append__loop_rebuild_h(
                g_block, sizeof(g_block), &block_used,
                "iteration hash=fnv1a64__loop_rebuild_h:%016llX  match_iteration_1=%s\n",
                iteration_hash,
                match ? "YES" : "NO"
            ) ||
            !text_append__loop_rebuild_h(
                hash_text, sizeof(hash_text), &hash_used,
                "ITERATION_HASH=fnv1a64__loop_rebuild_h:%016llX\n", iteration_hash
            )) {
            printf("FAIL: log capacity iteration %u\n", iteration);
            return 1;
        }

        if (snprintf(
                path, sizeof(path), "%s/iter_%02u/hash.txt",
                EYES_LOOP_DIR, iteration
            ) >= (int)sizeof(path) ||
            !write_text_file__loop_rebuild_h(path, hash_text) ||
            snprintf(
                path, sizeof(path), "%s/iter_%02u/log.txt",
                EYES_LOOP_DIR, iteration
            ) >= (int)sizeof(path) ||
            !write_text_file__loop_rebuild_h(path, g_block)) {
            printf("FAIL: write hash/log iteration %u\n", iteration);
            return 1;
        }

        if (!text_append__loop_rebuild_h(
                g_collected, sizeof(g_collected), &collected_used,
                "%s\n", g_block
            )) {
            printf("FAIL: collected log capacity\n");
            return 1;
        }

        printf(
            "iteration %02u: hash=fnv1a64__loop_rebuild_h:%016llX match_iteration_1=%s\n",
            iteration,
            iteration_hash,
            match ? "YES" : "NO"
        );
    }

    if (!text_append__loop_rebuild_h(
            g_collected, sizeof(g_collected), &collected_used,
            "RESULT: %s\n",
            all_exact
                ? "PASS iteration 20 exact: drift 0 on every page, "
                  "hash matches iteration 1"
                : "FAIL iteration 20 drifted from iteration 1"
        )) {
        printf("FAIL: collected log capacity\n");
        return 1;
    }

    if (snprintf(
            path, sizeof(path), "%s/%s-%s.pdf", EYES_LOOP_DIR, date, tag
        ) >= (int)sizeof(path) ||
        !write_pdf__loop_rebuild_h(path, g_collected)) {
        printf("FAIL: write pdf\n");
        return 1;
    }

    printf("wrote %s\n", path);

    if (!all_exact) {
        printf("FAIL: iteration 20 drifted from iteration 1\n");
        return 1;
    }

    printf(
        "PASS: iteration %u exact, drift 0 on every page, "
        "hash matches iteration 1\n",
        EYES_LOOP_ITERATIONS
    );
    return 0;
}
