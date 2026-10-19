#ifndef SHAKTI_SENSES_H
#define SHAKTI_SENSES_H
#include "shakti_app.h"

#define SHAKTI_OCR_TEXT_BYTES 8192U
#define SHAKTI_OCR_CHARACTERS 4096U
typedef struct {
    unsigned int width, height;
    const unsigned char *rgb; /* RGB8, row-major, tightly packed, top row first. */
    size_t bytes;
} shakti_rgb_image_t;
typedef struct {
    const unsigned char *pcm; /* Signed little-endian 16-bit interleaved PCM. */
    unsigned int rate, channels;
    size_t frames, bytes;
} shakti_pcm_wave_t;
typedef struct {
    unsigned char character;
    unsigned int x, y, width, height, row, column;
} shakti_ocr_character_t;
typedef struct {
    char text[SHAKTI_OCR_TEXT_BYTES];
    shakti_ocr_character_t characters[SHAKTI_OCR_CHARACTERS];
    size_t count, lines;
    uint64_t source_hash;
    unsigned int width, height;
} shakti_ocr_result_t;
typedef struct {
    unsigned char *visual[3]; /* reference, binary, rebuilt; decoded input is borrowed. */
    size_t visual_capacity;
    unsigned char *audio[2][4]; /* input, reference, binary, rebuilt for consecutive slices. */
    size_t audio_capacity;
} shakti_senses_storage_t;

/* Bounded decoded-file adapters. P6 RGB8 and P5 gray8 only; no hidden codec.
 * Conversions retain every input intensity. Outputs remain unchanged on refusal. */
int shakti_eyes_pnm(const unsigned char *file, size_t bytes,
    unsigned char *rgb, size_t capacity, shakti_rgb_image_t *image);
int shakti_ears_wave(const unsigned char *file, size_t bytes, shakti_pcm_wave_t *wave);
/* pcm=NULL and capacity=0 queries exact slice sizes without writing samples. */
int shakti_ears_slice(const shakti_pcm_wave_t *wave, uint64_t position64,
    unsigned char *pcm, size_t capacity, size_t *bytes, size_t *recorded_frames);

/* Exact, supplied-font, fixed-cell OCR on the full rebuilt RGB image.
 * Atlas is one row of equally sized cells, in alphabet order, including space.
 * Dimensions/cell grid are explicit; no resizing, nearest-match or guessing.
 * Unknown/ambiguous glyphs refuse the complete result. Refusal clears result.
 * This is not arbitrary-font, handwriting, PDF or natural-image recognition. */
int shakti_eyes_ocr(const shakti_rgb_image_t *image, const shakti_rgb_image_t *atlas,
    const char *alphabet, unsigned int cell_width, unsigned int cell_height,
    shakti_ocr_result_t *result);

/* One item/task id, window n: sight position 2n, audio positions 2n and 2n+1.
 * Queue through the receipted MCP entry. Prepare finishes reconstruction ahead
 * of time but publishes nothing. Drive normal app cycles to cross the heart gate.
 * Storage remains owned until the published view is released or cancelled.
 * Optional missing wave means sight-only, not manufactured silent evidence.
 * Uses MOMENT retention: no automatic raw movie history. A saved photo is an
 * explicit host action; a compact candidate still needs actual interpretation. */
int shakti_senses_prepare(shakti_runtime_t *runtime, const char *source_id,
    uint64_t window, const shakti_rgb_image_t *image, const shakti_pcm_wave_t *wave,
    shakti_senses_storage_t *storage);

/* Compact textual capsule candidate derived from the actual published view.
 * OCR must name that exact pixel hash. Thought is caller-supplied real output.
 * Metadata/solid-color/OCR are distinguished from uninterpreted media.
 * This creates no memory or reflection and never releases the source. */
int shakti_senses_describe(const shakti_sensory_group_t *group,
    const shakti_ocr_result_t *ocr, const char *thought, char *out, size_t capacity);
#endif
