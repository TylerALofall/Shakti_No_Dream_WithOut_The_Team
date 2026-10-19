#ifndef SHAKTI_PRENATAL_FLOW_H
#define SHAKTI_PRENATAL_FLOW_H
#include "shakti_app.h"

#define SHAKTI_PRENATAL_SOUND_BEAT UINT64_C(11491201)
#define SHAKTI_PRENATAL_LIGHT_BEAT UINT64_C(15120001)
#define SHAKTI_PRENATAL_END_BEAT UINT64_C(22982400)
#define SHAKTI_PRENATAL_OPENING_BEATS 16U
#define SHAKTI_PRENATAL_CHECKPOINT "data/control/prenatal_loader_v2.bin"
#define SHAKTI_PRENATAL_FRAME_BYTES (1920U * 1080U * 3U)

enum {
    SHAKTI_PRENATAL_OFF, SHAKTI_PRENATAL_ARMED, SHAKTI_PRENATAL_RUNNING,
    SHAKTI_PRENATAL_PAUSING, SHAKTI_PRENATAL_PAUSED, SHAKTI_PRENATAL_HELD,
    SHAKTI_PRENATAL_DELIVERED
};

typedef struct {
    unsigned int phase; /* 0 heartbeat, 1 founder sound, 2 full-frame color */
    unsigned int color; /* Host bookkeeping only: eight palette indices. */
    uint64_t phase_beat;
    size_t voice_sample;
} shakti_prenatal_plan_t;

typedef struct {
    unsigned int state, requested_rate, effective_rate, monitor_enabled;
    unsigned int window, phase, color, synthetic_fixture;
    uint64_t completed_beats, run_serial, replays, corpus_hash;
    uint64_t acknowledged_windows;
    size_t reserved_bytes;
    char source_id[SHAKTI_MEDIA_ID_BYTES];
    char hold_reason[128];
} shakti_prenatal_status_t;

/* Pure timeline lookup. It grants no training credit and advances nothing. */
int shakti_prenatal_plan(uint64_t beat, shakti_prenatal_plan_t *plan);
/* One serialized owner. Start requires an empty media queue and no active
 * episodic source. All nine local WAVs/profile must fit and validate first.
 * The source corpus stays resident; repeated exposure is not raw movie storage.
 * No recognition, reflection answers or mastery are fabricated by this loader. */
int shakti_prenatal_start(shakti_runtime_t *runtime, const char *asset_directory,
    const char *profile_path);
int shakti_prenatal_pause(shakti_runtime_t *runtime);
int shakti_prenatal_resume(shakti_runtime_t *runtime);
int shakti_prenatal_stop(shakti_runtime_t *runtime);
int shakti_prenatal_status(const shakti_runtime_t *runtime, shakti_prenatal_status_t *status);
void shakti_prenatal_print_status(const shakti_runtime_t *runtime);
int shakti_prenatal_command(shakti_runtime_t *runtime, const char *arguments);

/* Runtime integration. A returned sensory view keeps its ordinary lease until
 * the next serialized cycle. Only this loader's private descriptors/buffers
 * qualify for resident-corpus retention. Other media retains its normal arena.
 * Hearing is 16 kHz PCM16 with channel 0 founder voice, channel 1 heart;
 * the two temporal lanes remain consecutive 1/64-second microclips. No mixing. */
int shakti_prenatal_restore(shakti_runtime_t *runtime);
void shakti_prenatal_before_cycle(shakti_runtime_t *runtime, unsigned int slot);
void shakti_prenatal_feed(shakti_runtime_t *runtime, unsigned int slot);
int shakti_prenatal_gate(const shakti_runtime_t *runtime, const shakti_media_frame_t *frame);
int shakti_prenatal_owns(const shakti_runtime_t *runtime, const shakti_media_group_t *group);
int shakti_prenatal_published(shakti_runtime_t *runtime);
void shakti_prenatal_cycle_result(shakti_runtime_t *runtime, int success);
unsigned int shakti_prenatal_rate_requested(shakti_loop_state_t *loop, unsigned int rate);

#ifdef SHAKTI_PRENATAL_RUN_HOOKS
/* Synthetic boundary fixtures only. A marked checkpoint is rejected by a
 * normal build. This entry point is absent from production and the MCP menu. */
int shakti_prenatal_run_position(shakti_runtime_t *runtime, uint64_t completed_beats);
#endif
#endif
