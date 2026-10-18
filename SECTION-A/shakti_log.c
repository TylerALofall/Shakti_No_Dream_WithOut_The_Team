#include "shakti_log.h"

#include <inttypes.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shakti_config.h"
#include "shakti_time.h"

static uint32_t fnv1a_text(const char *text)
{
    uint32_t hash;

    hash = UINT32_C(2166136261);

    while (*text != '\0') {
        hash ^= (unsigned char)*text;
        hash *= UINT32_C(16777619);
        text++;
    }

    return hash;
}

static int sanitize_field(
    const char *source,
    char *destination,
    size_t destination_size
)
{
    size_t index;

    if (destination_size == 0U) {
        return 0;
    }

    index = 0U;

    while (*source != '\0') {
        unsigned char character;

        character = (unsigned char)*source;

        if (index + 1U >= destination_size) return 0;

        if (character == '\t' ||
            character == '\r' ||
            character == '\n') {
            destination[index++] = ' ';
        } else if (character >= 32U && character != 127U) {
            destination[index++] = (char)character;
        }

        source++;
    }

    destination[index] = '\0';
    return 1;
}

const char *shakti_channel_name(shakti_channel_t channel)
{
    switch (channel) {
        case SHAKTI_CHANNEL_TEXT:
            return "text";

        case SHAKTI_CHANNEL_WRITTEN_TEXT:
            return "written_text";

        case SHAKTI_CHANNEL_VISUAL_ART:
            return "visual_art";

        case SHAKTI_CHANNEL_SOUND_ART:
            return "sound_art";

        default:
            return "invalid";
    }
}

int shakti_log_append(
    const char *path,
    const char *prefix,
    const shakti_tick_t *tick,
    const char *section,
    unsigned int function_order,
    const char *phase,
    const char *channel,
    const char *subject,
    const char *property,
    const char *value
)
{
    FILE *file;
    char event_id[SHAKTI_EVENT_ID_CAPACITY];
    char safe_phase[SHAKTI_TEXT_CAPACITY];
    char safe_channel[SHAKTI_TEXT_CAPACITY];
    char safe_subject[SHAKTI_TEXT_CAPACITY];
    char safe_property[SHAKTI_TEXT_CAPACITY];
    char safe_value[SHAKTI_INPUT_CAPACITY];
    char payload[SHAKTI_LINE_CAPACITY];
    uint32_t checksum;
    int written;
    int success;

    if (path == NULL ||
        prefix == NULL ||
        tick == NULL ||
        section == NULL ||
        phase == NULL ||
        channel == NULL ||
        subject == NULL ||
        property == NULL ||
        value == NULL) {
        return 0;
    }

    if (!shakti_event_id_format(
            tick,
            section,
            function_order,
            event_id,
            sizeof(event_id))) {
        return 0;
    }

    if (!sanitize_field(phase, safe_phase, sizeof(safe_phase)) ||
        !sanitize_field(channel, safe_channel, sizeof(safe_channel)) ||
        !sanitize_field(subject, safe_subject, sizeof(safe_subject)) ||
        !sanitize_field(property, safe_property, sizeof(safe_property)) ||
        !sanitize_field(value, safe_value, sizeof(safe_value))) return 0;

    written = snprintf(
        payload,
        sizeof(payload),
        "%s\t%s\t%s\t%s\t%s\t%s\t%s",
        prefix,
        event_id,
        safe_phase,
        safe_channel,
        safe_subject,
        safe_property,
        safe_value
    );

    if (written < 0 || (size_t)written >= sizeof(payload)) {
        return 0;
    }

    checksum = fnv1a_text(payload);
    file = fopen(path, "a");

    if (file == NULL) {
        return 0;
    }

    success = fprintf(
        file,
        "%s\t%08" PRIx32 "\n",
        payload,
        checksum
    ) > 0;

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

int shakti_log_read_record(FILE *file, char *line, size_t capacity,
    char *fields[7], shakti_tick_t *tick)
{
    if (file == NULL || line == NULL || fields == NULL || tick == NULL ||
        capacity < 2U || capacity > (size_t)INT_MAX) return -1;
    for (;;) {
        char *checksum, *end, *cursor;
        unsigned long stored, frame;
        long epoch;
        size_t size, i;
        if (fgets(line, (int)capacity, file) == NULL) return ferror(file) ? -1 : 0;
        size = strlen(line);
        if (!size || line[size - 1U] != '\n') {
            int c;
            while ((c = fgetc(file)) != EOF && c != '\n') { }
            return -1;
        }
        line[size - 1U] = '\0';
        if (!line[0] || line[0] == '#') continue;
        checksum = strrchr(line, '\t');
        if (checksum == NULL || strlen(checksum + 1U) != 8U) return -1;
        *checksum++ = '\0';
        errno = 0;
        stored = strtoul(checksum, &end, 16);
        if (errno || *end || stored > UINT32_MAX ||
            (uint32_t)stored != fnv1a_text(line)) return -1;
        cursor = line;
        for (i = 0U; i < 7U; ++i) {
            fields[i] = cursor;
            end = strchr(cursor, '\t');
            if (i < 6U) {
                if (end == NULL) return -1;
                *end = '\0';
                cursor = end + 1U;
            } else if (end != NULL) return -1;
        }
        if (fields[1][0] != '[') return -1;
        errno = 0;
        epoch = strtol(fields[1] + 1U, &end, 10);
        if (errno || end == fields[1] + 1U || epoch < 0L || *end != ':') return -1;
        cursor = end + 1U;
        errno = 0;
        frame = strtoul(cursor, &end, 10);
        if (errno || end == cursor || frame > UINT_MAX || strncmp(end, "]-[", 3U) != 0) return -1;
        tick->epoch_seconds = (time_t)epoch;
        tick->frame = (unsigned int)frame;
        return 1;
    }
}

int shakti_log_latest_epoch(const char *path, time_t *latest)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY], *fields[7];
    shakti_tick_t tick;
    time_t candidate;
    int result, ok;
    if (path == NULL || latest == NULL) return 0;
    file = fopen(path, "rb");
    if (file == NULL) return errno == ENOENT;
    candidate = *latest;
    while ((result = shakti_log_read_record(file, line, sizeof(line), fields, &tick)) > 0)
        if (tick.epoch_seconds > candidate) candidate = tick.epoch_seconds;
    ok = fclose(file) == 0 && result == 0;
    if (ok) *latest = candidate;
    return ok;
}

int shakti_log_validate_file(const char *path,
    unsigned long *valid_lines, unsigned long *invalid_lines)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY], *fields[7];
    shakti_tick_t tick;
    unsigned long valid = 0UL, invalid = 0UL;
    int result, failed = 0;
    if (path == NULL || (file = fopen(path, "rb")) == NULL) return 0;
    while ((result = shakti_log_read_record(file, line, sizeof(line), fields, &tick)) != 0) {
        if (result > 0) ++valid;
        else ++invalid;
        if (ferror(file)) { failed = 1; break; }
    }
    if (fclose(file) != 0) failed = 1;
    if (valid_lines != NULL) *valid_lines = valid;
    if (invalid_lines != NULL) *invalid_lines = invalid;
    return !failed;
}
