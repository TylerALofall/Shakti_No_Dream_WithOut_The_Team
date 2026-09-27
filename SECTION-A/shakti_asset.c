#include "shakti_asset.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int append_character(
    char *destination,
    size_t destination_size,
    size_t *used,
    char character
)
{
    if (*used + 1U >= destination_size) {
        return 0;
    }

    destination[(*used)++] = character;
    destination[*used] = '\0';

    return 1;
}

static int append_text__asset_h(
    char *destination,
    size_t destination_size,
    size_t *used,
    const char *text
)
{
    size_t length;

    length = strlen(text);

    if (*used + length >= destination_size) {
        return 0;
    }

    memcpy(destination + *used, text, length + 1U);
    *used += length;

    return 1;
}

int shakti_asset_key_from_text(
    const char *text,
    char *destination,
    size_t destination_size
)
{
    const unsigned char *cursor;
    size_t used;
    size_t length;

    if (text == NULL ||
        destination == NULL ||
        destination_size == 0U ||
        text[0] == '\0') {
        return 0;
    }

    length = strlen(text);
    destination[0] = '\0';

    if (length == 1U &&
        !isalnum((unsigned char)text[0])) {
        int written;

        written = snprintf(
            destination,
            destination_size,
            "ascii_%03u",
            (unsigned int)(unsigned char)text[0]
        );

        return written >= 0 &&
               (size_t)written < destination_size;
    }

    if (text[0] == ' ' ||
        text[length - 1U] == ' ') {
        return 0;
    }

    used = 0U;
    cursor = (const unsigned char *)text;

    while (*cursor != '\0') {
        unsigned char character;

        character = *cursor++;

        if (character < 32U || character > 126U) {
            return 0;
        }

        if (isalnum(character) || character == '-') {
            if (!append_character(
                    destination,
                    destination_size,
                    &used,
                    (char)character)) {
                return 0;
            }

            continue;
        }

        if (character == ' ') {
            if (cursor[0] == ' ' ||
                !append_character(
                    destination,
                    destination_size,
                    &used,
                    '_')) {
                return 0;
            }

            continue;
        }

        {
            char encoded[4];
            int written;

            written = snprintf(
                encoded,
                sizeof(encoded),
                "%%%02X",
                (unsigned int)character
            );

            if (written < 0 ||
                (size_t)written >= sizeof(encoded) ||
                !append_text__asset_h(
                    destination,
                    destination_size,
                    &used,
                    encoded)) {
                return 0;
            }
        }
    }

    return used > 0U;
}

int shakti_asset_filename(
    const char *text,
    const char *extension,
    char *destination,
    size_t destination_size
)
{
    char key[192];
    int written;

    if (extension == NULL ||
        !shakti_asset_key_from_text(
            text,
            key,
            sizeof(key))) {
        return 0;
    }

    written = snprintf(
        destination,
        destination_size,
        "%s%s",
        key,
        extension
    );

    return written >= 0 &&
           (size_t)written < destination_size;
}

int shakti_asset_safe_filename(const char *filename)
{
    const unsigned char *cursor;

    if (filename == NULL ||
        filename[0] == '\0' ||
        strstr(filename, "..") != NULL) {
        return 0;
    }

    cursor = (const unsigned char *)filename;

    while (*cursor != '\0') {
        if (*cursor == '/' ||
            *cursor == '\\' ||
            *cursor < 32U ||
            *cursor == 127U) {
            return 0;
        }

        cursor++;
    }

    return 1;
}
