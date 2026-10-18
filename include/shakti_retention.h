#ifndef SHAKTI_RETENTION_H
#define SHAKTI_RETENTION_H
#include "shakti_media.h"
#define SHAKTI_ARENA_BYTES (512UL * 1024UL * 1024UL)
#define SHAKTI_RETENTION_EDGE_BYTES (32UL * 1024UL * 1024UL)
#define SHAKTI_RETENTION_HISTORY_BYTES (SHAKTI_ARENA_BYTES - 2UL * SHAKTI_RETENTION_EDGE_BYTES)
#define SHAKTI_RETENTION_WINDOWS 262144U
#define SHAKTI_TRANSCRIPT_BYTES (8UL * 1024UL * 1024UL)
#define SHAKTI_TRANSCRIPT_SEGMENTS 8192U
#define SHAKTI_RETENTION_PATH "data/learned/sensory_pending_v1.bin"
typedef struct {
    uint64_t epoch, start64, end64;
    unsigned int event_frame;
    size_t offset, bytes;
} shakti_transcript_segment_t;
typedef struct {
    unsigned int width, height, channels, sample_bytes, sample_rate, codec;
    size_t raw_bytes, encoded_bytes, payload_offset;
    uint64_t hash;
} shakti_retained_lane_t;
typedef struct {
    shakti_retained_lane_t lanes[3];
    uint64_t epoch, position64, ordinal, end_position64;
    unsigned int event_frame, mask;
    size_t bytes, offset;
} shakti_retained_window_t;
typedef struct {
    char source_id[SHAKTI_MEDIA_ID_BYTES];
    uint64_t first_position64, stride, windows_seen;
    size_t bytes_used, history_budget, count;
    unsigned char active, fault, blocked;
    size_t transcript_count, transcript_bytes;
    shakti_retained_window_t latest;
    shakti_retained_window_t windows[SHAKTI_RETENTION_WINDOWS];
    shakti_transcript_segment_t transcripts[SHAKTI_TRANSCRIPT_SEGMENTS];
} shakti_retention_state_t;
/* One serialized owner, one active source at a time. The 512 MiB arena belongs
 * to native C, not an outside process. 448 MiB history + two 32 MiB edge/work
 * regions. Full sensory reconstruction happens BEFORE retention sampling.
 * The native bounded recovery journal is short-term storage, never library.
 * No speech recognition or semantic categorization is fabricated here. */
int shakti_retention_restore(void);
const shakti_retention_state_t *shakti_retention_view(void);
int shakti_retention_accepts(const char *source_id);
int shakti_retention_capture(const shakti_sensory_group_t *group,
    uint64_t epoch, unsigned int event_frame);
/* SIZE_MAX selects latest. Decode whole lane into caller-owned fixed storage.
 * Recalled pixels must be resubmitted to the normal eyes/binary path. */
int shakti_retention_read(size_t index, unsigned int lane, unsigned char *out, size_t capacity);
/* Budget can be reduced for a synthetic RUN; reserved arena size stays 512 MiB. */
int shakti_retention_budget(size_t bytes);
/* Actual native ASR output only. An empty string records confirmed silence.
 * Until a range is transcribed, its PCM is pinned; exhausted space holds
 * intake instead of losing dialogue. Text segments survive video thinning. */
int shakti_retention_transcribe(uint64_t start64, uint64_t end64, const char *text,
    uint64_t epoch, unsigned int event_frame);
const char *shakti_retention_transcript(size_t index, size_t *bytes);
int shakti_retention_audio_pending(void);
/* Core entry for the app's checked five-reflection release route. */
int shakti_retention_release(void);
#endif
