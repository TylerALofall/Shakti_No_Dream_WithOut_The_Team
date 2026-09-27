#include "eyes_xml.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Section 1: fixed buffers for the standalone rebuild side. */
const eyes_xml_page_spec_t g_eyes_xml_document_pages[EYES_XML_PAGE_COUNT] =
    EYES_XML_DOCUMENT_PAGES_INIT;

static eyes_xml_page_record_t g_pages[EYES_XML_PAGE_COUNT];
static char g_bits[EYES_XML_PAGE_COUNT][EYES_XML_PAGE_BITS_CAPACITY];
static unsigned char g_original[EYES_XML_RGBA_CAPACITY];
static unsigned char g_rebuilt[EYES_XML_RGBA_CAPACITY];

/* Section 2: small strict parser helpers. */
static void strip_line_end__xml_rebuild_h(char *line)
{
    unsigned long length;

    if (line == NULL) {
        return;
    }

    length = strlen(line);

    while (length > 0UL &&
           (line[length - 1UL] == '\n' || line[length - 1UL] == '\r')) {
        line[length - 1UL] = '\0';
        --length;
    }
}

static int read_line(FILE *file, char *line, unsigned long capacity)
{
    if (file == NULL || line == NULL || capacity < 2UL) {
        return 0;
    }

    if (fgets(line, (int)capacity, file) == NULL) {
        return 0;
    }

    if (strchr(line, '\n') == NULL && !feof(file)) {
        return 0;
    }

    strip_line_end__xml_rebuild_h(line);
    return 1;
}

static int append_text__xml_rebuild_h(
    char *buffer,
    unsigned long capacity,
    unsigned long *used,
    const char *text
)
{
    unsigned long index;

    if (buffer == NULL || used == NULL || text == NULL) {
        return 0;
    }

    for (index = 0UL; text[index] != '\0'; ++index) {
        if (*used + 1UL >= capacity) {
            return 0;
        }

        buffer[*used] = text[index];
        ++(*used);
    }

    buffer[*used] = '\0';
    return 1;
}

static int build_document_path__xml_rebuild_h(
    const char *tag,
    char *path,
    unsigned long capacity
)
{
    unsigned long used;

    if (tag == NULL || path == NULL || capacity == 0UL) {
        return 0;
    }

    path[0] = '\0';
    used = 0UL;

    return append_text__xml_rebuild_h(path, capacity, &used, EYES_XML_OUTPUT_DIR "/") &&
           append_text__xml_rebuild_h(path, capacity, &used, tag) &&
           append_text__xml_rebuild_h(path, capacity, &used, "_document.xml");
}

static int argument_is_xml_path(const char *argument)
{
    size_t length;

    if (argument == NULL) {
        return 0;
    }

    if (strchr(argument, '/') != NULL || strchr(argument, '\\') != NULL) {
        return 1;
    }

    length = strlen(argument);

    return length >= 4U &&
           strcmp(argument + length - 4U, ".xml") == 0;
}

static int parse_unsigned__xml_rebuild_h(const char **cursor, unsigned int *value_out)
{
    unsigned long value;
    unsigned int digit_count;

    if (cursor == NULL || *cursor == NULL || value_out == NULL) {
        return 0;
    }

    value = 0UL;
    digit_count = 0U;

    while (**cursor >= '0' && **cursor <= '9') {
        value = value * 10UL + (unsigned long)(**cursor - '0');

        if (value > (unsigned long)UINT_MAX) {
            return 0;
        }

        ++(*cursor);
        ++digit_count;
    }

    if (digit_count == 0U) {
        return 0;
    }

    *value_out = (unsigned int)value;
    return 1;
}

static int match_text(const char **cursor, const char *text)
{
    unsigned long index;

    if (cursor == NULL || *cursor == NULL || text == NULL) {
        return 0;
    }

    for (index = 0UL; text[index] != '\0'; ++index) {
        if ((*cursor)[index] != text[index]) {
            return 0;
        }
    }

    *cursor += index;
    return 1;
}

static int parse_document_open(const char *line, unsigned int *page_count_out)
{
    const char *cursor;

    if (line == NULL || page_count_out == NULL) {
        return 0;
    }

    cursor = line;

    return match_text(&cursor, "<EYES_DOCUMENT version=\"1\" pages=\"") &&
           parse_unsigned__xml_rebuild_h(&cursor, page_count_out) &&
           match_text(&cursor, "\">") &&
           *cursor == '\0';
}

static int parse_page_open(
    const char *line,
    eyes_xml_page_record_t *page
)
{
    const char *cursor;

    if (line == NULL || page == NULL) {
        return 0;
    }

    cursor = line;

    if (!match_text(&cursor, "  <PAGE number=\"") ||
        !parse_unsigned__xml_rebuild_h(&cursor, &page->number) ||
        !match_text(&cursor, "\" width=\"") ||
        !parse_unsigned__xml_rebuild_h(&cursor, &page->width) ||
        !match_text(&cursor, "\" height=\"") ||
        !parse_unsigned__xml_rebuild_h(&cursor, &page->height) ||
        !match_text(&cursor, "\" kind=\"mono\">") ||
        *cursor != '\0') {
        return 0;
    }

    page->kind[0] = 'm';
    page->kind[1] = 'o';
    page->kind[2] = 'n';
    page->kind[3] = 'o';
    page->kind[4] = '\0';
    return 1;
}

/* Section 3: exact XML reader for EYES_XML_V1. */
int eyes_xml_read_document(
    const char *path,
    eyes_xml_page_record_t *pages,
    unsigned int page_capacity,
    char bits[][EYES_XML_PAGE_BITS_CAPACITY],
    unsigned int *page_count_out
)
{
    FILE *file;
    char line[EYES_XML_LINE_CAPACITY];
    unsigned int declared_pages;
    unsigned int page_index;

    if (path == NULL || pages == NULL || bits == NULL ||
        page_count_out == NULL || page_capacity == 0U) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    if (!read_line(file, line, sizeof(line)) ||
        !parse_document_open(line, &declared_pages) ||
        declared_pages == 0U ||
        declared_pages > page_capacity) {
        (void)fclose(file);
        return 0;
    }

    for (page_index = 0U; page_index < declared_pages; ++page_index) {
        unsigned long row;
        unsigned long used;
        unsigned long pixel_count;

        if (!read_line(file, line, sizeof(line)) ||
            !parse_page_open(line, &pages[page_index]) ||
            !read_line(file, line, sizeof(line)) ||
            strcmp(line, "    <BITS>") != 0) {
            (void)fclose(file);
            return 0;
        }

        pixel_count =
            (unsigned long)pages[page_index].width *
            (unsigned long)pages[page_index].height;

        if (pixel_count + 1UL > EYES_XML_PAGE_BITS_CAPACITY) {
            (void)fclose(file);
            return 0;
        }

        used = 0UL;

        for (row = 0UL; row < (unsigned long)pages[page_index].height; ++row) {
            unsigned long column;

            if (!read_line(file, line, sizeof(line)) ||
                strlen(line) != (size_t)pages[page_index].width) {
                (void)fclose(file);
                return 0;
            }

            for (column = 0UL; column < (unsigned long)pages[page_index].width;
                 ++column) {
                char bit;

                bit = line[column];

                if (bit != '0' && bit != '1') {
                    (void)fclose(file);
                    return 0;
                }

                bits[page_index][used] = bit;
                ++used;
            }
        }

        bits[page_index][used] = '\0';

        if (!read_line(file, line, sizeof(line)) ||
            strcmp(line, "    </BITS>") != 0 ||
            !read_line(file, line, sizeof(line)) ||
            strcmp(line, "  </PAGE>") != 0) {
            (void)fclose(file);
            return 0;
        }
    }

    if (!read_line(file, line, sizeof(line)) ||
        strcmp(line, "</EYES_DOCUMENT>") != 0 ||
        fgetc(file) != EOF) {
        (void)fclose(file);
        return 0;
    }

    if (fclose(file) != 0) {
        return 0;
    }

    *page_count_out = declared_pages;
    return 1;
}

/* Section 4: deterministic rebuild entry point. */
#define EYES_XML_REBUILD_MAIN main
int EYES_XML_REBUILD_MAIN(int argc, char **argv)
{
    const char *xml_path;
    char default_path[EYES_XML_PATH_CAPACITY];
    unsigned int page_count;
    unsigned int page_index;
    int failed;

    if (argc > 2) {
        printf("FAIL: usage: eyes_xml_rebuild [tag-or-path]\n");
        return 1;
    }

    if (argc == 2 && argument_is_xml_path(argv[1])) {
        xml_path = argv[1];
    } else {
        const char *tag;

        tag = argc == 2 && argv[1][0] != '\0' ? argv[1] : EYES_XML_DEFAULT_TAG;

        if (!build_document_path__xml_rebuild_h(tag, default_path, sizeof(default_path))) {
            printf("FAIL: path build\n");
            return 1;
        }

        xml_path = default_path;
    }

    if (!eyes_xml_read_document(
            xml_path,
            g_pages,
            EYES_XML_PAGE_COUNT,
            g_bits,
            &page_count
        )) {
        printf("FAIL: parse %s\n", xml_path);
        return 1;
    }

    if (page_count != EYES_XML_PAGE_COUNT) {
        printf("FAIL: page count %u, want %u\n", page_count, EYES_XML_PAGE_COUNT);
        return 1;
    }

    failed = 0;

    for (page_index = 0U; page_index < page_count; ++page_index) {
        const eyes_xml_page_spec_t *expected;
        unsigned long drift;

        expected = &g_eyes_xml_document_pages[page_index];

        if (g_pages[page_index].number != expected->number ||
            g_pages[page_index].width != expected->width ||
            g_pages[page_index].height != expected->height ||
            strcmp(g_pages[page_index].kind, "mono") != 0) {
            printf("FAIL page %u: metadata mismatch\n", page_index + 1U);
            return 1;
        }

        if (!eyes_reconstruct_mono(
                g_bits[page_index],
                expected->width,
                expected->height,
                g_rebuilt,
                sizeof(g_rebuilt)
            ) ||
            !eyes_load_document(
                expected->source_kind,
                expected->width,
                expected->height,
                &expected->text,
                expected->seed,
                g_original,
                sizeof(g_original)
            )) {
            printf("FAIL page %u: rebuild setup\n", expected->number);
            return 1;
        }

        drift = eyes_diff(
            g_original,
            g_rebuilt,
            expected->width,
            expected->height,
            1
        );

        if (drift == EYES_XML_DIFF_ERROR) {
            printf("FAIL page %u: diff rejected\n", expected->number);
            return 1;
        }

        printf(
            "page %u: %ux%u drift=%lu\n",
            expected->number,
            expected->width,
            expected->height,
            drift
        );

        if (drift != 0UL) {
            printf("FAIL page %u: drift=%lu\n", expected->number, drift);
            failed = 1;
        }
    }

    if (failed) {
        return 1;
    }

    printf("PASS: exact pixel rebuild on all %u pages\n", page_count);
    return 0;
}
