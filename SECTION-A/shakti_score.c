#include "shakti_score.h"

#include <stdio.h>

shakti_validation_score_t shakti_score_make(
    unsigned long passed,
    unsigned long required
)
{
    shakti_validation_score_t score;

    score.passed = passed;
    score.required = required;
    score.percent = required == 0UL
        ? 0U
        : (unsigned int)((passed * 100UL) / required);
    score.complete = required > 0UL && passed == required;

    return score;
}

void shakti_score_add(
    shakti_validation_score_t *total,
    const shakti_validation_score_t *part
)
{
    if (total == NULL || part == NULL) {
        return;
    }

    *total = shakti_score_make(
        total->passed + part->passed,
        total->required + part->required
    );
}

int shakti_score_format(
    const shakti_validation_score_t *score,
    char *destination,
    size_t destination_size
)
{
    int written;

    if (score == NULL ||
        destination == NULL ||
        destination_size == 0U) {
        return 0;
    }

    written = snprintf(
        destination,
        destination_size,
        "%lu/%lu = %u%%",
        score->passed,
        score->required,
        score->percent
    );

    return written >= 0 &&
           (size_t)written < destination_size;
}
