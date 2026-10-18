#ifndef SHAKTI_LOADER_H
#define SHAKTI_LOADER_H
#include <stddef.h>
#include "shakti_config.h"
#define SHAKTI_LOADER_DATA_CAPACITY 4096U
typedef enum {
    SHAKTI_LOADER_KIND_TEXT = 0,
    SHAKTI_LOADER_KIND_WRITTEN_TEXT = 1,
    SHAKTI_LOADER_KIND_VISUAL_ART = 2,
    SHAKTI_LOADER_KIND_SOUND_ART = 3
} shakti_loader_kind_t;
typedef struct {
    shakti_loader_kind_t kind;
    size_t size;
    char path[SHAKTI_PATH_CAPACITY];
    char data[SHAKTI_LOADER_DATA_CAPACITY];
} shakti_loader_result_t;
const char *shakti_loader_kind_name(shakti_loader_kind_t kind);
int shakti_loader_kind_from_text(const char *text, shakti_loader_kind_t *kind);
int shakti_loader_load(const char *path, shakti_loader_kind_t kind, shakti_loader_result_t *result);
#endif
