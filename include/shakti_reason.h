#ifndef SHAKTI_REASON_H
#define SHAKTI_REASON_H
#include "shakti_types.h"
void shakti_reason_init(shakti_reason_state_t *state);
int shakti_reason_load(shakti_reason_state_t *state, const char *facts_path, const char *thesaurus_path, const char *evidence_path);
int shakti_reason_record_evidence(shakti_reason_state_t *state, const char *evidence_path, const shakti_tick_t *tick, const char *question, const char *answer, shakti_source_t source, int contradiction);
shakti_decision_t shakti_reason_answer(const shakti_reason_state_t *state, const char *question);
const char *shakti_source_name(shakti_source_t source);
#endif
