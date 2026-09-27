#include "shakti_artifact.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "shakti_asset.h"
#include "shakti_config.h"

static uint16_t read_u16_le(const unsigned char bytes[2])
{
    return (uint16_t)(
        ((uint16_t)bytes[0]) |
        ((uint16_t)bytes[1] << 8U)
    );
}

static uint32_t read_u32_le(const unsigned char bytes[4])
{
    return
        ((uint32_t)bytes[0]) |
        ((uint32_t)bytes[1] << 8U) |
        ((uint32_t)bytes[2] << 16U) |
        ((uint32_t)bytes[3] << 24U);
}

int shakti_artifact_join(
    const char *root,
    const char *directory,
    const char *filename,
    char *destination,
    size_t destination_size
)
{
    size_t root_length;
    int written;

    if (root == NULL ||
        directory == NULL ||
        filename == NULL ||
        destination == NULL ||
        !shakti_asset_safe_filename(filename)) {
        return 0;
    }

    root_length = strlen(root);

    written = snprintf(
        destination,
        destination_size,
        "%s%s%s/%s",
        root,
        root_length > 0U &&
            root[root_length - 1U] != '/' &&
            root[root_length - 1U] != '\\'
            ? "/"
            : "",
        directory,
        filename
    );

    return written >= 0 &&
           (size_t)written < destination_size;
}

int shakti_artifact_validate_written_text(
    const char *path,
    const char *expected_text
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];
    char expected[SHAKTI_TEXT_CAPACITY + 32U];
    size_t character_count;
    size_t character_index;

    if (path == NULL || expected_text == NULL) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    if (fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, "SHAKTI_WRITTEN_TEXT_8X8_V1\n") != 0) {
        fclose(file);
        return 0;
    }

    if (snprintf(
            expected,
            sizeof(expected),
            "TEXT=%s\n",
            expected_text) < 0 ||
        fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, expected) != 0) {
        fclose(file);
        return 0;
    }

    character_count = strlen(expected_text);

    if (snprintf(
            expected,
            sizeof(expected),
            "CHARACTERS=%lu\n",
            (unsigned long)character_count) < 0 ||
        fgets(line, sizeof(line), file) == NULL ||
        strcmp(line, expected) != 0) {
        fclose(file);
        return 0;
    }

    for (character_index = 0U;
         character_index < character_count;
         ++character_index) {
        unsigned int parsed_index;
        unsigned int parsed_ascii;
        char parsed_text;
        size_t row;

        if (fgets(line, sizeof(line), file) == NULL ||
            sscanf(
                line,
                "CHARACTER=%u ASCII=%u TEXT=%c",
                &parsed_index,
                &parsed_ascii,
                &parsed_text) != 3 ||
            parsed_index != character_index ||
            parsed_ascii !=
                (unsigned int)(unsigned char)expected_text[character_index] ||
            parsed_text != expected_text[character_index]) {
            fclose(file);
            return 0;
        }

        for (row = 0U; row < 8U; ++row) {
            size_t column;

            if (fgets(line, sizeof(line), file) == NULL ||
                strlen(line) != 9U ||
                line[8] != '\n') {
                fclose(file);
                return 0;
            }

            for (column = 0U; column < 8U; ++column) {
                if (line[column] != '.' &&
                    line[column] != '#') {
                    fclose(file);
                    return 0;
                }
            }
        }
    }

    if (fgets(line, sizeof(line), file) != NULL ||
        ferror(file) ||
        fclose(file) != 0) {
        return 0;
    }

    return 1;
}

int shakti_artifact_validate_svg(const char *path)
{
    FILE *file;
    char buffer[1024];
    size_t used;

    if (path == NULL) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    used = fread(buffer, 1U, sizeof(buffer) - 1U, file);

    if (ferror(file)) {
        fclose(file);
        return 0;
    }

    buffer[used] = '\0';

    if (fclose(file) != 0) {
        return 0;
    }

    return strstr(buffer, "<svg") != NULL;
}

int shakti_artifact_validate_wav(const char *path)
{
    FILE *file;
    unsigned char header[12];
    int format_found;
    int data_found;
    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_size;

    if (path == NULL) {
        return 0;
    }

    file = fopen(path, "rb");

    if (file == NULL) {
        return 0;
    }

    if (fread(header, 1U, sizeof(header), file) != sizeof(header) ||
        memcmp(header, "RIFF", 4U) != 0 ||
        memcmp(header + 8U, "WAVE", 4U) != 0) {
        fclose(file);
        return 0;
    }

    format_found = 0;
    data_found = 0;
    audio_format = 0U;
    channels = 0U;
    sample_rate = 0U;
    bits_per_sample = 0U;
    data_size = 0U;

    for (;;) {
        unsigned char chunk_header[8];
        uint32_t chunk_size;
        long next_position;

        if (fread(
                chunk_header,
                1U,
                sizeof(chunk_header),
                file) != sizeof(chunk_header)) {
            break;
        }

        chunk_size = read_u32_le(chunk_header + 4U);
        next_position = ftell(file);

        if (next_position < 0L) {
            fclose(file);
            return 0;
        }

        next_position +=
            (long)chunk_size +
            (long)(chunk_size & UINT32_C(1));

        if (memcmp(chunk_header, "fmt ", 4U) == 0) {
            unsigned char format[16];

            if (chunk_size < sizeof(format) ||
                fread(format, 1U, sizeof(format), file) != sizeof(format)) {
                fclose(file);
                return 0;
            }

            audio_format = read_u16_le(format);
            channels = read_u16_le(format + 2U);
            sample_rate = read_u32_le(format + 4U);
            bits_per_sample = read_u16_le(format + 14U);
            format_found = 1;
        } else if (memcmp(chunk_header, "data", 4U) == 0) {
            data_size = chunk_size;
            data_found = 1;
        }

        if (fseek(file, next_position, SEEK_SET) != 0) {
            fclose(file);
            return 0;
        }
    }

    if (ferror(file) || fclose(file) != 0) {
        return 0;
    }

    return format_found &&
           data_found &&
           audio_format == 1U &&
           channels == 1U &&
           sample_rate == UINT32_C(16000) &&
           bits_per_sample == 16U &&
           data_size > 0U &&
           (data_size % 2U) == 0U;
}
