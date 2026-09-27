#include "shakti_school.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shakti_config.h"
#include "shakti_handwriting.h"
#include "shakti_log.h"
#include "shakti_memory.h"
#include "shakti_loop.h"
#include "shakti_time.h"

static char *trim_text__school_h(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t') {
        text++;
    }

    end = text + strlen(text);

    while (end > text &&
           (end[-1] == ' ' ||
            end[-1] == '\t' ||
            end[-1] == '\r' ||
            end[-1] == '\n')) {
        end--;
    }

    *end = '\0';

    return text;
}

static int append_school_line(
    const char *path,
    const shakti_tick_t *tick,
    const char *event,
    const char *symbol,
    unsigned int pass,
    unsigned int streak
)
{
    FILE *file;
    char tick_text[SHAKTI_TICK_CAPACITY];

    if (strchr(symbol, '|') != NULL ||
        !shakti_tick_format(tick, tick_text, sizeof(tick_text))) {
        return 0;
    }

    file = fopen(path, "a");

    if (file == NULL) {
        return 0;
    }

    {
        int success;

        success = fprintf(
            file,
            "SCH1|%s|%s|%s|%u|%u\n",
            tick_text,
            event,
            symbol,
            pass,
            streak
        ) >= 0;

        if (success) {
            success = fflush(file) == 0;
        }

        if (fclose(file) != 0) {
            success = 0;
        }

        return success;
    }
}

void shakti_school_init(shakti_school_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->pass = 1U;
    state->mastery_target = SHAKTI_DEFAULT_MASTERY_TARGET;
}

int shakti_school_load(shakti_school_state_t *state, const char *path)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];

    if (state == NULL || path == NULL) {
        return 0;
    }

    shakti_school_init(state);
    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char tag[8];
        char tick_text[SHAKTI_TICK_CAPACITY];
        char event[24];
        char symbol[SHAKTI_ANSWER_CAPACITY];
        unsigned int pass;
        unsigned int streak;
        if (line[0] == '#' || trim_text__school_h(line)[0] == '\0') {
            continue;
        }

        if (sscanf(
                line,
                "%7[^|]|%47[^|]|%23[^|]|%95[^|]|%u|%u",
                tag,
                tick_text,
                event,
                symbol,
                &pass,
                &streak) != 6 ||
            strcmp(tag, "SCH1") != 0 ||
            pass < 1U ||
            pass > 4U) {
            fclose(file);
            return 0;
        }

        state->pass = pass;
        state->current_streak = streak;

        if (symbol[0] != '-') {
            size_t length;

            length = strlen(symbol);

            if (length >= sizeof(state->active_symbol)) {
                fclose(file);
                return 0;
            }

            if (strcmp(state->active_symbol, symbol) != 0) {
                memcpy(state->active_symbol, symbol, length + 1U);
                state->total_attempts = 0U;
                state->total_correct = 0U;
                state->total_errors = 0U;
            }

            if (strcmp(event, "RIGHT") == 0) {
                state->total_attempts++;
                state->total_correct++;
            } else if (strcmp(event, "WRONG") == 0) {
                state->total_attempts++;
                state->total_errors++;
            }
        }
    }

    fclose(file);

    return 1;
}

int shakti_school_set_pass(
    shakti_school_state_t *state,
    const char *path,
    const shakti_tick_t *tick,
    unsigned int pass
)
{
    if (state == NULL ||
        path == NULL ||
        tick == NULL ||
        pass < 1U ||
        pass > 4U) {
        return 0;
    }

    if (!append_school_line(
            path,
            tick,
            "PASS",
            "-",
            pass,
            state->current_streak)) {
        return 0;
    }

    state->pass = pass;

    if (pass == 1U) {
        state->current_streak = 0U;
        state->active_symbol[0] = '\0';
    }

    return 1;
}

int shakti_school_record_trial(
    shakti_school_state_t *state,
    const char *path,
    const shakti_tick_t *tick,
    const char *symbol,
    int correct
)
{
    size_t length;

    if (state == NULL ||
        path == NULL ||
        tick == NULL ||
        symbol == NULL ||
        symbol[0] == '\0' ||
        state->pass < 2U ||
        state->pass > 4U) {
        return 0;
    }

    length = strlen(symbol);

    if (length >= sizeof(state->active_symbol)) {
        return 0;
    }

    if (strcmp(state->active_symbol, symbol) != 0) {
        memcpy(state->active_symbol, symbol, length + 1U);
        state->current_streak = 0U;
        state->total_attempts = 0U;
        state->total_correct = 0U;
        state->total_errors = 0U;
    }

    state->total_attempts++;

    if (correct) {
        state->total_correct++;

        if (state->current_streak < state->mastery_target) {
            state->current_streak++;
        }
    } else {
        state->total_errors++;
        state->current_streak = 0U;
    }

    return append_school_line(
        path,
        tick,
        correct ? "RIGHT" : "WRONG",
        symbol,
        state->pass,
        state->current_streak
    );
}

static const char *reward_for_streak(unsigned int streak)
{
    static const char *const rewards[10] = {
        "F642", "F60A", "F604", "F601", "F606",
        "F917", "ʵ0", "F31F", "F389", "F973"
    };

    if (streak == 0U) {
        return "";
    }

    if (streak > 10U) {
        streak = 10U;
    }

    return rewards[streak - 1U];
}

static void show_trial_progress(
    const shakti_school_state_t *state,
    int correct
)
{
    unsigned int remaining;

    remaining = state->current_streak < state->mastery_target
        ? state->mastery_target - state->current_streak
        : 0U;

    if (correct) {
        printf(
            "%s %u/%u",
            reward_for_streak(state->current_streak),
            state->current_streak,
            state->mastery_target
        );

        if (remaining > 0U) {
            printf(" — %u more!\n", remaining);
        } else {
            fputc('\n', stdout);
        }
    } else {
        printf(
            "Not yet. Streak returned to 0/%u. "
            "History: %u correct, %u errors, %u attempts.\n",
            state->mastery_target,
            state->total_correct,
            state->total_errors,
            state->total_attempts
        );
    }
}

static void show_mastery_reward(
    const shakti_school_state_t *state
)
{
    unsigned int index;

    puts("");
    puts("YOU DID IT!");

    for (index = 0U; index < state->mastery_target; ++index) {
        fputs("F973", stdout);
    }

    printf(
        "\n%u/%u — 100%% MASTERY\n",
        state->current_streak,
        state->mastery_target
    );
    printf(
        "History preserved: %u correct, %u errors, %u attempts.\n",
        state->total_correct,
        state->total_errors,
        state->total_attempts
    );
    fputc('\a', stdout);
    fflush(stdout);
}

int shakti_school_show_written_text_pixels(const char *text)
{
    size_t row;
    size_t index;

    if (!shakti_handwriting_supports_text(text)) {
        return 0;
    }

    for (row = 0U; row < 8U; ++row) {
        for (index = 0U; text[index] != '\0'; ++index) {
            char rows[8][9];

            if (!shakti_handwriting_render_char(
                    (unsigned char)text[index],
                    rows)) {
                return 0;
            }

            fputs(rows[row], stdout);
            fputc(' ', stdout);
        }

        fputc('\n', stdout);
    }

    return 1;
}

int shakti_school_draft_snapshots(
    shakti_runtime_t *runtime,
    const char *text
)
{
    char snapshot[SHAKTI_TEXT_CAPACITY];
    size_t length;
    size_t index;

    if (runtime == NULL || text == NULL) {
        return 0;
    }

    length = strlen(text);

    if (length == 0U || length >= sizeof(snapshot)) {
        return 0;
    }

    snapshot[0] = '\0';

    for (index = 0U; index < length; ++index) {
        shakti_tick_t tick;

        snapshot[index] = text[index];
        snapshot[index + 1U] = '\0';

        if (!shakti_tick_next(&runtime->clock, &tick)) {
            return 0;
        }

        printf("Draft frame: %s\n", snapshot);

        if (!shakti_school_show_written_text_pixels(snapshot)) {
            puts("The 8x8 handwriting font does not support this text.");
            return 0;
        }

        if (!shakti_log_append(
                SHAKTI_LEARNED_STREAM_PATH,
                "LRN1",
                &tick,
                "Hg",
                1U,
                "SCHOOL",
                "written_text",
                "draft",
                "snapshot",
                snapshot)) {
            return 0;
        }

        shakti_memory_remember(
            &runtime->memory,
            &tick,
            snapshot
        );
    }

    return 1;
}


typedef enum {
    SHAKTI_DRILL_INPUT_ANSWER = 0,
    SHAKTI_DRILL_INPUT_CONTINUE = 1,
    SHAKTI_DRILL_INPUT_STOP = 2,
    SHAKTI_DRILL_INPUT_QUIT = 3
} shakti_drill_input_result_t;

static void print_drill_status(const shakti_runtime_t *runtime)
{
    printf(
        "School pass: %u. Active text: %s. Progress: %u of %u.\n",
        runtime->school.pass,
        runtime->school.active_symbol[0] != '\0'
            ? runtime->school.active_symbol
            : "none",
        runtime->school.current_streak,
        runtime->school.mastery_target
    );
    printf(
        "History: %u correct, %u errors, %u attempts.\n",
        runtime->school.total_correct,
        runtime->school.total_errors,
        runtime->school.total_attempts
    );
    shakti_loop_print_status(&runtime->loop);
}

static void print_drill_controls(void)
{
    puts("Drill controls:");
    puts("/status/     Show School and loop status.");
    puts("/help/       Show these drill controls.");
    puts("/interrupt/  Stop the drill and MCP tools; Shakti remains awake.");
    puts("/resume/     Restore MCP tools and continue the drill.");
    puts("/stop/       Stop the drill without changing the School pass.");
    puts("/quit/       Stop the drill and close Shakti safely.");
}

static shakti_drill_input_result_t handle_drill_control(
    shakti_runtime_t *runtime,
    const char *input
)
{
    if (strcmp(input, "/status/") == 0) {
        print_drill_status(runtime);
        return SHAKTI_DRILL_INPUT_CONTINUE;
    }

    if (strcmp(input, "/help/") == 0) {
        print_drill_controls();
        return SHAKTI_DRILL_INPUT_CONTINUE;
    }

    if (strcmp(input, "/interrupt/") == 0) {
        shakti_loop_interrupt_tools(&runtime->loop);
        puts("MCP tools stopped. Shakti remains awake.");
        puts("The active drill stopped without scoring the command.");
        return SHAKTI_DRILL_INPUT_STOP;
    }

    if (strcmp(input, "/resume/") == 0) {
        shakti_loop_resume_tools(&runtime->loop);
        puts("MCP tools restored.");
        return SHAKTI_DRILL_INPUT_CONTINUE;
    }

    if (strcmp(input, "/stop/") == 0 ||
        strcmp(input, "/stop") == 0) {
        puts("Drill stopped without changing the School pass.");
        return SHAKTI_DRILL_INPUT_STOP;
    }

    if (strcmp(input, "/quit/") == 0) {
        puts("Quit requested. The active drill stopped safely.");
        return SHAKTI_DRILL_INPUT_QUIT;
    }

    return SHAKTI_DRILL_INPUT_ANSWER;
}

int shakti_school_run_drill(
    shakti_runtime_t *runtime,
    const char *symbol
)
{
    char input[SHAKTI_INPUT_CAPACITY];

    if (runtime == NULL ||
        symbol == NULL ||
        symbol[0] == '\0') {
        return 0;
    }

    if (runtime->school.pass == 1U) {
        puts(
            "Pass 1 is stateless witness. Set School to Pass 2, 3, or 4 "
            "before using the actuator."
        );
        return 1;
    }

    printf(
        "Type \"%s\" until mastery reaches %u.\n",
        symbol,
        runtime->school.mastery_target
    );

    while (runtime->school.current_streak <
           runtime->school.mastery_target) {
        char *answer;
        int correct;
        shakti_tick_t tick;
        char status[SHAKTI_TEXT_CAPACITY];

        printf(
            "Pass %u. Progress %u of %u. Type %s: ",
            runtime->school.pass,
            runtime->school.current_streak,
            runtime->school.mastery_target,
            symbol
        );
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            return 0;
        }

        answer = trim_text__school_h(input);

        {
            shakti_drill_input_result_t control_result;

            control_result = handle_drill_control(runtime, answer);

            if (control_result == SHAKTI_DRILL_INPUT_CONTINUE) {
                continue;
            }

            if (control_result == SHAKTI_DRILL_INPUT_STOP) {
                return 1;
            }

            if (control_result == SHAKTI_DRILL_INPUT_QUIT) {
                return 0;
            }
        }

        if (!shakti_school_draft_snapshots(runtime, answer)) {
            return 0;
        }

        correct = strcmp(answer, symbol) == 0;

        if (!shakti_tick_next(&runtime->clock, &tick) ||
            !shakti_school_record_trial(
                &runtime->school,
                SHAKTI_SCHOOL_PATH,
                &tick,
                symbol,
                correct)) {
            return 0;
        }

        snprintf(
            status,
            sizeof(status),
            "%s symbol=%s streak=%u",
            correct ? "right" : "wrong",
            symbol,
            runtime->school.current_streak
        );

        if (!shakti_log_append(
                SHAKTI_LEARNED_STREAM_PATH,
                "LRN1",
                &tick,
                "Jb",
                1U,
                "SCHOOL",
                "text",
                symbol,
                correct ? "right" : "wrong",
                status)) {
            return 0;
        }

        show_trial_progress(&runtime->school, correct);
    }

    {
        shakti_tick_t reward_tick;

        if (!shakti_tick_next(&runtime->clock, &reward_tick)) {
            return 0;
        }

        show_mastery_reward(&runtime->school);

        if (!shakti_log_append(
                SHAKTI_LEARNED_STREAM_PATH,
                "LRN1",
                &reward_tick,
                "Kn",
                1U,
                "SCHOOL",
                "visual_art",
                symbol,
                "reward",
                "happy_face") ||
            !shakti_log_append(
                SHAKTI_LEARNED_STREAM_PATH,
                "LRN1",
                &reward_tick,
                "Kn",
                2U,
                "SCHOOL",
                "actuator",
                symbol,
                "sound_request",
                "terminal_bell_coin_adapter_pending")) {
            return 0;
        }
    }

    return 1;
}
