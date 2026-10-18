#ifndef SHAKTI_MEDIA_H
#define SHAKTI_MEDIA_H
#include <stddef.h>
#include <stdint.h>

#define SHAKTI_MEDIA_GROUPS 4U
#define SHAKTI_MEDIA_ID_BYTES 64U
#define SHAKTI_MEDIA_CONTEXT_BYTES 256U
#define SHAKTI_MEDIA_SIGHT 1U
#define SHAKTI_MEDIA_AUDIO_PAIR 6U
#define SHAKTI_MEDIA_MOVIE 7U

/* The native controller deposits decoded pixels/PCM in caller-owned storage.
 * Queueing borrows these buffers until release/cancel. Take immediately makes
 * an independent reference and a complete binary processing copy.
 * Output is built solely by the binary decoder, never by copying the input.
 * No resizing, quantization, codec, allocation, or outward communication. */
typedef struct {
    char source_id[SHAKTI_MEDIA_ID_BYTES];
    char context[SHAKTI_MEDIA_CONTEXT_BYTES];
    uint64_t position64;
    unsigned int lane;                 /* 0 sight, 1 first audio, 2 second audio */
    unsigned int required_mask;        /* 1 picture/page, 6 sound, 7 movie */
    unsigned int width, height, channels, sample_bytes, sample_rate;
    const unsigned char *input;
    unsigned char *reference;
    unsigned char *binary;
    unsigned char *rebuilt;
    size_t bytes, reference_capacity, binary_capacity, rebuilt_capacity;
    uint64_t expected_hash;            /* 0: compare to the held reference only */
    uint64_t validated_hash;
} shakti_media_frame_t;

typedef struct {
    shakti_media_frame_t parts[3];
    uint64_t order;
    unsigned int queued_mask, checked_mask, required_mask;
    unsigned char in_use, taken, held;
} shakti_media_group_t;

typedef struct {
    char source_id[SHAKTI_MEDIA_ID_BYTES];
    char context[SHAKTI_MEDIA_CONTEXT_BYTES];
    uint64_t position64;
    unsigned int lane, width, height, channels, sample_bytes, sample_rate;
    const unsigned char *data; /* Actual rebuilt bytes in the shared arena. */
    size_t bytes;
    uint64_t validated_hash;
} shakti_sensory_frame_t;
typedef struct {
    shakti_sensory_frame_t parts[3];
    unsigned int checked_mask;
} shakti_sensory_group_t;

typedef struct {
    shakti_media_group_t pending[SHAKTI_MEDIA_GROUPS];
    shakti_media_group_t current;
    shakti_sensory_group_t sensory;
    uint64_t next_order, submitted, validated_parts, published, rejected;
    unsigned int active;
    unsigned char current_ready;
} shakti_media_state_t;

size_t shakti_binary_capacity(size_t bytes);
int shakti_binary_encode(const unsigned char *input, size_t bytes, unsigned char *binary, size_t capacity);
int shakti_binary_rebuild(const unsigned char *binary, size_t bytes, unsigned char *out, size_t capacity);
uint64_t shakti_media_hash(const unsigned char *bytes, size_t size);
void shakti_media_init(shakti_media_state_t *state);
int shakti_media_queue(shakti_media_state_t *state, const shakti_media_frame_t *frame);
int shakti_media_take(shakti_media_state_t *state);
/* -1 held bad item, 1 continuing/waiting, 2 complete synchronized publication. */
int shakti_media_step(shakti_media_state_t *state, unsigned int heartbeat_slot);
/* Intake diagnostics only. Perception consumers use perception(), which
 * exposes only the reconstructed data shared with convergence. */
const shakti_sensory_group_t *shakti_media_perception(const shakti_media_state_t *state);
const shakti_media_group_t *shakti_media_current(const shakti_media_state_t *state);
void shakti_media_release(shakti_media_state_t *state);
int shakti_media_cancel(shakti_media_state_t *state, const char *source_id, uint64_t window);
#endif
