#ifndef SHAKTI_ASSET_H
#define SHAKTI_ASSET_H
#include <stddef.h>
int shakti_asset_key_from_text(const char *text, char *destination, size_t destination_size);
int shakti_asset_filename(const char *text, const char *extension, char *destination, size_t destination_size);
int shakti_asset_safe_filename(const char *filename);
#endif
