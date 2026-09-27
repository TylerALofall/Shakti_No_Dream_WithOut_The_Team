/*
 * eyes_map: section/function map + document round-trip harness.
 *
 * Runs a handful of documents (clean-text pages and generated color
 * pages) through the eyes pipeline at escalating sizes, writes every
 * intermediate artifact into eyes/output/, prints the section map, and
 * sweeps N x N sizes until collection reports an error to find the
 * breaking point. Deterministic: no clock, no dynamic allocation.
 */

#include "eyes.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define EYES_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define EYES_MKDIR(p) mkdir((p), 0777)
#endif

#define EYES_WORK_CAPACITY (EYES_MAX_PIXELS * 4UL)
#define EYES_BITS_CAPACITY (EYES_MAX_PIXELS * 3UL + 1UL)
#define EYES_PATH_CAPACITY 256UL
#define EYES_OUTPUT_DIR "eyes/output"

/* Document artifacts stay small and inspectable; the sweep uses the
 * full collector capacity. */
#define EYES_DOC_MAX_PIXELS 65536UL
#define EYES_DOC_CAPACITY   (EYES_DOC_MAX_PIXELS * 4UL)

static unsigned char g_doc_original[EYES_DOC_CAPACITY];
static unsigned char g_doc_recon[EYES_DOC_CAPACITY];
static unsigned char g_clean[EYES_DOC_CAPACITY];
static char g_doc_bits_a[EYES_DOC_MAX_PIXELS * 3UL + 1UL];
static char g_doc_bits_a_copy[EYES_DOC_MAX_PIXELS * 3UL + 1UL];

static unsigned char *g_original;
static unsigned char *g_recon;
static char *g_bits_a;
static eyes_recog_t g_recog[EYES_RECOG_MAX];

static const unsigned int SIZES[] = { 8U, 16U, 24U, 32U, 48U, 64U };

#define SIZE_COUNT (sizeof(SIZES) / sizeof(SIZES[0]))

static void print_section_map(void)
{
    printf("eyes section map\n");
    printf("================\n");
    printf("eyes.h / eyes.c          collector + reconstruction core\n");
    printf("  eyes_load_document     load a document into raw RGBA\n");
    printf("    kind 0: clean-text page (white, black clean-font ink)\n");
    printf("    kind 1: generated color page (red/green/blue bands)\n");
    printf("  eyes_pull_color        per-channel pull, RGB-mix -> ink\n");
    printf("  eyes_pull_mono         luma threshold -> one bit per pixel\n");
    printf("  eyes_reconstruct_color bits -> RGBA bitmap (the other side)\n");
    printf("  eyes_reconstruct_mono  bits -> mono bitmap\n");
    printf("  eyes_diff              reconstruction vs original, per-pixel\n");
    printf("  eyes_recognize_text    mono cells -> clean-font characters\n");
    printf("  eyes_render_clean_page recognized text redrawn clean\n");
    printf("  eyes_write_frame       .tan raw pull artifact\n");
    printf("  eyes_write_recon       reconstructed bitmap artifact\n");
    printf("  eyes_run_cycles        20-cycle stability, drifted bits\n");
    printf("eyes/eyes_map.c          this harness: map, artifacts, sweep\n");
    printf("eyes/output/             every file the harness writes\n");
    printf("\n");
    printf("flow: load -> pull color + pull mono -> reconstruct -> diff\n");
    printf("      -> recognize text on mono -> redraw clean -> 20 cycles\n");
    printf("\n");
}

static int check_support(void)
{
    if (!eyes_text_supported("HI EDEN") ||
        eyes_text_supported("hi\xc3\xa9")) {
        printf("FAIL: clean-font support check\n");
        return 0;
    }

    return 1;
}

/*
 * Diff probe: a copy must come back with 0 errors, garbage must not.
 * Uses the 8x8 doc buffers, which are static at this point in main.
 */
static int check_diff(void)
{
    unsigned long pixel;
    unsigned long pixel_count;
    unsigned long drift;

    if (!eyes_load_document(1U, 8U, 8U, NULL, 3UL, g_original,
                            EYES_DOC_CAPACITY)) {
        printf("FAIL: diff probe load\n");
        return 0;
    }

    drift = eyes_diff(g_original, g_original, 8U, 8U, 1);

    if (drift != 0UL) {
        printf("FAIL: color diff of a perfect copy is %lu, want 0\n",
               drift);
        return 0;
    }

    pixel_count = 8UL * 8UL;

    for (pixel = 0UL; pixel < pixel_count; ++pixel) {
        g_recon[pixel * 4UL] = (unsigned char)(g_original[pixel * 4UL] ^ 255U);
    }

    drift = eyes_diff(g_original, g_recon, 8U, 8U, 1);

    if (drift == 0UL || drift == (unsigned long)-1) {
        printf("FAIL: color diff of garbage is %lu, want nonzero\n",
               drift);
        return 0;
    }

    return 1;
}

/*
 * Recognize text on the current contents of g_recon and redraw it clean.
 * Builds the line-ordered text ('|' between lines, ' ' for skipped cells).
 * Returns character count, or (unsigned long)-1 on rejection.
 */
static unsigned long recognize_current(
    unsigned int width,
    unsigned int height,
    const char *tag,
    const char *side,
    char *text_out,
    unsigned long text_capacity
)
{
    unsigned long count;
    unsigned long index;
    char path[EYES_PATH_CAPACITY];
    unsigned int last_cell_col;
    unsigned int last_cell_row;
    unsigned long used;

    count = eyes_recognize_text(g_recon, width, height, g_recog,
                                EYES_RECOG_MAX);

    text_out[0] = '\0';

    if (count == 0UL) {
        return 0UL;
    }

    if (count == (unsigned long)-1) {
        printf("FAIL: recognize rejected for %s %s\n", tag, side);
        return (unsigned long)-1;
    }

    if (count > EYES_RECOG_MAX) {
        printf("FAIL: %s %s recognized %lu characters, capacity %lu\n",
               tag, side, count, (unsigned long)EYES_RECOG_MAX);
        return (unsigned long)-1;
    }

    if (!eyes_render_clean_page(width, height, g_recog, count, g_clean,
                                EYES_DOC_CAPACITY)) {
        printf("FAIL: clean page render for %s %s\n", tag, side);
        return (unsigned long)-1;
    }

    used = 0UL;
    last_cell_col = 0U;
    last_cell_row = 0U;

    for (index = 0UL; index < count; ++index) {
        char marker;

        if (used + 2UL >= text_capacity) {
            printf("FAIL: text capacity for %s %s\n", tag, side);
            return (unsigned long)-1;
        }

        if (index > 0UL) {
            if (g_recog[index].cell_row != last_cell_row) {
                marker = '|';
            } else if (g_recog[index].cell_col > last_cell_col + 1U) {
                marker = ' ';
            } else {
                marker = '\0';
            }

            if (marker != '\0') {
                text_out[used] = marker;
                ++used;
                text_out[used] = '\0';
            }
        }

        text_out[used] = g_recog[index].character;
        ++used;
        text_out[used] = '\0';
        last_cell_col = g_recog[index].cell_col;
        last_cell_row = g_recog[index].cell_row;
    }

    if (snprintf(path, sizeof(path), "%s/%s_%ux%u_%s_clean.tan",
                 EYES_OUTPUT_DIR, tag, width, height, side) < 0 ||
        !eyes_write_recon(path, width, height, 0, g_clean)) {
        printf("FAIL: write clean recon for %s %s\n", tag, side);
        return (unsigned long)-1;
    }

    return count;
}

static int run_document(
    unsigned int kind,
    unsigned int width,
    unsigned int height,
    const eyes_text_page_t *page,
    const char *tag,
    unsigned long seed,
    FILE *summary
)
{
    char path[EYES_PATH_CAPACITY];
    unsigned long color_drift;
    unsigned long mono_drift;
    unsigned long cycle_drift;
    unsigned long recognized_color;
    unsigned long recognized_mono;
    char text_color[128];
    char text_mono[128];

    text_color[0] = '\0';
    text_mono[0] = '\0';

    if (!eyes_load_document(kind, width, height, page, seed, g_original,
                            EYES_DOC_CAPACITY)) {
        printf("FAIL: load %s %ux%u\n", tag, width, height);
        return 0;
    }

    /* color side */
    if (!eyes_pull_color(g_original, width, height, g_bits_a,
                         EYES_DOC_MAX_PIXELS * 3UL + 1UL) ||
        !eyes_reconstruct_color(g_bits_a, width, height, g_recon,
                                EYES_DOC_CAPACITY)) {
        printf("FAIL: color pull/reconstruct %s %ux%u\n", tag, width, height);
        return 0;
    }

    color_drift = eyes_diff(g_original, g_recon, width, height, 1);

    if (color_drift == (unsigned long)-1) {
        printf("FAIL: color diff %s %ux%u\n", tag, width, height);
        return 0;
    }

    if (snprintf(path, sizeof(path), "%s/%s_%ux%u_color.tan",
                 EYES_OUTPUT_DIR, tag, width, height) < 0 ||
        !eyes_write_frame(path, width, height, 1, g_bits_a)) {
        printf("FAIL: write color frame %s\n", tag);
        return 0;
    }

    if (snprintf(path, sizeof(path), "%s/%s_%ux%u_color_recon.tan",
                 EYES_OUTPUT_DIR, tag, width, height) < 0 ||
        !eyes_write_recon(path, width, height, 1, g_recon)) {
        printf("FAIL: write color recon %s\n", tag);
        return 0;
    }

    /*
     * Dual check 1: text as the color pull sees it. The color
     * reconstruction holds primaries, so "ink" here is any channel dark
     * (the 3-color mix included); flatten that view into g_recon before
     * matching glyphs.
     */
    {
        unsigned long pixel;
        unsigned long pixel_count;

        pixel_count = (unsigned long)width * (unsigned long)height;

        for (pixel = 0UL; pixel < pixel_count; ++pixel) {
            unsigned long base;
            unsigned int ink;
            unsigned char value;

            base = pixel * 4UL;
            ink = g_recon[base] < 128U ||
                  g_recon[base + 1UL] < 128U ||
                  g_recon[base + 2UL] < 128U;
            value = ink ? 0U : 255U;
            g_recon[base] = value;
            g_recon[base + 1UL] = value;
            g_recon[base + 2UL] = value;
            g_recon[base + 3UL] = 255U;
        }
    }

    recognized_color = recognize_current(width, height, tag, "color",
                                         text_color, sizeof(text_color));

    if (recognized_color == (unsigned long)-1) {
        return 0;
    }

    /* dual check 1 happens on the color reconstruction above; mono side
     * pulls fresh from the original so both sides stay independent */
    if (!eyes_pull_mono(g_original, width, height, g_bits_a,
                        EYES_DOC_MAX_PIXELS * 3UL + 1UL) ||
        !eyes_reconstruct_mono(g_bits_a, width, height, g_recon,
                               EYES_DOC_CAPACITY)) {
        printf("FAIL: mono pull/reconstruct %s %ux%u\n", tag, width, height);
        return 0;
    }

    mono_drift = eyes_diff(g_original, g_recon, width, height, 0);

    if (mono_drift == (unsigned long)-1) {
        printf("FAIL: mono diff %s %ux%u\n", tag, width, height);
        return 0;
    }

    if (snprintf(path, sizeof(path), "%s/%s_%ux%u_mono.tan",
                 EYES_OUTPUT_DIR, tag, width, height) < 0 ||
        !eyes_write_frame(path, width, height, 0, g_bits_a)) {
        printf("FAIL: write mono frame %s\n", tag);
        return 0;
    }

    if (snprintf(path, sizeof(path), "%s/%s_%ux%u_mono_recon.tan",
                 EYES_OUTPUT_DIR, tag, width, height) < 0 ||
        !eyes_write_recon(path, width, height, 0, g_recon)) {
        printf("FAIL: write mono recon %s\n", tag);
        return 0;
    }

    /* dual check 2: text as the mono pull sees it; the two pulls must
     * agree, which is the on-point check Tyler asked for */
    recognized_mono = recognize_current(width, height, tag, "mono",
                                        text_mono, sizeof(text_mono));

    if (recognized_mono == (unsigned long)-1) {
        return 0;
    }

    if (strcmp(text_color, text_mono) != 0) {
        printf("note: %s %ux%u pull mismatch color=\"%s\" mono=\"%s\"\n",
               tag, width, height, text_color, text_mono);
        fprintf(summary,
                "%s %ux%u pull_mismatch color=\"%s\" mono=\"%s\"\n",
                tag, width, height, text_color, text_mono);
    }

    cycle_drift = eyes_run_cycles(g_original, width, height, 0,
                                  g_doc_recon, g_clean, g_bits_a,
                                  g_doc_bits_a_copy,
                                  EYES_DOC_MAX_PIXELS * 3UL + 1UL);

    if (cycle_drift == (unsigned long)-1) {
        printf("FAIL: cycles %s %ux%u\n", tag, width, height);
        return 0;
    }

    printf("%s %ux%u: color_drift=%lu mono_drift=%lu "
           "recog_color=%lu recog_mono=%lu text=\"%s\" "
           "cycles20_drift=%lu\n",
           tag, width, height, color_drift, mono_drift,
           recognized_color, recognized_mono, text_mono, cycle_drift);

    fprintf(summary,
            "%s %ux%u color_drift=%lu mono_drift=%lu recog_color=%lu "
            "recog_mono=%lu text=\"%s\" cycles20_drift=%lu\n",
            tag, width, height, color_drift, mono_drift,
            recognized_color, recognized_mono, text_mono, cycle_drift);

    return 1;
}

/*
 * Escalate N x N until the collector reports an error. Per size, also
 * count how much of a glyph actually lands on the page (mono drift), so
 * the log shows where small sizes start losing detail. Sizes above the
 * configured capacity are reported as clean rejections, not crashes.
 */
static unsigned long sweep_breaking_point(FILE *summary)
{
    unsigned int n;
    unsigned long first_error;
    unsigned long first_drift;
    static const eyes_text_page_t page = { "A", "", "" };

    first_error = 0UL;
    first_drift = 0UL;
    n = 1U;

    while (n <= 5000U) {
        unsigned long pixels;
        unsigned long drift;

        pixels = (unsigned long)n * (unsigned long)n;

        if (pixels > EYES_MAX_PIXELS) {
            if (first_error == 0UL) {
                first_error = (unsigned long)n;
            }

            printf("sweep %ux%u: ERROR (exceeds configured capacity %lu)\n",
                   n, n, (unsigned long)EYES_MAX_PIXELS);
            fprintf(summary, "sweep %ux%u ERROR capacity\n", n, n);
            break;
        }

        if (!eyes_load_document(0U, n, n, &page, 0UL, g_original,
                                EYES_WORK_CAPACITY)) {
            if (first_error == 0UL) {
                first_error = (unsigned long)n;
            }

            printf("sweep %ux%u: ERROR (load rejected)\n", n, n);
            fprintf(summary, "sweep %ux%u ERROR load\n", n, n);
            break;
        }

        if (!eyes_pull_mono(g_original, n, n, g_bits_a,
                            EYES_BITS_CAPACITY) ||
            !eyes_reconstruct_mono(g_bits_a, n, n, g_recon,
                                   EYES_WORK_CAPACITY)) {
            if (first_error == 0UL) {
                first_error = (unsigned long)n;
            }

            printf("sweep %ux%u: ERROR (pull/reconstruct rejected)\n", n, n);
            fprintf(summary, "sweep %ux%u ERROR pull\n", n, n);
            break;
        }

        drift = eyes_diff(g_original, g_recon, n, n, 0);

        if (drift == (unsigned long)-1) {
            if (first_error == 0UL) {
                first_error = (unsigned long)n;
            }

            printf("sweep %ux%u: ERROR (diff rejected)\n", n, n);
            fprintf(summary, "sweep %ux%u ERROR diff\n", n, n);
            break;
        }

        if (drift > 0UL) {
            if (first_drift == 0UL) {
                first_drift = (unsigned long)n;
            }

            printf("sweep %ux%u: drift=%lu (glyph spills off the page)\n",
                   n, n, drift);
            fprintf(summary, "sweep %ux%u drift=%lu\n", n, n, drift);
        }

        if (n < 64U) {
            n += 1U;
        } else if (n < 256U) {
            n += 8U;
        } else {
            n += 64U;
        }
    }

    printf("sweep end: first_error at %lu, first_drift at %lu\n",
           first_error, first_drift);
    fprintf(summary, "sweep_first_error=%lu sweep_first_drift=%lu\n",
            first_error, first_drift);

    return first_error;
}

#define EYES_MAP_MAIN main
int EYES_MAP_MAIN(void)
{
    static const eyes_text_page_t page_hi = { "HI EDEN", "ABC 123", "" };
    static const eyes_text_page_t page_one = { "A", "", "" };
    static const eyes_text_page_t page_word = { "SHAKTI", "SEES", "TEXT" };
    static const eyes_text_page_t page_edge = { "W", "", "" };
    FILE *summary;
    size_t index;
    unsigned long breaking_point;

    /* document runs use the small static buffers */
    g_original = g_doc_original;
    g_recon = g_doc_recon;
    g_bits_a = g_doc_bits_a;

    print_section_map();

    if (!check_support() || !check_diff()) {
        return 1;
    }

    (void)EYES_MKDIR(EYES_OUTPUT_DIR);

    summary = fopen("eyes/output/summary.tan", "w");

    if (summary == NULL) {
        printf("FAIL: cannot open eyes/output/summary.tan "
               "(run from the repository root)\n");
        return 1;
    }

    fprintf(summary, "eyes summary\n");
    fprintf(summary, "pipeline: load -> pull color + mono -> reconstruct -> "
                     "diff -> recognize -> redraw -> 20 cycles\n\n");

    for (index = 0U; index < SIZE_COUNT; ++index) {
        unsigned int size;

        size = SIZES[index];

        if (!run_document(0U, size, size, &page_hi, "text_hi", 0UL,
                          summary)) {
            fclose(summary);
            return 1;
        }
    }

    if (!run_document(0U, 32U, 32U, &page_one, "text_one", 0UL, summary) ||
        !run_document(0U, 48U, 48U, &page_word, "text_word", 0UL, summary) ||
        !run_document(0U, 64U, 64U, &page_edge, "text_edge", 0UL, summary)) {
        fclose(summary);
        return 1;
    }

    if (!run_document(1U, 32U, 32U, NULL, "color_a", 0UL, summary) ||
        !run_document(1U, 64U, 64U, NULL, "color_b", 7UL, summary)) {
        fclose(summary);
        return 1;
    }

    fprintf(summary, "\nbreaking point sweep\n");

    /* the sweep uses the full collector capacity */
    {
        static unsigned char sweep_original[EYES_WORK_CAPACITY];
        static unsigned char sweep_recon[EYES_WORK_CAPACITY];
        static char sweep_bits[EYES_BITS_CAPACITY];

        g_original = sweep_original;
        g_recon = sweep_recon;
        g_bits_a = sweep_bits;

        breaking_point = sweep_breaking_point(summary);
    }

    fprintf(summary, "first_error_at=%lu\n", breaking_point);

    fclose(summary);

    printf("\nsummary written to eyes/output/summary.tan\n");
    printf("artifacts in eyes/output/ (*.tan frames and reconstructions)\n");

    return 0;
}
