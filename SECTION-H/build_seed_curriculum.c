/* File: tools/build_seed_curriculum.c */

#include "shakti_build_seed_curriculum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shakti_artifact.h"
#include "shakti_config.h"
#include "shakti_asset.h"
#include "shakti_handwriting.h"

#define ASCII_FIRST 32U
#define ASCII_LAST 126U
#define COUNTING_STONES 11U
#define ALPHABET_LETTERS 26U
#define TEXT_SVG_COLUMNS 13U
#define TEXT_SVG_CELL 72U
#define TEXT_SVG_MARGIN 8U

static const char *COUNTING_WORDS[COUNTING_STONES] = {
    "zero", "one", "two", "three", "four", "five",
    "six", "seven", "eight", "nine", "ten"
};

static int xml_write_escaped__seed_h(FILE *file, const char *text)
{
    while (*text != '\0') {
        const char *entity;

        entity = NULL;

        switch (*text) {
            case '&':
                entity = "&amp;";
                break;
            case '<':
                entity = "&lt;";
                break;
            case '>':
                entity = "&gt;";
                break;
            case '"':
                entity = "&quot;";
                break;
            case '\'':
                entity = "&apos;";
                break;
            default:
                break;
        }

        if (entity != NULL) {
            if (fputs(entity, file) < 0) {
                return 0;
            }
        } else if (fputc((unsigned char)*text, file) == EOF) {
            return 0;
        }

        text++;
    }

    return 1;
}

static int write_tag__seed_h(
    FILE *file,
    const char *indent,
    const char *tag,
    const char *value
)
{
    return fprintf(file, "%s<%s>", indent, tag) >= 0 &&
           xml_write_escaped__seed_h(file, value) &&
           fprintf(file, "</%s>\n", tag) >= 0;
}

static int append_text__seed_h(
    char *destination,
    size_t destination_size,
    const char *text
)
{
    size_t used;
    size_t added;

    used = strlen(destination);
    added = strlen(text);

    if (used + added >= destination_size) {
        return 0;
    }

    memcpy(destination + used, text, added + 1U);

    return 1;
}

static int append_sound_clip(
    char *sequence,
    size_t sequence_size,
    const char *clip
)
{
    if (sequence[0] != '\0' &&
        !append_text__seed_h(sequence, sequence_size, "|")) {
        return 0;
    }

    return append_text__seed_h(sequence, sequence_size, clip);
}

static int append_character_sound(
    char *sequence,
    size_t sequence_size,
    unsigned char character
)
{
    char text[2];
    char filename[SHAKTI_PATH_CAPACITY];

    if (character == ' ') {
        return 1;
    }

    text[0] = (char)character;
    text[1] = '\0';

    return shakti_asset_filename(
               text,
               ".wav",
               filename,
               sizeof(filename)) &&
           append_sound_clip(
               sequence,
               sequence_size,
               filename
           );
}

static int make_character_sound_sequence(
    const char *text,
    char *sequence,
    size_t sequence_size
)
{
    const unsigned char *cursor;

    sequence[0] = '\0';
    cursor = (const unsigned char *)text;

    while (*cursor != '\0') {
        if (!append_character_sound(
                sequence,
                sequence_size,
                *cursor)) {
            return 0;
        }

        cursor++;
    }

    return sequence[0] != '\0';
}

static int write_handwriting_svg(
    const char *text,
    const char *path
)
{
    FILE *file;
    size_t length;
    size_t columns;
    size_t rows_needed;
    size_t index;
    int success;

    if (text == NULL ||
        path == NULL ||
        !shakti_handwriting_supports_text(text)) {
        return 0;
    }

    length = strlen(text);
    columns = length < TEXT_SVG_COLUMNS
        ? length
        : TEXT_SVG_COLUMNS;

    if (columns == 0U) {
        columns = 1U;
    }

    rows_needed =
        (length + TEXT_SVG_COLUMNS - 1U) /
        TEXT_SVG_COLUMNS;

    if (rows_needed == 0U) {
        rows_needed = 1U;
    }

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success = fprintf(
        file,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "viewBox=\"0 0 %lu %lu\" role=\"img\">\n"
        "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n",
        (unsigned long)(
            TEXT_SVG_MARGIN * 2U +
            columns * TEXT_SVG_CELL
        ),
        (unsigned long)(
            TEXT_SVG_MARGIN * 2U +
            rows_needed * TEXT_SVG_CELL
        )
    ) > 0;

    for (index = 0U; success && index < length; ++index) {
        char pixels[8][9];
        size_t pixel_row;
        size_t pixel_column;
        size_t text_column;
        size_t text_row;

        if (!shakti_handwriting_render_char(
                (unsigned char)text[index],
                pixels)) {
            success = 0;
            break;
        }

        text_column = index % TEXT_SVG_COLUMNS;
        text_row = index / TEXT_SVG_COLUMNS;

        for (pixel_row = 0U;
             success && pixel_row < 8U;
             ++pixel_row) {
            for (pixel_column = 0U;
                 success && pixel_column < 8U;
                 ++pixel_column) {
                if (pixels[pixel_row][pixel_column] == '#') {
                    success = fprintf(
                        file,
                        "  <rect x=\"%lu\" y=\"%lu\" "
                        "width=\"8\" height=\"8\" fill=\"black\"/>\n",
                        (unsigned long)(
                            TEXT_SVG_MARGIN +
                            text_column * TEXT_SVG_CELL +
                            pixel_column * 8U
                        ),
                        (unsigned long)(
                            TEXT_SVG_MARGIN +
                            text_row * TEXT_SVG_CELL +
                            pixel_row * 8U
                        )
                    ) > 0;
                }
            }
        }
    }

    if (success) {
        success = fputs("</svg>\n", file) >= 0;
    }

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int write_counting_svg(
    unsigned int quantity,
    const char *word,
    const char *path
)
{
    FILE *file;
    unsigned int index;
    int success;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success = fprintf(
        file,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "viewBox=\"0 0 640 240\" role=\"img\">\n"
        "  <rect width=\"640\" height=\"240\" fill=\"white\"/>\n"
        "  <text x=\"40\" y=\"85\" font-size=\"72\" "
        "font-family=\"monospace\">%u</text>\n"
        "  <text x=\"40\" y=\"165\" font-size=\"48\" "
        "font-family=\"monospace\">%s</text>\n",
        quantity,
        word
    ) > 0;

    for (index = 0U; success && index < quantity; ++index) {
        unsigned int column;
        unsigned int row;
        unsigned int x;
        unsigned int y;

        column = index % 5U;
        row = index / 5U;
        x = 330U + column * 55U;
        y = 65U + row * 70U;

        success = fprintf(
            file,
            "  <circle cx=\"%u\" cy=\"%u\" r=\"20\" "
            "fill=\"black\"/>\n",
            x,
            y
        ) > 0;
    }

    if (success) {
        success = fputs("</svg>\n", file) >= 0;
    }

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int prepare_text_artifacts(
    const char *text,
    const char *artifact_root,
    char *written,
    size_t written_size,
    char *visual,
    size_t visual_size
)
{
    char written_path[SHAKTI_PATH_CAPACITY];
    char visual_path[SHAKTI_PATH_CAPACITY];

    return shakti_asset_filename(
               text,
               ".8x8.txt",
               written,
               written_size) &&
           shakti_asset_filename(
               text,
               ".svg",
               visual,
               visual_size) &&
           shakti_artifact_join(
               artifact_root,
               "Visual_text",
               written,
               written_path,
               sizeof(written_path)) &&
           shakti_artifact_join(
               artifact_root,
               "Visual_art",
               visual,
               visual_path,
               sizeof(visual_path)) &&
           shakti_handwriting_write_text_artifact(
               text,
               written_path) &&
           write_handwriting_svg(
               text,
               visual_path);
}

static int write_stone_header(
    FILE *file,
    unsigned int order,
    const char *text,
    const char *written,
    const char *sound,
    const char *visual
)
{
    return fprintf(file, "  <stone order=\"%u\">\n", order) > 0 &&
           write_tag__seed_h(file, "    ", "text", text) &&
           write_tag__seed_h(file, "    ", "written_text", written) &&
           write_tag__seed_h(file, "    ", "sound_art", sound) &&
           write_tag__seed_h(file, "    ", "visual_art", visual);
}

static int write_tablet_start(
    FILE *file,
    const char *level,
    const char *lesson,
    unsigned int stone_count
)
{
    return fprintf(
        file,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<tablet schema=\"SHAKTI_TABLET_4S_V2\">\n"
        "  <level>%s</level>\n"
        "  <lesson>%s</lesson>\n"
        "  <stone_count>%u</stone_count>\n",
        level,
        lesson,
        stone_count
    ) > 0;
}

static int finish_tablet(FILE *file, int success)
{
    if (success) {
        success = fputs("</tablet>\n", file) >= 0;
    }

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

static int build_ascii(
    const char *xml_path,
    const char *artifact_root
)
{
    FILE *xml;
    unsigned int character;
    int success;

    xml = fopen(xml_path, "w");

    if (xml == NULL) {
        return 0;
    }

    success = write_tablet_start(
        xml,
        "foundation_ascii",
        "ascii_32_126_solo",
        95U
    );

    for (character = ASCII_FIRST;
         success && character <= ASCII_LAST;
         ++character) {
        char text[2];
        char written[SHAKTI_PATH_CAPACITY];
        char sound[SHAKTI_PATH_CAPACITY];
        char visual[SHAKTI_PATH_CAPACITY];
        char ascii_code[16];

        text[0] = (char)character;
        text[1] = '\0';

        success =
            prepare_text_artifacts(
                text,
                artifact_root,
                written,
                sizeof(written),
                visual,
                sizeof(visual)) &&
            shakti_asset_filename(
                text,
                ".wav",
                sound,
                sizeof(sound)) &&
            write_stone_header(
                xml,
                character - ASCII_FIRST + 1U,
                text,
                written,
                sound,
                visual);

        if (!success) {
            break;
        }

        snprintf(ascii_code, sizeof(ascii_code), "%u", character);

        success =
            write_tag__seed_h(xml, "    ", "ascii_code", ascii_code) &&
            fprintf(xml, "  </stone>\n") > 0;
    }

    return finish_tablet(xml, success);
}

static int build_counting(
    const char *xml_path,
    const char *lesson,
    const char *artifact_root
)
{
    FILE *xml;
    unsigned int quantity;
    int success;

    xml = fopen(xml_path, "w");

    if (xml == NULL) {
        return 0;
    }

    success = write_tablet_start(
        xml,
        "counting",
        lesson,
        COUNTING_STONES
    );

    for (quantity = 0U;
         success && quantity < COUNTING_STONES;
         ++quantity) {
        const char *text;
        char written[SHAKTI_PATH_CAPACITY];
        char sound[SHAKTI_PATH_CAPACITY];
        char visual[SHAKTI_PATH_CAPACITY];
        char visual_path[SHAKTI_PATH_CAPACITY];
        char number[32];
        char duration[32];

        text = COUNTING_WORDS[quantity];

        success =
            shakti_asset_filename(
                text,
                ".8x8.txt",
                written,
                sizeof(written)) &&
            shakti_asset_filename(
                text,
                ".wav",
                sound,
                sizeof(sound)) &&
            shakti_asset_filename(
                text,
                ".svg",
                visual,
                sizeof(visual)) &&
            shakti_artifact_join(
                artifact_root,
                "Visual_text",
                written,
                visual_path,
                sizeof(visual_path)) &&
            shakti_handwriting_write_text_artifact(
                text,
                visual_path) &&
            shakti_artifact_join(
                artifact_root,
                "Visual_art",
                visual,
                visual_path,
                sizeof(visual_path)) &&
            write_counting_svg(
                quantity,
                text,
                visual_path) &&
            write_stone_header(
                xml,
                quantity + 1U,
                text,
                written,
                sound,
                visual);

        if (!success) {
            break;
        }

        snprintf(number, sizeof(number), "%u", quantity);
        snprintf(
            duration,
            sizeof(duration),
            "%u",
            quantity * 1000U
        );

        success =
            write_tag__seed_h(xml, "    ", "numeral", number) &&
            write_tag__seed_h(xml, "    ", "quantity", number) &&
            write_tag__seed_h(xml, "    ", "duration_ms", duration) &&
            fprintf(xml, "  </stone>\n") > 0;
    }

    return finish_tablet(xml, success);
}

static int build_alphabet_solo(
    const char *xml_path,
    const char *lesson,
    const char *artifact_root,
    unsigned char first
)
{
    FILE *xml;
    unsigned int index;
    int success;

    xml = fopen(xml_path, "w");

    if (xml == NULL) {
        return 0;
    }

    success = write_tablet_start(
        xml,
        "alphabet",
        lesson,
        ALPHABET_LETTERS
    );

    for (index = 0U;
         success && index < ALPHABET_LETTERS;
         ++index) {
        char text[2];
        char written[SHAKTI_PATH_CAPACITY];
        char sound[SHAKTI_PATH_CAPACITY];
        char visual[SHAKTI_PATH_CAPACITY];
        char position[16];

        text[0] = (char)(first + index);
        text[1] = '\0';

        success =
            prepare_text_artifacts(
                text,
                artifact_root,
                written,
                sizeof(written),
                visual,
                sizeof(visual)) &&
            shakti_asset_filename(
                text,
                ".wav",
                sound,
                sizeof(sound)) &&
            write_stone_header(
                xml,
                index + 1U,
                text,
                written,
                sound,
                visual);

        if (!success) {
            break;
        }

        snprintf(position, sizeof(position), "%u", index + 1U);

        success =
            write_tag__seed_h(xml, "    ", "alphabet_position", position) &&
            fprintf(xml, "  </stone>\n") > 0;
    }

    return finish_tablet(xml, success);
}

static int build_case_pairs_and_counting(
    const char *xml_path,
    const char *artifact_root
)
{
    FILE *xml;
    unsigned int index;
    unsigned int order;
    int success;

    xml = fopen(xml_path, "w");

    if (xml == NULL) {
        return 0;
    }

    success = write_tablet_start(
        xml,
        "alphabet",
        "alphabet_case_pairs_and_counting",
        ALPHABET_LETTERS + 10U
    );

    order = 1U;

    for (index = 0U;
         success && index < ALPHABET_LETTERS;
         ++index) {
        char text[3];
        char written[SHAKTI_PATH_CAPACITY];
        char sound[SHAKTI_PATH_CAPACITY];
        char visual[SHAKTI_PATH_CAPACITY];
        char upper[2];
        char lower[2];
        char position[16];

        upper[0] = (char)('A' + index);
        upper[1] = '\0';
        lower[0] = (char)('a' + index);
        lower[1] = '\0';

        text[0] = upper[0];
        text[1] = lower[0];
        text[2] = '\0';

        success =
            prepare_text_artifacts(
                text,
                artifact_root,
                written,
                sizeof(written),
                visual,
                sizeof(visual)) &&
            make_character_sound_sequence(
                text,
                sound,
                sizeof(sound)) &&
            write_stone_header(
                xml,
                order,
                text,
                written,
                sound,
                visual);

        if (!success) {
            break;
        }

        snprintf(position, sizeof(position), "%u", index + 1U);

        success =
            write_tag__seed_h(xml, "    ", "uppercase", upper) &&
            write_tag__seed_h(xml, "    ", "lowercase", lower) &&
            write_tag__seed_h(xml, "    ", "alphabet_position", position) &&
            write_tag__seed_h(xml, "    ", "bridge_kind", "case_pair") &&
            fprintf(xml, "  </stone>\n") > 0;

        order++;
    }

    for (index = 0U; success && index < 10U; ++index) {
        char text[32];
        char written[SHAKTI_PATH_CAPACITY];
        char sound[SHAKTI_PATH_CAPACITY];
        char visual[SHAKTI_PATH_CAPACITY];
        char count_sound[SHAKTI_PATH_CAPACITY];
        char upper[2];
        char lower[2];
        char position[16];

        upper[0] = (char)('A' + index);
        upper[1] = '\0';
        lower[0] = (char)('a' + index);
        lower[1] = '\0';

        if (snprintf(
                text,
                sizeof(text),
                "%u %c %c",
                index + 1U,
                upper[0],
                lower[0]) < 0 ||
            !shakti_asset_filename(
                COUNTING_WORDS[index + 1U],
                ".wav",
                count_sound,
                sizeof(count_sound))) {
            success = 0;
            break;
        }

        sound[0] = '\0';

        success =
            prepare_text_artifacts(
                text,
                artifact_root,
                written,
                sizeof(written),
                visual,
                sizeof(visual)) &&
            append_sound_clip(
                sound,
                sizeof(sound),
                count_sound) &&
            append_character_sound(
                sound,
                sizeof(sound),
                (unsigned char)upper[0]) &&
            append_character_sound(
                sound,
                sizeof(sound),
                (unsigned char)lower[0]) &&
            write_stone_header(
                xml,
                order,
                text,
                written,
                sound,
                visual);

        if (!success) {
            break;
        }

        snprintf(position, sizeof(position), "%u", index + 1U);

        success =
            write_tag__seed_h(xml, "    ", "quantity", position) &&
            write_tag__seed_h(xml, "    ", "uppercase", upper) &&
            write_tag__seed_h(xml, "    ", "lowercase", lower) &&
            write_tag__seed_h(xml, "    ", "bridge_kind", "counting_case") &&
            fprintf(xml, "  </stone>\n") > 0;

        order++;
    }

    return finish_tablet(xml, success);
}

static int write_sequence_stone(
    FILE *xml,
    unsigned int order,
    const char *text,
    const char *sequence_kind,
    unsigned int sequence_length,
    const char *artifact_root
)
{
    char written[SHAKTI_PATH_CAPACITY];
    char sound[SHAKTI_PATH_CAPACITY];
    char visual[SHAKTI_PATH_CAPACITY];
    char length_text[16];

    if (!prepare_text_artifacts(
            text,
            artifact_root,
            written,
            sizeof(written),
            visual,
            sizeof(visual)) ||
        !make_character_sound_sequence(
            text,
            sound,
            sizeof(sound)) ||
        !write_stone_header(
            xml,
            order,
            text,
            written,
            sound,
            visual)) {
        return 0;
    }

    snprintf(
        length_text,
        sizeof(length_text),
        "%u",
        sequence_length
    );

    return write_tag__seed_h(
               xml,
               "    ",
               "sequence_kind",
               sequence_kind) &&
           write_tag__seed_h(
               xml,
               "    ",
               "sequence_length",
               length_text) &&
           fprintf(xml, "  </stone>\n") > 0;
}

static int build_growing_sequences(
    const char *xml_path,
    const char *artifact_root
)
{
    FILE *xml;
    char sequence[SHAKTI_TEXT_CAPACITY];
    unsigned int length;
    unsigned int order;
    int success;

    xml = fopen(xml_path, "w");

    if (xml == NULL) {
        return 0;
    }

    success = write_tablet_start(
        xml,
        "alphabet",
        "alphabet_growing_sequences",
        77U
    );

    sequence[0] = '\0';
    order = 1U;

    for (length = 1U;
         success && length <= ALPHABET_LETTERS;
         ++length) {
        char character[2];

        character[0] = (char)('A' + length - 1U);
        character[1] = '\0';

        success =
            append_text__seed_h(
                sequence,
                sizeof(sequence),
                character) &&
            write_sequence_stone(
                xml,
                order,
                sequence,
                "uppercase_prefix",
                length,
                artifact_root);

        order++;
    }

    sequence[0] = 'A';
    sequence[1] = '\0';

    for (length = 2U;
         success && length <= ALPHABET_LETTERS;
         ++length) {
        char character[2];

        character[0] = (char)('a' + length - 1U);
        character[1] = '\0';

        success =
            append_text__seed_h(
                sequence,
                sizeof(sequence),
                character) &&
            write_sequence_stone(
                xml,
                order,
                sequence,
                "capitalized_prefix",
                length,
                artifact_root);

        order++;
    }

    sequence[0] = '\0';

    for (length = 1U;
         success && length <= ALPHABET_LETTERS;
         ++length) {
        char pair[3];

        pair[0] = (char)('A' + length - 1U);
        pair[1] = (char)('a' + length - 1U);
        pair[2] = '\0';

        success =
            append_text__seed_h(
                sequence,
                sizeof(sequence),
                pair) &&
            write_sequence_stone(
                xml,
                order,
                sequence,
                "case_pair_prefix",
                length,
                artifact_root);

        order++;
    }

    return finish_tablet(xml, success && order == 78U);
}

static int join_xml_path(
    char *destination,
    size_t destination_size,
    const char *eden_data_root,
    const char *filename
)
{
    int written;

    written = snprintf(
        destination,
        destination_size,
        "%s/XML_text/%s",
        eden_data_root,
        filename
    );

    return written >= 0 &&
           (size_t)written < destination_size;
}

int shakti_build_seed_curriculum(
    const char *eden_data_root,
    const char *artifact_root
)
{
    char ascii_xml[SHAKTI_PATH_CAPACITY];
    char counting_xml[SHAKTI_PATH_CAPACITY];
    char timed_xml[SHAKTI_PATH_CAPACITY];
    char uppercase_xml[SHAKTI_PATH_CAPACITY];
    char lowercase_xml[SHAKTI_PATH_CAPACITY];
    char pairs_xml[SHAKTI_PATH_CAPACITY];
    char sequences_xml[SHAKTI_PATH_CAPACITY];

    if (eden_data_root == NULL || artifact_root == NULL) {
        return 0;
    }

    if (!join_xml_path(
            ascii_xml,
            sizeof(ascii_xml),
            eden_data_root,
            "00_ascii_32_126_solo.xml") ||
        !join_xml_path(
            counting_xml,
            sizeof(counting_xml),
            eden_data_root,
            "01_counting_zero_to_ten_solo.xml") ||
        !join_xml_path(
            timed_xml,
            sizeof(timed_xml),
            eden_data_root,
            "01_counting_zero_to_ten_timed_light.xml") ||
        !join_xml_path(
            uppercase_xml,
            sizeof(uppercase_xml),
            eden_data_root,
            "02_alphabet_uppercase_solo.xml") ||
        !join_xml_path(
            lowercase_xml,
            sizeof(lowercase_xml),
            eden_data_root,
            "02_alphabet_lowercase_solo.xml") ||
        !join_xml_path(
            pairs_xml,
            sizeof(pairs_xml),
            eden_data_root,
            "02_alphabet_case_pairs_and_counting.xml") ||
        !join_xml_path(
            sequences_xml,
            sizeof(sequences_xml),
            eden_data_root,
            "02_alphabet_growing_sequences.xml")) {
        return 0;
    }

    if (!build_ascii(ascii_xml, artifact_root) ||
        !build_counting(
            counting_xml,
            "counting_zero_to_ten_solo",
            artifact_root) ||
        !build_counting(
            timed_xml,
            "counting_zero_to_ten_timed_light",
            artifact_root) ||
        !build_alphabet_solo(
            uppercase_xml,
            "alphabet_uppercase_solo",
            artifact_root,
            (unsigned char)'A') ||
        !build_alphabet_solo(
            lowercase_xml,
            "alphabet_lowercase_solo",
            artifact_root,
            (unsigned char)'a') ||
        !build_case_pairs_and_counting(
            pairs_xml,
            artifact_root) ||
        !build_growing_sequences(
            sequences_xml,
            artifact_root)) {
        fputs("Could not build the seed curriculum.\n", stderr);
        return 0;
    }

    puts(
        "Foundation, counting, and alphabet curriculum generated."
    );

    return 1;
}

#ifndef SHAKTI_TOOL_NO_MAIN
#define BUILD_SEED_CURRICULUM_MAIN main
int BUILD_SEED_CURRICULUM_MAIN(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(
            stderr,
            "Usage: %s EDEN_DATA_ROOT ARTIFACT_ROOT\n",
            argv[0]
        );
        return EXIT_FAILURE;
    }

    if (!shakti_build_seed_curriculum(argv[1], argv[2])) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
#endif
