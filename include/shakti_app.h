#ifndef SHAKTI_APP_H
#define SHAKTI_APP_H
#include "shakti_types.h"
#include "shakti_retention.h"
/* The native controller owns its timer and serialized calls. NULL is a quiet
 * cycle. No input read or outward reply is required by either core entry point.
 * Call at the shared heartbeat rate using shakti_loop_delay_us(). */
int shakti_app_initialize(shakti_runtime_t *runtime);
int shakti_app_cycle(shakti_runtime_t *runtime, const char *command);
typedef struct {
    const shakti_sensory_group_t *sensory;
    const shakti_reason_state_t *eden;
    const shakti_memory_item_t *working;
    size_t working_count;
    const shakti_memory_item_t *short_term;
    size_t short_term_count;
    size_t pending_reviews;
    const shakti_retention_state_t *retained_media;
    const char *thought;
    shakti_tick_t tick;
} shakti_convergence_view_t;
/* Queue through this receipted MCP entry point, not by writing state fields.
 * A view is borrowed until the next serialized cycle. Frame buffers remain
 * owned until shakti_media_release() or explicit cancellation. */
int shakti_app_queue_frame(shakti_runtime_t *runtime, const shakti_media_frame_t *frame);
int shakti_app_convergence_view(const shakti_runtime_t *runtime, shakti_convergence_view_t *view);
/* Release native short-term video only after a five-review capsule explicitly
 * references this source and covers its first through last timeline point. */
int shakti_app_release_artifact(shakti_runtime_t *runtime, const char *capsule_id);
int shakti_app_transcribe(shakti_runtime_t *runtime, uint64_t start64,
    uint64_t end64, const char *actual_text);
#endif
