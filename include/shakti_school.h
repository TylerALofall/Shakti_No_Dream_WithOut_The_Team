#ifndef SHAKTI_SCHOOL_H
#define SHAKTI_SCHOOL_H
#include "shakti_types.h"
void shakti_school_init(shakti_school_state_t *state);
int shakti_school_load(shakti_school_state_t *state, const char *path);
int shakti_school_set_pass(shakti_school_state_t *state, const char *path, const shakti_tick_t *tick, unsigned int pass);
int shakti_school_record_trial(shakti_school_state_t *state, const char *path, const shakti_tick_t *tick, const char *symbol, int correct);
int shakti_school_run_drill(shakti_runtime_t *runtime, const char *symbol);
int shakti_school_submit_trial(shakti_runtime_t *runtime, const char *answer);
int shakti_school_show_written_text_pixels(const char *text);
int shakti_school_draft_snapshots(shakti_runtime_t *runtime, const char *text);
#endif
