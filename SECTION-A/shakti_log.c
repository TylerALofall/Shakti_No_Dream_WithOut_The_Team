#include "shakti_log.h"

#include <inttypes.h>
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

static void sanitize_field(
    const char *source,
    char *destination,
    size_t destination_size
)
{
    size_t index;

    if (destination_size == 0U) {
        return;
    }

    index = 0U;

    while (*source != '\0' && index + 1U < destination_size) {
        unsigned char character;

        character = (unsigned char)*source;

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
    char safe_value[SHAKTI_TEXT_CAPACITY];
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

    sanitize_field(phase, safe_phase, sizeof(safe_phase));
    sanitize_field(channel, safe_channel, sizeof(safe_channel));
    sanitize_field(subject, safe_subject, sizeof(safe_subject));
    sanitize_field(property, safe_property, sizeof(safe_property));
    sanitize_field(value, safe_value, sizeof(safe_value));

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

int shakti_log_validate_file(
    const char *path,
    unsigned long *valid_lines,
    unsigned long *invalid_lines
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];
    unsigned long valid;
    unsigned long invalid;

    if (path == NULL) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    valid = 0UL;
    invalid = 0UL;

    while (fgets(line, sizeof(line), file) != NULL) {
        char *checksum_text;
        char *end;
        unsigned long stored_checksum;
        uint32_t calculated_checksum;
        size_t length;

        length = strlen(line);

        if (length == 0U || line[length - 1U] != '\n') {
            invalid++;
            continue;
        }

        line[length - 1U] = '\0';

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        checksum_text = strrchr(line, '\t');

        if (checksum_text == NULL) {
            invalid++;
            continue;
        }

        *checksum_text = '\0';
        checksum_text++;
        stored_checksum = strtoul(checksum_text, &end, 16);

        if (end == checksum_text || *end != '\0') {
            invalid++;
            continue;
        }

        calculated_checksum = fnv1a_text(line);

        if ((uint32_t)stored_checksum == calculated_checksum) {
            valid++;
        } else {
            invalid++;
        }
    }

    fclose(file);

    if (valid_lines != NULL) {
        *valid_lines = valid;
    }

    if (invalid_lines != NULL) {
        *invalid_lines = invalid;
    }

    return 1;
}
