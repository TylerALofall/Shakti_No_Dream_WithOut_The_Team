#include "shakti_choice.h"

#include <inttypes.h>
#include <limits.h>
#include <string.h>

static int text_present(const char *text)
{
    return text != NULL && text[0] != '\0';
}

static int copy_checked(char *destination, size_t capacity, const char *source)
{
    size_t length;

    if (destination == NULL || capacity == 0U || !text_present(source)) {
        return 0;
    }

    length = strlen(source);
    if (length >= capacity) {
        return 0;
    }

    memcpy(destination, source, length + 1U);
    return 1;
}

static int xml_value_valid(const char *text)
{
    const unsigned char *cursor;

    if (!text_present(text)) {
        return 0;
    }

    cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        if (*cursor < 0x20U && *cursor != '\t' &&
            *cursor != '\n' && *cursor != '\r') {
            return 0;
        }
        cursor++;
    }

    return 1;
}

static int valid_evidence(shakti_choice_evidence_t evidence)
{
    return evidence >= SHAKTI_EVIDENCE_NONE &&
           evidence <= SHAKTI_EVIDENCE_VERIFIED;
}

static int authoritative_evidence(shakti_choice_evidence_t evidence)
{
    return evidence == SHAKTI_EVIDENCE_TAUGHT ||
           evidence == SHAKTI_EVIDENCE_OBSERVED ||
           evidence == SHAKTI_EVIDENCE_VERIFIED;
}

static int valid_truth(shakti_truth_t truth)
{
    return truth == SHAKTI_TRUTH_UNKNOWN ||
           truth == SHAKTI_TRUTH_FALSE ||
           truth == SHAKTI_TRUTH_TRUE;
}

static int valid_option_kind(shakti_option_kind_t kind)
{
    return kind >= SHAKTI_OPTION_ACTION &&
           kind <= SHAKTI_OPTION_ALTERNATIVE;
}

static int valid_bool(unsigned char value)
{
    return value == 0U || value == 1U;
}

static int consequence_valid(const shakti_consequence_t *consequence)
{
    if (consequence == NULL || consequence->present != 1U ||
        !valid_truth(consequence->reachable) ||
        !valid_evidence(consequence->evidence) ||
        consequence->confidence > SHAKTI_CHOICE_CONFIDENCE_MAX ||
        !valid_bool(consequence->reversible) ||
        !valid_bool(consequence->protected_boundary_breach) ||
        !text_present(consequence->evidence_ref)) {
        return 0;
    }

    if (consequence->evidence == SHAKTI_EVIDENCE_NONE ||
        consequence->evidence == SHAKTI_EVIDENCE_CREATIVE) {
        return 0;
    }

    return 1;
}

static int subtract_i64(int64_t left, int64_t right, int64_t *answer)
{
    if (answer == NULL) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) {
        return SHAKTI_CHOICE_OVERFLOW;
    }

    *answer = left - right;
    return SHAKTI_CHOICE_OK;
}

void shakti_choice_plan_init(shakti_choice_plan_t *plan)
{
    if (plan == NULL) {
        return;
    }

    memset(plan, 0, sizeof(*plan));
    plan->baseline_index = -1;
}

int shakti_choice_add_condition(
    shakti_choice_plan_t *plan,
    const char *name,
    shakti_choice_evidence_t evidence,
    const char *evidence_ref
)
{
    shakti_choice_condition_t *condition;
    size_t index;

    if (plan == NULL || plan->enumeration_closed ||
        !text_present(name) || !text_present(evidence_ref) ||
        !authoritative_evidence(evidence)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (plan->condition_count >= SHAKTI_CHOICE_MAX_CONDITIONS) {
        return SHAKTI_CHOICE_FULL;
    }

    for (index = 0U; index < plan->condition_count; ++index) {
        if (strcmp(plan->conditions[index].name, name) == 0) {
            return SHAKTI_CHOICE_REFUSED;
        }
    }

    condition = &plan->conditions[plan->condition_count];
    memset(condition, 0, sizeof(*condition));

    if (!copy_checked(condition->name, sizeof(condition->name), name) ||
        !copy_checked(
            condition->evidence_ref,
            sizeof(condition->evidence_ref),
            evidence_ref
        )) {
        memset(condition, 0, sizeof(*condition));
        return SHAKTI_CHOICE_REFUSED;
    }

    condition->evidence = evidence;
    plan->condition_count++;
    return (int)(plan->condition_count - 1U);
}

int shakti_choice_add_horizon(
    shakti_choice_plan_t *plan,
    uint64_t ticks_from_now,
    const char *name
)
{
    shakti_choice_horizon_t *horizon;
    size_t index;

    if (plan == NULL || plan->enumeration_closed || !text_present(name)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (plan->horizon_count >= SHAKTI_CHOICE_MAX_HORIZONS) {
        return SHAKTI_CHOICE_FULL;
    }

    for (index = 0U; index < plan->horizon_count; ++index) {
        if (plan->horizons[index].ticks_from_now == ticks_from_now ||
            strcmp(plan->horizons[index].name, name) == 0) {
            return SHAKTI_CHOICE_REFUSED;
        }
    }

    horizon = &plan->horizons[plan->horizon_count];
    memset(horizon, 0, sizeof(*horizon));
    horizon->ticks_from_now = ticks_from_now;

    if (!copy_checked(horizon->name, sizeof(horizon->name), name)) {
        memset(horizon, 0, sizeof(*horizon));
        return SHAKTI_CHOICE_REFUSED;
    }

    plan->horizon_count++;
    return (int)(plan->horizon_count - 1U);
}

int shakti_choice_add_option(
    shakti_choice_plan_t *plan,
    shakti_option_kind_t kind,
    const char *name,
    shakti_choice_evidence_t discovery_evidence,
    const char *discovery_ref
)
{
    shakti_choice_option_t *option;
    size_t index;

    if (plan == NULL || plan->enumeration_closed ||
        !valid_option_kind(kind) || !text_present(name) ||
        !valid_evidence(discovery_evidence) ||
        discovery_evidence == SHAKTI_EVIDENCE_NONE ||
        !text_present(discovery_ref)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (plan->option_count >= SHAKTI_CHOICE_MAX_OPTIONS) {
        return SHAKTI_CHOICE_FULL;
    }

    if (kind == SHAKTI_OPTION_NO_ACTION && plan->baseline_index >= 0) {
        return SHAKTI_CHOICE_REFUSED;
    }

    for (index = 0U; index < plan->option_count; ++index) {
        if (strcmp(plan->options[index].name, name) == 0) {
            return SHAKTI_CHOICE_REFUSED;
        }
    }

    option = &plan->options[plan->option_count];
    memset(option, 0, sizeof(*option));

    if (!copy_checked(option->name, sizeof(option->name), name) ||
        !copy_checked(
            option->discovery_ref,
            sizeof(option->discovery_ref),
            discovery_ref
        )) {
        memset(option, 0, sizeof(*option));
        return SHAKTI_CHOICE_REFUSED;
    }

    option->kind = kind;
    option->discovery_evidence = discovery_evidence;

    if (kind == SHAKTI_OPTION_NO_ACTION) {
        plan->baseline_index = (int)plan->option_count;
    }

    plan->option_count++;
    return (int)(plan->option_count - 1U);
}

int shakti_choice_add_claim(
    shakti_choice_plan_t *plan,
    const char *text,
    const char *speaker_ref
)
{
    shakti_choice_claim_t *claim;

    if (plan == NULL || plan->enumeration_closed ||
        !text_present(text) || !text_present(speaker_ref)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (plan->claim_count >= SHAKTI_CHOICE_MAX_CLAIMS) {
        return SHAKTI_CHOICE_FULL;
    }

    claim = &plan->claims[plan->claim_count];
    memset(claim, 0, sizeof(*claim));

    if (!copy_checked(claim->text, sizeof(claim->text), text) ||
        !copy_checked(
            claim->speaker_ref,
            sizeof(claim->speaker_ref),
            speaker_ref
        )) {
        memset(claim, 0, sizeof(*claim));
        return SHAKTI_CHOICE_REFUSED;
    }

    plan->claim_count++;
    return (int)(plan->claim_count - 1U);
}

int shakti_choice_set_consequence(
    shakti_choice_plan_t *plan,
    size_t option_index,
    size_t condition_index,
    size_t horizon_index,
    const shakti_consequence_t *consequence
)
{
    if (plan == NULL || plan->enumeration_closed ||
        option_index >= plan->option_count ||
        condition_index >= plan->condition_count ||
        horizon_index >= plan->horizon_count ||
        !consequence_valid(consequence)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    plan->options[option_index].consequence[condition_index][horizon_index] =
        *consequence;
    return SHAKTI_CHOICE_OK;
}

int shakti_choice_close_enumeration(
    shakti_choice_plan_t *plan,
    const char *enumeration_ref
)
{
    if (plan == NULL || plan->enumeration_closed ||
        plan->option_count < 2U || plan->condition_count == 0U ||
        plan->horizon_count == 0U || plan->baseline_index < 0 ||
        !text_present(enumeration_ref)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (!copy_checked(
            plan->enumeration_ref,
            sizeof(plan->enumeration_ref),
            enumeration_ref
        )) {
        return SHAKTI_CHOICE_REFUSED;
    }

    plan->enumeration_closed = 1U;
    return SHAKTI_CHOICE_OK;
}

int shakti_choice_filter(
    const shakti_choice_plan_t *plan,
    const shakti_choice_policy_t *policy,
    shakti_choice_result_t *result
)
{
    size_t option_index;

    if (plan == NULL || policy == NULL || result == NULL ||
        !plan->enumeration_closed || plan->baseline_index < 0 ||
        policy->catastrophic_severity == 0U ||
        policy->required_confidence == 0U ||
        policy->required_confidence > SHAKTI_CHOICE_CONFIDENCE_MAX) {
        return SHAKTI_CHOICE_REFUSED;
    }

    memset(result, 0, sizeof(*result));
    result->unverified_claim_count = (unsigned int)plan->claim_count;

    for (option_index = 0U;
         option_index < plan->option_count;
         ++option_index) {
        int filtered = 0;
        int evidence_open = 0;
        size_t condition_index;

        for (condition_index = 0U;
             condition_index < plan->condition_count;
             ++condition_index) {
            size_t horizon_index;

            for (horizon_index = 0U;
                 horizon_index < plan->horizon_count;
                 ++horizon_index) {
                const shakti_consequence_t *consequence;

                consequence = &plan->options[option_index]
                    .consequence[condition_index][horizon_index];

                if (!consequence->present ||
                    consequence->reachable == SHAKTI_TRUTH_UNKNOWN ||
                    !authoritative_evidence(consequence->evidence) ||
                    consequence->confidence < policy->required_confidence) {
                    evidence_open = 1;
                    continue;
                }

                if (consequence->reachable == SHAKTI_TRUTH_TRUE &&
                    consequence->protected_boundary_breach &&
                    consequence->severity >= policy->catastrophic_severity &&
                    !consequence->reversible) {
                    filtered = 1;
                }
            }
        }

        if (filtered) {
            result->option_status[option_index] = SHAKTI_OPTION_FILTERED;
            result->filtered_count++;
        } else if (evidence_open) {
            result->option_status[option_index] =
                SHAKTI_OPTION_EVIDENCE_OPEN;
            result->evidence_open_count++;
        } else {
            result->option_status[option_index] = SHAKTI_OPTION_SURVIVES;
            result->surviving_count++;
        }
    }

    result->baseline_survives =
        result->option_status[plan->baseline_index] == SHAKTI_OPTION_SURVIVES;
    result->baseline_filtered =
        result->option_status[plan->baseline_index] == SHAKTI_OPTION_FILTERED;

    return SHAKTI_CHOICE_OK;
}

int shakti_choice_delta(
    const shakti_choice_plan_t *plan,
    size_t option_index,
    size_t condition_index,
    size_t horizon_index,
    int64_t *delta
)
{
    const shakti_consequence_t *choice;
    const shakti_consequence_t *baseline;

    if (plan == NULL || delta == NULL || plan->baseline_index < 0 ||
        option_index >= plan->option_count ||
        condition_index >= plan->condition_count ||
        horizon_index >= plan->horizon_count) {
        return SHAKTI_CHOICE_REFUSED;
    }

    choice = &plan->options[option_index]
        .consequence[condition_index][horizon_index];
    baseline = &plan->options[plan->baseline_index]
        .consequence[condition_index][horizon_index];

    if (!choice->present || !baseline->present ||
        choice->reachable != SHAKTI_TRUTH_TRUE ||
        baseline->reachable != SHAKTI_TRUTH_TRUE ||
        !authoritative_evidence(choice->evidence) ||
        !authoritative_evidence(baseline->evidence)) {
        return SHAKTI_CHOICE_OPEN;
    }

    return subtract_i64(choice->value, baseline->value, delta);
}

int shakti_choice_weigh_range(int64_t best, int64_t worst, int reversible)
{
    int64_t spread;
    int64_t weight;
    int beats = 0;

    if ((reversible != 0 && reversible != 1) || best < worst) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (subtract_i64(best, worst, &spread) != SHAKTI_CHOICE_OK) {
        return SHAKTI_CHOICE_OVERFLOW;
    }

    if (!reversible) {
        if (spread > INT64_MAX / SHAKTI_CHOICE_IRREVERSIBLE) {
            return SHAKTI_CHOICE_OVERFLOW;
        }
        weight = spread * SHAKTI_CHOICE_IRREVERSIBLE;
    } else {
        weight = spread;
    }

    while (weight >= 2 && beats < SHAKTI_CHOICE_DELIB_MAX) {
        weight /= SHAKTI_CHOICE_LADDER;
        beats++;
    }

    return beats;
}

int shakti_choice_survivor_ladder(
    const shakti_choice_plan_t *plan,
    const shakti_choice_result_t *result,
    int64_t *best_delta,
    int64_t *worst_delta
)
{
    int found = 0;
    int all_reversible = 1;
    int64_t best = 0;
    int64_t worst = 0;
    size_t option_index;

    if (plan == NULL || result == NULL ||
        best_delta == NULL || worst_delta == NULL ||
        plan->baseline_index < 0) {
        return SHAKTI_CHOICE_REFUSED;
    }

    for (option_index = 0U;
         option_index < plan->option_count;
         ++option_index) {
        size_t condition_index;

        if (result->option_status[option_index] != SHAKTI_OPTION_SURVIVES ||
            (int)option_index == plan->baseline_index) {
            continue;
        }

        for (condition_index = 0U;
             condition_index < plan->condition_count;
             ++condition_index) {
            size_t horizon_index;

            for (horizon_index = 0U;
                 horizon_index < plan->horizon_count;
                 ++horizon_index) {
                int64_t delta;
                int code;

                code = shakti_choice_delta(
                    plan,
                    option_index,
                    condition_index,
                    horizon_index,
                    &delta
                );

                if (code == SHAKTI_CHOICE_OVERFLOW) {
                    return code;
                }
                if (code != SHAKTI_CHOICE_OK) {
                    continue;
                }

                if (!found || delta > best) {
                    best = delta;
                }
                if (!found || delta < worst) {
                    worst = delta;
                }
                found = 1;

                if (!plan->options[option_index]
                    .consequence[condition_index][horizon_index]
                    .reversible) {
                    all_reversible = 0;
                }
            }
        }
    }

    if (!found) {
        return SHAKTI_CHOICE_OPEN;
    }

    *best_delta = best;
    *worst_delta = worst;
    return shakti_choice_weigh_range(best, worst, all_reversible);
}

const char *shakti_choice_evidence_name(shakti_choice_evidence_t evidence)
{
    switch (evidence) {
        case SHAKTI_EVIDENCE_NONE: return "NONE";
        case SHAKTI_EVIDENCE_CREATIVE: return "CREATIVE";
        case SHAKTI_EVIDENCE_CLAIM: return "CLAIM";
        case SHAKTI_EVIDENCE_TAUGHT: return "TAUGHT";
        case SHAKTI_EVIDENCE_OBSERVED: return "OBSERVED";
        case SHAKTI_EVIDENCE_VERIFIED: return "VERIFIED";
        default: return "INVALID";
    }
}

const char *shakti_choice_option_kind_name(shakti_option_kind_t kind)
{
    switch (kind) {
        case SHAKTI_OPTION_ACTION: return "ACTION";
        case SHAKTI_OPTION_NO_ACTION: return "NO_ACTION";
        case SHAKTI_OPTION_WAIT: return "WAIT";
        case SHAKTI_OPTION_REFUSE: return "REFUSE";
        case SHAKTI_OPTION_TEACH_ME: return "TEACH_ME";
        case SHAKTI_OPTION_ALTERNATIVE: return "ALTERNATIVE";
        default: return "INVALID";
    }
}

const char *shakti_choice_option_status_name(shakti_option_status_t status)
{
    switch (status) {
        case SHAKTI_OPTION_UNCHECKED: return "UNCHECKED";
        case SHAKTI_OPTION_SURVIVES: return "SURVIVES";
        case SHAKTI_OPTION_FILTERED: return "FILTERED";
        case SHAKTI_OPTION_EVIDENCE_OPEN: return "EVIDENCE_OPEN";
        default: return "INVALID";
    }
}

const char *shakti_choice_sister_name(shakti_sister_t sister)
{
    switch (sister) {
        case SHAKTI_SISTER_DESTROYER: return "DESTROYER";
        case SHAKTI_SISTER_PRESERVER: return "PRESERVER";
        case SHAKTI_SISTER_CREATOR: return "CREATOR";
        default: return "INVALID";
    }
}

void shakti_geometry_init(shakti_geometry_schema_t *schema)
{
    if (schema != NULL) {
        memset(schema, 0, sizeof(*schema));
    }
}

int shakti_geometry_define_frame(
    shakti_geometry_schema_t *schema,
    unsigned int frame,
    const char *name,
    const char *question,
    const char *evidence_ref,
    int locked
)
{
    shakti_frame_definition_t definition;

    if (schema == NULL || frame >= SHAKTI_CHOICE_FRAME_COUNT ||
        schema->frame[frame].locked ||
        locked != 1 || !text_present(name) || !text_present(question) ||
        !text_present(evidence_ref)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    memset(&definition, 0, sizeof(definition));
    if (!copy_checked(definition.name, sizeof(definition.name), name) ||
        !copy_checked(
            definition.question,
            sizeof(definition.question),
            question
        ) ||
        !copy_checked(
            definition.evidence_ref,
            sizeof(definition.evidence_ref),
            evidence_ref
        )) {
        return SHAKTI_CHOICE_REFUSED;
    }

    definition.locked = 1U;
    schema->frame[frame] = definition;
    return SHAKTI_CHOICE_OK;
}

int shakti_geometry_define_pole(
    shakti_geometry_schema_t *schema,
    unsigned int pole,
    const char *name,
    const char *evidence_ref,
    int locked
)
{
    shakti_pole_definition_t definition;

    if (schema == NULL || pole >= SHAKTI_CHOICE_POLE_COUNT ||
        schema->pole[pole].locked ||
        locked != 1 || !text_present(name) || !text_present(evidence_ref)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    memset(&definition, 0, sizeof(definition));
    if (!copy_checked(definition.name, sizeof(definition.name), name) ||
        !copy_checked(
            definition.evidence_ref,
            sizeof(definition.evidence_ref),
            evidence_ref
        )) {
        return SHAKTI_CHOICE_REFUSED;
    }

    definition.locked = 1U;
    schema->pole[pole] = definition;
    return SHAKTI_CHOICE_OK;
}

int shakti_geometry_ready(const shakti_geometry_schema_t *schema)
{
    unsigned int left;

    if (schema == NULL) {
        return 0;
    }

    for (left = 0U; left < SHAKTI_CHOICE_FRAME_COUNT; ++left) {
        unsigned int right;

        if (!schema->frame[left].locked ||
            !text_present(schema->frame[left].name) ||
            !text_present(schema->frame[left].question) ||
            !text_present(schema->frame[left].evidence_ref)) {
            return 0;
        }

        for (right = left + 1U;
             right < SHAKTI_CHOICE_FRAME_COUNT;
             ++right) {
            if (schema->frame[right].locked &&
                strcmp(
                    schema->frame[left].name,
                    schema->frame[right].name
                ) == 0) {
                return 0;
            }
        }
    }

    for (left = 0U; left < SHAKTI_CHOICE_POLE_COUNT; ++left) {
        if (!schema->pole[left].locked ||
            !text_present(schema->pole[left].name) ||
            !text_present(schema->pole[left].evidence_ref)) {
            return 0;
        }
    }

    return strcmp(schema->pole[0].name, schema->pole[1].name) != 0;
}

int shakti_geometry_vertex(
    unsigned int frame_a,
    unsigned int frame_b,
    unsigned int pole,
    unsigned int *vertex
)
{
    static const unsigned int pair_vertex[5][5][2] = {
        {{99U,99U},{0U,7U},{1U,6U},{2U,5U},{3U,4U}},
        {{0U,7U},{99U,99U},{13U,14U},{17U,18U},{9U,10U}},
        {{1U,6U},{13U,14U},{99U,99U},{8U,11U},{16U,19U}},
        {{2U,5U},{17U,18U},{8U,11U},{99U,99U},{12U,15U}},
        {{3U,4U},{9U,10U},{16U,19U},{12U,15U},{99U,99U}}
    };

    if (vertex == NULL || frame_a >= SHAKTI_CHOICE_FRAME_COUNT ||
        frame_b >= SHAKTI_CHOICE_FRAME_COUNT || frame_a == frame_b ||
        pole >= SHAKTI_CHOICE_POLE_COUNT) {
        return SHAKTI_CHOICE_REFUSED;
    }

    *vertex = pair_vertex[frame_a][frame_b][pole];
    return *vertex < 20U ? SHAKTI_CHOICE_OK : SHAKTI_CHOICE_REFUSED;
}

int shakti_geometry_place_use(
    const shakti_geometry_schema_t *schema,
    shakti_use_geometry_t *use,
    const char *source_memory_ref,
    const char *use_event_ref,
    unsigned int frame_a,
    unsigned int frame_b,
    unsigned int pole
)
{
    shakti_use_geometry_t placement;
    unsigned int vertex;

    if (schema == NULL || use == NULL ||
        !shakti_geometry_ready(schema) ||
        !text_present(source_memory_ref) || !text_present(use_event_ref) ||
        shakti_geometry_vertex(frame_a, frame_b, pole, &vertex) !=
            SHAKTI_CHOICE_OK) {
        return SHAKTI_CHOICE_REFUSED;
    }

    memset(&placement, 0, sizeof(placement));
    if (!copy_checked(
            placement.source_memory_ref,
            sizeof(placement.source_memory_ref),
            source_memory_ref
        ) ||
        !copy_checked(
            placement.use_event_ref,
            sizeof(placement.use_event_ref),
            use_event_ref
        )) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (frame_a < frame_b) {
        placement.frame[0] = frame_a;
        placement.frame[1] = frame_b;
    } else {
        placement.frame[0] = frame_b;
        placement.frame[1] = frame_a;
    }
    placement.pole = pole;
    placement.vertex = vertex;
    *use = placement;
    return SHAKTI_CHOICE_OK;
}

int shakti_geometry_set_sister_reading(
    shakti_use_geometry_t *use,
    unsigned int frame_slot,
    shakti_sister_t sister,
    int64_t value,
    shakti_choice_evidence_t evidence,
    const char *evidence_ref
)
{
    shakti_sister_reading_t reading;

    if (use == NULL || frame_slot >= 2U ||
        sister < SHAKTI_SISTER_DESTROYER ||
        sister > SHAKTI_SISTER_CREATOR ||
        !authoritative_evidence(evidence) || !text_present(evidence_ref)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    memset(&reading, 0, sizeof(reading));
    if (!copy_checked(
            reading.evidence_ref,
            sizeof(reading.evidence_ref),
            evidence_ref
        )) {
        return SHAKTI_CHOICE_REFUSED;
    }

    reading.present = 1U;
    reading.value = value;
    reading.evidence = evidence;
    use->reading[frame_slot][sister] = reading;
    return SHAKTI_CHOICE_OK;
}

static int xml_text(FILE *output, const char *text)
{
    const unsigned char *cursor;

    if (output == NULL || text == NULL) {
        return 0;
    }

    cursor = (const unsigned char *)text;
    while (*cursor != '\0') {
        const char *replacement = NULL;

        if (*cursor < 0x20U && *cursor != '\t' &&
            *cursor != '\n' && *cursor != '\r') {
            return 0;
        }

        switch (*cursor) {
            case '&': replacement = "&amp;"; break;
            case '<': replacement = "&lt;"; break;
            case '>': replacement = "&gt;"; break;
            case '"': replacement = "&quot;"; break;
            case '\'': replacement = "&apos;"; break;
            default: break;
        }

        if (replacement != NULL) {
            if (fputs(replacement, output) == EOF) {
                return 0;
            }
        } else if (fputc((int)*cursor, output) == EOF) {
            return 0;
        }

        cursor++;
    }

    return 1;
}

int shakti_geometry_write_xml(
    FILE *output,
    const shakti_geometry_schema_t *schema,
    const shakti_use_geometry_t *use
)
{
    unsigned int slot;

    if (output == NULL || schema == NULL || use == NULL ||
        !shakti_geometry_ready(schema) ||
        !xml_value_valid(use->source_memory_ref) ||
        !xml_value_valid(use->use_event_ref) ||
        use->frame[0] >= SHAKTI_CHOICE_FRAME_COUNT ||
        use->frame[1] >= SHAKTI_CHOICE_FRAME_COUNT ||
        use->frame[0] == use->frame[1] ||
        use->pole >= SHAKTI_CHOICE_POLE_COUNT || use->vertex >= 20U) {
        return SHAKTI_CHOICE_REFUSED;
    }

    for (slot = 0U; slot < 2U; ++slot) {
        unsigned int sister;
        unsigned int frame = use->frame[slot];

        if (!xml_value_valid(schema->frame[frame].name)) {
            return SHAKTI_CHOICE_REFUSED;
        }
        for (sister = 0U; sister < SHAKTI_CHOICE_SISTER_COUNT; ++sister) {
            if (!use->reading[slot][sister].present ||
                !authoritative_evidence(
                    use->reading[slot][sister].evidence
                ) ||
                !xml_value_valid(use->reading[slot][sister].evidence_ref)) {
                return SHAKTI_CHOICE_OPEN;
            }
        }
    }

    if (fputs("<use_geometry schema=\"SHAKTI_USE_GEOMETRY_V1\">\n", output) == EOF ||
        fputs("  <source_memory_ref>", output) == EOF ||
        !xml_text(output, use->source_memory_ref) ||
        fputs("</source_memory_ref>\n  <use_event_ref>", output) == EOF ||
        !xml_text(output, use->use_event_ref) ||
        fputs("</use_event_ref>\n", output) == EOF ||
        fprintf(
            output,
            "  <placement vertex=\"%u\" pole=\"%u\">\n",
            use->vertex,
            use->pole
        ) < 0) {
        return SHAKTI_CHOICE_REFUSED;
    }

    for (slot = 0U; slot < 2U; ++slot) {
        unsigned int sister;
        unsigned int frame = use->frame[slot];

        if (fprintf(output, "    <frame index=\"%u\" name=\"", frame) < 0 ||
            !xml_text(output, schema->frame[frame].name) ||
            fputs("\">\n", output) == EOF) {
            return SHAKTI_CHOICE_REFUSED;
        }

        for (sister = 0U; sister < SHAKTI_CHOICE_SISTER_COUNT; ++sister) {
            const shakti_sister_reading_t *reading;

            reading = &use->reading[slot][sister];
            if (fprintf(
                    output,
                    "      <sister name=\"%s\" value=\"%" PRId64
                    "\" evidence=\"%s\"><evidence_ref>",
                    shakti_choice_sister_name((shakti_sister_t)sister),
                    reading->value,
                    shakti_choice_evidence_name(reading->evidence)
                ) < 0 ||
                !xml_text(output, reading->evidence_ref) ||
                fputs("</evidence_ref></sister>\n", output) == EOF) {
                return SHAKTI_CHOICE_REFUSED;
            }
        }

        if (fputs("    </frame>\n", output) == EOF) {
            return SHAKTI_CHOICE_REFUSED;
        }
    }

    return fputs("  </placement>\n</use_geometry>\n", output) == EOF
        ? SHAKTI_CHOICE_REFUSED
        : SHAKTI_CHOICE_OK;
}

int shakti_person_reading_write_xml(
    FILE *output,
    const shakti_person_reading_t *reading
)
{
    if (output == NULL || reading == NULL || reading->reading_id == 0U ||
        reading->visible_to_subject != 1U ||
        !valid_evidence(reading->evidence) ||
        reading->evidence == SHAKTI_EVIDENCE_NONE ||
        !xml_value_valid(reading->subject) ||
        !xml_value_valid(reading->history_ref) ||
        !xml_value_valid(reading->state_reading) ||
        !xml_value_valid(reading->obligation_ref) ||
        !xml_value_valid(reading->conclusion)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (fprintf(
            output,
            "<person_reading schema=\"SHAKTI_PERSON_READING_V1\" "
            "id=\"%" PRIu64 "\" evidence=\"%s\" visible=\"TRUE\">\n",
            reading->reading_id,
            shakti_choice_evidence_name(reading->evidence)
        ) < 0 ||
        fputs("  <subject>", output) == EOF ||
        !xml_text(output, reading->subject) ||
        fputs("</subject>\n  <history_ref>", output) == EOF ||
        !xml_text(output, reading->history_ref) ||
        fputs("</history_ref>\n  <state_reading>", output) == EOF ||
        !xml_text(output, reading->state_reading) ||
        fputs("</state_reading>\n  <obligation_ref>", output) == EOF ||
        !xml_text(output, reading->obligation_ref) ||
        fputs("</obligation_ref>\n  <conclusion>", output) == EOF ||
        !xml_text(output, reading->conclusion) ||
        fputs("</conclusion>\n</person_reading>\n", output) == EOF) {
        return SHAKTI_CHOICE_REFUSED;
    }

    return SHAKTI_CHOICE_OK;
}

int shakti_person_correction_write_xml(
    FILE *output,
    uint64_t reading_id,
    const char *subject,
    const char *correction,
    const char *evidence_ref
)
{
    if (output == NULL || reading_id == 0U || !xml_value_valid(subject) ||
        !xml_value_valid(correction) || !xml_value_valid(evidence_ref)) {
        return SHAKTI_CHOICE_REFUSED;
    }

    if (fprintf(
            output,
            "<person_correction schema=\"SHAKTI_PERSON_CORRECTION_V1\" "
            "reading_id=\"%" PRIu64 "\">\n",
            reading_id
        ) < 0 ||
        fputs("  <subject>", output) == EOF ||
        !xml_text(output, subject) ||
        fputs("</subject>\n  <correction>", output) == EOF ||
        !xml_text(output, correction) ||
        fputs("</correction>\n  <evidence_ref>", output) == EOF ||
        !xml_text(output, evidence_ref) ||
        fputs("</evidence_ref>\n</person_correction>\n", output) == EOF) {
        return SHAKTI_CHOICE_REFUSED;
    }

    return SHAKTI_CHOICE_OK;
}
