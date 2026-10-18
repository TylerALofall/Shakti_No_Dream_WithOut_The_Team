#ifndef SHAKTI_TABLET_H
#define SHAKTI_TABLET_H
#include "shakti_types.h"
void shakti_tablet_init(shakti_tablet_t *tablet);
int shakti_tablet_parse(shakti_tablet_t *tablet, const char *xml_path);
int shakti_tablet_load(shakti_tablet_t *tablet, const char *xml_path, const char *artifact_root);
int shakti_tablet_audit(const shakti_tablet_t *tablet, const char *artifact_root, shakti_tablet_audit_t *audit);
void shakti_tablet_print(const shakti_tablet_t *tablet);
#endif
