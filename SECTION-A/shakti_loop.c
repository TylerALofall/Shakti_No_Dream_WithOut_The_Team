#include "shakti_loop.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include "shakti_reason.h"

#include "shakti_config.h"
#include "shakti_log.h"
#include "shakti_memory.h"
#include "shakti_time.h"

static const char *REFLECTION_QUESTIONS[] = {
    "Title the last 10 tool calls",
    "Summary",
    "Did I finish — if no, the reason",
    "Was it successful",
    "If I was to redo it, what would I do different",
    "Tools used",
    "What tools would have helped",
    "What could Tyler have done different to help",
    "What files did I save, and their paths",
    "Memories recalled that helped, by the epoch of the first memory in the batch",
    "Notes",
    "Meta tags for auto-search — shoot for 15",
    "Cross reference — override all additional memory sets to link this to:"
};

#define REFLECTION_QUESTION_COUNT \
    (sizeof(REFLECTION_QUESTIONS) / sizeof(REFLECTION_QUESTIONS[0]))

static void copy_text__loop_h(
    char *destination,
    size_t destination_size,
    const char *source
)
{
    size_t length;

    if (destination == NULL || destination_size == 0U) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    length = strlen(source);

    if (length >= destination_size) {
        length = destination_size - 1U;
    }

    memcpy(destination, source, length);
    destination[length] = '\0';
}

static char *trim_text__loop_h(char *text)
{
    char *end;

    while (*text != '\0' &&
           isspace((unsigned char)*text)) {
        text++;
    }

    end = text + strlen(text);

    while (end > text &&
           isspace((unsigned char)end[-1])) {
        end--;
    }

    *end = '\0';

    return text;
}

static int read_whole_file(
    const char *path,
    char *destination,
    size_t destination_size
)
{
    FILE *file;
    size_t used;

    if (path == NULL || destination == NULL || destination_size == 0U) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    used = fread(destination, 1U, destination_size - 1U, file);

    if (ferror(file) || !feof(file)) {
        fclose(file);
        destination[0] = '\0';
        return 0;
    }

    destination[used] = '\0';

    return fclose(file) == 0;
}

static int write_whole_file(const char *path, const char *text)
{
    FILE *file;
    int success;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success = fputs(text, file) >= 0 && fputc('\n', file) != EOF;

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int parse_menu(shakti_loop_state_t *state)
{
    char buffer[SHAKTI_LONG_TEXT_CAPACITY];
    char *line;
    shakti_menu_section_t *current;

    if (state == NULL) {
        return 0;
    }

    copy_text__loop_h(buffer, sizeof(buffer), state->menu_text);
    state->menu_section_count = 0U;
    current = NULL;
    line = strtok(buffer, "\n");

    while (line != NULL) {
        char *trimmed;

        trimmed = trim_text__loop_h(line);

        if (trimmed[0] == '[') {
            char *close;

            close = strrchr(trimmed, ']');

            if (close != NULL &&
                close[1] == '\0' &&
                state->menu_section_count < SHAKTI_MAX_MENU_SECTIONS) {
                size_t title_length;

                *close = '\0';
                title_length = strlen(trimmed + 1);

                if (title_length > 0U &&
                    title_length < SHAKTI_MENU_SECTION_CAPACITY) {
                    current =
                        &state->menu_sections[state->menu_section_count];
                    memset(current, 0, sizeof(*current));
                    copy_text__loop_h(
                        current->title,
                        sizeof(current->title),
                        trimmed + 1
                    );
                    state->menu_section_count++;
                }
            }
        } else if (current != NULL && trimmed[0] != '\0') {
            size_t used;
            size_t remaining;

            used = strlen(current->description);
            remaining = sizeof(current->description) - used;

            if (remaining > 1U) {
                if (used > 0U) {
                    strncat(
                        current->description,
                        " ",
                        remaining - 1U
                    );
                    used = strlen(current->description);
                    remaining = sizeof(current->description) - used;
                }

                if (remaining > 1U) {
                    strncat(
                        current->description,
                        trimmed,
                        remaining - 1U
                    );
                }
            }
        }

        line = strtok(NULL, "\n");
    }

    return state->menu_section_count > 0U;
}

static int append_loop_log(
    shakti_runtime_t *runtime,
    const char *path,
    const char *prefix,
    const char *section,
    unsigned int function_order,
    const char *route,
    const char *subject,
    const char *property,
    const char *value,
    shakti_tick_t *used_tick
)
{
    shakti_tick_t tick;

    if (!shakti_tick_next(&runtime->clock, &tick)) {
        return 0;
    }

    if (!shakti_log_append(
            path,
            prefix,
            &tick,
            section,
            function_order,
            "AWAKE",
            "text",
            subject,
            property,
            value)) {
        return 0;
    }

    if (strcmp(path, SHAKTI_LEARNED_STREAM_PATH) != 0) {
        if (!shakti_log_append(
                SHAKTI_LEARNED_STREAM_PATH,
                "LRN1",
                &tick,
                section,
                function_order,
                "AWAKE",
                "text",
                subject,
                route,
                value)) {
            return 0;
        }
    }

    if (used_tick != NULL) {
        *used_tick = tick;
    }

    return 1;
}

unsigned long shakti_loop_delay_us(shakti_loop_state_t *state)
{
    unsigned int denominator, remainder;
    unsigned long delay;
    if (state == NULL || state->heartbeat_rate == 0U) return 0UL;
    denominator = SHAKTI_HEARING_SLOTS * state->heartbeat_rate;
    delay = 1000000UL / denominator;
    remainder = 1000000U % denominator;
    state->pace_remainder += remainder;
    if (state->pace_remainder >= denominator) {
        state->pace_remainder -= denominator;
        ++delay;
    }
    return delay;
}

const int16_t *shakti_loop_heartbeat_audio(const shakti_loop_state_t *state, size_t *samples)
{
    if (samples != NULL) *samples = state != NULL ? state->heartbeat_samples : 0U;
    return state != NULL ? state->heartbeat_audio : NULL;
}

void shakti_loop_init(shakti_loop_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->heartbeat_rate = 1U;
    state->memory_streaming = 1U;
}

int shakti_loop_load(shakti_loop_state_t *state, const char *goal_path,
    const char *notebook_path, const char *menu_path)
{
    if (state == NULL) return 0;
    return read_whole_file(goal_path, state->goal, sizeof(state->goal)) &&
        read_whole_file(notebook_path, state->notebook, sizeof(state->notebook)) &&
        read_whole_file(menu_path, state->menu_text, sizeof(state->menu_text)) &&
        parse_menu(state);
}

static int hex_value(unsigned char c)
{
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return (int)(c - 'a') + 10;
    return -1;
}

int shakti_loop_restore(shakti_runtime_t *runtime)
{
    shakti_loop_state_t *loop;
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY], *fields[7];
    char last_completion[SHAKTI_MEMORY_ID_BYTES] = "";
    shakti_tick_t tick;
    int result;
    if (runtime == NULL) return 0;
    loop = &runtime->loop;
    file = fopen(SHAKTI_LEARNED_STREAM_PATH, "rb");
    if (file == NULL && errno != ENOENT) return 0;
    if (file != NULL) {
        while ((result = shakti_log_read_record(file, line, sizeof(line), fields, &tick)) > 0) {
            if (strcmp(fields[5], "actual_call") == 0) {
                unsigned int index = (unsigned int)(loop->tool_calls % SHAKTI_REFLECTION_INTERVAL);
                int n;
                if (loop->tool_calls == ULONG_MAX) { fclose(file); return 0; }
                n = snprintf(loop->tool_trace[index], SHAKTI_INPUT_CAPACITY,
                    "%s%s%s", fields[4], fields[6][0] ? " " : "", fields[6]);
                if (n < 0 || (size_t)n >= SHAKTI_INPUT_CAPACITY) { fclose(file); return 0; }
                if (index == 0U) loop->reflection_batch_tick = tick;
                ++loop->tool_calls;
            }
        }
        if (fclose(file) != 0 || result < 0) return 0;
    }
    file = fopen(SHAKTI_REFLECTION_PATH, "rb");
    if (file == NULL && errno != ENOENT) return 0;
    if (file != NULL) {
        while ((result = shakti_log_read_record(file, line, sizeof(line), fields, &tick)) > 0) {
            if (strcmp(fields[5], "complete_13_answers") == 0) {
                size_t size = strlen(fields[4]);
                if (size == 0U || size >= sizeof(last_completion)) { fclose(file); return 0; }
                if (strcmp(last_completion, fields[4]) != 0) {
                    if (loop->reflection_count == ULONG_MAX) { fclose(file); return 0; }
                    ++loop->reflection_count;
                    memcpy(last_completion, fields[4], size + 1U);
                }
                loop->reflection_answered_mask = 0U;
                memset(loop->reflection_answers, 0, sizeof(loop->reflection_answers));
            } else if (strcmp(fields[5], "actual_answer_hex") == 0) {
                char *end;
                unsigned long question;
                size_t i, bytes = strlen(fields[6]);
                if (strncmp(fields[4], "question_", 9U) != 0) { fclose(file); return 0; }
                errno = 0;
                question = strtoul(fields[4] + 9U, &end, 10);
                if (errno || *end || question < 1UL || question > REFLECTION_QUESTION_COUNT ||
                    bytes == 0U || (bytes & 1U) || bytes / 2U >= SHAKTI_REFLECTION_ANSWER_BYTES) {
                    fclose(file); return 0;
                }
                for (i = 0U; i < bytes / 2U; ++i) {
                    int high = hex_value((unsigned char)fields[6][i * 2U]);
                    int low = hex_value((unsigned char)fields[6][i * 2U + 1U]);
                    if (high < 0 || low < 0 || (high == 0 && low == 0)) { fclose(file); return 0; }
                    loop->reflection_answers[question - 1UL][i] = (char)(high * 16 + low);
                }
                loop->reflection_answers[question - 1UL][bytes / 2U] = '\0';
                loop->reflection_answered_mask |= 1U << (question - 1UL);
            }
        }
        if (fclose(file) != 0 || result < 0) return 0;
    }
    if (loop->reflection_count > loop->tool_calls / SHAKTI_REFLECTION_INTERVAL ||
        loop->tool_calls - loop->reflection_count * SHAKTI_REFLECTION_INTERVAL >
            SHAKTI_REFLECTION_INTERVAL) return 0;
    loop->turns_since_reflection = (unsigned int)(loop->tool_calls -
        loop->reflection_count * SHAKTI_REFLECTION_INTERVAL);
    loop->reflection_due = (unsigned char)(loop->turns_since_reflection == SHAKTI_REFLECTION_INTERVAL);
    if (last_completion[0]) {
        shakti_memory_item_t completed;
        if (shakti_memory_fetch(&runtime->memory, last_completion, &completed) != 1) return 0;
        memcpy(loop->pending_recall, completed.tags, strlen(completed.tags) + 1U);
    }
    return loop->reflection_due || loop->reflection_answered_mask == 0U;
}

static int complete_reflection(shakti_runtime_t *runtime);

int shakti_loop_begin_cycle(shakti_runtime_t *runtime, shakti_tick_t *cycle_tick)
{
    shakti_tick_t tick;
    char event[SHAKTI_TEXT_CAPACITY];
    unsigned int slot;
    unsigned long recalled;
    size_t i;
    int n;
    if (runtime == NULL || runtime->loop.cycle_count == ULONG_MAX) return 0;
    slot = (unsigned int)(runtime->loop.cycle_count % SHAKTI_HEARING_SLOTS);
    if (slot == 0U && runtime->loop.cycle_count != 0UL &&
        !shakti_tick_clock_beat(&runtime->clock)) return 0;
    runtime->loop.heartbeat_slot = slot;
    ++runtime->loop.cycle_count;
    ++runtime->loop.hearing_frames;
    if ((slot & 1U) == 0U) ++runtime->loop.vision_frames;
    if (slot == 0U) ++runtime->loop.heartbeat_count;
    {
        unsigned int denominator = SHAKTI_HEARING_SLOTS * runtime->loop.heartbeat_rate;
        runtime->loop.heartbeat_samples = 48000U / denominator;
        runtime->loop.audio_remainder += 48000U % denominator;
        if (runtime->loop.audio_remainder >= denominator) {
            ++runtime->loop.heartbeat_samples;
            runtime->loop.audio_remainder -= denominator;
        }
    }
    for (i = 0U; i < SHAKTI_HEARTBEAT_SAMPLES; ++i) {
        runtime->loop.heartbeat_audio[i] = slot == 0U &&
            i < runtime->loop.heartbeat_samples &&
            i < (480U + runtime->loop.heartbeat_rate - 1U) / runtime->loop.heartbeat_rate
            ? (int16_t)(((i / 120U) & 1U) ? -6000 : 6000) : (int16_t)0;
    }
    if (!shakti_tick_next(&runtime->clock, &tick)) return 0;
    if (cycle_tick != NULL) *cycle_tick = tick;
    n = snprintf(event, sizeof(event),
        "beat=%" PRIu64 " slot=%u hearing=%" PRIu64 " vision=%" PRIu64 " rate=%u",
        runtime->loop.heartbeat_count, slot, runtime->loop.hearing_frames,
        runtime->loop.vision_frames, runtime->loop.heartbeat_rate);
    if (n < 0 || (size_t)n >= sizeof(event) ||
        !shakti_log_append(SHAKTI_LEARNED_STREAM_PATH, "LRN1", &tick,
            "Ga", 1U, "AWAKE", "internal", "cycle", "heartbeat_slot", event)) return 0;
    shakti_memory_bind_eden(&runtime->memory, &runtime->reason);
    recalled = runtime->memory.recalled_count;
    if (!shakti_memory_step(&runtime->memory, runtime->loop.cycle_count))
        runtime->loop.tools_interrupted = 1U;
    else if (runtime->memory.recalled_count > recalled && runtime->memory.working_count) {
        const shakti_memory_item_t *item = &runtime->memory.working[0];
        n = snprintf(event, sizeof(event), "id=%s origin=%ld:%u bytes=%lu hash=%016" PRIx64,
            item->id, (long)item->tick.epoch_seconds, item->tick.frame,
            (unsigned long)item->size, item->checksum);
        if (n < 0 || (size_t)n >= sizeof(event) ||
            !append_loop_log(runtime, SHAKTI_LEARNED_STREAM_PATH, "LRN1",
                "shakti_memory.c:shakti_memory_step", 26U, "memory", item->id,
                "whole_container_recalled", event, NULL)) return 0;
    }
    {
        int media = shakti_media_step(&runtime->media, slot);
        if (media == 2) {
            const shakti_media_group_t *group = shakti_media_current(&runtime->media);
            const shakti_media_frame_t *first = &group->parts[(group->required_mask & 1U) ? 0U : 1U];
            shakti_tick_t media_tick;
            n = snprintf(event, sizeof(event), "source=%s window=%" PRIu64 " mask=%u hashes=%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64,
                first->source_id, first->position64 / 2U, group->checked_mask,
                group->parts[0].validated_hash, group->parts[1].validated_hash, group->parts[2].validated_hash);
            if (n < 0 || (size_t)n >= sizeof(event) || !shakti_tick_next(&runtime->clock, &media_tick) ||
                !shakti_log_append(SHAKTI_LEARNED_STREAM_PATH, "LRN1", &media_tick,
                    "shakti_media.c:shakti_media_step", 13U, "AWAKE", "internal",
                    first->source_id, "validated_convergence", event)) return 0;
            if (!shakti_memory_remember(&runtime->memory, &media_tick, event)) return 0;
        } else if (media < 0) {
            shakti_tick_t hold_tick;
            if (!shakti_tick_next(&runtime->clock, &hold_tick) ||
                !shakti_log_append(SHAKTI_LEARNED_STREAM_PATH, "LRN1", &hold_tick,
                    "shakti_media.c:shakti_media_step", 13U, "AWAKE", "internal",
                    "media", "validation_hold", "Source window held; no unvalidated content admitted")) return 0;
        }
    }
    if ((slot & 1U) == 0U && runtime->loop.focus_question[0] != '\0') {
        shakti_decision_t decision, check;
        decision = shakti_reason_answer(&runtime->reason, runtime->loop.focus_question);
        check = shakti_reason_answer(&runtime->reason, runtime->loop.focus_question);
        if (decision.kind != check.kind || decision.score != check.score ||
            strcmp(decision.answer, check.answer) != 0) {
            fputs("BENCH reasoning: repeat evaluation disagreed\n", stderr);
            runtime->loop.tools_interrupted = 1U;
        } else if (strcmp(runtime->loop.current_thought, decision.answer) != 0) {
            shakti_tick_t thought_tick;
            copy_text__loop_h(runtime->loop.current_thought,
                sizeof(runtime->loop.current_thought), decision.answer);
            if (!append_loop_log(runtime, SHAKTI_LEARNED_STREAM_PATH, "LRN1",
                "Aj", 1U, "reasoning", runtime->loop.focus_question,
                "internal_thought", decision.answer, &thought_tick) ||
                !shakti_memory_remember(&runtime->memory, &thought_tick, decision.answer)) return 0;
        }
    }
    return 1;
}

int shakti_loop_finish_cycle(shakti_runtime_t *runtime)
{
    shakti_loop_state_t *loop;
    unsigned int question;
    if (runtime == NULL) return 0;
    loop = &runtime->loop;
    if (loop->pending_recall[0] && runtime->memory.recall_file == NULL) {
        if (!shakti_memory_request(&runtime->memory, loop->pending_recall)) return 0;
        loop->pending_recall[0] = '\0';
    }
    if (loop->reflection_due && loop->reflection_provider != NULL) {
        for (question = 1U; question <= SHAKTI_REFLECTION_QUESTIONS; ++question) {
            if (!(loop->reflection_answered_mask & (1U << (question - 1U)))) {
                char answer[SHAKTI_REFLECTION_ANSWER_BYTES];
                int result;
                memset(answer, 0, sizeof(answer));
                result = loop->reflection_provider(loop->reflection_context, question,
                    shakti_loop_reflection_question(question), answer, sizeof(answer));
                if (result != 0 && (result != 1 ||
                    memchr(answer, '\0', sizeof(answer)) == NULL ||
                    !shakti_loop_answer_reflection(runtime, question, answer))) return 0;
                break;
            }
        }
    }
    return !loop->reflection_due || complete_reflection(runtime);
}

int shakti_loop_note_tool(shakti_runtime_t *runtime, const char *tool, const char *arguments)
{
    shakti_tick_t tick;
    unsigned int index;
    int n;
    if (runtime == NULL || tool == NULL || arguments == NULL ||
        runtime->loop.reflection_due || runtime->loop.tool_calls == ULONG_MAX) return 0;
    index = runtime->loop.turns_since_reflection;
    if (index >= SHAKTI_REFLECTION_INTERVAL) return 0;
    n = snprintf(runtime->loop.tool_trace[index], SHAKTI_INPUT_CAPACITY,
        "%s%s%s", tool, arguments[0] ? " " : "", arguments);
    if (n < 0 || (size_t)n >= SHAKTI_INPUT_CAPACITY) return 0;
    if (!append_loop_log(runtime, SHAKTI_LEARNED_STREAM_PATH, "LRN1", "Gf", 1U,
        "mcp_tool_call", tool, "actual_call", arguments, &tick)) return 0;
    if (index == 0U) runtime->loop.reflection_batch_tick = tick;
    ++runtime->loop.tool_calls;
    ++runtime->loop.turns_since_reflection;
    if (runtime->loop.turns_since_reflection == SHAKTI_REFLECTION_INTERVAL) {
        runtime->loop.reflection_due = 1U;
        runtime->loop.reflection_answered_mask = 0U;
        runtime->loop.reflection_stored = 0U;
        memset(runtime->loop.reflection_answers, 0,
            sizeof(runtime->loop.reflection_answers));
    }
    return 1;
}

int shakti_loop_set_heartbeat(shakti_loop_state_t *state, unsigned int beats_per_second)
{
    if (state == NULL || beats_per_second == 0U ||
        beats_per_second > SHAKTI_HEARTBEAT_MAX_RATE ||
        (beats_per_second & (beats_per_second - 1U)) != 0U) return 0;
    state->heartbeat_rate = beats_per_second;
    state->pace_remainder = 0U;
    state->audio_remainder = 0U;
    return 1;
}

int shakti_loop_set_goal(
    shakti_runtime_t *runtime,
    const char *goal
)
{
    shakti_tick_t tick;

    if (runtime == NULL || goal == NULL || goal[0] == '\0') {
        return 0;
    }

    if (!write_whole_file(SHAKTI_GOAL_PATH, goal)) {
        return 0;
    }

    copy_text__loop_h(runtime->loop.goal, sizeof(runtime->loop.goal), goal);

    if (!shakti_memory_reload_goal(
            &runtime->memory,
            SHAKTI_GOAL_PATH) ||
        !append_loop_log(
            runtime,
            SHAKTI_LEARNED_STREAM_PATH,
            "LRN1",
            "Gc",
            2U,
            "goal",
            "goal",
            "changed",
            goal,
            &tick)) {
        return 0;
    }

    return 1;
}

int shakti_loop_notebook(
    shakti_runtime_t *runtime,
    const char *entry
)
{
    FILE *file;

    if (runtime == NULL) {
        return 0;
    }

    if (entry == NULL || entry[0] == '\0') {
        fputs(runtime->loop.notebook, stdout);

        if (runtime->loop.notebook[0] != '\0' &&
            runtime->loop.notebook[
                strlen(runtime->loop.notebook) - 1U] != '\n') {
            fputc('\n', stdout);
        }

        return 1;
    }

    file = fopen(SHAKTI_NOTEBOOK_PATH, "a");

    if (file == NULL) {
        return 0;
    }

    {
        int success;

        success = fprintf(file, "%s\n", entry) >= 0;

        if (success) {
            success = fflush(file) == 0;
        }

        if (fclose(file) != 0) {
            success = 0;
        }

        if (!success) {
            return 0;
        }
    }

    if (!read_whole_file(
            SHAKTI_NOTEBOOK_PATH,
            runtime->loop.notebook,
            sizeof(runtime->loop.notebook)) ||
        !shakti_memory_reload_notebook(
            &runtime->memory,
            SHAKTI_NOTEBOOK_PATH) ||
        !append_loop_log(
            runtime,
            SHAKTI_LEARNED_STREAM_PATH,
            "LRN1",
            "Gd",
            2U,
            "notebook",
            "notebook",
            "entry",
            entry,
            NULL)) {
        return 0;
    }

    return 1;
}

void shakti_loop_menu_titles(const shakti_loop_state_t *state)
{
    size_t index;

    if (state == NULL) {
        return;
    }

    puts("Menu titles:");

    for (index = 0U; index < state->menu_section_count; ++index) {
        printf("%s\n", state->menu_sections[index].title);
    }
}

int shakti_loop_menu_section(
    const shakti_loop_state_t *state,
    const char *title
)
{
    size_t index;

    if (state == NULL || title == NULL || title[0] == '\0') {
        return 0;
    }

    for (index = 0U; index < state->menu_section_count; ++index) {
        if (strcmp(state->menu_sections[index].title, title) == 0) {
            printf(
                "%s: %s\n",
                state->menu_sections[index].title,
                state->menu_sections[index].description
            );
            return 1;
        }
    }

    return 0;
}

int shakti_loop_record_message(
    shakti_runtime_t *runtime,
    const char *route,
    const char *text
)
{
    const char *section;
    const char *property;

    if (runtime == NULL ||
        route == NULL ||
        text == NULL ||
        text[0] == '\0') {
        return 0;
    }

    if (strcmp(route, "message_tyler") == 0) {
        section = "Gg";
        property = "message";
        runtime->loop.waiting_for_tyler = 0U;
    } else if (strcmp(route, "note_tyler") == 0) {
        section = "Gg";
        property = "note";
    } else if (strcmp(route, "message_shakti") == 0) {
        section = "Gh";
        property = "message";
        runtime->loop.waiting_for_tyler = 0U;
    } else if (strcmp(route, "note_shakti") == 0) {
        section = "Gh";
        property = "note";
    } else {
        return 0;
    }

    return append_loop_log(
        runtime,
        SHAKTI_MESSAGES_PATH,
        "MSG1",
        section,
        1U,
        route,
        route,
        property,
        text,
        NULL
    );
}

void shakti_loop_interrupt_tools(shakti_loop_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->tools_interrupted = 1U;
}

void shakti_loop_resume_tools(shakti_loop_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->tools_interrupted = 0U;
}

int shakti_loop_tools_available(const shakti_loop_state_t *state)
{
    return state != NULL && !state->tools_interrupted;
}

int shakti_loop_defer_reflection(shakti_loop_state_t *state)
{
    (void)state;
    return 0;
}

void shakti_loop_print_reflection_questions(FILE *output)
{
    size_t index;

    if (output == NULL) {
        return;
    }

    for (index = 0U; index < REFLECTION_QUESTION_COUNT; ++index) {
        fprintf(
            output,
            "%lu. %s\n",
            (unsigned long)(index + 1U),
            REFLECTION_QUESTIONS[index]
        );
    }
}

const char *shakti_loop_reflection_question(unsigned int question)
{
    return question >= 1U && question <= REFLECTION_QUESTION_COUNT
        ? REFLECTION_QUESTIONS[question - 1U] : NULL;
}

void shakti_loop_set_reflection_provider(shakti_loop_state_t *state,
    shakti_reflection_provider_t provider, void *context)
{
    if (state == NULL) return;
    state->reflection_provider = provider;
    state->reflection_context = context;
}

int shakti_loop_answer_reflection(shakti_runtime_t *runtime,
    unsigned int question, const char *answer)
{
    size_t size, i;
    unsigned int bit;
    char subject[24], encoded[SHAKTI_REFLECTION_ANSWER_BYTES * 2U];
    static const char hex[] = "0123456789abcdef";
    int n, nonblank = 0;
    if (runtime == NULL || answer == NULL || !runtime->loop.reflection_due ||
        runtime->loop.reflection_stored || question < 1U ||
        question > REFLECTION_QUESTION_COUNT) return 0;
    size = strlen(answer);
    if (size == 0U || size >= SHAKTI_REFLECTION_ANSWER_BYTES) return 0;
    for (i = 0U; i < size; ++i)
        if (!isspace((unsigned char)answer[i])) nonblank = 1;
    if (!nonblank) return 0;
    for (i = 0U; i < size; ++i) {
        unsigned char c = (unsigned char)answer[i];
        encoded[i * 2U] = hex[c >> 4];
        encoded[i * 2U + 1U] = hex[c & 15U];
    }
    encoded[size * 2U] = '\0';
    bit = 1U << (question - 1U);
    n = snprintf(subject, sizeof(subject), "question_%u", question);
    if (n < 0 || (size_t)n >= sizeof(subject) ||
        !append_loop_log(runtime, SHAKTI_REFLECTION_PATH, "REF1", "Gi", 1U,
            "reflection", subject, "actual_answer_hex", encoded, NULL)) return 0;
    memcpy(runtime->loop.reflection_answers[question - 1U], answer, size + 1U);
    runtime->loop.reflection_answered_mask |= bit;
    return 1;
}

static void selected_tags(const char *answer, char *out, size_t capacity)
{
    unsigned int count = 0U;
    size_t used = 0U;
    const char *p = answer;
    out[0] = '\0';
    while (*p != '\0' && count < 15U) {
        const char *start;
        size_t size;
        while (*p && !isalnum((unsigned char)*p)) ++p;
        start = p;
        while (*p && isalnum((unsigned char)*p)) ++p;
        size = (size_t)(p - start);
        if (size == 0U) break;
        /* Keep complete trigger words; the original metadata answer is also
         * stored intact in the reflection container. */
        if (size + used + (used ? 1U : 0U) >= capacity) break;
        if (used) out[used++] = ' ';
        memcpy(out + used, start, size);
        used += size;
        out[used] = '\0';
        ++count;
    }
}

static int complete_reflection(shakti_runtime_t *runtime)
{
    char block[SHAKTI_MEMORY_BYTES];
    char tags[SHAKTI_MEMORY_TAG_BYTES];
    shakti_memory_item_t previous;
    shakti_loop_state_t *loop = &runtime->loop;
    size_t i, used = 0U;
    int n;
    const unsigned int all = (1U << SHAKTI_REFLECTION_QUESTIONS) - 1U;
    if (!loop->reflection_due || loop->reflection_answered_mask != all ||
        loop->pending_recall[0] != '\0') return 1;
    if (loop->reflection_count == ULONG_MAX) return 0;
    selected_tags(loop->reflection_answers[11], tags, sizeof(tags));
    if (!loop->reflection_stored) {
        for (i = 0U; i < REFLECTION_QUESTION_COUNT; ++i) {
            n = snprintf(block + used, sizeof(block) - used, "%lu. %s\n%s\n",
                (unsigned long)(i + 1U), REFLECTION_QUESTIONS[i], loop->reflection_answers[i]);
            if (n < 0 || (size_t)n >= sizeof(block) - used) return 0;
            used += (size_t)n;
        }
        int exists;
        n = snprintf(loop->reflection_id, sizeof(loop->reflection_id), "reflection-batch-%ld-%u",
            (long)loop->reflection_batch_tick.epoch_seconds, loop->reflection_batch_tick.frame);
        if (n < 0 || (size_t)n >= sizeof(loop->reflection_id)) return 0;
        exists = shakti_memory_fetch(&runtime->memory, loop->reflection_id, &previous);
        if (exists < 0) return 0;
        if (exists) {
            if (previous.size != used || strcmp(previous.tags, tags) != 0 ||
                memcmp(previous.text, block, used) != 0) {
                fputs("BENCH reflection: committed batch container disagrees with actual answers\n", stderr);
                return 0;
            }
            loop->reflection_tick = previous.tick;
        } else {
            if (!shakti_tick_next(&runtime->clock, &loop->reflection_tick) ||
                !shakti_memory_store(&runtime->memory, &loop->reflection_tick,
                    loop->reflection_id, tags, block, 0)) return 0;
        }
        loop->reflection_stored = 1U;
    }
    if (!shakti_log_append(SHAKTI_REFLECTION_PATH, "REF1", &loop->reflection_tick,
            "Gi", 1U, "AWAKE", "internal", loop->reflection_id,
            "complete_13_answers", loop->reflection_id)) return 0;
    ++loop->reflection_count;
    loop->turns_since_reflection = 0U;
    loop->reflection_due = 0U;
    loop->reflection_deferrals = 0U;
    loop->reflection_stored = 0U;
    memcpy(loop->pending_recall, tags, strlen(tags) + 1U);
    return 1;
}

int shakti_loop_run_reflection(shakti_runtime_t *runtime, FILE *input, FILE *output)
{
    size_t i;
    (void)input;
    if (runtime == NULL || output == NULL) return 0;
    fprintf(output, "Reflection %s; actual answers are supplied by the reasoning adapter.\n",
        runtime->loop.reflection_due ? "pending" : "idle");
    for (i = 0U; i < REFLECTION_QUESTION_COUNT; ++i)
        fprintf(output, "%lu. %s\n%s\n", (unsigned long)(i + 1U),
            REFLECTION_QUESTIONS[i], runtime->loop.reflection_answers[i][0]
                ? runtime->loop.reflection_answers[i] : "[answer pending]");
    return 1;
}

void shakti_loop_print_status(const shakti_loop_state_t *state)
{
    if (state == NULL) return;
    printf("Heartbeat: %u beat(s)/second; all channels share this pace.\n", state->heartbeat_rate);
    printf("Cycles: %lu. Beats: %" PRIu64 ". Vision slots: %" PRIu64 ". Hearing slots: %" PRIu64 ".\n",
        state->cycle_count, state->heartbeat_count, state->vision_frames, state->hearing_frames);
    printf("Actual tool calls: %lu; %u since reflection; complete reflections: %lu.\n",
        state->tool_calls, state->turns_since_reflection, state->reflection_count);
    printf("MCP tools: %s. Reply optional; the loop remains active.\n",
        state->tools_interrupted ? "interrupted" :
        (state->reflection_due ? "held for thirteen reflection answers" : "available"));
}
