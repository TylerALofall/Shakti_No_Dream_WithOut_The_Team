#include "shakti_loader.h"

#include <stdio.h>
#include <string.h>

#include "shakti_asset.h"

const char *shakti_loader_kind_name(shakti_loader_kind_t kind)
{
    switch (kind) {
        case SHAKTI_LOADER_KIND_TEXT:
            return "text";
        case SHAKTI_LOADER_KIND_WRITTEN_TEXT:
            return "written_text";
        case SHAKTI_LOADER_KIND_VISUAL_ART:
            return "visual_art";
        case SHAKTI_LOADER_KIND_SOUND_ART:
            return "sound_art";
    }

    return "text";
}

int shakti_loader_kind_from_text(
    const char *text,
    shakti_loader_kind_t *kind
)
{
    if (text == NULL || kind == NULL) {
        return 0;
    }

    if (strcmp(text, "text") == 0) {
        *kind = SHAKTI_LOADER_KIND_TEXT;
        return 1;
    }

    if (strcmp(text, "written_text") == 0) {
        *kind = SHAKTI_LOADER_KIND_WRITTEN_TEXT;
        return 1;
    }

    if (strcmp(text, "visual_art") == 0) {
        *kind = SHAKTI_LOADER_KIND_VISUAL_ART;
        return 1;
    }

    if (strcmp(text, "sound_art") == 0) {
        *kind = SHAKTI_LOADER_KIND_SOUND_ART;
        return 1;
    }

    return 0;
}

static int safe_path(const char *path)
{
    const char *base;

    if (strstr(path, "..") != NULL) {
        return 0;
    }

    base = strrchr(path, '/');
    base = base == NULL ? path : base + 1;

    return shakti_asset_safe_filename(base);
}

int shakti_loader_load(
    const char *path,
    shakti_loader_kind_t kind,
    shakti_loader_result_t *result
)
{
    FILE *file;
    size_t size;
    int written;

    if (path == NULL ||
        path[0] == '\0' ||
        result == NULL ||
        !safe_path(path)) {
        return 0;
    }

    memset(result, 0, sizeof(*result));
    result->kind = kind;

    file = fopen(path, "rb");

    if (file == NULL) {
        return 0;
    }

    size = fread(result->data, 1U, SHAKTI_LOADER_DATA_CAPACITY, file);

    if (ferror(file) != 0) {
        fclose(file);
        return 0;
    }

    if (fclose(file) != 0) {
        return 0;
    }

    if (kind != SHAKTI_LOADER_KIND_TEXT &&
        kind != SHAKTI_LOADER_KIND_WRITTEN_TEXT &&
        kind != SHAKTI_LOADER_KIND_VISUAL_ART &&
        kind != SHAKTI_LOADER_KIND_SOUND_ART) {
        return 0;
    }

    result->size = size;

    written = snprintf(result->path, sizeof(result->path), "%s", path);

    if (written < 0 || (size_t)written >= sizeof(result->path)) {
        return 0;
    }

    return 1;
}
