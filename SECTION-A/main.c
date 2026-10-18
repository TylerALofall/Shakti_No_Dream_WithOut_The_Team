#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include "shakti_app.h"

#include "shakti_config.h"
#include "shakti_loader.h"
#include "shakti_log.h"
#include "shakti_loop.h"
#include "shakti_memory.h"
#include "shakti_manifest.h"
#include "shakti_reason.h"
#include "shakti_report.h"
#include "shakti_school.h"
#include "shakti_tablet.h"
#include "shakti_time.h"
#include "shakti_types.h"

static volatile sig_atomic_t external_interrupt_requested = 0;

static void handle_external_interrupt(int signal_number)
{
    (void)signal_number;
    external_interrupt_requested = 1;
}

static char *trim_text__app_h(char *text)
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

static int split_pipe_fields(
    char *text,
    char **fields,
    size_t field_count
)
{
    size_t index;
    char *cursor;

    cursor = text;

    for (index = 0U; index < field_count; ++index) {
        char *separator;

        fields[index] = trim_text__app_h(cursor);

        if (index + 1U == field_count) {
            return fields[index][0] != '\0' &&
                   strchr(fields[index], '|') == NULL;
        }

        separator = strchr(fields[index], '|');

        if (separator == NULL) {
            return 0;
        }

        *separator = '\0';
        fields[index] = trim_text__app_h(fields[index]);

        if (fields[index][0] == '\0') {
            return 0;
        }

        cursor = separator + 1;
    }

    return 0;
}

static int parse_source(
    const char *text,
    shakti_source_t *source
)
{
    if (strcmp(text, "observed") == 0) {
        *source = SHAKTI_SOURCE_OBSERVED;
        return 1;
    }

    if (strcmp(text, "curriculum") == 0) {
        *source = SHAKTI_SOURCE_CURRICULUM;
        return 1;
    }

    if (strcmp(text, "verified") == 0) {
        *source = SHAKTI_SOURCE_VERIFIED;
        return 1;
    }

    return 0;
}

static int parse_channel(
    const char *text,
    shakti_channel_t *channel
)
{
    if (strcmp(text, "text") == 0) {
        *channel = SHAKTI_CHANNEL_TEXT;
        return 1;
    }

    if (strcmp(text, "written_text") == 0) {
        *channel = SHAKTI_CHANNEL_WRITTEN_TEXT;
        return 1;
    }

    if (strcmp(text, "visual_art") == 0) {
        *channel = SHAKTI_CHANNEL_VISUAL_ART;
        return 1;
    }

    if (strcmp(text, "sound_art") == 0) {
        *channel = SHAKTI_CHANNEL_SOUND_ART;
        return 1;
    }

    return 0;
}

static int read_token(
    const char **cursor,
    char *destination,
    size_t destination_size
)
{
    const char *start;
    const char *end;
    size_t length;

    start = *cursor;

    while (*start == ' ' || *start == '\t') {
        start++;
    }

    if (*start == '\0') {
        return 0;
    }

    end = start;

    while (*end != '\0' &&
           *end != ' ' &&
           *end != '\t') {
        end++;
    }

    length = (size_t)(end - start);

    if (length == 0U || length >= destination_size) {
        return 0;
    }

    memcpy(destination, start, length);
    destination[length] = '\0';
    *cursor = end;

    return 1;
}

static int copy_remainder(
    const char *cursor,
    char *destination,
    size_t destination_size
)
{
    const char *start;
    size_t length;

    start = cursor;

    while (*start == ' ' || *start == '\t') {
        start++;
    }

    length = strlen(start);

    if (length == 0U || length >= destination_size) {
        return 0;
    }

    memcpy(destination, start, length + 1U);

    return 1;
}

static void print_decision(const shakti_decision_t *decision)
{
    if (decision->kind == SHAKTI_DECISION_KNOWN) {
        printf("Shakti: %s\n", decision->answer);
        printf(
            "Evidence score %u. Lead %u. %s\n",
            decision->score,
            decision->margin,
            decision->reason
        );
        return;
    }

    if (decision->kind == SHAKTI_DECISION_CAUTIOUS) {
        printf(
            "Shakti: My best grounded answer is %s, "
            "and logic marks it cautious.\n",
            decision->answer
        );
        printf(
            "Evidence score %u. Lead %u. %s\n",
            decision->score,
            decision->margin,
            decision->reason
        );
        return;
    }

    puts("Shakti: I do not know.");

    if (decision->reason[0] != '\0') {
        printf("%s\n", decision->reason);
    }

    if (decision->creative_candidates_examined > 0U) {
        printf(
            "Creative candidates examined: %u. Logic authorized zero.\n",
            decision->creative_candidates_examined
        );
    }
}

static int log_tool_call(shakti_runtime_t *runtime, const char *tool, const char *arguments)
{
    return shakti_loop_note_tool(runtime, tool, arguments);
}

static int handle_load(
    shakti_runtime_t *runtime,
    char *arguments
)
{
    char *fields[2];
    shakti_loader_kind_t kind;
    shakti_loader_result_t result;

    (void)runtime;

    if (!split_pipe_fields(arguments, fields, 2U) ||
        !shakti_loader_kind_from_text(fields[1], &kind)) {
        puts(
            "Use: load path | text, written_text, visual_art, or sound_art"
        );
        return 1;
    }

    if (!shakti_loader_load(fields[0], kind, &result)) {
        puts("Load failed.");
        return 1;
    }

    printf(
        "Inspected %s (%s): %lu bytes. Native sensory input uses the frame queue.\n",
        result.path,
        shakti_loader_kind_name(result.kind),
        (unsigned long)result.size
    );

    return 1;
}

static int handle_ask(
    shakti_runtime_t *runtime,
    const char *question
)
{
    shakti_decision_t decision;
    shakti_tick_t tick;
    const char *output;

    if (strlen(question) >= sizeof(runtime->loop.focus_question)) {
        fputs("BENCH question exceeds fixed capacity\n", stderr);
        return 1;
    }
    memcpy(runtime->loop.focus_question, question, strlen(question) + 1U);
    if (runtime->memory.recall_file == NULL && question[0] != '\0')
        (void)shakti_memory_request(&runtime->memory, question);

    decision = shakti_reason_answer(&runtime->reason, question);
    print_decision(&decision);

    output = decision.kind == SHAKTI_DECISION_UNKNOWN
        ? "I do not know."
        : decision.answer;

    if (!shakti_tick_next(&runtime->clock, &tick) ||
        !shakti_log_append(
            SHAKTI_LEARNED_STREAM_PATH,
            "LRN1",
            &tick,
            "Jc",
            1U,
            "AWAKE",
            "text",
            question,
            decision.kind == SHAKTI_DECISION_UNKNOWN
                ? "abstain"
                : "answer",
            output)) {
        return 0;
    }

    if (!shakti_memory_remember(&runtime->memory, &tick, output)) return 0;

    return 1;
}

static int handle_evidence(
    shakti_runtime_t *runtime,
    char *arguments,
    int contradiction
)
{
    char *fields[3];
    shakti_source_t source;
    shakti_tick_t tick;

    if (!split_pipe_fields(arguments, fields, 3U) ||
        !parse_source(fields[2], &source)) {
        puts(
            "Use: learn question | answer | observed, curriculum, or verified"
        );
        return 1;
    }

    if (!shakti_tick_next(&runtime->clock, &tick) ||
        !shakti_reason_record_evidence(
            &runtime->reason,
            SHAKTI_EVIDENCE_PATH,
            &tick,
            fields[0],
            fields[1],
            source,
            contradiction)) {
        return 0;
    }

    puts(
        contradiction
            ? "Contradiction recorded."
            : "Grounded evidence recorded."
    );

    return 1;
}

static int handle_sense(
    shakti_runtime_t *runtime,
    const char *arguments
)
{
    char channel_text[24];
    char subject[SHAKTI_ANSWER_CAPACITY];
    char property[SHAKTI_RELATION_CAPACITY];
    char value[SHAKTI_TEXT_CAPACITY];
    unsigned char binary[SHAKTI_TEXT_CAPACITY + 7U];
    unsigned char rebuilt[SHAKTI_TEXT_CAPACITY];
    shakti_channel_t channel;
    shakti_tick_t tick;

    if (!read_token(
            &arguments,
            channel_text,
            sizeof(channel_text)) ||
        !read_token(
            &arguments,
            subject,
            sizeof(subject)) ||
        !read_token(
            &arguments,
            property,
            sizeof(property)) ||
        !copy_remainder(
            arguments,
            value,
            sizeof(value)) ||
        !parse_channel(channel_text, &channel)) {
        puts(
            "Use: sense text|written_text "
            "subject property value"
        );
        return 1;
    }

    if (channel == SHAKTI_CHANNEL_VISUAL_ART || channel == SHAKTI_CHANNEL_SOUND_ART) {
        puts("Pixel/PCM input requires the native frame queue; symbolic media is not admitted.");
        return 1;
    }
    if (!shakti_binary_encode((const unsigned char *)value, strlen(value), binary, sizeof(binary)) ||
        !shakti_binary_rebuild(binary, strlen(value), rebuilt, sizeof(rebuilt)) ||
        memcmp(value, rebuilt, strlen(value)) != 0) {
        fputs("BENCH text reconstruction mismatch\n", stderr);
        return 1;
    }

    if (!shakti_tick_next(&runtime->clock, &tick) ||
        !shakti_log_append(
            SHAKTI_LEARNED_STREAM_PATH,
            "LRN1",
            &tick,
            "Ca",
            1U,
            "AWAKE",
            shakti_channel_name(channel),
            subject,
            property,
            value)) {
        return 0;
    }

    if (!shakti_memory_remember(&runtime->memory, &tick, value)) return 0;
    puts("Validated text context accepted.");

    return 1;
}

static int handle_pass(
    shakti_runtime_t *runtime,
    const char *arguments
)
{
    char *end;
    unsigned long pass;
    shakti_tick_t tick;

    pass = strtoul(arguments, &end, 10);

    while (*end != '\0' &&
           isspace((unsigned char)*end)) {
        end++;
    }

    if (end == arguments ||
        *end != '\0' ||
        pass < 1UL ||
        pass > 4UL) {
        puts("Use: pass 1, pass 2, pass 3, or pass 4.");
        return 1;
    }

    if (!shakti_tick_next(&runtime->clock, &tick) ||
        !shakti_school_set_pass(
            &runtime->school,
            SHAKTI_SCHOOL_PATH,
            &tick,
            (unsigned int)pass)) {
        return 0;
    }

    printf("School pass is now %lu.\n", pass);

    if (pass == 2UL) {
        puts("");
        puts("  .-.");
        puts(" ( ^ )");
        puts("  '-'");
        fputc('\a', stdout);
        fflush(stdout);

        if (!shakti_log_append(
                SHAKTI_LEARNED_STREAM_PATH,
                "LRN1",
                &tick,
                "Ic",
                1U,
                "SCHOOL",
                "visual_art",
                "pass_transition",
                "reward",
                "happy_face") ||
            !shakti_log_append(
                SHAKTI_LEARNED_STREAM_PATH,
                "LRN1",
                &tick,
                "Ic",
                2U,
                "SCHOOL",
                "internal",
                "pass_transition",
                "sound_request",
                "coin_adapter_pending_confirmation")) {
            return 0;
        }
    }

    return 1;
}

static int handle_tablet(
    shakti_runtime_t *runtime,
    const char *arguments
)
{
    char xml_path[SHAKTI_PATH_CAPACITY];
    char artifact_root[SHAKTI_PATH_CAPACITY];

    if (!read_token(
            &arguments,
            xml_path,
            sizeof(xml_path)) ||
        !read_token(
            &arguments,
            artifact_root,
            sizeof(artifact_root)) ||
        trim_text__app_h((char *)arguments)[0] != '\0') {
        puts("Use: tablet XML_PATH ARTIFACT_ROOT");
        return 1;
    }

    if (!shakti_tablet_load(
            &runtime->tablet,
            xml_path,
            artifact_root)) {
        puts(
            "Tablet load failed. Check its four exact channels and artifact "
            "paths."
        );
        return 1;
    }

    shakti_tablet_print(&runtime->tablet);

    return 1;
}

static int handle_manifest(
    shakti_runtime_t *runtime,
    const char *arguments
)
{
    char manifest_path[SHAKTI_PATH_CAPACITY];
    char ledger_path[SHAKTI_PATH_CAPACITY];

    if (!read_token(
            &arguments,
            manifest_path,
            sizeof(manifest_path)) ||
        !read_token(
            &arguments,
            ledger_path,
            sizeof(ledger_path)) ||
        trim_text__app_h((char *)arguments)[0] != '\0') {
        puts("Use: manifest MANIFEST_XML LEDGER_TXT");
        return 1;
    }

    if (!shakti_manifest_load(
            &runtime->manifest,
            manifest_path)) {
        puts(
            "Manifest load failed. Check level order, prerequisites, "
            "tablet metadata, and XML structure."
        );
        return 1;
    }

    if (!shakti_manifest_write_ledger(
            &runtime->manifest,
            manifest_path,
            ledger_path)) {
        puts(
            "Manifest loaded and ledger written, but a ready tablet "
            "failed validation."
        );
        return 1;
    }

    shakti_manifest_print(&runtime->manifest);
    printf("Manifest ledger written to %s.\n", ledger_path);

    return 1;
}

static int validate_logs(void)
{
    unsigned long valid;
    unsigned long invalid;

    if (!shakti_log_validate_file(
            SHAKTI_LEARNED_STREAM_PATH,
            &valid,
            &invalid)) {
        puts("The Learned stream could not be opened.");
        return 1;
    }

    printf(
        "Learned stream: %lu valid lines, %lu invalid lines.\n",
        valid,
        invalid
    );

    return invalid == 0UL;
}

static void print_status(const shakti_runtime_t *runtime)
{
    printf(
        "Eden facts: %lu of %u.\n",
        (unsigned long)runtime->reason.fact_count,
        (unsigned int)SHAKTI_MAX_FACTS
    );
    printf(
        "Approved thesaurus entries: %lu of %u.\n",
        (unsigned long)runtime->reason.synonym_count,
        (unsigned int)SHAKTI_MAX_SYNONYMS
    );
    printf(
        "Learned evidence entries: %lu of %u.\n",
        (unsigned long)runtime->reason.evidence_count,
        (unsigned int)SHAKTI_MAX_EVIDENCE
    );
    printf(
        "School pass: %u. Active text: %s. Progress: %u of %u.\n",
        runtime->school.pass,
        runtime->school.active_symbol[0] != '\0'
            ? runtime->school.active_symbol
            : "none",
        runtime->school.current_streak,
        runtime->school.mastery_target
    );

    shakti_memory_print_status(&runtime->memory);
    shakti_loop_print_status(&runtime->loop);

    printf(
        "Active tablet: %s, stones=%lu.\n",
        runtime->tablet.loaded ? runtime->tablet.lesson : "none",
        (unsigned long)runtime->tablet.stone_count
    );
    printf(
        "Eden manifest: %s, levels=%lu, tablets=%lu.\n",
        runtime->manifest.loaded ? runtime->manifest.status : "missing",
        (unsigned long)runtime->manifest.level_count,
        (unsigned long)runtime->manifest.tablet_count
    );
    printf(
        "Total fixed runtime state: %lu bytes.\n",
        (unsigned long)sizeof(*runtime)
    );
    puts(
        "Channels: text, written_text, visual_art, sound_art."
    );
    puts(
        "Creative proposes. Logic cross-validates and authorizes."
    );
}

static void print_help(void)
{
    puts("Loop controls:");
    puts("/heartbeat/ 1|2|4|8|16|32|64 (beats per second)");
    puts("/memory_decay/ BEATS (0 leaves timed expiry disabled)");
    puts("/goal/ system message");
    puts("/notebook/");
    puts("/notebook/ reminder or note");
    puts("/menu/");
    puts("/menu/ SECTION_TITLE");
    puts("/shakti_run/ TOOL ARGUMENTS");
    puts("/message_tyler/ message");
    puts("/note_tyler/ note");
    puts("/message_shakti/ message");
    puts("/note_shakti/ note");
    puts("/reflection/");
    puts("/reflection/answer/ 1..13 actual answer");
    puts("/trial/ ANSWER (one handwriting trial; the loop continues)");
    puts("/reflection/questions/");
    puts("/interrupt/");
    puts("/resume/");
    puts("/status/");
    puts("/help/");
    puts("/quit/ closes the tool gate; the heartbeat continues.");
    puts("");
    puts("MCP tools:");
    puts("ask question");
    puts("learn question | answer | observed");
    puts("learn question | answer | curriculum");
    puts("learn question | answer | verified");
    puts("reject question | answer | observed");
    puts(
        "sense text|written_text "
        "subject property value"
    );
    puts("pass 1|2|3|4");
    puts("school exact-text");
    puts("draft exact-text");
    puts("tablet XML_PATH ARTIFACT_ROOT");
    puts("manifest MANIFEST_XML LEDGER_TXT");
    puts("load path | text|written_text|visual_art|sound_art");
    puts("recall trigger words");
    puts("remember id | tags | recent|training | complete text");
    puts("activate id (whole container already in RAM)");
    puts("validate");
    puts("status");
}

static int process_tool(
    shakti_runtime_t *runtime,
    char *command
)
{
    char *arguments;

    if (!shakti_loop_tools_available(&runtime->loop)) {
        puts(
            "MCP tools are interrupted. Shakti remains awake. "
            "Use /resume/ to restore tool calls."
        );
        return 1;
    }

    if (runtime->loop.reflection_due) {
        puts("The required 13-answer reflection must complete before another tool call.");
        return 1;
    }

    command = trim_text__app_h(command);

    if (command[0] == '\0') {
        puts("Use: /shakti_run/ TOOL ARGUMENTS");
        return 1;
    }

    arguments = strchr(command, ' ');

    if (arguments != NULL) {
        *arguments = '\0';
        arguments = trim_text__app_h(arguments + 1);
    } else {
        arguments = command + strlen(command);
    }

    {
        static const char * const names[] = {
            "ask", "learn", "reject", "sense", "pass", "school", "draft",
            "tablet", "manifest", "load", "recall", "remember", "activate", "validate", "status"
        };
        size_t i;
        int known = 0;
        for (i = 0U; i < sizeof(names) / sizeof(names[0]); ++i)
            if (strcmp(command, names[i]) == 0) known = 1;
        if (!known) { puts("Unknown MCP tool."); return 1; }
    }

    if (!log_tool_call(runtime, command, arguments)) {
        return 0;
    }

    if (strcmp(command, "ask") == 0) {
        return handle_ask(runtime, arguments);
    }

    if (strcmp(command, "learn") == 0) {
        return handle_evidence(runtime, arguments, 0);
    }

    if (strcmp(command, "reject") == 0) {
        return handle_evidence(runtime, arguments, 1);
    }

    if (strcmp(command, "sense") == 0) {
        return handle_sense(runtime, arguments);
    }

    if (strcmp(command, "pass") == 0) {
        return handle_pass(runtime, arguments);
    }

    if (strcmp(command, "school") == 0) {
        return shakti_school_run_drill(runtime, arguments);
    }

    if (strcmp(command, "draft") == 0) {
        return shakti_school_draft_snapshots(runtime, arguments);
    }

    if (strcmp(command, "tablet") == 0) {
        return handle_tablet(runtime, arguments);
    }

    if (strcmp(command, "manifest") == 0) {
        return handle_manifest(runtime, arguments);
    }

    if (strcmp(command, "load") == 0) {
        return handle_load(runtime, arguments);
    }

    if (strcmp(command, "remember") == 0) {
        char text[SHAKTI_INPUT_CAPACITY];
        char *fields[4];
        shakti_tick_t tick;
        if (strlen(arguments) >= sizeof(text)) return 0;
        memcpy(text, arguments, strlen(arguments) + 1U);
        if (!split_pipe_fields(text, fields, 4U) ||
            (strcmp(fields[2], "recent") != 0 && strcmp(fields[2], "training") != 0)) {
            puts("Use: remember id | tags | recent|training | complete text");
            return 1;
        }
        if (!shakti_tick_next(&runtime->clock, &tick) ||
            !shakti_memory_store(&runtime->memory, &tick, fields[0], fields[1], fields[3],
                strcmp(fields[2], "training") == 0)) return 0;
        puts("Whole memory container stored and active.");
        return 1;
    }

    if (strcmp(command, "activate") == 0) {
        puts(shakti_memory_activate(&runtime->memory, arguments)
            ? "Whole RAM container active in convergence." : "Container is not in RAM; recall it first.");
        return 1;
    }

    if (strcmp(command, "recall") == 0) {
        puts(shakti_memory_request(&runtime->memory, arguments)
            ? "Whole-container memory lookup queued." : "Memory lookup is busy or refused.");
        return 1;
    }

    if (strcmp(command, "validate") == 0) {
        return validate_logs();
    }

    if (strcmp(command, "status") == 0) {
        print_status(runtime);
        return 1;
    }

    puts("Unknown MCP tool. Use /menu/ or /help/.");

    return 1;
}

static int parse_unsigned__app_h(
    const char *text,
    unsigned int *value
)
{
    char *end;
    unsigned long parsed;

    parsed = strtoul(text, &end, 10);

    while (*end != '\0' &&
           isspace((unsigned char)*end)) {
        end++;
    }

    if (end == text ||
        *end != '\0' ||
        parsed > 0xFFFFFFFFUL) {
        return 0;
    }

    *value = (unsigned int)parsed;

    return 1;
}

static int process_control(
    shakti_runtime_t *runtime,
    char *line
)
{
    if (strcmp(line, "/quit/") == 0) {
        shakti_loop_interrupt_tools(&runtime->loop);
        puts("Tool gate closed. Shakti continues cycling; no reply is required.");
        return 1;
    }

    if (strcmp(line, "/help/") == 0) {
        print_help();
        return 1;
    }

    if (strcmp(line, "/status/") == 0) {
        print_status(runtime);
        return 1;
    }

    if (strcmp(line, "/interrupt/") == 0) {
        shakti_loop_interrupt_tools(&runtime->loop);
        puts("MCP tools stopped. Shakti remains awake.");
        return 1;
    }

    if (strcmp(line, "/resume/") == 0) {
        shakti_loop_resume_tools(&runtime->loop);
        puts("MCP tools restored.");
        return 1;
    }

    if (strcmp(line, "/notebook/") == 0) {
        return shakti_loop_notebook(runtime, "");
    }

    if (strncmp(line, "/notebook/ ", 11U) == 0) {
        return shakti_loop_notebook(
            runtime,
            trim_text__app_h(line + 11U)
        );
    }

    if (strcmp(line, "/menu/") == 0) {
        shakti_loop_menu_titles(&runtime->loop);
        return 1;
    }

    if (strncmp(line, "/menu/ ", 7U) == 0) {
        if (!shakti_loop_menu_section(
                &runtime->loop,
                trim_text__app_h(line + 7U))) {
            puts("Menu section not found.");
        }

        return 1;
    }

    if (strncmp(line, "/shakti_run/ ", 13U) == 0) {
        return process_tool(runtime, line + 13U);
    }

    if (strncmp(line, "/heartbeat/ ", 12U) == 0) {
        unsigned int rate;
        if (!parse_unsigned__app_h(trim_text__app_h(line + 12U), &rate) ||
            !shakti_loop_set_heartbeat(&runtime->loop, rate)) {
            puts("Heartbeat accepts 1, 2, 4, 8, 16, 32, or 64 beats per second.");
        } else {
            printf("Heartbeat: %u beats/second. Every channel follows.\n", rate);
        }
        return 1;
    }
    if (strncmp(line, "/memory_decay/ ", 15U) == 0) {
        unsigned int beats;
        if (!parse_unsigned__app_h(trim_text__app_h(line + 15U), &beats)) {
            puts("Use: /memory_decay/ BEATS");
        } else {
            runtime->memory.recent_decay_beats = beats;
            printf("Recent-memory timed expiry: %u heartbeat beats; permanent lessons retained.\n", beats);
        }
        return 1;
    }
    if (strncmp(line, "/trial/ ", 8U) == 0)
        return shakti_school_submit_trial(runtime, trim_text__app_h(line + 8U));

    if (strncmp(line, "/goal/ ", 7U) == 0) {
        if (!shakti_loop_set_goal(
                runtime,
                trim_text__app_h(line + 7U))) {
            puts("Goal update failed.");
        }

        return 1;
    }

    if (strncmp(line, "/message_tyler/ ", 16U) == 0) {
        return shakti_loop_record_message(
            runtime,
            "message_tyler",
            trim_text__app_h(line + 16U)
        );
    }

    if (strncmp(line, "/note_tyler/ ", 13U) == 0) {
        return shakti_loop_record_message(
            runtime,
            "note_tyler",
            trim_text__app_h(line + 13U)
        );
    }

    if (strncmp(line, "/message_shakti/ ", 17U) == 0) {
        return shakti_loop_record_message(
            runtime,
            "message_shakti",
            trim_text__app_h(line + 17U)
        );
    }

    if (strncmp(line, "/note_shakti/ ", 14U) == 0) {
        return shakti_loop_record_message(
            runtime,
            "note_shakti",
            trim_text__app_h(line + 14U)
        );
    }

    if (strcmp(line, "/reflection/questions/") == 0) {
        shakti_loop_print_reflection_questions(stdout);
        return 1;
    }

    if (strncmp(line, "/reflection/answer/ ", sizeof("/reflection/answer/ ") - 1U) == 0) {
        const char *value = line + sizeof("/reflection/answer/ ") - 1U;
        char *end;
        unsigned long question = strtoul(value, &end, 10);
        if (end == value || !isspace((unsigned char)*end) ||
            question < 1UL || question > SHAKTI_REFLECTION_QUESTIONS ||
            !shakti_loop_answer_reflection(runtime, (unsigned int)question,
                trim_text__app_h(end)))
            puts("Use /reflection/answer/ 1..13 actual answer while reflection is pending.");
        return 1;
    }

    if (strcmp(line, "/reflection/defer/") == 0) {
        puts("Reflection requires all thirteen actual answers. The heartbeat continues.");
        return 1;
    }

    if (strcmp(line, "/reflection/") == 0) {
        return shakti_loop_run_reflection(
            runtime,
            stdin,
            stdout
        );
    }

    puts("Unknown loop command. Use /help/.");

    return 1;
}


static int resolve_manifest_path(
    const char *manifest_path,
    const char *relative_path,
    char *destination,
    size_t destination_size
)
{
    const char *slash;
    const char *backslash;
    const char *separator;
    size_t directory_length;
    int written;

    if (manifest_path == NULL ||
        relative_path == NULL ||
        destination == NULL ||
        destination_size == 0U) {
        return 0;
    }

    if (relative_path[0] == '/' ||
        relative_path[0] == '\\' ||
        (relative_path[0] != '\0' &&
         relative_path[1] == ':')) {
        written = snprintf(
            destination,
            destination_size,
            "%s",
            relative_path
        );

        return written >= 0 &&
               (size_t)written < destination_size;
    }

    slash = strrchr(manifest_path, '/');
    backslash = strrchr(manifest_path, '\\');

    if (slash == NULL) {
        separator = backslash;
    } else if (backslash == NULL || slash > backslash) {
        separator = slash;
    } else {
        separator = backslash;
    }

    directory_length = separator == NULL
        ? 0U
        : (size_t)(separator - manifest_path) + 1U;

    if (directory_length + strlen(relative_path) + 1U >
        destination_size) {
        return 0;
    }

    if (directory_length > 0U) {
        memcpy(destination, manifest_path, directory_length);
    }

    memcpy(
        destination + directory_length,
        relative_path,
        strlen(relative_path) + 1U
    );

    return 1;
}

static int initialize_runtime(shakti_runtime_t *runtime)
{
    if (runtime == NULL) {
        return 0;
    }

    memset(runtime, 0, sizeof(*runtime));
    shakti_tick_clock_init(&runtime->clock);
    shakti_memory_init(&runtime->memory);
    shakti_loop_init(&runtime->loop);
    shakti_tablet_init(&runtime->tablet);
    shakti_manifest_init(&runtime->manifest);

    return shakti_reason_load(
               &runtime->reason,
               SHAKTI_EDEN_FACTS_PATH,
               SHAKTI_EDEN_THESAURUS_PATH,
               SHAKTI_EVIDENCE_PATH) &&
           shakti_school_load(
               &runtime->school,
               SHAKTI_SCHOOL_PATH) &&
           shakti_loop_load(
               &runtime->loop,
               SHAKTI_GOAL_PATH,
               SHAKTI_NOTEBOOK_PATH,
               SHAKTI_MENU_PATH) &&
           shakti_memory_load_always(
               &runtime->memory,
               SHAKTI_GOAL_PATH,
               SHAKTI_NOTEBOOK_PATH,
               SHAKTI_MENU_PATH,
               SHAKTI_LEARNED_STREAM_PATH) &&
           shakti_manifest_load(
               &runtime->manifest,
               SHAKTI_EDEN_MANIFEST_PATH);
}

static int run_startup_report(
    shakti_runtime_t *runtime,
    shakti_report_t *report
)
{
    return shakti_report_run(
        &runtime->manifest,
        SHAKTI_EDEN_MANIFEST_PATH,
        SHAKTI_EDEN_LEDGER_PATH,
        SHAKTI_STARTUP_REPORT_PATH,
        &runtime->school,
        report
    );
}

static int write_startup_anchor(shakti_runtime_t *runtime)
{
    shakti_tick_t tick;

    if (runtime == NULL ||
        !shakti_tick_next(&runtime->clock, &tick)) {
        return 0;
    }

    puts("");
    puts("Shakti.");
    puts("Welcome to Eden.");
    puts("");

    return shakti_log_append(
               SHAKTI_LEARNED_STREAM_PATH,
               "LRN1",
               &tick,
               "Aa",
               1U,
               "STARTUP",
               "text",
               "Shakti",
               "security_anchor",
               "Shakti") &&
           shakti_log_append(
               SHAKTI_LEARNED_STREAM_PATH,
               "LRN1",
               &tick,
               "Aa",
               2U,
               "STARTUP",
               "text",
               "Welcome_to_Eden",
               "security_anchor",
               "Welcome to Eden");
}

static int run_first_lesson_demo(shakti_runtime_t *runtime)
{
    size_t tablet_index;
    size_t stone_index;
    char tablet_path[SHAKTI_PATH_CAPACITY];
    char artifact_root[SHAKTI_PATH_CAPACITY];
    shakti_tablet_audit_t audit;
    shakti_validation_score_t score;
    const shakti_manifest_tablet_t *entry;

    entry = NULL;

    for (tablet_index = 0U;
         tablet_index < runtime->manifest.tablet_count;
         ++tablet_index) {
        if (strcmp(
                runtime->manifest.tablets[tablet_index].state,
                "ready") == 0 ||
            strcmp(
                runtime->manifest.tablets[tablet_index].state,
                "staged") == 0) {
            entry = &runtime->manifest.tablets[tablet_index];
            break;
        }
    }

    if (entry == NULL) {
        puts("Demo unavailable: no validated staged or ready tablet exists.");
        return 0;
    }

    if (!resolve_manifest_path(
            SHAKTI_EDEN_MANIFEST_PATH,
            entry->path,
            tablet_path,
            sizeof(tablet_path)) ||
        !resolve_manifest_path(
            SHAKTI_EDEN_MANIFEST_PATH,
            runtime->manifest.artifact_root,
            artifact_root,
            sizeof(artifact_root)) ||
        !shakti_tablet_load(
            &runtime->tablet,
            tablet_path,
            artifact_root) ||
        !shakti_tablet_audit(
            &runtime->tablet,
            artifact_root,
            &audit)) {
        puts("Demo stopped: the first staged or ready tablet failed validation.");
        return 0;
    }

    score = audit.score;

    puts("PASS 1 — WATCH");
    printf(
        "Tablet: %s. Verified checks: %lu/%lu = %u%%.\n",
        runtime->tablet.lesson,
        score.passed,
        score.required,
        score.percent
    );
    puts(
        "This demonstration does not change mastery or create a learned answer."
    );
    puts("");

    for (stone_index = 0U;
         stone_index < runtime->tablet.stone_count &&
         stone_index < 5U;
         ++stone_index) {
        const shakti_stone_t *stone;

        stone = &runtime->tablet.stones[stone_index];

        printf(
            "Stone %u: %s\n",
            stone->order,
            stone->text
        );

        if (!shakti_school_show_written_text_pixels(stone->text)) {
            puts("The written-text atlas could not render this stone.");
            return 0;
        }

        printf("sound_art: %s\n", stone->sound_art);
        printf("visual_art: %s\n\n", stone->visual_art);
    }

    puts("Demo complete. School mastery remains unchanged.");

    return 1;
}

static void print_usage(const char *program_name)
{
    printf(
        "Usage: %s [--check | --demo]\n",
        program_name != NULL ? program_name : "shakti"
    );
    puts("  --check  Validate startup data and exit.");
    puts("  --demo   Run a read-only Pass 1 demonstration and exit.");
    puts("  --cycles N  Bounded local run; default operation remains continuous.");
    puts("  --epoch N   Explicit epoch origin for reproducible local runs.");
    puts("  no flag     Continuous heartbeat loop; input and replies are optional.");
}

#ifdef SHAKTI_APP_NO_MAIN
#define SHAKTI_APP_MAIN shakti_app_main
#else
#define SHAKTI_APP_MAIN main
#endif

int shakti_app_initialize(shakti_runtime_t *runtime)
{
    if (!initialize_runtime(runtime)) return 0;
    runtime->recorded_epoch = runtime->memory.serial
        ? runtime->memory.newest_epoch : (time_t)-1;
    if (!shakti_log_latest_epoch(SHAKTI_LEARNED_STREAM_PATH, &runtime->recorded_epoch) ||
        !shakti_log_latest_epoch(SHAKTI_MESSAGES_PATH, &runtime->recorded_epoch) ||
        !shakti_log_latest_epoch(SHAKTI_REFLECTION_PATH, &runtime->recorded_epoch) ||
        !shakti_loop_restore(runtime)) return 0;
    if (runtime->recorded_epoch >= runtime->clock.last_epoch_seconds) {
        if (runtime->recorded_epoch >= (time_t)LONG_MAX) return 0;
        shakti_tick_clock_seed(&runtime->clock, runtime->recorded_epoch + (time_t)1);
    }
    shakti_media_init(&runtime->media);
    shakti_memory_bind_eden(&runtime->memory, &runtime->reason);
    return 1;
}

int shakti_app_queue_frame(shakti_runtime_t *runtime, const shakti_media_frame_t *frame)
{
    shakti_media_state_t before;
    shakti_tick_t tick;
    char event[SHAKTI_TEXT_CAPACITY];
    int n;
    if (runtime == NULL || frame == NULL) return 0;
    before = runtime->media;
    if (!shakti_media_queue(&runtime->media, frame)) return 0;
    n = snprintf(event, sizeof(event), "source=%s position64=%" PRIu64 " lane=%u bytes=%lu",
        frame->source_id, frame->position64, frame->lane, (unsigned long)frame->bytes);
    if (n < 0 || (size_t)n >= sizeof(event) || !shakti_tick_next(&runtime->clock, &tick) ||
        !shakti_log_append(SHAKTI_LEARNED_STREAM_PATH, "LRN1", &tick,
            "shakti_media.c:shakti_media_queue", 11U, "AWAKE", "internal",
            frame->source_id, "loading_zone", event)) {
        runtime->media = before;
        return 0;
    }
    return 1;
}

int shakti_app_convergence_view(const shakti_runtime_t *runtime, shakti_convergence_view_t *view)
{
    if (runtime == NULL || view == NULL) return 0;
    view->sensory = shakti_media_current(&runtime->media);
    view->eden = runtime->memory.eden;
    view->working = runtime->memory.working;
    view->working_count = runtime->memory.working_count;
    view->thought = runtime->loop.current_thought;
    view->tick.epoch_seconds = runtime->clock.last_epoch_seconds;
    view->tick.frame = runtime->clock.next_frame ? runtime->clock.next_frame - 1U : 0U;
    return 1;
}

int shakti_app_cycle(shakti_runtime_t *runtime, const char *command)
{
    char input[SHAKTI_INPUT_CAPACITY];
    shakti_tick_t tick;
    int ok = 1;
    if (runtime == NULL || !shakti_loop_begin_cycle(runtime, &tick)) return 0;
    if (command != NULL && command[0] != '\0') {
        if (strlen(command) >= sizeof(input)) {
            fputs("BENCH input exceeds fixed capacity\n", stderr);
        } else {
            char *line;
            memcpy(input, command, strlen(command) + 1U);
            line = trim_text__app_h(input);
            if (line[0] != '\0') {
                if (!shakti_loop_record_message(runtime, "message_shakti", line))
                    ok = 0;
                else if (!process_control(runtime, line)) ok = 0;
            }
        }
    }
    if (!shakti_loop_finish_cycle(runtime)) ok = 0;
    if (!ok) runtime->loop.tools_interrupted = 1U;
    return ok;
}

typedef struct {
    char text[SHAKTI_INPUT_CAPACITY];
    size_t used;
    int open;
    int discarding;
} console_input_t;

/* A partial line is retained; neither a partial line nor EOF blocks a beat. */
static int console_poll(console_input_t *state)
{
    size_t count;
    if (!state->open) return 0;
    for (count = 0U; count < SHAKTI_INPUT_CAPACITY; ++count) {
        unsigned char c;
        ssize_t got = read(STDIN_FILENO, &c, 1U);
        if (got == 0) {
            state->open = 0;
            if (state->used && !state->discarding) {
                state->text[state->used] = '\0';
                state->used = 0U;
                return 1;
            }
            return 0;
        }
        if (got < 0) {
            if (errno == EINTR) continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) state->open = 0;
            return 0;
        }
        if (c == '\n') {
            if (state->discarding) {
                state->discarding = 0;
                state->used = 0U;
                continue;
            }
            state->text[state->used] = '\0';
            state->used = 0U;
            return 1;
        }
        if (state->discarding) continue;
        if (c == 0U || state->used + 1U >= sizeof(state->text)) {
            fputs("BENCH invalid or over-capacity console input; line refused\n", stderr);
            state->discarding = 1;
            state->used = 0U;
            continue;
        }
        state->text[state->used++] = (char)c;
    }
    return 0;
}

static int wait_for_slot(struct timespec *deadline, unsigned long delay_us)
{
    struct timespec now, delay;
    uint64_t nanos = (uint64_t)deadline->tv_nsec + (uint64_t)delay_us * 1000U;
    deadline->tv_sec += (time_t)(nanos / UINT64_C(1000000000));
    deadline->tv_nsec = (long)(nanos % UINT64_C(1000000000));
    for (;;) {
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
        if (now.tv_sec > deadline->tv_sec ||
            (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec)) return 1;
        delay.tv_sec = deadline->tv_sec - now.tv_sec;
        delay.tv_nsec = deadline->tv_nsec - now.tv_nsec;
        if (delay.tv_nsec < 0L) { --delay.tv_sec; delay.tv_nsec += 1000000000L; }
        if (nanosleep(&delay, NULL) != 0 && errno != EINTR) return 0;
    }
}

int SHAKTI_APP_MAIN(int argc, char **argv)
{
    static shakti_runtime_t runtime;
    static console_input_t console;
    shakti_report_t startup_report;
    struct sigaction interrupt_action;
    struct timespec deadline;
    unsigned long limit = 0UL;
    time_t explicit_epoch = (time_t)0;
    int have_epoch = 0, check_only = 0, demo_only = 0, input_flags, arg;
    int benched = 0;
    for (arg = 1; arg < argc; ++arg) {
        if (strcmp(argv[arg], "--check") == 0) check_only = 1;
        else if (strcmp(argv[arg], "--demo") == 0) demo_only = 1;
        else if (strcmp(argv[arg], "--help") == 0) { print_usage(argv[0]); return EXIT_SUCCESS; }
        else if ((strcmp(argv[arg], "--cycles") == 0 || strcmp(argv[arg], "--epoch") == 0) && arg + 1 < argc) {
            char *end;
            unsigned long number;
            int is_epoch = strcmp(argv[arg], "--epoch") == 0;
            errno = 0;
            ++arg;
            number = strtoul(argv[arg], &end, 10);
            if (errno || end == argv[arg] || *end || argv[arg][0] == '-' ||
                (is_epoch && number > (unsigned long)LONG_MAX) || (!is_epoch && number == 0UL)) {
                print_usage(argv[0]); return EXIT_FAILURE;
            }
            if (is_epoch) { explicit_epoch = (time_t)number; have_epoch = 1; }
            else limit = number;
        } else { print_usage(argv[0]); return EXIT_FAILURE; }
    }
    if (check_only && demo_only) { print_usage(argv[0]); return EXIT_FAILURE; }
    memset(&interrupt_action, 0, sizeof(interrupt_action));
    interrupt_action.sa_handler = handle_external_interrupt;
    interrupt_action.sa_flags = SA_RESTART;
    if (sigemptyset(&interrupt_action.sa_mask) != 0 ||
        sigaction(SIGINT, &interrupt_action, NULL) != 0) return EXIT_FAILURE;
    if (!shakti_app_initialize(&runtime)) {
        fputs("BENCH startup: required resident data could not be restored.\n", stderr);
        return EXIT_FAILURE;
    }
    if (have_epoch) {
        if (runtime.recorded_epoch >= (time_t)0 && explicit_epoch <= runtime.recorded_epoch) {
            fputs("BENCH explicit epoch precedes the recorded history\n", stderr);
            return EXIT_FAILURE;
        }
        shakti_tick_clock_seed(&runtime.clock, explicit_epoch);
    }
    if (!run_startup_report(&runtime, &startup_report)) {
        fputs("BENCH startup report\n", stderr);
        return EXIT_FAILURE;
    }
    if (check_only) return startup_report.runnable ? EXIT_SUCCESS : EXIT_FAILURE;
    if (!write_startup_anchor(&runtime)) return EXIT_FAILURE;
    if (demo_only) return run_first_lesson_demo(&runtime) ? EXIT_SUCCESS : EXIT_FAILURE;
    memset(&console, 0, sizeof(console));
    console.open = 1;
    input_flags = fcntl(STDIN_FILENO, F_GETFL);
    if (input_flags < 0 || fcntl(STDIN_FILENO, F_SETFL, input_flags | O_NONBLOCK) != 0) {
        fputs("BENCH console: nonblocking input unavailable\n", stderr);
        return EXIT_FAILURE;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
        (void)fcntl(STDIN_FILENO, F_SETFL, input_flags);
        shakti_memory_close(&runtime.memory);
        return EXIT_FAILURE;
    }
    puts("Shakti active. Input and replies are optional; /interrupt/ holds tools only.");
    for (;;) {
        const char *command = NULL;
        if (external_interrupt_requested) {
            external_interrupt_requested = 0;
            shakti_loop_interrupt_tools(&runtime.loop);
        }
        if (console_poll(&console)) command = console.text;
        if (!shakti_app_cycle(&runtime, command)) {
            if (!benched) fputs("BENCH runtime work held; heartbeat scheduling continues\n", stderr);
            benched = 1;
            shakti_loop_interrupt_tools(&runtime.loop);
        }
        if (limit && runtime.loop.cycle_count >= limit) break;
        if (!wait_for_slot(&deadline, shakti_loop_delay_us(&runtime.loop))) {
            fputs("BENCH host pacer\n", stderr);
            (void)fcntl(STDIN_FILENO, F_SETFL, input_flags);
            shakti_memory_close(&runtime.memory);
            return EXIT_FAILURE;
        }
    }
    (void)fcntl(STDIN_FILENO, F_SETFL, input_flags);
    shakti_memory_close(&runtime.memory);
    printf("RUN cycles=%lu hearing=%" PRIu64 " vision=%" PRIu64 " tool_calls=%lu reflections=%lu\n",
        runtime.loop.cycle_count, runtime.loop.hearing_frames, runtime.loop.vision_frames,
        runtime.loop.tool_calls, runtime.loop.reflection_count);
    return benched ? EXIT_FAILURE : EXIT_SUCCESS;
}
