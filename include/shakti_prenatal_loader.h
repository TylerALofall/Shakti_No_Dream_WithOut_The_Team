#ifndef SHAKTI_PRENATAL_LOADER_H
#define SHAKTI_PRENATAL_LOADER_H

#include "shakti_app.h"

#define SHAKTI_PRENATAL_RATE 16000U
#define SHAKTI_PRENATAL_AUDIO_BYTES 500U
#define SHAKTI_PRENATAL_PULSE_BYTES 32000U
#define SHAKTI_PRENATAL_PROFILE_BYTES 8192U
#define SHAKTI_PRENATAL_TOTAL_BEATS UINT64_C(22982400)

/* Input preparation only. The owner supplies all phase, color and source
 * positions. No clock, training counter, media step, release or memory call
 * belongs to this module. Keep supplied storage alive under workflow leases. */
typedef struct {
    unsigned int width, height;
    unsigned int pulse_on_numerator, pulse_on_denominator;
    unsigned int voice_to_pulse_ratio;
    unsigned int monitor_enabled;
    unsigned int density[32]; /* 0..4 colored pixels per complete group of four */
} shakti_prenatal_config_t;

typedef struct {
    const unsigned char *samples; /* Little-endian signed 16-bit mono bytes. */
    size_t sample_count;
    unsigned int sample_rate;
} shakti_prenatal_wave_t;

void shakti_prenatal_defaults(shakti_prenatal_config_t *config);
/* A strict flat XML profile. All fields are required; unknown/duplicate fields
 * and out-of-capacity input refuse the complete profile. Output is unchanged
 * on failure. The native host may also supply the same struct directly. */
int shakti_prenatal_config_parse(const char *xml, size_t bytes,
    shakti_prenatal_config_t *config);
int shakti_prenatal_wave_parse(const unsigned char *file, size_t bytes,
    shakti_prenatal_wave_t *wave);
int shakti_prenatal_wave_load(const char *path, unsigned char *storage,
    size_t capacity, size_t *loaded_bytes, shakti_prenatal_wave_t *wave);

/* Produce one reusable input atom from the first complete source second.
 * Gate at the requested rational fraction, rounded to the nearest PCM sample.
 * Scale its peak relative to the supplied reference voice; retain originals.
 * The returned atom is input to the ordinary binary breakdown, never a
 * reconstructed/perceptual output. No speaker is opened. */
int shakti_prenatal_pulse(const shakti_prenatal_wave_t *source,
    const shakti_prenatal_wave_t *reference_voice,
    const shakti_prenatal_config_t *config, unsigned char *input,
    size_t capacity, size_t *active_samples);

/* Load one requested 1/64-second input chunk. Exact recorded samples are
 * retained; a final partial chunk is zero-padded. The workflow owns offsets,
 * continuation to further beats, repetition and the 64:32:1 relationship. */
int shakti_prenatal_audio(const shakti_prenatal_wave_t *wave,
    size_t sample_offset, unsigned char *input, size_t capacity,
    size_t *recorded_samples);

/* Exact RGB byte values preserve the old hex palette and brightness. Density
 * changes which pixels are lit while preserving each lit pixel's RGB bytes.
 * This fills a full native frame, including its black pixels. */
int shakti_prenatal_pixels_rgb(const shakti_prenatal_config_t *config,
    const unsigned char rgb[3], unsigned int density,
    unsigned char *input, size_t capacity, size_t *bytes);
/* Eight-corner shorthand: bit0=red, bit1=green, bit2=blue. */
int shakti_prenatal_pixels(const shakti_prenatal_config_t *config,
    unsigned int rgb_bits, unsigned int density,
    unsigned char *input, size_t capacity, size_t *bytes);
int shakti_prenatal_density(const shakti_prenatal_config_t *config,
    unsigned int hearing_slot, unsigned int *density);

/* The only model-facing call. Pass a complete caller-owned ordinary media
 * descriptor. An empty prenatal context is mandatory. Returns the actual
 * shakti_app_queue_frame() admission result and advances nothing downstream. */
int shakti_prenatal_deposit(shakti_runtime_t *runtime,
    const shakti_media_frame_t *frame);

#endif
