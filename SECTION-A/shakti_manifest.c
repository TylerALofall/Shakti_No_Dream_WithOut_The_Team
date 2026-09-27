#include "shakti_manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shakti_asset.h"
#include "shakti_config.h"
#include "shakti_tablet.h"
#include "shakti_score.h"

static char *trim_text__manifest_h(char *text)
{
    char *end;

    while (*text == ' ' || *text == '\t' ||
           *text == '\r' || *text == '\n') {
        text++;
    }

    end = text + strlen(text);

    while (end > text &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }

    *end = '\0';

    return text;
}

static int xml_unescape__manifest_h(
    const char *source,
    char *destination,
    size_t destination_size
)
{
    size_t used;

    used = 0U;

    while (*source != '\0') {
        const char *replacement;
        size_t consumed;

        replacement = NULL;
        consumed = 1U;

        if (strncmp(source, "&amp;", 5U) == 0) {
            replacement = "&";
            consumed = 5U;
        } else if (strncmp(source, "&lt;", 4U) == 0) {
            replacement = "<";
            consumed = 4U;
        } else if (strncmp(source, "&gt;", 4U) == 0) {
            replacement = ">";
            consumed = 4U;
        } else if (strncmp(source, "&quot;", 6U) == 0) {
            replacement = "\"";
            consumed = 6U;
        } else if (strncmp(source, "&apos;", 6U) == 0) {
            replacement = "'";
            consumed = 6U;
        }

        if (used + 1U >= destination_size) {
            return 0;
        }

        destination[used++] =
            replacement != NULL ? replacement[0] : *source;
        source += consumed;
    }

    destination[used] = '\0';

    return 1;
}

static int extract_tag__manifest_h(
    const char *line,
    const char *tag,
    char *destination,
    size_t destination_size
)
{
    char opening[64];
    char closing[64];
    const char *start;
    const char *end;
    char encoded[SHAKTI_PATH_CAPACITY];
    size_t length;
    int written;

    written = snprintf(opening, sizeof(opening), "<%s>", tag);

    if (written < 0 ||
        (size_t)written >= sizeof(opening)) {
        return 0;
    }

    written = snprintf(closing, sizeof(closing), "</%s>", tag);

    if (written < 0 ||
        (size_t)written >= sizeof(closing)) {
        return 0;
    }

    start = strstr(line, opening);

    if (start == NULL) {
        return 0;
    }

    start += strlen(opening);
    end = strstr(start, closing);

    if (end == NULL ||
        end[strlen(closing)] != '\0') {
        return 0;
    }

    length = (size_t)(end - start);

    if (length >= sizeof(encoded)) {
        return 0;
    }

    memcpy(encoded, start, length);
    encoded[length] = '\0';

    return xml_unescape__manifest_h(
        encoded,
        destination,
        destination_size
    );
}

static int extract_order__manifest_h(
    const char *line,
    const char *element,
    unsigned int *order
)
{
    char prefix[64];
    const char *start;
    char *end;
    unsigned long parsed;
    int written;

    written = snprintf(
        prefix,
        sizeof(prefix),
        "<%s order=\"",
        element
    );

    if (written < 0 ||
        (size_t)written >= sizeof(prefix) ||
        strncmp(line, prefix, strlen(prefix)) != 0) {
        return 0;
    }

    start = line + strlen(prefix);
    parsed = strtoul(start, &end, 10);

    if (end == start ||
        strcmp(end, "\">") != 0 ||
        parsed > 4294967295UL) {
        return 0;
    }

    *order = (unsigned int)parsed;

    return 1;
}

static int is_absolute_path(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (path[0] == '/' || path[0] == '\\') {
        return 1;
    }

    return path[1] == ':' &&
           ((path[0] >= 'A' && path[0] <= 'Z') ||
            (path[0] >= 'a' && path[0] <= 'z'));
}

static int resolve_path(
    const char *manifest_path,
    const char *relative,
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
        relative == NULL ||
        destination == NULL) {
        return 0;
    }

    if (is_absolute_path(relative)) {
        written = snprintf(
            destination,
            destination_size,
            "%s",
            relative
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

    if (separator == NULL) {
        directory_length = 0U;
    } else {
        directory_length =
            (size_t)(separator - manifest_path) + 1U;
    }

    if (directory_length + strlen(relative) >= destination_size) {
        return 0;
    }

    if (directory_length > 0U) {
        memcpy(
            destination,
            manifest_path,
            directory_length
        );
    }

    strcpy(destination + directory_length, relative);

    return 1;
}

static const shakti_manifest_level_t *find_level(
    const shakti_manifest_t *manifest,
    const char *id
)
{
    size_t index;

    for (index = 0U;
         index < manifest->level_count;
         ++index) {
        if (strcmp(manifest->levels[index].id, id) == 0) {
            return &manifest->levels[index];
        }
    }

    return NULL;
}

static int requirements_are_valid(
    const shakti_manifest_t *manifest,
    const char *requirements,
    unsigned int owner_order
)
{
    char copy[SHAKTI_REQUIREMENTS_CAPACITY];
    char *token;

    if (strcmp(requirements, "none") == 0) {
        return 1;
    }

    if (strlen(requirements) >= sizeof(copy)) {
        return 0;
    }

    strcpy(copy, requirements);
    token = strtok(copy, ",");

    while (token != NULL) {
        const shakti_manifest_level_t *required;
        char *trimmed;

        trimmed = trim_text__manifest_h(token);
        required = find_level(manifest, trimmed);

        if (required == NULL ||
            required->order >= owner_order) {
            return 0;
        }

        token = strtok(NULL, ",");
    }

    return 1;
}

static int validate_manifest(const shakti_manifest_t *manifest)
{
    size_t index;
    size_t other;

    if (manifest->status[0] == '\0' ||
        manifest->artifact_root[0] == '\0' ||
        manifest->level_count == 0U) {
        return 0;
    }

    if (strcmp(manifest->status, "OPEN") != 0 &&
        strcmp(
            manifest->status,
            "APPROVED_FOR_LOCK") != 0) {
        return 0;
    }

    for (index = 0U;
         index < manifest->level_count;
         ++index) {
        const shakti_manifest_level_t *level;

        level = &manifest->levels[index];

        if (level->id[0] == '\0' ||
            level->title[0] == '\0' ||
            level->requires[0] == '\0') {
            return 0;
        }

        for (other = index + 1U;
             other < manifest->level_count;
             ++other) {
            if (level->order ==
                    manifest->levels[other].order ||
                strcmp(
                    level->id,
                    manifest->levels[other].id) == 0) {
                return 0;
            }
        }

        if (!requirements_are_valid(
                manifest,
                level->requires,
                level->order)) {
            return 0;
        }
    }

    for (index = 0U;
         index < manifest->tablet_count;
         ++index) {
        const shakti_manifest_tablet_t *tablet_entry;
        const shakti_manifest_level_t *level;

        tablet_entry = &manifest->tablets[index];
        level = find_level(
            manifest,
            tablet_entry->level
        );

        if (level == NULL ||
            tablet_entry->lesson[0] == '\0' ||
            tablet_entry->mode[0] == '\0' ||
            tablet_entry->requires[0] == '\0' ||
            tablet_entry->state[0] == '\0' ||
            tablet_entry->path[0] == '\0' ||
            (strcmp(
                tablet_entry->state,
                "planned") != 0 &&
             strcmp(
                tablet_entry->state,
                "ready") != 0 &&
             strcmp(
                tablet_entry->state,
                "staged") != 0) ||
            !requirements_are_valid(
                manifest,
                tablet_entry->requires,
                level->order)) {
            return 0;
        }

        for (other = index + 1U;
             other < manifest->tablet_count;
             ++other) {
            if (tablet_entry->order ==
                    manifest->tablets[other].order ||
                strcmp(
                    tablet_entry->path,
                    manifest->tablets[other].path) == 0) {
                return 0;
            }
        }
    }

    return 1;
}

void shakti_manifest_init(shakti_manifest_t *manifest)
{
    if (manifest != NULL) {
        memset(manifest, 0, sizeof(*manifest));
    }
}

int shakti_manifest_load(
    shakti_manifest_t *manifest,
    const char *manifest_path
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];
    shakti_manifest_level_t current_level;
    shakti_manifest_tablet_t current_tablet;
    unsigned int level_fields;
    unsigned int tablet_fields;
    int inside_level;
    int inside_tablet;
    int schema_seen;

    if (manifest == NULL || manifest_path == NULL) {
        return 0;
    }

    shakti_manifest_init(manifest);
    file = fopen(manifest_path, "r");

    if (file == NULL) {
        return 0;
    }

    memset(&current_level, 0, sizeof(current_level));
    memset(&current_tablet, 0, sizeof(current_tablet));
    level_fields = 0U;
    tablet_fields = 0U;
    inside_level = 0;
    inside_tablet = 0;
    schema_seen = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *trimmed;

        if (strchr(line, '\n') == NULL && !feof(file)) {
            fclose(file);
            return 0;
        }

        trimmed = trim_text__manifest_h(line);

        if (strcmp(
                trimmed,
                "<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">") == 0) {
            schema_seen = 1;
            continue;
        }

        if (strncmp(trimmed, "<level order=\"", 14U) == 0) {
            if (inside_level ||
                inside_tablet ||
                manifest->level_count >=
                    SHAKTI_MAX_MANIFEST_LEVELS ||
                !extract_order__manifest_h(
                    trimmed,
                    "level",
                    &current_level.order)) {
                fclose(file);
                return 0;
            }

            inside_level = 1;
            level_fields = 0U;
            continue;
        }

        if (strcmp(trimmed, "</level>") == 0) {
            if (!inside_level ||
                level_fields != 7U) {
                fclose(file);
                return 0;
            }

            manifest->levels[manifest->level_count++] =
                current_level;
            memset(&current_level, 0, sizeof(current_level));
            inside_level = 0;
            level_fields = 0U;
            continue;
        }

        if (strncmp(trimmed, "<tablet order=\"", 15U) == 0) {
            if (inside_level ||
                inside_tablet ||
                manifest->tablet_count >=
                    SHAKTI_MAX_MANIFEST_TABLETS ||
                !extract_order__manifest_h(
                    trimmed,
                    "tablet",
                    &current_tablet.order) ||
                current_tablet.order == 0U) {
                fclose(file);
                return 0;
            }

            inside_tablet = 1;
            tablet_fields = 0U;
            continue;
        }

        if (strcmp(trimmed, "</tablet>") == 0) {
            if (!inside_tablet ||
                tablet_fields != 63U) {
                fclose(file);
                return 0;
            }

            manifest->tablets[manifest->tablet_count++] =
                current_tablet;
            memset(
                &current_tablet,
                0,
                sizeof(current_tablet)
            );
            inside_tablet = 0;
            tablet_fields = 0U;
            continue;
        }

        if (inside_level) {
            unsigned int field;

            field = 0U;

            if (extract_tag__manifest_h(
                    trimmed,
                    "id",
                    current_level.id,
                    sizeof(current_level.id))) {
                field = 1U;
            } else if (extract_tag__manifest_h(
                    trimmed,
                    "title",
                    current_level.title,
                    sizeof(current_level.title))) {
                field = 2U;
            } else if (extract_tag__manifest_h(
                    trimmed,
                    "requires",
                    current_level.requires,
                    sizeof(current_level.requires))) {
                field = 4U;
            }

            if (field != 0U) {
                if ((level_fields & field) != 0U) {
                    fclose(file);
                    return 0;
                }

                level_fields |= field;
            }

            continue;
        }

        if (inside_tablet) {
            unsigned int field;

            field = 0U;

            if (extract_tag__manifest_h(
                    trimmed,
                    "level",
                    current_tablet.level,
                    sizeof(current_tablet.level))) {
                field = 1U;
            } else if (extract_tag__manifest_h(
                    trimmed,
                    "lesson",
                    current_tablet.lesson,
                    sizeof(current_tablet.lesson))) {
                field = 2U;
            } else if (extract_tag__manifest_h(
                    trimmed,
                    "mode",
                    current_tablet.mode,
                    sizeof(current_tablet.mode))) {
                field = 4U;
            } else if (extract_tag__manifest_h(
                    trimmed,
                    "requires",
                    current_tablet.requires,
                    sizeof(current_tablet.requires))) {
                field = 8U;
            } else if (extract_tag__manifest_h(
                    trimmed,
                    "state",
                    current_tablet.state,
                    sizeof(current_tablet.state))) {
                field = 16U;
            } else if (extract_tag__manifest_h(
                    trimmed,
                    "path",
                    current_tablet.path,
                    sizeof(current_tablet.path))) {
                field = 32U;
            }

            if (field != 0U) {
                if ((tablet_fields & field) != 0U) {
                    fclose(file);
                    return 0;
                }

                tablet_fields |= field;
            }

            continue;
        }

        if (extract_tag__manifest_h(
                trimmed,
                "status",
                manifest->status,
                sizeof(manifest->status)) ||
            extract_tag__manifest_h(
                trimmed,
                "artifact_root",
                manifest->artifact_root,
                sizeof(manifest->artifact_root))) {
            continue;
        }
    }

    if (ferror(file) ||
        fclose(file) != 0 ||
        !schema_seen ||
        inside_level ||
        inside_tablet ||
        !validate_manifest(manifest)) {
        shakti_manifest_init(manifest);
        return 0;
    }

    manifest->loaded = 1U;

    return 1;
}

static int write_ledger_text(
    FILE *file,
    const char *text
)
{
    while (*text != '\0') {
        if (*text == '\t' ||
            *text == '\r' ||
            *text == '\n') {
            if (fputc(' ', file) == EOF) {
                return 0;
            }
        } else if (fputc((unsigned char)*text, file) == EOF) {
            return 0;
        }

        text++;
    }

    return 1;
}

int shakti_manifest_write_ledger(
    const shakti_manifest_t *manifest,
    const char *manifest_path,
    const char *ledger_path
)
{
    FILE *ledger;
    char artifact_root[SHAKTI_PATH_CAPACITY];
    size_t tablet_index;
    shakti_validation_score_t eden_score;
    unsigned long audited_tablets;
    unsigned long complete_tablets;
    int ready_entries_valid;

    if (manifest == NULL ||
        manifest_path == NULL ||
        ledger_path == NULL ||
        !manifest->loaded ||
        !resolve_path(
            manifest_path,
            manifest->artifact_root,
            artifact_root,
            sizeof(artifact_root))) {
        return 0;
    }

    ledger = fopen(ledger_path, "w");

    if (ledger == NULL) {
        return 0;
    }

    eden_score = shakti_score_make(0UL, 0UL);
    audited_tablets = 0UL;
    complete_tablets = 0UL;
    ready_entries_valid = 1;

    if (fputs(
            "record_type\ttablet_order\tlevel_order\tlevel\tlesson\tmode\t"
            "tablet_state\ttablet_path\tstone_order\ttext\tasset_key\t"
            "written_text\tsound_art\tvisual_art\ttext_ok\twritten_text_ok\t"
            "sound_art_ok\tvisual_art_ok\tpassed_checks\trequired_checks\t"
            "confidence_percent\tlock_eligible\tload_status\n",
            ledger) < 0) {
        fclose(ledger);
        return 0;
    }

    for (tablet_index = 0U;
         tablet_index < manifest->tablet_count;
         ++tablet_index) {
        const shakti_manifest_tablet_t *entry;
        const shakti_manifest_level_t *level;
        char tablet_path[SHAKTI_PATH_CAPACITY];

        entry = &manifest->tablets[tablet_index];
        level = find_level(manifest, entry->level);

        if (level == NULL ||
            !resolve_path(
                manifest_path,
                entry->path,
                tablet_path,
                sizeof(tablet_path))) {
            fclose(ledger);
            return 0;
        }

        if (strcmp(entry->state, "planned") == 0) {
            if (fprintf(
                    ledger,
                    "TABLET\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t0\t\t\t\t\t\t"
                    "0\t0\t0\t0\t0\t0\t0\tNO\tPLANNED\n",
                    entry->order,
                    level->order,
                    entry->level,
                    entry->lesson,
                    entry->mode,
                    entry->state,
                    entry->path) < 0) {
                fclose(ledger);
                return 0;
            }

            continue;
        }

        {
            shakti_tablet_t tablet;
            shakti_tablet_audit_t audit;
            size_t stone_index;
            int structure_ok;

            structure_ok =
                shakti_tablet_parse(&tablet, tablet_path) &&
                strcmp(tablet.level, entry->level) == 0 &&
                strcmp(tablet.lesson, entry->lesson) == 0;

            if (!structure_ok ||
                !shakti_tablet_audit(
                    &tablet,
                    artifact_root,
                    &audit)) {
                if (strcmp(entry->state, "ready") == 0) {
                    ready_entries_valid = 0;
                }

                if (fprintf(
                        ledger,
                        "TABLET\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t0\t\t\t\t\t\t"
                        "0\t0\t0\t0\t0\t0\t0\tNO\tTABLET_STRUCTURE_FAILED\n",
                        entry->order,
                        level->order,
                        entry->level,
                        entry->lesson,
                        entry->mode,
                        entry->state,
                        entry->path) < 0) {
                    fclose(ledger);
                    return 0;
                }

                continue;
            }

            audited_tablets++;
            shakti_score_add(&eden_score, &audit.score);

            if (audit.score.complete) {
                complete_tablets++;
            }

            if (strcmp(entry->state, "ready") == 0 &&
                !audit.score.complete) {
                ready_entries_valid = 0;
            }

            for (stone_index = 0U;
                 stone_index < tablet.stone_count;
                 ++stone_index) {
                const shakti_stone_t *stone;
                const shakti_stone_audit_t *stone_audit;
                char key[SHAKTI_KEY_CAPACITY];

                stone = &tablet.stones[stone_index];
                stone_audit = &audit.stones[stone_index];

                if (!shakti_asset_key_from_text(
                        stone->text,
                        key,
                        sizeof(key)) ||
                    fprintf(
                        ledger,
                        "STONE\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t%u\t",
                        entry->order,
                        level->order,
                        entry->level,
                        entry->lesson,
                        entry->mode,
                        entry->state,
                        entry->path,
                        stone->order) < 0 ||
                    !write_ledger_text(ledger, stone->text) ||
                    fprintf(
                        ledger,
                        "\t%s\t%s\t%s\t%s\t%u\t%u\t%u\t%u\t"
                        "%lu\t%lu\t%u\t%s\t%s\n",
                        key,
                        stone->written_text,
                        stone->sound_art,
                        stone->visual_art,
                        (unsigned int)stone_audit->text_ok,
                        (unsigned int)stone_audit->written_text_ok,
                        (unsigned int)stone_audit->sound_art_ok,
                        (unsigned int)stone_audit->visual_art_ok,
                        stone_audit->score.passed,
                        stone_audit->score.required,
                        stone_audit->score.percent,
                        stone_audit->score.complete ? "YES" : "NO",
                        stone_audit->score.complete
                            ? "VERIFIED"
                            : "INCOMPLETE") < 0) {
                    fclose(ledger);
                    return 0;
                }
            }

            if (fprintf(
                    ledger,
                    "TABLET_SUMMARY\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t0\t\t\t\t\t\t"
                    "0\t0\t0\t0\t%lu\t%lu\t%u\t%s\t%s\n",
                    entry->order,
                    level->order,
                    entry->level,
                    entry->lesson,
                    entry->mode,
                    entry->state,
                    entry->path,
                    audit.score.passed,
                    audit.score.required,
                    audit.score.percent,
                    audit.score.complete ? "YES" : "NO",
                    audit.score.complete
                        ? "100_PERCENT_VERIFIED"
                        : "INCOMPLETE") < 0) {
                fclose(ledger);
                return 0;
            }
        }
    }

    if (fprintf(
            ledger,
            "EDEN_SUMMARY\t0\t0\tALL\tALL\tALL\t%s\t%s\t0\t\t\t\t\t\t"
            "0\t0\t0\t0\t%lu\t%lu\t%u\t%s\t"
            "AUDITED_TABLETS=%lu;COMPLETE_TABLETS=%lu;TOTAL_TABLETS=%lu\n",
            manifest->status,
            manifest_path,
            eden_score.passed,
            eden_score.required,
            eden_score.percent,
            eden_score.complete &&
                complete_tablets == (unsigned long)manifest->tablet_count
                ? "YES"
                : "NO",
            audited_tablets,
            complete_tablets,
            (unsigned long)manifest->tablet_count) < 0) {
        fclose(ledger);
        return 0;
    }

    if (fflush(ledger) != 0 ||
        fclose(ledger) != 0) {
        return 0;
    }

    return ready_entries_valid;
}

void shakti_manifest_print(const shakti_manifest_t *manifest)
{
    size_t index;

    if (manifest == NULL || !manifest->loaded) {
        puts("No Eden manifest is loaded.");
        return;
    }

    printf(
        "Eden manifest status %s. Levels %lu. Tablets %lu.\n",
        manifest->status,
        (unsigned long)manifest->level_count,
        (unsigned long)manifest->tablet_count
    );

    for (index = 0U;
         index < manifest->level_count;
         ++index) {
        const shakti_manifest_level_t *level;

        level = &manifest->levels[index];

        printf(
            "Level %u: %s. %s. Requires %s.\n",
            level->order,
            level->id,
            level->title,
            level->requires
        );
    }
}
