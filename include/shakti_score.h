#ifndef SHAKTI_SCORE_H
#define SHAKTI_SCORE_H
#include "shakti_types.h"
shakti_validation_score_t shakti_score_make(unsigned long passed, unsigned long required);
void shakti_score_add(shakti_validation_score_t *total, const shakti_validation_score_t *part);
int shakti_score_format(const shakti_validation_score_t *score, char *destination, size_t destination_size);
#endif
