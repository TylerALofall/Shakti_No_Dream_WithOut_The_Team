#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "shakti_handwriting.h"
#include "shakti_asset.h"
#include "shakti_artifact.h"
#include "shakti_loader.h"
#include "shakti_log.h"
#include "shakti_loop.h"
#include "shakti_memory.h"
#include "shakti_reason.h"
#include "shakti_receptor.h"
#include "shakti_school.h"
#include "shakti_score.h"
#include "shakti_time.h"
#include "shakti_types.h"

#define TEST_FACTS "tests/test_facts.txt"
#define TEST_THESAURUS "tests/test_thesaurus.txt"
#define TEST_EVIDENCE "tests/test_evidence.log"
#define TEST_LOG "tests/test_stream.log"
#define TEST_SCHOOL "tests/test_school.log"
#define TEST_GOAL "tests/test_goal.txt"
#define TEST_NOTEBOOK "tests/test_notebook.log"
#define TEST_MENU "tests/test_menu.txt"
#define TEST_LONG_TERM "tests/test_long_term.log"
#define TEST_WRITTEN "tests/test_written_text.txt"
#define TEST_FRAME "tests/test_frame_artifact.txt"
#define TEST_LOADER "tests/test_loader_fixture.txt"

static void write_text_file__test_shakti_h(const char *path, const char *text)
{
    FILE *file;

    file = fopen(path, "w");
    assert(file != NULL);
    assert(fputs(text, file) >= 0);
    assert(fclose(file) == 0);
}


static void test_exact_validation_score(void)
{
    shakti_validation_score_t score;
    shakti_validation_score_t total;
    char formatted[64];

    score = shakti_score_make(53UL, 53UL);
    assert(score.complete);
    assert(score.percent == 100U);
    assert(shakti_score_format(
        &score,
        formatted,
        sizeof(formatted)
    ));
    assert(strcmp(formatted, "53/53 = 100%") == 0);

    total = shakti_score_make(0UL, 0UL);
    shakti_score_add(&total, &score);
    shakti_score_add(
        &total,
        &(shakti_validation_score_t){ 10UL, 11UL, 90U, 0U }
    );
    assert(total.passed == 63UL);
    assert(total.required == 64UL);
    assert(total.percent == 98U);
    assert(!total.complete);
}

static void test_tick_order(void)
{
    shakti_tick_clock_t clock_state;
    shakti_tick_t first;
    shakti_tick_t second;
    char event_id[SHAKTI_EVENT_ID_CAPACITY];

    shakti_tick_clock_init(&clock_state);
    assert(shakti_tick_next(&clock_state, &first));
    assert(shakti_tick_next(&clock_state, &second));
    assert(
        second.epoch_seconds > first.epoch_seconds ||
        second.frame > first.frame
    );
    assert(first.frame < SHAKTI_FRAME_LIMIT);
    assert(second.frame < SHAKTI_FRAME_LIMIT);
    assert(shakti_event_id_format(
        &first,
        "Aa",
        1U,
        event_id,
        sizeof(event_id)
    ));
    assert(strstr(event_id, "]-[Aa]-[01]") != NULL);
}

static void test_text_log(void)
{
    shakti_tick_clock_t clock_state;
    shakti_tick_t tick;
    unsigned long valid;
    unsigned long invalid;

    write_text_file__test_shakti_h(TEST_LOG, "");
    shakti_tick_clock_init(&clock_state);
    assert(shakti_tick_next(&clock_state, &tick));

    assert(shakti_log_append(
        TEST_LOG,
        "LRN1",
        &tick,
        "Ca",
        1U,
        "TEST",
        "visual_art",
        "A",
        "color",
        "red"
    ));

    assert(shakti_log_validate_file(TEST_LOG, &valid, &invalid));
    assert(valid == 1UL);
    assert(invalid == 0UL);
    assert(strcmp(
        shakti_channel_name(SHAKTI_CHANNEL_TEXT),
        "text"
    ) == 0);
    assert(strcmp(
        shakti_channel_name(SHAKTI_CHANNEL_WRITTEN_TEXT),
        "written_text"
    ) == 0);
}

static void test_handwriting(void)
{
    char upper_rows[8][9];
    char lower_rows[8][9];
    FILE *file;
    char line[128];

    assert(shakti_handwriting_render_char('A', upper_rows));
    assert(shakti_handwriting_render_char('a', lower_rows));
    assert(memcmp(upper_rows, lower_rows, sizeof(upper_rows)) != 0);
    {
        unsigned int character;

        for (character = 32U; character <= 126U; ++character) {
            char text[2];

            text[0] = (char)character;
            text[1] = '\0';
            assert(shakti_handwriting_supports_text(text));
        }
    }

    assert(shakti_handwriting_supports_text("Aa 2+2=4"));
    assert(shakti_handwriting_write_text_artifact(
        "Aa",
        TEST_WRITTEN
    ));

    file = fopen(TEST_WRITTEN, "r");
    assert(file != NULL);
    assert(fgets(line, sizeof(line), file) != NULL);
    assert(strcmp(line, "SHAKTI_WRITTEN_TEXT_8X8_V1\n") == 0);
    assert(fgets(line, sizeof(line), file) != NULL);
    assert(strcmp(line, "TEXT=Aa\n") == 0);
    assert(fclose(file) == 0);
}

static void test_asset_keys(void)
{
    char value[SHAKTI_PATH_CAPACITY];

    assert(shakti_asset_filename(
        "one",
        ".wav",
        value,
        sizeof(value)
    ));
    assert(strcmp(value, "one.wav") == 0);

    assert(shakti_asset_filename(
        "Welcome to Eden",
        ".8x8.txt",
        value,
        sizeof(value)
    ));
    assert(strcmp(value, "Welcome_to_Eden.8x8.txt") == 0);

    assert(shakti_asset_filename(
        "/",
        ".svg",
        value,
        sizeof(value)
    ));
    assert(strcmp(value, "ascii_047.svg") == 0);

    assert(shakti_asset_filename(
        "2 + 2 = 4",
        ".svg",
        value,
        sizeof(value)
    ));
    assert(strcmp(value, "2_%2B_2_%3D_4.svg") == 0);
}

static void test_memory_and_loop(void)
{
    shakti_memory_state_t memory;
    shakti_loop_state_t loop;

    write_text_file__test_shakti_h(TEST_GOAL, "Ground one relationship.\n");
    write_text_file__test_shakti_h(TEST_NOTEBOOK, "Remember A.\n");
    write_text_file__test_shakti_h(
        TEST_MENU,
        "[Memory]\nRecall exact chunks.\n"
        "[School]\nRun exact drills.\n"
    );
    write_text_file__test_shakti_h(TEST_LONG_TERM, "");

    shakti_memory_init(&memory);
    assert(shakti_memory_load_always(
        &memory,
        TEST_GOAL,
        TEST_NOTEBOOK,
        TEST_MENU,
        TEST_LONG_TERM
    ));
    assert(memory.goal_loaded);
    assert(memory.notebook_loaded);
    assert(memory.menu_loaded);
    assert(memory.long_term_ready);

    shakti_loop_init(&loop);
    assert(shakti_loop_load(
        &loop,
        TEST_GOAL,
        TEST_NOTEBOOK,
        TEST_MENU
    ));
    assert(loop.menu_section_count == 2U);
    assert(shakti_loop_set_heartbeat(&loop, 0U));
    assert(shakti_loop_set_heartbeat(&loop, 30U));
    assert(!shakti_loop_set_heartbeat(&loop, 31U));

    shakti_loop_interrupt_tools(&loop);
    assert(!shakti_loop_tools_available(&loop));
    shakti_loop_resume_tools(&loop);
    assert(shakti_loop_tools_available(&loop));

    loop.reflection_due = 1U;
    assert(shakti_loop_defer_reflection(&loop));
    assert(shakti_loop_defer_reflection(&loop));
    assert(shakti_loop_defer_reflection(&loop));
    assert(!shakti_loop_defer_reflection(&loop));
}

static void test_reason_gate(void)
{
    shakti_reason_state_t state;
    shakti_decision_t decision;
    shakti_tick_clock_t clock_state;
    shakti_tick_t tick;
    unsigned int index;

    write_text_file__test_shakti_h(
        TEST_FACTS,
        "what follows a|B|sequence|tablet_a_b\n"
        "two times two|four|multiplication|tablet_math_2x2\n"
    );

    write_text_file__test_shakti_h(
        TEST_THESAURUS,
        "after|follows\n"
    );

    write_text_file__test_shakti_h(TEST_EVIDENCE, "");

    assert(shakti_reason_load(
        &state,
        TEST_FACTS,
        TEST_THESAURUS,
        TEST_EVIDENCE
    ));

    decision = shakti_reason_answer(&state, "two times two");
    assert(decision.kind == SHAKTI_DECISION_KNOWN);
    assert(strcmp(decision.answer, "four") == 0);

    decision = shakti_reason_answer(&state, "what after a");
    assert(decision.kind == SHAKTI_DECISION_KNOWN);
    assert(decision.used_creative_rewrite == 1);
    assert(strcmp(decision.answer, "B") == 0);

    decision = shakti_reason_answer(&state, "unknown question");
    assert(decision.kind == SHAKTI_DECISION_UNKNOWN);

    shakti_tick_clock_init(&clock_state);

    for (index = 0U; index < 6U; ++index) {
        assert(shakti_tick_next(&clock_state, &tick));
        assert(shakti_reason_record_evidence(
            &state,
            TEST_EVIDENCE,
            &tick,
            "grounded test",
            "answer",
            SHAKTI_SOURCE_OBSERVED,
            0
        ));
    }

    decision = shakti_reason_answer(&state, "grounded test");
    assert(decision.kind == SHAKTI_DECISION_KNOWN);
    assert(strcmp(decision.answer, "answer") == 0);
}

static void test_receptor_capture(void)
{
    shakti_receptor_t receptor;
    shakti_receptor_t repeat;
    shakti_tick_clock_t clock_state;
    shakti_memory_state_t memory;
    shakti_tick_t tick;
    unsigned long valid;
    unsigned long invalid;

    write_text_file__test_shakti_h(TEST_LOG, "");

    shakti_receptor_init(&receptor);
    shakti_tick_clock_init(&clock_state);

    assert(!shakti_receptor_configure(
        &receptor,
        0U,
        8U,
        2U,
        1U,
        1U
    ));
    assert(!shakti_receptor_configure(
        &receptor,
        8U,
        8U,
        2U,
        5U,
        1U
    ));
    assert(!shakti_receptor_configure(
        &receptor,
        9U,
        8U,
        2U,
        1U,
        1U
    ));
    assert(!shakti_receptor_configure(
        &receptor,
        8U,
        8U,
        2U,
        1U,
        3U
    ));
    assert(shakti_receptor_configure(
        &receptor,
        8U,
        8U,
        2U,
        1U,
        1U
    ));

    assert(!shakti_receptor_poll(&receptor));
    assert(!shakti_receptor_readout(&receptor));
    assert(shakti_receptor_open_shutter(&receptor, &clock_state));
    assert(!shakti_receptor_open_shutter(&receptor, &clock_state));
    assert(!shakti_receptor_readout(&receptor));
    assert(!shakti_receptor_poll(&receptor));
    assert(!shakti_receptor_readout(&receptor));
    assert(shakti_receptor_poll(&receptor));
    assert(shakti_receptor_readout(&receptor));
    assert(strlen(receptor.frame_text) == 64U);
    {
        unsigned int index;
        char copied[SHAKTI_RECEPTOR_TEXT_CAPACITY];

        for (index = 0U; index < 64U; ++index) {
            assert(receptor.frame_text[index] == '0' ||
                   receptor.frame_text[index] == '1');
        }

        assert(shakti_receptor_binarize_frame(
            receptor.frame_text,
            64U,
            copied,
            sizeof(copied)
        ));
        assert(strcmp(copied, receptor.frame_text) == 0);
        copied[0] = '2';
        assert(!shakti_receptor_binarize_frame(copied, 64U, copied,
            sizeof(copied)));
    }

    shakti_receptor_init(&repeat);
    assert(shakti_receptor_configure(
        &repeat,
        8U,
        8U,
        2U,
        1U,
        1U
    ));
    repeat.exposure_start = receptor.exposure_start;
    repeat.poll_count = receptor.exposure_ticks;
    repeat.shutter_open = 1U;
    assert(shakti_receptor_readout(&repeat));
    assert(strcmp(receptor.frame_text, repeat.frame_text) == 0);

    assert(shakti_receptor_write_frame_artifact(&receptor, TEST_FRAME));
    assert(shakti_artifact_validate_written_text(
        TEST_FRAME,
        receptor.frame_text
    ));

    assert(shakti_tick_next(&clock_state, &tick));
    assert(shakti_log_append(
        TEST_LOG,
        "LRN1",
        &tick,
        "Ca",
        1U,
        "AWAKE",
        shakti_channel_name(SHAKTI_CHANNEL_VISUAL_ART),
        "receptor_frame",
        "capture",
        receptor.frame_text
    ));
    assert(shakti_log_validate_file(TEST_LOG, &valid, &invalid));
    assert(valid == 1UL);
    assert(invalid == 0UL);

    shakti_memory_init(&memory);
    shakti_memory_remember(&memory, &tick, receptor.frame_text);
    assert(memory.working_count == 1U);
    assert(strcmp(memory.working[0].text, receptor.frame_text) == 0);
    assert(memory.working[0].tick.frame == tick.frame);
}

static void test_school_penalty(void)
{
    shakti_school_state_t state;
    shakti_tick_clock_t clock_state;
    shakti_tick_t tick;

    write_text_file__test_shakti_h(TEST_SCHOOL, "");
    shakti_school_init(&state);
    shakti_tick_clock_init(&clock_state);

    assert(shakti_tick_next(&clock_state, &tick));
    assert(shakti_school_set_pass(
        &state,
        TEST_SCHOOL,
        &tick,
        2U
    ));

    assert(shakti_tick_next(&clock_state, &tick));
    assert(shakti_school_record_trial(
        &state,
        TEST_SCHOOL,
        &tick,
        "A",
        1
    ));
    assert(state.current_streak == 1U);

    assert(shakti_tick_next(&clock_state, &tick));
    assert(shakti_school_record_trial(
        &state,
        TEST_SCHOOL,
        &tick,
        "A",
        0
    ));
    assert(state.current_streak == 0U);

    {
        shakti_school_state_t restored;

        assert(shakti_school_load(&restored, TEST_SCHOOL));
        assert(restored.pass == 2U);
        assert(restored.current_streak == 0U);
        assert(strcmp(restored.active_symbol, "A") == 0);
    }
}

static void test_loader(void)
{
    shakti_loader_result_t result;
    shakti_loader_kind_t kind;

    write_text_file__test_shakti_h(TEST_LOADER, "two");

    assert(shakti_loader_kind_from_text("text", &kind));
    assert(kind == SHAKTI_LOADER_KIND_TEXT);
    assert(!shakti_loader_kind_from_text("unknown", &kind));

    assert(shakti_loader_load(
        TEST_LOADER,
        SHAKTI_LOADER_KIND_TEXT,
        &result
    ));
    assert(result.kind == SHAKTI_LOADER_KIND_TEXT);
    assert(result.size == 3U);
    assert(memcmp(result.data, "two", 3U) == 0);
    assert(strcmp(result.path, TEST_LOADER) == 0);

    assert(shakti_loader_load(
        TEST_LOADER,
        SHAKTI_LOADER_KIND_SOUND_ART,
        &result
    ));
    assert(result.kind == SHAKTI_LOADER_KIND_SOUND_ART);
    assert(result.size == 3U);

    assert(!shakti_loader_load(
        "../etc/passwd",
        SHAKTI_LOADER_KIND_TEXT,
        &result
    ));
    assert(!shakti_loader_load(
        "tests/does_not_exist.txt",
        SHAKTI_LOADER_KIND_TEXT,
        &result
    ));
}

#define TEST_SHAKTI_MAIN main
int TEST_SHAKTI_MAIN(void)
{
    test_exact_validation_score();
    test_tick_order();
    test_text_log();
    test_handwriting();
    test_asset_keys();
    test_memory_and_loop();
    test_reason_gate();
    test_receptor_capture();
    test_school_penalty();
    test_loader();

    remove(TEST_FACTS);
    remove(TEST_THESAURUS);
    remove(TEST_EVIDENCE);
    remove(TEST_LOG);
    remove(TEST_SCHOOL);
    remove(TEST_GOAL);
    remove(TEST_NOTEBOOK);
    remove(TEST_MENU);
    remove(TEST_LONG_TERM);
    remove(TEST_WRITTEN);
    remove(TEST_FRAME);
    remove(TEST_LOADER);

    puts("All Shakti C99 tests passed.");

    return 0;
}
