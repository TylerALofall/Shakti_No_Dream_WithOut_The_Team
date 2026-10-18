#ifndef SHAKTI_MANIFEST_H
#define SHAKTI_MANIFEST_H
#include "shakti_types.h"
void shakti_manifest_init(shakti_manifest_t *manifest);
int shakti_manifest_load(shakti_manifest_t *manifest, const char *manifest_path);
int shakti_manifest_write_ledger(const shakti_manifest_t *manifest, const char *manifest_path, const char *ledger_path);
void shakti_manifest_print(const shakti_manifest_t *manifest);
#endif
