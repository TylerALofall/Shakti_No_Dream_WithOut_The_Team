#ifndef SHAKTI_LOOP_H
#define SHAKTI_LOOP_H
#include <stdio.h>
#include "shakti_types.h"
void shakti_loop_init(shakti_loop_state_t *state);
int shakti_loop_load(shakti_loop_state_t *state, const char *goal_path, const char *notebook_path, const char *menu_path);
int shakti_loop_restore(shakti_runtime_t *runtime);
int shakti_loop_begin_cycle(shakti_runtime_t *runtime, shakti_tick_t *cycle_tick);
int shakti_loop_finish_cycle(shakti_runtime_t *runtime);
int shakti_loop_set_heartbeat(shakti_loop_state_t *state, unsigned int beats_per_second);
unsigned long shakti_loop_delay_us(shakti_loop_state_t *state);
int shakti_loop_note_tool(shakti_runtime_t *runtime, const char *tool, const char *arguments);
const int16_t *shakti_loop_heartbeat_audio(const shakti_loop_state_t *state, size_t *samples);
int shakti_loop_set_goal(shakti_runtime_t *runtime, const char *goal);
int shakti_loop_notebook(shakti_runtime_t *runtime, const char *entry);
void shakti_loop_menu_titles(const shakti_loop_state_t *state);
int shakti_loop_menu_section(const shakti_loop_state_t *state, const char *title);
int shakti_loop_record_message(shakti_runtime_t *runtime, const char *route, const char *text);
void shakti_loop_interrupt_tools(shakti_loop_state_t *state);
void shakti_loop_resume_tools(shakti_loop_state_t *state);
int shakti_loop_tools_available(const shakti_loop_state_t *state);
int shakti_loop_defer_reflection(shakti_loop_state_t *state);
const char *shakti_loop_reflection_question(unsigned int question);
void shakti_loop_set_reflection_provider(shakti_loop_state_t *state,
    shakti_reflection_provider_t provider, void *context);
int shakti_loop_answer_reflection(shakti_runtime_t *runtime,
    unsigned int question, const char *answer);
int shakti_loop_run_reflection(shakti_runtime_t *runtime, FILE *input, FILE *output);
void shakti_loop_print_reflection_questions(FILE *output);
void shakti_loop_print_status(const shakti_loop_state_t *state);
#endif
