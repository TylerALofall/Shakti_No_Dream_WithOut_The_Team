#ifndef SHAKTI_MEMORY_H
#define SHAKTI_MEMORY_H
#include "shakti_types.h"
void shakti_memory_init(shakti_memory_state_t *memory);
int shakti_memory_load_always(shakti_memory_state_t *memory, const char *goal_path, const char *notebook_path, const char *menu_path, const char *long_term_path);
int shakti_memory_reload_notebook(shakti_memory_state_t *memory, const char *notebook_path);
int shakti_memory_reload_goal(shakti_memory_state_t *memory, const char *goal_path);
int shakti_memory_remember(shakti_memory_state_t *memory, const shakti_tick_t *tick, const char *text);
void shakti_memory_print_status(const shakti_memory_state_t *memory);
int shakti_memory_recall_text_file(const char *path, const char *term, unsigned int maximum_matches);
void shakti_memory_bind_eden(shakti_memory_state_t *memory, const shakti_reason_state_t *eden);
int shakti_memory_store(shakti_memory_state_t *memory, const shakti_tick_t *tick, const char *id, const char *tags, const char *text, int permanent);
/* 1 complete item copied, 0 absent, -1 storage/integrity error. */
int shakti_memory_fetch(shakti_memory_state_t *memory, const char *id,
    shakti_memory_item_t *out);
int shakti_memory_request(shakti_memory_state_t *memory, const char *query);
int shakti_memory_step(shakti_memory_state_t *memory, uint64_t slot);
int shakti_memory_activate(shakti_memory_state_t *memory, const char *id);
void shakti_memory_close(shakti_memory_state_t *memory);
#endif
