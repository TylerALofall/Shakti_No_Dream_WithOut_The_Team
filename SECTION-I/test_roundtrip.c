#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "eyes.h"

#define ROUNDTRIP_PASSES 20U
#define ROUNDTRIP_WIDTH 32U
#define ROUNDTRIP_HEIGHT 16U

static unsigned long recognize_text(
    const unsigned char *mono_rgba,
    unsigned int width,
    unsigned int height,
    eyes_recog_t *recog,
    unsigned long recog_capacity,
    char *text,
    unsigned long text_capacity
)
{
    unsigned long count;
    unsigned long index;
    unsigned long used;
    unsigned int last_col;
    unsigned int last_row;

    count = eyes_recognize_text(mono_rgba, width, height, recog, recog_capacity);
    assert(count > 0UL);

    text[0] = '\0';
    used = 0UL;
    last_col = 0U;
    last_row = 0U;

    for (index = 0UL; index < count; ++index) {
        if (index > 0UL) {
            if (recog[index].cell_row != last_row) {
                assert(used + 1UL < text_capacity);
                text[used++] = '|';
            } else if (recog[index].cell_col > last_col + 1U) {
                assert(used + 1UL < text_capacity);
                text[used++] = ' ';
            }
        }

        assert(used + 1UL < text_capacity);
        text[used++] = recog[index].character;
        text[used] = '\0';
        last_col = recog[index].cell_col;
        last_row = recog[index].cell_row;
    }

    return count;
}

#define TEST_ROUNDTRIP_MAIN main
int TEST_ROUNDTRIP_MAIN(void)
{
    static const eyes_text_page_t page = { "HI", "", "" };
    static unsigned char original[ROUNDTRIP_WIDTH * ROUNDTRIP_HEIGHT * 4U];
    static unsigned char reconstructed[ROUNDTRIP_WIDTH * ROUNDTRIP_HEIGHT * 4U];
    static unsigned char clean[ROUNDTRIP_WIDTH * ROUNDTRIP_HEIGHT * 4U];
    static char current_bits[ROUNDTRIP_WIDTH * ROUNDTRIP_HEIGHT + 1U];
    static char next_bits[ROUNDTRIP_WIDTH * ROUNDTRIP_HEIGHT + 1U];
    static eyes_recog_t recognized[EYES_RECOG_MAX];
    char reference_text[32];
    char current_text[32];
    unsigned int pass;
    unsigned long total_drift;
    unsigned long recognized_count;

    assert(eyes_load_document(
        0U,
        ROUNDTRIP_WIDTH,
        ROUNDTRIP_HEIGHT,
        &page,
        0UL,
        original,
        sizeof(original)
    ));
    assert(eyes_pull_mono(
        original,
        ROUNDTRIP_WIDTH,
        ROUNDTRIP_HEIGHT,
        current_bits,
        sizeof(current_bits)
    ));

    assert(eyes_reconstruct_mono(
        current_bits,
        ROUNDTRIP_WIDTH,
        ROUNDTRIP_HEIGHT,
        reconstructed,
        sizeof(reconstructed)
    ));
    assert(eyes_diff(
        original,
        reconstructed,
        ROUNDTRIP_WIDTH,
        ROUNDTRIP_HEIGHT,
        0
    ) == 0UL);

    recognized_count = recognize_text(
        reconstructed,
        ROUNDTRIP_WIDTH,
        ROUNDTRIP_HEIGHT,
        recognized,
        EYES_RECOG_MAX,
        reference_text,
        sizeof(reference_text)
    );
    assert(recognized_count > 0UL);

    total_drift = 0UL;

    printf("pass drift cumulative text\n");
    fflush(stdout);

    for (pass = 1U; pass <= ROUNDTRIP_PASSES; ++pass) {
        unsigned long drift;

        assert(eyes_reconstruct_mono(
            current_bits,
            ROUNDTRIP_WIDTH,
            ROUNDTRIP_HEIGHT,
            reconstructed,
            sizeof(reconstructed)
        ));
        recognized_count = recognize_text(
            reconstructed,
            ROUNDTRIP_WIDTH,
            ROUNDTRIP_HEIGHT,
            recognized,
            EYES_RECOG_MAX,
            current_text,
            sizeof(current_text)
        );
        assert(recognized_count > 0UL);
        assert(strcmp(current_text, reference_text) == 0);
        assert(eyes_render_clean_page(
            ROUNDTRIP_WIDTH,
            ROUNDTRIP_HEIGHT,
            recognized,
            recognized_count,
            clean,
            sizeof(clean)
        ));
        drift = eyes_diff(
            original,
            clean,
            ROUNDTRIP_WIDTH,
            ROUNDTRIP_HEIGHT,
            0
        );
        assert(drift != (unsigned long)-1);
        assert(eyes_pull_mono(
            clean,
            ROUNDTRIP_WIDTH,
            ROUNDTRIP_HEIGHT,
            next_bits,
            sizeof(next_bits)
        ));

        total_drift += drift;
        printf("%4u %5lu %10lu %s\n", pass, drift, total_drift, current_text);
        fflush(stdout);

        strcpy(current_bits, next_bits);
    }

    assert(total_drift == 0UL);
    printf("round-trip stable: 0 pixels lost over %u passes\n",
        ROUNDTRIP_PASSES);

    return 0;
}
