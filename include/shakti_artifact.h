#ifndef SHAKTI_ARTIFACT_H
#define SHAKTI_ARTIFACT_H
#include <stddef.h>
int shakti_artifact_join(const char *root, const char *directory, const char *filename, char *destination, size_t destination_size);
int shakti_artifact_validate_written_text(const char *path, const char *expected_text);
int shakti_artifact_validate_svg(const char *path);
int shakti_artifact_validate_wav(const char *path);
#endif
