#include "eyes_xml.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define EYES_XML_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define EYES_XML_MKDIR(path) mkdir((path), 0777)
#endif

/* Section 1: fixed buffers for the standalone collector. */
const eyes_xml_page_spec_t g_eyes_xml_document_pages[EYES_XML_PAGE_COUNT] =
    EYES_XML_DOCUMENT_PAGES_INIT;

static unsigned char g_original[EYES_XML_RGBA_CAPACITY];
static char g_bits[EYES_XML_PAGE_COUNT][EYES_XML_PAGE_BITS_CAPACITY];

/* Section 2: small path helpers, no heap, no shell. */
static int append_text__xml_collect_h(
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

static int append_number(
    char *buffer,
    unsigned long capacity,
    unsigned long *used,
    unsigned int value
)
{
    char digits[10];
    unsigned long count;

    if (buffer == NULL || used == NULL) {
        return 0;
    }

    if (value == 0U) {
        return append_text__xml_collect_h(buffer, capacity, used, "0");
    }

    count = 0UL;

    while (value > 0U && count < sizeof(digits)) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        ++count;
    }

    while (count > 0UL) {
        if (*used + 1UL >= capacity) {
            return 0;
        }

        --count;
        buffer[*used] = digits[count];
        ++(*used);
    }

    buffer[*used] = '\0';
    return 1;
}

static int build_document_path__xml_collect_h(
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

    return append_text__xml_collect_h(path, capacity, &used, EYES_XML_OUTPUT_DIR "/") &&
           append_text__xml_collect_h(path, capacity, &used, tag) &&
           append_text__xml_collect_h(path, capacity, &used, "_document.xml");
}

static int build_original_path(
    const char *tag,
    unsigned int page_number,
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

    return append_text__xml_collect_h(path, capacity, &used, EYES_XML_OUTPUT_DIR "/") &&
           append_text__xml_collect_h(path, capacity, &used, tag) &&
           append_text__xml_collect_h(path, capacity, &used, "_page") &&
           append_number(path, capacity, &used, page_number) &&
           append_text__xml_collect_h(path, capacity, &used, "_original.tan");
}

static int ensure_output_dir(void)
{
    if (EYES_XML_MKDIR(EYES_XML_OUTPUT_DIR) == 0) {
        return 1;
    }

    return errno == EEXIST;
}

/* Section 3: exact XML writer for EYES_XML_V1. */
int eyes_xml_write_document(
    const char *path,
    const eyes_xml_page_spec_t *pages,
    unsigned int page_count,
    char bits[][EYES_XML_PAGE_BITS_CAPACITY]
)
{
    FILE *file;
    unsigned int page_index;
    int success;

    if (path == NULL || pages == NULL || bits == NULL || page_count == 0U) {
        return 0;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success = fprintf(
        file,
        "<EYES_DOCUMENT version=\"%u\" pages=\"%u\">\n",
        EYES_XML_VERSION,
        page_count
    ) > 0;

    for (page_index = 0U; success && page_index < page_count; ++page_index) {
        const eyes_xml_page_spec_t *page;
        unsigned long row;
        unsigned long used;
        unsigned long pixel_count;

        page = &pages[page_index];
        pixel_count = (unsigned long)page->width * (unsigned long)page->height;
        used = strlen(bits[page_index]);

        if (used != pixel_count) {
            success = 0;
            break;
        }

        success =
            fprintf(
                file,
                "  <PAGE number=\"%u\" width=\"%u\" height=\"%u\" kind=\"mono\">\n",
                page->number,
                page->width,
                page->height
            ) > 0 &&
            fprintf(file, "    <BITS>\n") > 0;

        for (row = 0UL; success && row < (unsigned long)page->height; ++row) {
            unsigned long offset;

            offset = row * (unsigned long)page->width;

            success =
                fwrite(bits[page_index] + offset, 1U, page->width, file) ==
                (size_t)page->width &&
                fputc('\n', file) != EOF;
        }

        if (success) {
            success =
                fprintf(file, "    </BITS>\n") > 0 &&
                fprintf(file, "  </PAGE>\n") > 0;
        }
    }

    if (success) {
        success = fprintf(file, "</EYES_DOCUMENT>\n") > 0;
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

/* Section 4: deterministic document collection entry point. */
#define EYES_XML_COLLECT_MAIN main
int EYES_XML_COLLECT_MAIN(int argc, char **argv)
{
    const char *tag;
    char path[EYES_XML_PATH_CAPACITY];
    unsigned int page_index;

    if (argc > 2) {
        printf("FAIL: usage: eyes_xml_collect [tag]\n");
        return 1;
    }

    tag = argc == 2 && argv[1][0] != '\0' ? argv[1] : EYES_XML_DEFAULT_TAG;

    if (!ensure_output_dir()) {
        printf("FAIL: could not create %s\n", EYES_XML_OUTPUT_DIR);
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
                g_original,
                sizeof(g_original)
            )) {
            printf("FAIL: load page %u\n", page->number);
            return 1;
        }

        if (!eyes_pull_mono(
                g_original,
                page->width,
                page->height,
                g_bits[page_index],
                sizeof(g_bits[page_index])
            )) {
            printf("FAIL: mono pull page %u\n", page->number);
            return 1;
        }

        if (!build_original_path(tag, page->number, path, sizeof(path)) ||
            !eyes_write_recon(path, page->width, page->height, 0, g_original)) {
            printf("FAIL: write original page %u\n", page->number);
            return 1;
        }

        printf(
            "collected page %u: %ux%u -> %s\n",
            page->number,
            page->width,
            page->height,
            path
        );
    }

    if (!build_document_path__xml_collect_h(tag, path, sizeof(path)) ||
        !eyes_xml_write_document(
            path,
            g_eyes_xml_document_pages,
            EYES_XML_PAGE_COUNT,
            g_bits
        )) {
        printf("FAIL: write document xml\n");
        return 1;
    }

    printf("wrote %s\n", path);
    return 0;
}
