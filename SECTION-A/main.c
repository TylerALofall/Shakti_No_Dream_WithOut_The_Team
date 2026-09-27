#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int log_tool_call(
    shakti_runtime_t *runtime,
    const char *tool,
    const char *arguments
)
{
    shakti_tick_t tick;

    if (!shakti_tick_next(&runtime->clock, &tick)) {
        return 0;
    }

    return shakti_log_append(
        SHAKTI_LEARNED_STREAM_PATH,
        "LRN1",
        &tick,
        "Gf",
        1U,
        "AWAKE",
        "internal",
        tool,
        "mcp_tool_call",
        arguments
    );
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
        "Loaded %s (%s): %lu bytes.\n",
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

    shakti_memory_remember(&runtime->memory, &tick, output);

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
            "Use: sense text|written_text|visual_art|sound_art "
            "subject property value"
        );
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

    shakti_memory_remember(&runtime->memory, &tick, value);
    puts("Grounded channel event accepted.");

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
        puts("Use: manifest MANIFEST_XML LEDGER_TSV");
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
    puts("/heartbeat/ 0..30");
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
    puts("/reflection/defer/");
    puts("/reflection/questions/");
    puts("/interrupt/");
    puts("/resume/");
    puts("/status/");
    puts("/help/");
    puts("/quit/");
    puts("");
    puts("MCP tools:");
    puts("ask question");
    puts("learn question | answer | observed");
    puts("learn question | answer | curriculum");
    puts("learn question | answer | verified");
    puts("reject question | answer | observed");
    puts(
        "sense text|written_text|visual_art|sound_art "
        "subject property value"
    );
    puts("pass 1|2|3|4");
    puts("school exact-text");
    puts("draft exact-text");
    puts("tablet XML_PATH ARTIFACT_ROOT");
    puts("manifest MANIFEST_XML LEDGER_TSV");
    puts("load path | text|written_text|visual_art|sound_art");
    puts("recall exact-text-fragment");
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

    if (runtime->loop.reflection_due &&
        runtime->loop.reflection_deferrals >=
            SHAKTI_REFLECTION_MAX_DEFERRALS) {
        puts(
            "Reflection reached three deferrals. Complete /reflection/ "
            "before the next tool call."
        );
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

    if (strcmp(command, "recall") == 0) {
        if (!shakti_memory_recall_text_file(
                SHAKTI_LEARNED_STREAM_PATH,
                arguments,
                20U)) {
            puts("Recall failed.");
        }

        return 1;
    }

    if (strcmp(command, "validate") == 0) {
        validate_logs();
        return 1;
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
        return 0;
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
        unsigned int minutes;

        if (!parse_unsigned__app_h(
                trim_text__app_h(line + 12U),
                &minutes) ||
            !shakti_loop_set_heartbeat(
                &runtime->loop,
                minutes)) {
            puts("Heartbeat accepts 0 through 30 minutes.");
            return 1;
        }

        printf("Heartbeat set to %u minute(s).\n", minutes);
        return 1;
    }

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

    if (strcmp(line, "/reflection/defer/") == 0) {
        if (shakti_loop_defer_reflection(&runtime->loop)) {
            printf(
                "Reflection deferred. %u of %u deferrals used.\n",
                runtime->loop.reflection_deferrals,
                (unsigned int)SHAKTI_REFLECTION_MAX_DEFERRALS
            );
        } else {
            puts(
                "Reflection deferral is available only when due and below "
                "three deferrals."
            );
        }

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
    puts("  no flag  Start the interactive nine-stage loop.");
}

#ifdef SHAKTI_APP_NO_MAIN
#define SHAKTI_APP_MAIN shakti_app_main
#else
#define SHAKTI_APP_MAIN main
#endif

int SHAKTI_APP_MAIN(int argc, char **argv)
{
    static shakti_runtime_t runtime;
    shakti_report_t startup_report;
    char input[SHAKTI_INPUT_CAPACITY];
    int check_only;
    int demo_only;

    check_only = 0;
    demo_only = 0;

    if (argc > 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "--check") == 0) {
            check_only = 1;
        } else if (strcmp(argv[1], "--demo") == 0) {
            demo_only = 1;
        } else if (strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (signal(SIGINT, handle_external_interrupt) == SIG_ERR) {
        fputs("Shakti stopped: external interrupt could not be installed.\n",
              stderr);
        return EXIT_FAILURE;
    }

    if (!initialize_runtime(&runtime)) {
        fputs(
            "Shakti stopped: an always-loaded block could not be restored.\n",
            stderr
        );
        return EXIT_FAILURE;
    }

    if (!run_startup_report(&runtime, &startup_report)) {
        fputs(
            "Shakti stopped: the startup report could not be produced.\n",
            stderr
        );
        return EXIT_FAILURE;
    }

    if (check_only) {
        return startup_report.runnable
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    if (!write_startup_anchor(&runtime)) {
        fputs(
            "Shakti stopped: the startup anchor could not be preserved.\n",
            stderr
        );
        return EXIT_FAILURE;
    }

    if (demo_only) {
        return run_first_lesson_demo(&runtime)
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
    }

    puts("Shakti awake.");
    puts(
        "Eden manifest, School, goal, notebook, menu, and memory routes are loaded."
    );
    puts(
        "Four channels are active: text, written_text, visual_art, sound_art."
    );
    puts(
        "The internal MCP routes every tool. An external interrupt stops "
        "tools and leaves Shakti awake."
    );
    puts("Use /help/ for commands.");

    for (;;) {
        char *line;
        size_t length;
        shakti_tick_t cycle_tick;
        int continue_running;

        if (external_interrupt_requested) {
            external_interrupt_requested = 0;
            shakti_loop_interrupt_tools(&runtime.loop);
            puts(
                "\nExternal interrupt received. MCP tools stopped. "
                "Shakti remains awake."
            );
        }

        if (!shakti_loop_begin_cycle(&runtime, &cycle_tick)) {
            fputs("Shakti stopped to protect the loop log.\n", stderr);
            return EXIT_FAILURE;
        }

        fputs("Tyler: ", stdout);
        fflush(stdout);

        errno = 0;

        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (external_interrupt_requested) {
                clearerr(stdin);
                continue;
            }

            break;
        }

        length = strlen(input);

        if (length > 0U &&
            input[length - 1U] != '\n' &&
            !feof(stdin)) {
            int character;

            while ((character = getchar()) != '\n' &&
                   character != EOF) {
            }

            puts("Input was too long and was not accepted.");
            continue;
        }

        line = trim_text__app_h(input);

        if (line[0] == '\0') {
            if (!shakti_loop_finish_cycle(&runtime)) {
                return EXIT_FAILURE;
            }
            continue;
        }

        if (!shakti_loop_record_message(
                &runtime,
                "message_shakti",
                line)) {
            fputs("Shakti stopped to protect message memory.\n", stderr);
            return EXIT_FAILURE;
        }

        continue_running = process_control(&runtime, line);

        if (!shakti_loop_finish_cycle(&runtime)) {
            fputs("Shakti stopped to protect reflection scheduling.\n", stderr);
            return EXIT_FAILURE;
        }

        if (!continue_running) {
            break;
        }
    }

    puts("Shakti sleeping. Readable memory remains.");

    return EXIT_SUCCESS;
}
