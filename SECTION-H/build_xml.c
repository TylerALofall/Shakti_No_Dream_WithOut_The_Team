#include "shakti_build_xml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shakti_artifact.h"
#include "shakti_asset.h"
#include "shakti_config.h"
#include "shakti_handwriting.h"

#define BUILDER_MAX_STONES SHAKTI_MAX_TABLET_STONES

static char *trim_line_ending(char *text)
{
    size_t length;

    length = strlen(text);

    while (length > 0U &&
           (text[length - 1U] == '\n' ||
            text[length - 1U] == '\r')) {
        text[--length] = '\0';
    }

    return text;
}

static int xml_write_escaped__build_xml_h(FILE *file, const char *text)
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

static int count_stones(FILE *list, size_t *count)
{
    char line[SHAKTI_TEXT_CAPACITY];
    size_t total;

    total = 0U;

    while (fgets(line, sizeof(line), list) != NULL) {
        char key[SHAKTI_KEY_CAPACITY];
        size_t length;
        char *text;

        length = strlen(line);

        if (length > 0U &&
            line[length - 1U] != '\n' &&
            !feof(list)) {
            return 0;
        }

        text = trim_line_ending(line);

        if (text[0] == '\0' || text[0] == '#') {
            continue;
        }

        if (!shakti_handwriting_supports_text(text) ||
            !shakti_asset_key_from_text(
                text,
                key,
                sizeof(key)) ||
            total >= BUILDER_MAX_STONES) {
            return 0;
        }

        total++;
    }

    if (ferror(list) || total == 0U) {
        return 0;
    }

    *count = total;

    return 1;
}

static int write_tag__build_xml_h(
    FILE *file,
    const char *indent,
    const char *tag,
    const char *value
)
{
    return fprintf(file, "%s<%s>", indent, tag) >= 0 &&
           xml_write_escaped__build_xml_h(file, value) &&
           fprintf(file, "</%s>\n", tag) >= 0;
}

static int write_stone(
    FILE *output,
    const char *artifact_root,
    const char *text,
    size_t order
)
{
    char written_name[SHAKTI_PATH_CAPACITY];
    char sound_name[SHAKTI_PATH_CAPACITY];
    char visual_name[SHAKTI_PATH_CAPACITY];
    char written_path[SHAKTI_PATH_CAPACITY];
    char sound_path[SHAKTI_PATH_CAPACITY];
    char visual_path[SHAKTI_PATH_CAPACITY];

    if (!shakti_asset_filename(
            text,
            ".8x8.txt",
            written_name,
            sizeof(written_name)) ||
        !shakti_asset_filename(
            text,
            ".wav",
            sound_name,
            sizeof(sound_name)) ||
        !shakti_asset_filename(
            text,
            ".svg",
            visual_name,
            sizeof(visual_name)) ||
        !shakti_artifact_join(
            artifact_root,
            "Visual_text",
            written_name,
            written_path,
            sizeof(written_path)) ||
        !shakti_artifact_join(
            artifact_root,
            "Sound_art",
            sound_name,
            sound_path,
            sizeof(sound_path)) ||
        !shakti_artifact_join(
            artifact_root,
            "Visual_art",
            visual_name,
            visual_path,
            sizeof(visual_path))) {
        return 0;
    }

    if (!shakti_artifact_validate_wav(sound_path) ||
        !shakti_artifact_validate_svg(visual_path) ||
        !shakti_handwriting_write_text_artifact(
            text,
            written_path) ||
        !shakti_artifact_validate_written_text(
            written_path,
            text)) {
        return 0;
    }

    if (fprintf(
            output,
            "  <stone order=\"%lu\">\n",
            (unsigned long)order) < 0 ||
        !write_tag__build_xml_h(output, "    ", "text", text) ||
        !write_tag__build_xml_h(
            output,
            "    ",
            "written_text",
            written_name) ||
        !write_tag__build_xml_h(
            output,
            "    ",
            "sound_art",
            sound_name) ||
        !write_tag__build_xml_h(
            output,
            "    ",
            "visual_art",
            visual_name) ||
        fprintf(output, "  </stone>\n") < 0) {
        return 0;
    }

    return 1;
}

int shakti_build_xml_file(
    const char *list_path,
    const char *output_path,
    const char *level,
    const char *lesson,
    const char *artifact_root
)
{
    FILE *list;
    FILE *output;
    char line[SHAKTI_TEXT_CAPACITY];
    size_t stone_count;
    size_t order;
    int success;

    if (list_path == NULL ||
        output_path == NULL ||
        level == NULL ||
        lesson == NULL ||
        artifact_root == NULL) {
        return 0;
    }

    list = fopen(list_path, "r");

    if (list == NULL) {
        perror("Could not open list");
        return 0;
    }

    if (!count_stones(list, &stone_count) ||
        fseek(list, 0L, SEEK_SET) != 0) {
        fclose(list);
        fputs(
            "The list contains unsupported text, an overlong line, "
            "or too many stones.\n",
            stderr
        );
        return 0;
    }

    output = fopen(output_path, "w");

    if (output == NULL) {
        fclose(list);
        perror("Could not create XML");
        return 0;
    }

    success =
        fprintf(
            output,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<tablet schema=\"SHAKTI_TABLET_4S_V2\">\n"
        ) > 0 &&
        write_tag__build_xml_h(output, "  ", "level", level) &&
        write_tag__build_xml_h(output, "  ", "lesson", lesson) &&
        fprintf(
            output,
            "  <stone_count>%lu</stone_count>\n",
            (unsigned long)stone_count
        ) > 0;

    order = 1U;

    while (success &&
           fgets(line, sizeof(line), list) != NULL) {
        char *text;

        text = trim_line_ending(line);

        if (text[0] == '\0' || text[0] == '#') {
            continue;
        }

        if (!write_stone(
                output,
                artifact_root,
                text,
                order)) {
            fprintf(
                stderr,
                "Stone '%s' needs validated Sound_art and Visual_art "
                "files using its generated asset key.\n",
                text
            );
            success = 0;
            break;
        }

        order++;
    }

    if (success) {
        success = fprintf(output, "</tablet>\n") > 0;
    }

    if (success) {
        success = fflush(output) == 0;
    }

    if (fclose(output) != 0) {
        success = 0;
    }

    if (fclose(list) != 0) {
        success = 0;
    }

    if (!success) {
        remove(output_path);
        return 0;
    }

    printf(
        "Built %s with %lu manifest-ready stones.\n",
        output_path,
        (unsigned long)stone_count
    );

    return 1;
}

#ifndef SHAKTI_TOOL_NO_MAIN
#define BUILD_XML_MAIN main
int BUILD_XML_MAIN(int argc, char **argv)
{
    if (argc != 6) {
        fprintf(
            stderr,
            "Usage: %s LIST.txt OUTPUT.xml LEVEL LESSON ARTIFACT_ROOT\n",
            argv[0]
        );
        return EXIT_FAILURE;
    }

    if (!shakti_build_xml_file(
            argv[1],
            argv[2],
            argv[3],
            argv[4],
            argv[5])) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
#endif
