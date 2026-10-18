#ifndef SHAKTI_TYPES_H
#define SHAKTI_TYPES_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include "shakti_config.h"
#include "shakti_media.h"
typedef enum {
    SHAKTI_CHANNEL_TEXT = 0,
    SHAKTI_CHANNEL_WRITTEN_TEXT = 1,
    SHAKTI_CHANNEL_VISUAL_ART = 2,
    SHAKTI_CHANNEL_SOUND_ART = 3,
    SHAKTI_CHANNEL_COUNT = 4
} shakti_channel_t;
typedef enum {
    SHAKTI_SOURCE_CREATIVE = 0,
    SHAKTI_SOURCE_OBSERVED = 1,
    SHAKTI_SOURCE_CURRICULUM = 2,
    SHAKTI_SOURCE_VERIFIED = 3,
    SHAKTI_SOURCE_EDEN = 4
} shakti_source_t;
typedef enum {
    SHAKTI_DECISION_UNKNOWN = 0,
    SHAKTI_DECISION_CAUTIOUS = 1,
    SHAKTI_DECISION_KNOWN = 2
} shakti_decision_kind_t;
typedef struct { time_t epoch_seconds; unsigned int frame; } shakti_tick_t;
typedef struct { time_t last_epoch_seconds; unsigned int next_frame; } shakti_tick_clock_t;
typedef struct {
    char question[SHAKTI_QUESTION_CAPACITY];
    char answer[SHAKTI_ANSWER_CAPACITY];
    char relation[SHAKTI_RELATION_CAPACITY];
    char source_id[SHAKTI_SOURCE_CAPACITY];
} shakti_fact_t;
typedef struct { char variant[SHAKTI_TOKEN_CAPACITY]; char canonical[SHAKTI_TOKEN_CAPACITY]; } shakti_synonym_t;
typedef struct {
    unsigned char in_use;
    unsigned char source;
    unsigned char converged_once;
    unsigned char currently_stable;
    unsigned int confirmations;
    unsigned int contradictions;
    unsigned int convergence_step;
    unsigned int last_score;
    time_t first_seen_epoch;
    unsigned int first_seen_frame;
    time_t converged_epoch;
    unsigned int converged_frame;
    char question[SHAKTI_QUESTION_CAPACITY];
    char answer[SHAKTI_ANSWER_CAPACITY];
} shakti_evidence_t;
typedef struct {
    shakti_fact_t facts[SHAKTI_MAX_FACTS];
    shakti_synonym_t synonyms[SHAKTI_MAX_SYNONYMS];
    shakti_evidence_t evidence[SHAKTI_MAX_EVIDENCE];
    size_t fact_count;
    size_t synonym_count;
    size_t evidence_count;
    unsigned int teaching_steps;
} shakti_reason_state_t;
typedef struct {
    char text[SHAKTI_MEMORY_BYTES];
    char tags[SHAKTI_MEMORY_TAG_BYTES];
    char id[SHAKTI_MEMORY_ID_BYTES];
    shakti_tick_t tick;
    size_t size;
    uint64_t checksum;
    uint64_t last_used_slot;
    unsigned char permanent;
    unsigned char in_use;
} shakti_memory_item_t;
typedef struct {
    char title[SHAKTI_MENU_SECTION_CAPACITY];
    char description[SHAKTI_MENU_DESCRIPTION_CAPACITY];
} shakti_menu_section_t;
typedef struct {
    shakti_memory_item_t working[SHAKTI_WORKING_SLOTS];
    shakti_memory_item_t short_term[SHAKTI_SHORT_TERM_SLOTS];
    size_t working_count;
    size_t short_term_count;
    size_t short_term_next;
    char locked_goal[SHAKTI_LONG_TEXT_CAPACITY];
    char locked_notebook[SHAKTI_LONG_TEXT_CAPACITY];
    char locked_menu[SHAKTI_LONG_TEXT_CAPACITY];
    unsigned char goal_loaded;
    unsigned char notebook_loaded;
    unsigned char menu_loaded;
    unsigned char long_term_ready;
    unsigned char fault;
    unsigned long recent_decay_beats;
    uint64_t current_slot;
    uint64_t serial;
    time_t newest_epoch;
    char container_path[SHAKTI_PATH_CAPACITY];
    const shakti_reason_state_t *eden;
    FILE *recall_file;
    char recall_query[SHAKTI_MEMORY_TAG_BYTES];
    unsigned long recalled_count;
    shakti_memory_item_t recall_candidate;
} shakti_memory_state_t;
typedef struct {
    unsigned int pass;
    unsigned int mastery_target;
    unsigned int current_streak;
    unsigned int total_attempts;
    unsigned int total_correct;
    unsigned int total_errors;
    char active_symbol[SHAKTI_ANSWER_CAPACITY];
    unsigned char drill_active;
} shakti_school_state_t;
typedef struct {
    unsigned int order;
    char text[SHAKTI_TEXT_CAPACITY];
    char written_text[SHAKTI_PATH_CAPACITY];
    char visual_art[SHAKTI_PATH_CAPACITY];
    char sound_art[SHAKTI_PATH_CAPACITY];
} shakti_stone_t;
typedef struct {
    char level[SHAKTI_TEXT_CAPACITY];
    char lesson[SHAKTI_TEXT_CAPACITY];
    shakti_stone_t stones[SHAKTI_MAX_TABLET_STONES];
    size_t stone_count;
    unsigned char loaded;
} shakti_tablet_t;
typedef struct {
    unsigned int order;
    char id[SHAKTI_LEVEL_ID_CAPACITY];
    char title[SHAKTI_TEXT_CAPACITY];
    char requires[SHAKTI_REQUIREMENTS_CAPACITY];
} shakti_manifest_level_t;
typedef struct {
    unsigned int order;
    char level[SHAKTI_LEVEL_ID_CAPACITY];
    char lesson[SHAKTI_TEXT_CAPACITY];
    char mode[SHAKTI_MODE_CAPACITY];
    char requires[SHAKTI_REQUIREMENTS_CAPACITY];
    char state[SHAKTI_STATE_CAPACITY];
    char path[SHAKTI_PATH_CAPACITY];
} shakti_manifest_tablet_t;
typedef struct {
    char status[SHAKTI_STATE_CAPACITY];
    char artifact_root[SHAKTI_PATH_CAPACITY];
    shakti_manifest_level_t levels[SHAKTI_MAX_MANIFEST_LEVELS];
    shakti_manifest_tablet_t tablets[SHAKTI_MAX_MANIFEST_TABLETS];
    size_t level_count;
    size_t tablet_count;
    unsigned char loaded;
} shakti_manifest_t;
typedef struct { unsigned long passed; unsigned long required; unsigned int percent; unsigned char complete; } shakti_validation_score_t;
typedef struct {
    unsigned char text_ok;
    unsigned char written_text_ok;
    unsigned char sound_art_ok;
    unsigned char visual_art_ok;
    shakti_validation_score_t score;
} shakti_stone_audit_t;
typedef struct {
    shakti_stone_audit_t stones[SHAKTI_MAX_TABLET_STONES];
    size_t stone_count;
    shakti_validation_score_t score;
} shakti_tablet_audit_t;
/* A compiled reasoning adapter supplies actual answers, one at a time.
 * Return 1 with a complete NUL-terminated answer, 0 when it is not ready.
 * Calls are serialized with the heartbeat; the adapter must not block. */
typedef int (*shakti_reflection_provider_t)(void *context, unsigned int question,
    const char *prompt, char *answer, size_t capacity);
typedef struct {
    unsigned int heartbeat_minutes;
    time_t next_heartbeat_epoch;
    unsigned int turns_since_reflection;
    unsigned int reflection_deferrals;
    unsigned long cycle_count;
    unsigned char reflection_due;
    unsigned char tools_interrupted;
    unsigned char waiting_for_tyler;
    unsigned char memory_streaming;
    char goal[SHAKTI_LONG_TEXT_CAPACITY];
    char notebook[SHAKTI_LONG_TEXT_CAPACITY];
    char menu_text[SHAKTI_LONG_TEXT_CAPACITY];
    shakti_menu_section_t menu_sections[SHAKTI_MAX_MENU_SECTIONS];
    size_t menu_section_count;
    unsigned int heartbeat_rate;
    unsigned int heartbeat_slot;
    unsigned int pace_remainder;
    unsigned int audio_remainder;
    size_t heartbeat_samples;
    uint64_t heartbeat_count;
    uint64_t hearing_frames;
    uint64_t vision_frames;
    unsigned long tool_calls;
    char tool_trace[SHAKTI_REFLECTION_INTERVAL][SHAKTI_INPUT_CAPACITY];
    char reflection_answers[SHAKTI_REFLECTION_QUESTIONS][SHAKTI_REFLECTION_ANSWER_BYTES];
    unsigned long reflection_count;
    unsigned int reflection_answered_mask;
    unsigned char reflection_stored;
    shakti_tick_t reflection_tick;
    shakti_tick_t reflection_batch_tick;
    char reflection_id[SHAKTI_MEMORY_ID_BYTES];
    char pending_recall[SHAKTI_MEMORY_TAG_BYTES];
    shakti_reflection_provider_t reflection_provider;
    void *reflection_context;
    char focus_question[SHAKTI_QUESTION_CAPACITY];
    char current_thought[SHAKTI_ANSWER_CAPACITY];
    int16_t heartbeat_audio[SHAKTI_HEARTBEAT_SAMPLES];
} shakti_loop_state_t;
typedef struct {
    shakti_decision_kind_t kind;
    unsigned int score;
    unsigned int second_score;
    unsigned int margin;
    unsigned int creative_candidates_examined;
    int used_creative_rewrite;
    char answer[SHAKTI_ANSWER_CAPACITY];
    char reason[SHAKTI_TEXT_CAPACITY];
} shakti_decision_t;
typedef struct {
    shakti_tick_clock_t clock;
    time_t recorded_epoch;
    shakti_reason_state_t reason;
    shakti_memory_state_t memory;
    shakti_school_state_t school;
    shakti_loop_state_t loop;
    shakti_tablet_t tablet;
    shakti_manifest_t manifest;
    shakti_media_state_t media;
} shakti_runtime_t;
#endif
