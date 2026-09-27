#include "shakti_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shakti_config.h"
#include "shakti_manifest.h"

#define SHAKTI_LEDGER_FIELDS 23U

static size_t split_tabs(
    char *line,
    char *fields[SHAKTI_LEDGER_FIELDS]
)
{
    size_t count;
    char *cursor;

    count = 0U;
    cursor = line;

    while (count < SHAKTI_LEDGER_FIELDS) {
        char *tab;

        fields[count++] = cursor;
        tab = strchr(cursor, '\t');

        if (tab == NULL) {
            break;
        }

        *tab = '\0';
        cursor = tab + 1;
    }

    if (count > 0U) {
        char *end;

        end = fields[count - 1U] + strlen(fields[count - 1U]);

        while (end > fields[count - 1U] &&
               (end[-1] == '\n' || end[-1] == '\r')) {
            end--;
        }

        *end = '\0';
    }

    return count;
}

static unsigned long parse_ulong(const char *text)
{
    char *end;
    unsigned long value;

    value = strtoul(text, &end, 10);

    if (end == text || *end != '\0') {
        return 0UL;
    }

    return value;
}

static void count_tablet_state(
    shakti_report_t *report,
    const char *state
)
{
    if (strcmp(state, "planned") == 0) {
        report->planned_tablets++;
    } else if (strcmp(state, "staged") == 0) {
        report->staged_tablets++;
    } else if (strcmp(state, "ready") == 0) {
        report->ready_tablets++;
    }
}

void shakti_report_init(shakti_report_t *report)
{
    if (report != NULL) {
        memset(report, 0, sizeof(*report));
    }
}

int shakti_report_read_ledger(
    const char *ledger_path,
    shakti_report_t *report
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];
    int header_seen;

    if (ledger_path == NULL || report == NULL) {
        return 0;
    }

    shakti_report_init(report);
    file = fopen(ledger_path, "r");

    if (file == NULL) {
        return 0;
    }

    header_seen = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[SHAKTI_LEDGER_FIELDS];
        size_t count;

        if (!header_seen) {
            header_seen = 1;
            continue;
        }

        count = split_tabs(line, fields);

        if (count != SHAKTI_LEDGER_FIELDS) {
            fclose(file);
            return 0;
        }

        if (strcmp(fields[0], "STONE") == 0) {
            int foundation;

            report->stones++;
            foundation = strcmp(fields[3], "foundation_ascii") == 0;

            if (strcmp(fields[21], "YES") == 0) {
                report->verified_stones++;
            } else {
                report->incomplete_stones++;
            }

            if (strcmp(fields[14], "0") == 0) {
                report->required_text_missing++;
            }

            if (strcmp(fields[15], "0") == 0) {
                report->required_written_text_missing++;
            }

            if (strcmp(fields[16], "0") == 0) {
                if (foundation) {
                    report->optional_foundation_sound_missing++;
                } else {
                    report->required_sound_missing++;
                }
            }

            if (strcmp(fields[17], "0") == 0) {
                report->required_visual_missing++;
            }
        } else if (strcmp(fields[0], "TABLET") == 0) {
            count_tablet_state(report, fields[6]);
        } else if (strcmp(fields[0], "TABLET_SUMMARY") == 0) {
            count_tablet_state(report, fields[6]);

            if (strcmp(fields[21], "YES") == 0) {
                report->complete_tablets++;
            } else {
                report->incomplete_tablets++;
            }
        } else if (strcmp(fields[0], "EDEN_SUMMARY") == 0) {
            report->passed_checks = parse_ulong(fields[18]);
            report->required_checks = parse_ulong(fields[19]);
            report->confidence_percent =
                (unsigned int)parse_ulong(fields[20]);
            report->eden_lock_ready =
                strcmp(fields[21], "YES") == 0;
        }
    }

    if (ferror(file) || fclose(file) != 0 || !header_seen) {
        return 0;
    }

    report->runnable =
        report->required_text_missing == 0UL &&
        report->required_written_text_missing == 0UL &&
        report->required_sound_missing == 0UL &&
        report->required_visual_missing == 0UL &&
        report->incomplete_tablets == 0UL &&
        report->complete_tablets > 0UL;

    return 1;
}

static int write_report_body(
    FILE *file,
    const shakti_report_t *report,
    const shakti_school_state_t *school
)
{
    return fprintf(
        file,
        "SHAKTI STARTUP REPORT V1\n"
        "RUNNABLE=%s\n"
        "EDEN_LOCK_READY=%s\n"
        "CHECKS=%lu/%lu\n"
        "CONFIDENCE_PERCENT=%u\n"
        "STONES=%lu\n"
        "VERIFIED_STONES=%lu\n"
        "INCOMPLETE_STONES=%lu\n"
        "COMPLETE_TABLETS=%lu\n"
        "INCOMPLETE_TABLETS=%lu\n"
        "PLANNED_TABLETS=%lu\n"
        "STAGED_TABLETS=%lu\n"
        "READY_TABLETS=%lu\n"
        "REQUIRED_TEXT_MISSING=%lu\n"
        "REQUIRED_WRITTEN_TEXT_MISSING=%lu\n"
        "REQUIRED_SOUND_MISSING=%lu\n"
        "REQUIRED_VISUAL_MISSING=%lu\n"
        "OPTIONAL_FOUNDATION_SOUND_MISSING=%lu\n"
        "SCHOOL_PASS=%u\n"
        "ACTIVE_SYMBOL=%s\n"
        "MASTERY_STREAK=%u/%u\n"
        "TRIAL_HISTORY=%u_correct,%u_errors,%u_attempts\n",
        report->runnable ? "YES" : "NO",
        report->eden_lock_ready ? "YES" : "NO",
        report->passed_checks,
        report->required_checks,
        report->confidence_percent,
        report->stones,
        report->verified_stones,
        report->incomplete_stones,
        report->complete_tablets,
        report->incomplete_tablets,
        report->planned_tablets,
        report->staged_tablets,
        report->ready_tablets,
        report->required_text_missing,
        report->required_written_text_missing,
        report->required_sound_missing,
        report->required_visual_missing,
        report->optional_foundation_sound_missing,
        school->pass,
        school->active_symbol[0] != '\0'
            ? school->active_symbol
            : "none",
        school->current_streak,
        school->mastery_target,
        school->total_correct,
        school->total_errors,
        school->total_attempts
    ) >= 0;
}

int shakti_report_write(
    const char *report_path,
    const shakti_report_t *report,
    const shakti_school_state_t *school
)
{
    FILE *file;
    int success;

    if (report_path == NULL || report == NULL || school == NULL) {
        return 0;
    }

    file = fopen(report_path, "w");

    if (file == NULL) {
        return 0;
    }

    success = write_report_body(file, report, school);

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

void shakti_report_print(
    const shakti_report_t *report,
    const shakti_school_state_t *school
)
{
    if (report == NULL || school == NULL) {
        return;
    }

    puts("");
    puts("Shakti startup self-report:");
    printf(
        "Implemented data: %lu/%lu checks = %u%%.\n",
        report->passed_checks,
        report->required_checks,
        report->confidence_percent
    );
    printf(
        "Verified stones: %lu/%lu. Complete tablets: %lu.\n",
        report->verified_stones,
        report->stones,
        report->complete_tablets
    );
    printf(
        "Required gaps: text=%lu, written_text=%lu, sound=%lu, visual=%lu.\n",
        report->required_text_missing,
        report->required_written_text_missing,
        report->required_sound_missing,
        report->required_visual_missing
    );
    printf(
        "Optional Foundation sounds deferred: %lu.\n",
        report->optional_foundation_sound_missing
    );
    printf(
        "Future planned tablets: %lu. Eden lock ready: %s.\n",
        report->planned_tablets,
        report->eden_lock_ready ? "yes" : "no"
    );
    printf(
        "School: pass %u, symbol %s, streak %u/%u, "
        "history %u correct / %u errors / %u attempts.\n",
        school->pass,
        school->active_symbol[0] != '\0'
            ? school->active_symbol
            : "none",
        school->current_streak,
        school->mastery_target,
        school->total_correct,
        school->total_errors,
        school->total_attempts
    );
    printf(
        "Runnable beginning: %s.\n",
        report->runnable ? "YES" : "NO"
    );
    puts("");
}

int shakti_report_run(
    const shakti_manifest_t *manifest,
    const char *manifest_path,
    const char *ledger_path,
    const char *report_path,
    const shakti_school_state_t *school,
    shakti_report_t *report
)
{
    if (manifest == NULL ||
        manifest_path == NULL ||
        ledger_path == NULL ||
        report_path == NULL ||
        school == NULL ||
        report == NULL ||
        !manifest->loaded) {
        return 0;
    }

    /*
     * The ledger is always rewritten from the current XML and artifacts.
     * A failed ready-tablet check still leaves the readable ledger behind.
     */
    (void)shakti_manifest_write_ledger(
        manifest,
        manifest_path,
        ledger_path
    );

    if (!shakti_report_read_ledger(ledger_path, report) ||
        !shakti_report_write(report_path, report, school)) {
        return 0;
    }

    shakti_report_print(report, school);

    return 1;
}
