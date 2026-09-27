#include "shakti_tablet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shakti_artifact.h"
#include "shakti_asset.h"
#include "shakti_config.h"
#include "shakti_score.h"

static char *trim_text__tablet_h(char *text)
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

static int xml_unescape__tablet_h(
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

static int extract_tag__tablet_h(
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

    return xml_unescape__tablet_h(
        encoded,
        destination,
        destination_size
    );
}

static int extract_order__tablet_h(
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
        parsed == 0UL ||
        parsed > 4294967295UL) {
        return 0;
    }

    *order = (unsigned int)parsed;

    return 1;
}

static int sound_sequence_visit(
    const char *sequence,
    int (*visitor)(const char *clip, void *context),
    void *context
)
{
    const char *cursor;
    unsigned int clip_count;

    if (sequence == NULL || sequence[0] == '\0') {
        return 0;
    }

    cursor = sequence;
    clip_count = 0U;

    while (*cursor != '\0') {
        const char *separator;
        size_t length;
        char clip[SHAKTI_PATH_CAPACITY];

        separator = strchr(cursor, '|');
        length = separator == NULL
            ? strlen(cursor)
            : (size_t)(separator - cursor);

        if (length == 0U ||
            length >= sizeof(clip) ||
            clip_count >= SHAKTI_MAX_SOUND_SEQUENCE_CLIPS) {
            return 0;
        }

        memcpy(clip, cursor, length);
        clip[length] = '\0';

        if (!shakti_asset_safe_filename(clip) ||
            length < 5U ||
            strcmp(clip + length - 4U, ".wav") != 0 ||
            (visitor != NULL && !visitor(clip, context))) {
            return 0;
        }

        clip_count++;

        if (separator == NULL) {
            break;
        }

        cursor = separator + 1;

        if (*cursor == '\0') {
            return 0;
        }
    }

    return clip_count > 0U;
}

static int validate_sound_contract(
    const shakti_stone_t *stone
)
{
    char expected[SHAKTI_PATH_CAPACITY];

    if (strchr(stone->sound_art, '|') == NULL) {
        return shakti_asset_filename(
                   stone->text,
                   ".wav",
                   expected,
                   sizeof(expected)) &&
               strcmp(expected, stone->sound_art) == 0 &&
               shakti_asset_safe_filename(stone->sound_art);
    }

    return sound_sequence_visit(
        stone->sound_art,
        NULL,
        NULL
    );
}

static int validate_filename_contract(
    const shakti_stone_t *stone
)
{
    char expected[SHAKTI_PATH_CAPACITY];

    if (!shakti_asset_filename(
            stone->text,
            ".8x8.txt",
            expected,
            sizeof(expected)) ||
        strcmp(expected, stone->written_text) != 0 ||
        !validate_sound_contract(stone) ||
        !shakti_asset_filename(
            stone->text,
            ".svg",
            expected,
            sizeof(expected)) ||
        strcmp(expected, stone->visual_art) != 0) {
        return 0;
    }

    return shakti_asset_safe_filename(stone->written_text) &&
           shakti_asset_safe_filename(stone->visual_art);
}

typedef struct {
    const char *artifact_root;
} sound_validation_context_t;

static int validate_sound_clip(
    const char *clip,
    void *context
)
{
    const sound_validation_context_t *validation;
    char path[SHAKTI_PATH_CAPACITY];

    validation = (const sound_validation_context_t *)context;

    return shakti_artifact_join(
               validation->artifact_root,
               "Sound_art",
               clip,
               path,
               sizeof(path)) &&
           shakti_artifact_validate_wav(path);
}

static int validate_sound_artifacts(
    const shakti_stone_t *stone,
    const char *artifact_root
)
{
    sound_validation_context_t context;

    context.artifact_root = artifact_root;

    return sound_sequence_visit(
        stone->sound_art,
        validate_sound_clip,
        &context
    );
}

static int validate_artifacts(
    const shakti_stone_t *stone,
    const char *artifact_root,
    int sound_required
)
{
    char path[SHAKTI_PATH_CAPACITY];

    if (!shakti_artifact_join(
            artifact_root,
            "Visual_text",
            stone->written_text,
            path,
            sizeof(path)) ||
        !shakti_artifact_validate_written_text(
            path,
            stone->text) ||
        (sound_required &&
         !validate_sound_artifacts(stone, artifact_root)) ||
        !shakti_artifact_join(
            artifact_root,
            "Visual_art",
            stone->visual_art,
            path,
            sizeof(path)) ||
        !shakti_artifact_validate_svg(path)) {
        return 0;
    }

    return 1;
}

void shakti_tablet_init(shakti_tablet_t *tablet)
{
    if (tablet != NULL) {
        memset(tablet, 0, sizeof(*tablet));
    }
}

static int shakti_tablet_load_internal(
    shakti_tablet_t *tablet,
    const char *xml_path,
    const char *artifact_root,
    int require_artifacts
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];
    shakti_stone_t current;
    size_t declared_count;
    unsigned int expected_order;
    unsigned int fields_seen;
    int inside_stone;
    int schema_seen;

    if (tablet == NULL ||
        xml_path == NULL ||
        (require_artifacts && artifact_root == NULL)) {
        return 0;
    }

    shakti_tablet_init(tablet);
    file = fopen(xml_path, "r");

    if (file == NULL) {
        return 0;
    }

    memset(&current, 0, sizeof(current));
    declared_count = 0U;
    expected_order = 1U;
    fields_seen = 0U;
    inside_stone = 0;
    schema_seen = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *trimmed;

        if (strchr(line, '\n') == NULL && !feof(file)) {
            fclose(file);
            return 0;
        }

        trimmed = trim_text__tablet_h(line);

        if (strcmp(
                trimmed,
                "<tablet schema=\"SHAKTI_TABLET_4S_V2\">") == 0) {
            schema_seen = 1;
            continue;
        }

        if (strncmp(trimmed, "<stone order=\"", 14U) == 0) {
            unsigned int order;

            if (inside_stone ||
                !extract_order__tablet_h(trimmed, "stone", &order) ||
                order != expected_order ||
                tablet->stone_count >= SHAKTI_MAX_TABLET_STONES) {
                fclose(file);
                return 0;
            }

            memset(&current, 0, sizeof(current));
            current.order = order;
            inside_stone = 1;
            fields_seen = 0U;
            continue;
        }

        if (strcmp(trimmed, "</stone>") == 0) {
            if (!inside_stone ||
                fields_seen != 15U ||
                !validate_filename_contract(&current) ||
                (require_artifacts &&
                 !validate_artifacts(
                     &current,
                     artifact_root,
                     strcmp(tablet->level, "foundation_ascii") != 0))) {
                fclose(file);
                return 0;
            }

            tablet->stones[tablet->stone_count++] = current;
            expected_order++;
            inside_stone = 0;
            fields_seen = 0U;
            continue;
        }

        if (!inside_stone) {
            char count_text[32];
            char alias[SHAKTI_PATH_CAPACITY];

            if (extract_tag__tablet_h(
                    trimmed,
                    "level",
                    tablet->level,
                    sizeof(tablet->level)) ||
                extract_tag__tablet_h(
                    trimmed,
                    "lesson",
                    tablet->lesson,
                    sizeof(tablet->lesson))) {
                continue;
            }

            if (extract_tag__tablet_h(
                    trimmed,
                    "stone_count",
                    count_text,
                    sizeof(count_text))) {
                char *end;
                unsigned long parsed;

                parsed = strtoul(count_text, &end, 10);

                if (end == count_text ||
                    *end != '\0' ||
                    parsed == 0UL ||
                    parsed > SHAKTI_MAX_TABLET_STONES) {
                    fclose(file);
                    return 0;
                }

                declared_count = (size_t)parsed;
                continue;
            }

            if (extract_tag__tablet_h(
                    trimmed,
                    "visual_text",
                    alias,
                    sizeof(alias))) {
                fclose(file);
                return 0;
            }

            continue;
        }

        if (extract_tag__tablet_h(
                trimmed,
                "text",
                current.text,
                sizeof(current.text))) {
            if ((fields_seen & 1U) != 0U) {
                fclose(file);
                return 0;
            }

            fields_seen |= 1U;
            continue;
        }

        if (extract_tag__tablet_h(
                trimmed,
                "written_text",
                current.written_text,
                sizeof(current.written_text)) ||
            extract_tag__tablet_h(
                trimmed,
                "visual_text",
                current.written_text,
                sizeof(current.written_text))) {
            if ((fields_seen & 2U) != 0U) {
                fclose(file);
                return 0;
            }

            fields_seen |= 2U;
            continue;
        }

        if (extract_tag__tablet_h(
                trimmed,
                "sound_art",
                current.sound_art,
                sizeof(current.sound_art))) {
            if ((fields_seen & 4U) != 0U) {
                fclose(file);
                return 0;
            }

            fields_seen |= 4U;
            continue;
        }

        if (extract_tag__tablet_h(
                trimmed,
                "visual_art",
                current.visual_art,
                sizeof(current.visual_art))) {
            if ((fields_seen & 8U) != 0U) {
                fclose(file);
                return 0;
            }

            fields_seen |= 8U;
            continue;
        }
    }

    if (ferror(file) ||
        fclose(file) != 0 ||
        !schema_seen ||
        inside_stone ||
        tablet->level[0] == '\0' ||
        tablet->lesson[0] == '\0' ||
        declared_count == 0U ||
        tablet->stone_count != declared_count) {
        shakti_tablet_init(tablet);
        return 0;
    }

    tablet->loaded = 1U;

    return 1;
}


int shakti_tablet_parse(
    shakti_tablet_t *tablet,
    const char *xml_path
)
{
    return shakti_tablet_load_internal(
        tablet,
        xml_path,
        NULL,
        0
    );
}

int shakti_tablet_load(
    shakti_tablet_t *tablet,
    const char *xml_path,
    const char *artifact_root
)
{
    return shakti_tablet_load_internal(
        tablet,
        xml_path,
        artifact_root,
        1
    );
}

int shakti_tablet_audit(
    const shakti_tablet_t *tablet,
    const char *artifact_root,
    shakti_tablet_audit_t *audit
)
{
    size_t index;

    if (tablet == NULL ||
        artifact_root == NULL ||
        audit == NULL ||
        !tablet->loaded) {
        return 0;
    }

    memset(audit, 0, sizeof(*audit));
    audit->stone_count = tablet->stone_count;
    audit->score = shakti_score_make(0UL, 0UL);

    for (index = 0U; index < tablet->stone_count; ++index) {
        const shakti_stone_t *stone;
        shakti_stone_audit_t *stone_audit;
        char path[SHAKTI_PATH_CAPACITY];
        unsigned long passed;
        unsigned long required;
        int sound_required;

        stone = &tablet->stones[index];
        stone_audit = &audit->stones[index];
        passed = 0UL;
        sound_required =
            strcmp(tablet->level, "foundation_ascii") != 0;
        required = sound_required ? 4UL : 3UL;

        stone_audit->text_ok =
            stone->text[0] != '\0' &&
            validate_filename_contract(stone);

        if (stone_audit->text_ok) {
            passed++;
        }

        if (shakti_artifact_join(
                artifact_root,
                "Visual_text",
                stone->written_text,
                path,
                sizeof(path)) &&
            shakti_artifact_validate_written_text(
                path,
                stone->text)) {
            stone_audit->written_text_ok = 1U;
            passed++;
        }

        if (validate_sound_artifacts(
                stone,
                artifact_root)) {
            stone_audit->sound_art_ok = 1U;

            if (sound_required) {
                passed++;
            }
        }

        if (shakti_artifact_join(
                artifact_root,
                "Visual_art",
                stone->visual_art,
                path,
                sizeof(path)) &&
            shakti_artifact_validate_svg(path)) {
            stone_audit->visual_art_ok = 1U;
            passed++;
        }

        stone_audit->score = shakti_score_make(passed, required);
        shakti_score_add(&audit->score, &stone_audit->score);
    }

    return 1;
}

void shakti_tablet_print(const shakti_tablet_t *tablet)
{
    size_t index;

    if (tablet == NULL || !tablet->loaded) {
        puts("No tablet is loaded.");
        return;
    }

    printf(
        "Tablet level %s, lesson %s, stones %lu.\n",
        tablet->level,
        tablet->lesson,
        (unsigned long)tablet->stone_count
    );

    for (index = 0U; index < tablet->stone_count; ++index) {
        const shakti_stone_t *stone;

        stone = &tablet->stones[index];

        printf(
            "%u. text=%s written_text=%s sound_art=%s visual_art=%s\n",
            stone->order,
            stone->text,
            stone->written_text,
            stone->sound_art,
            stone->visual_art
        );
    }
}
