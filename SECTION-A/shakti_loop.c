#include "shakti_loop.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "shakti_config.h"
#include "shakti_log.h"
#include "shakti_memory.h"
#include "shakti_time.h"

static const char *REFLECTION_QUESTIONS[] = {
    "Title the last 10 cycles",
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

    shakti_memory_remember(&runtime->memory, &tick, value);

    if (used_tick != NULL) {
        *used_tick = tick;
    }

    return 1;
}

static int heartbeat_is_due(shakti_loop_state_t *state, time_t now)
{
    if (state->heartbeat_minutes == 0U) {
        return 1;
    }

    return now >= state->next_heartbeat_epoch;
}

static void schedule_next_heartbeat(
    shakti_loop_state_t *state,
    time_t now
)
{
    if (state->heartbeat_minutes == 0U) {
        state->next_heartbeat_epoch = now;
        return;
    }

    state->next_heartbeat_epoch =
        now + (time_t)(state->heartbeat_minutes * 60U);
}

void shakti_loop_init(shakti_loop_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->heartbeat_minutes = 1U;
    state->memory_streaming = 1U;
}

int shakti_loop_load(
    shakti_loop_state_t *state,
    const char *goal_path,
    const char *notebook_path,
    const char *menu_path
)
{
    time_t now;

    if (state == NULL) {
        return 0;
    }

    if (!read_whole_file(
            goal_path,
            state->goal,
            sizeof(state->goal)) ||
        !read_whole_file(
            notebook_path,
            state->notebook,
            sizeof(state->notebook)) ||
        !read_whole_file(
            menu_path,
            state->menu_text,
            sizeof(state->menu_text)) ||
        !parse_menu(state)) {
        return 0;
    }

    now = time(NULL);

    if (now == (time_t)-1) {
        return 0;
    }

    schedule_next_heartbeat(state, now);

    return 1;
}

int shakti_loop_begin_cycle(
    shakti_runtime_t *runtime,
    shakti_tick_t *cycle_tick
)
{
    shakti_tick_t tick;
    char tick_text[SHAKTI_TICK_CAPACITY];
    time_t now;

    if (runtime == NULL) {
        return 0;
    }

    if (!shakti_tick_next(&runtime->clock, &tick) ||
        !shakti_tick_format(&tick, tick_text, sizeof(tick_text)) ||
        !shakti_log_append(
            SHAKTI_LEARNED_STREAM_PATH,
            "LRN1",
            &tick,
            "Ga",
            1U,
            "AWAKE",
            "internal",
            "epoch",
            "cycle_start",
            tick_text)) {
        return 0;
    }

    if (cycle_tick != NULL) {
        *cycle_tick = tick;
    }

    runtime->loop.cycle_count++;
    printf("Epoch: %s\n", tick_text);

    now = time(NULL);

    if (now == (time_t)-1) {
        return 0;
    }

    if (heartbeat_is_due(&runtime->loop, now)) {
        puts("Heartbeat: keep going.");

        if (!append_loop_log(
                runtime,
                SHAKTI_LEARNED_STREAM_PATH,
                "LRN1",
                "Gb",
                1U,
                "heartbeat",
                "heartbeat",
                "prompt",
                "keep going",
                NULL)) {
            return 0;
        }

        schedule_next_heartbeat(&runtime->loop, now);
    }

    printf("Goal/system message: %s\n", runtime->loop.goal);
    puts("Notebook is loaded as Shakti's first item.");
    puts("Menu titles are available through /menu/.");

    if (!append_loop_log(
            runtime,
            SHAKTI_LEARNED_STREAM_PATH,
            "LRN1",
            "Gc",
            1U,
            "goal",
            "goal",
            "resident",
            runtime->loop.goal,
            NULL) ||
        !append_loop_log(
            runtime,
            SHAKTI_LEARNED_STREAM_PATH,
            "LRN1",
            "Gd",
            1U,
            "notebook",
            "notebook",
            "resident",
            "loaded",
            NULL) ||
        !append_loop_log(
            runtime,
            SHAKTI_LEARNED_STREAM_PATH,
            "LRN1",
            "Ge",
            1U,
            "menu",
            "menu",
            "two_tier",
            "titles_then_selected_description",
            NULL)) {
        return 0;
    }

    if (runtime->loop.reflection_due) {
        printf(
            "Reflection is due. Deferrals used: %u of %u.\n",
            runtime->loop.reflection_deferrals,
            (unsigned int)SHAKTI_REFLECTION_MAX_DEFERRALS
        );
    }

    return 1;
}

int shakti_loop_finish_cycle(shakti_runtime_t *runtime)
{
    if (runtime == NULL) {
        return 0;
    }

    runtime->loop.turns_since_reflection++;

    if (runtime->loop.turns_since_reflection >=
        SHAKTI_REFLECTION_INTERVAL) {
        runtime->loop.reflection_due = 1U;
    }

    return append_loop_log(
        runtime,
        SHAKTI_LEARNED_STREAM_PATH,
        "LRN1",
        "Gi",
        1U,
        "reflection",
        "reflection",
        runtime->loop.reflection_due ? "due" : "scheduled",
        runtime->loop.reflection_due
            ? "reflection prompt active"
            : "reflection remains scheduled",
        NULL
    );
}

int shakti_loop_set_heartbeat(
    shakti_loop_state_t *state,
    unsigned int minutes
)
{
    time_t now;

    if (state == NULL || minutes > SHAKTI_HEARTBEAT_MAX_MINUTES) {
        return 0;
    }

    now = time(NULL);

    if (now == (time_t)-1) {
        return 0;
    }

    state->heartbeat_minutes = minutes;
    schedule_next_heartbeat(state, now);

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
        runtime->loop.waiting_for_tyler = 1U;
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
    if (state == NULL ||
        !state->reflection_due ||
        state->reflection_deferrals >=
            SHAKTI_REFLECTION_MAX_DEFERRALS) {
        return 0;
    }

    state->reflection_deferrals++;

    return 1;
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

int shakti_loop_run_reflection(
    shakti_runtime_t *runtime,
    FILE *input,
    FILE *output
)
{
    size_t index;
    char answer[SHAKTI_INPUT_CAPACITY];

    if (runtime == NULL || input == NULL || output == NULL) {
        return 0;
    }

    for (index = 0U; index < REFLECTION_QUESTION_COUNT; ++index) {
        char property[48];

        fprintf(
            output,
            "%lu. %s\n> ",
            (unsigned long)(index + 1U),
            REFLECTION_QUESTIONS[index]
        );
        fflush(output);

        if (fgets(answer, sizeof(answer), input) == NULL) {
            return 0;
        }

        {
            char *trimmed;

            trimmed = trim_text__loop_h(answer);

            if (trimmed[0] == '\0') {
                trimmed = "(left blank)";
            }

            if (snprintf(
                    property,
                    sizeof(property),
                    "answer_%02lu",
                    (unsigned long)(index + 1U)) < 0) {
                return 0;
            }

            if (!append_loop_log(
                    runtime,
                    SHAKTI_REFLECTION_PATH,
                    "REF1",
                    "Gi",
                    (unsigned int)(index + 1U),
                    "reflection",
                    REFLECTION_QUESTIONS[index],
                    property,
                    trimmed,
                    NULL)) {
                return 0;
            }
        }
    }

    runtime->loop.turns_since_reflection = 0U;
    runtime->loop.reflection_deferrals = 0U;
    runtime->loop.reflection_due = 0U;

    puts("Reflection appended to long-term memory.");

    return 1;
}

void shakti_loop_print_status(const shakti_loop_state_t *state)
{
    if (state == NULL) {
        return;
    }

    printf(
        "Heartbeat: %u minute(s), accepted range 0 through %u.\n",
        state->heartbeat_minutes,
        (unsigned int)SHAKTI_HEARTBEAT_MAX_MINUTES
    );
    printf("Cycle count: %lu.\n", state->cycle_count);
    printf(
        "Reflection: %u turn(s) since completion, due=%s, deferrals=%u of %u.\n",
        state->turns_since_reflection,
        state->reflection_due ? "yes" : "no",
        state->reflection_deferrals,
        (unsigned int)SHAKTI_REFLECTION_MAX_DEFERRALS
    );
    printf(
        "MCP tools: %s. Shakti remains awake.\n",
        state->tools_interrupted ? "interrupted" : "available"
    );
    printf(
        "Message state: %s.\n",
        state->waiting_for_tyler
            ? "Shakti is waiting for Tyler"
            : "Shakti may continue"
    );
    puts(
        "Memory streaming: active. Goal, notebook, menu, Eden, School, and "
        "locked blocks remain resident."
    );
}
