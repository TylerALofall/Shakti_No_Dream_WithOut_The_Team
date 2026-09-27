#include "shakti_reason.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shakti_config.h"
#include "shakti_time.h"

typedef struct {
    int present;
    unsigned int score;
    char answer[SHAKTI_ANSWER_CAPACITY];
} scored_answer_t;

static char *trim_text__reason_h(char *text)
{
    char *end;

    while (*text != '\0' &&
           isspace((unsigned char)*text)) {
        text++;
    }

    end = text + strlen(text);

    while (end > text &&
           isspace((unsigned char)end[-1])) {
        end--;
    }

    *end = '\0';

    return text;
}

static int copy_checked(
    char *destination,
    size_t destination_size,
    const char *source
)
{
    size_t length;

    length = strlen(source);

    if (length >= destination_size) {
        return 0;
    }

    memcpy(destination, source, length + 1U);

    return 1;
}

static int split_fields(
    char *line,
    char **fields,
    size_t field_count
)
{
    size_t index;
    char *cursor;

    cursor = line;

    for (index = 0U; index < field_count; ++index) {
        char *separator;

        fields[index] = trim_text__reason_h(cursor);

        if (index + 1U == field_count) {
            return fields[index][0] != '\0' &&
                   strchr(fields[index], '|') == NULL;
        }

        separator = strchr(fields[index], '|');

        if (separator == NULL) {
            return 0;
        }

        *separator = '\0';
        fields[index] = trim_text__reason_h(fields[index]);

        if (fields[index][0] == '\0') {
            return 0;
        }

        cursor = separator + 1;
    }

    return 0;
}

static int allowed_symbol(unsigned char character)
{
    return character == '+' ||
           character == '-' ||
           character == '*' ||
           character == '/' ||
           character == '=' ||
           character == '<' ||
           character == '>' ||
           character == '.' ||
           character == ':';
}

static int normalize_basic(
    const char *input,
    char *output,
    size_t output_size
)
{
    size_t output_length;
    int pending_space;

    if (input == NULL || output == NULL || output_size == 0U) {
        return 0;
    }

    output_length = 0U;
    pending_space = 0;

    while (*input != '\0') {
        unsigned char character;

        character = (unsigned char)*input;

        if (isalnum(character) || allowed_symbol(character)) {
            if (pending_space && output_length > 0U) {
                if (output_length + 1U >= output_size) {
                    return 0;
                }

                output[output_length++] = ' ';
            }

            if (output_length + 1U >= output_size) {
                return 0;
            }

            output[output_length++] = (char)tolower(character);
            pending_space = 0;
        } else {
            pending_space = 1;
        }

        input++;
    }

    output[output_length] = '\0';

    return output_length > 0U;
}

static const char *find_canonical_token(
    const shakti_reason_state_t *state,
    const char *token
)
{
    size_t index;

    for (index = 0U; index < state->synonym_count; ++index) {
        if (strcmp(state->synonyms[index].variant, token) == 0) {
            return state->synonyms[index].canonical;
        }
    }

    return token;
}

static int creative_rewrite_question(
    const shakti_reason_state_t *state,
    const char *basic_question,
    char *rewritten,
    size_t rewritten_size
)
{
    char copy[SHAKTI_QUESTION_CAPACITY];
    char *cursor;
    size_t output_length;
    int changed;

    if (!copy_checked(copy, sizeof(copy), basic_question)) {
        return 0;
    }

    output_length = 0U;
    changed = 0;
    cursor = strtok(copy, " ");

    while (cursor != NULL) {
        const char *canonical;
        size_t token_length;

        canonical = find_canonical_token(state, cursor);
        token_length = strlen(canonical);

        if (strcmp(canonical, cursor) != 0) {
            changed = 1;
        }

        if (output_length > 0U) {
            if (output_length + 1U >= rewritten_size) {
                return 0;
            }

            rewritten[output_length++] = ' ';
        }

        if (output_length + token_length >= rewritten_size) {
            return 0;
        }

        memcpy(rewritten + output_length, canonical, token_length);
        output_length += token_length;
        cursor = strtok(NULL, " ");
    }

    rewritten[output_length] = '\0';

    return changed;
}

static unsigned int evidence_score(const shakti_evidence_t *entry)
{
    unsigned int score;
    unsigned int confirmations;
    unsigned int penalty;

    confirmations = entry->confirmations;

    if (confirmations > 20U) {
        confirmations = 20U;
    }

    switch ((shakti_source_t)entry->source) {
        case SHAKTI_SOURCE_VERIFIED:
            score = 850U + confirmations * 25U;
            break;

        case SHAKTI_SOURCE_CURRICULUM:
            score = 700U + confirmations * 50U;
            break;

        case SHAKTI_SOURCE_OBSERVED:
            score = 450U + confirmations * 75U;
            break;

        default:
            score = 0U;
            break;
    }

    if (score > 1000U) {
        score = 1000U;
    }

    penalty = entry->contradictions * 250U;

    if (penalty >= score) {
        return 0U;
    }

    return score - penalty;
}

static int same_answer(
    const char *left,
    const char *right
)
{
    char left_key[SHAKTI_ANSWER_CAPACITY];
    char right_key[SHAKTI_ANSWER_CAPACITY];

    if (!normalize_basic(left, left_key, sizeof(left_key)) ||
        !normalize_basic(right, right_key, sizeof(right_key))) {
        return 0;
    }

    return strcmp(left_key, right_key) == 0;
}

static void consider_answer(
    scored_answer_t *best,
    scored_answer_t *second,
    const char *answer,
    unsigned int score
)
{
    if (best->present && same_answer(best->answer, answer)) {
        if (score > best->score) {
            best->score = score;
        }

        return;
    }

    if (second->present && same_answer(second->answer, answer)) {
        if (score > second->score) {
            second->score = score;
        }

        if (second->score > best->score) {
            scored_answer_t temporary;

            temporary = *best;
            *best = *second;
            *second = temporary;
        }

        return;
    }

    if (!best->present || score > best->score) {
        *second = *best;
        best->present = 1;
        best->score = score;
        copy_checked(best->answer, sizeof(best->answer), answer);
    } else if (!second->present || score > second->score) {
        second->present = 1;
        second->score = score;
        copy_checked(second->answer, sizeof(second->answer), answer);
    }
}

static shakti_decision_t evaluate_exact_question(
    const shakti_reason_state_t *state,
    const char *question
)
{
    shakti_decision_t decision;
    scored_answer_t best;
    scored_answer_t second;
    size_t index;

    memset(&decision, 0, sizeof(decision));
    memset(&best, 0, sizeof(best));
    memset(&second, 0, sizeof(second));

    for (index = 0U; index < state->fact_count; ++index) {
        if (strcmp(state->facts[index].question, question) == 0) {
            consider_answer(
                &best,
                &second,
                state->facts[index].answer,
                1000U
            );
        }
    }

    for (index = 0U; index < state->evidence_count; ++index) {
        const shakti_evidence_t *entry;

        entry = &state->evidence[index];

        if (!entry->in_use ||
            strcmp(entry->question, question) != 0) {
            continue;
        }

        consider_answer(
            &best,
            &second,
            entry->answer,
            evidence_score(entry)
        );
    }

    if (!best.present) {
        decision.kind = SHAKTI_DECISION_UNKNOWN;
        copy_checked(
            decision.reason,
            sizeof(decision.reason),
            "No exact grounded answer exists."
        );
        return decision;
    }

    decision.score = best.score;
    decision.second_score = second.present ? second.score : 0U;
    decision.margin = second.present
        ? best.score - second.score
        : 1000U;

    copy_checked(
        decision.answer,
        sizeof(decision.answer),
        best.answer
    );

    if (best.score >= SHAKTI_ANSWER_SCORE &&
        decision.margin >= SHAKTI_REQUIRED_MARGIN) {
        decision.kind = SHAKTI_DECISION_KNOWN;
        copy_checked(
            decision.reason,
            sizeof(decision.reason),
            "Logic found strong exact grounded evidence."
        );
    } else if (best.score >= SHAKTI_CAUTIOUS_SCORE &&
               decision.margin >= SHAKTI_REQUIRED_MARGIN) {
        decision.kind = SHAKTI_DECISION_CAUTIOUS;
        copy_checked(
            decision.reason,
            sizeof(decision.reason),
            "Logic found usable evidence that has not fully converged."
        );
    } else if (second.present &&
               decision.margin < SHAKTI_REQUIRED_MARGIN) {
        decision.kind = SHAKTI_DECISION_UNKNOWN;
        copy_checked(
            decision.reason,
            sizeof(decision.reason),
            "Stored answers conflict, so logic stopped."
        );
    } else {
        decision.kind = SHAKTI_DECISION_UNKNOWN;
        copy_checked(
            decision.reason,
            sizeof(decision.reason),
            "Evidence is below the answer gate."
        );
    }

    return decision;
}

static unsigned int count_shared_tokens(
    const char *left,
    const char *right
)
{
    char left_copy[SHAKTI_QUESTION_CAPACITY];
    char right_copy[SHAKTI_QUESTION_CAPACITY];
    char left_tokens[SHAKTI_MAX_TOKENS][SHAKTI_TOKEN_CAPACITY];
    size_t left_count;
    char *token;
    unsigned int shared;
    size_t index;

    if (!copy_checked(left_copy, sizeof(left_copy), left) ||
        !copy_checked(right_copy, sizeof(right_copy), right)) {
        return 0U;
    }

    left_count = 0U;
    token = strtok(left_copy, " ");

    while (token != NULL && left_count < SHAKTI_MAX_TOKENS) {
        if (!copy_checked(
                left_tokens[left_count],
                sizeof(left_tokens[left_count]),
                token)) {
            return 0U;
        }

        left_count++;
        token = strtok(NULL, " ");
    }

    shared = 0U;
    token = strtok(right_copy, " ");

    while (token != NULL) {
        for (index = 0U; index < left_count; ++index) {
            if (strcmp(token, left_tokens[index]) == 0) {
                shared++;
                break;
            }
        }

        token = strtok(NULL, " ");
    }

    return shared;
}

static unsigned int count_creative_candidates(
    const shakti_reason_state_t *state,
    const char *question
)
{
    unsigned int count;
    size_t index;

    count = 0U;

    for (index = 0U; index < state->fact_count; ++index) {
        if (count_shared_tokens(
                question,
                state->facts[index].question) > 0U) {
            count++;

            if (count >= SHAKTI_MAX_CREATIVE_CANDIDATES) {
                break;
            }
        }
    }

    return count;
}

static shakti_evidence_t *find_evidence(
    shakti_reason_state_t *state,
    const char *question,
    const char *answer
)
{
    size_t index;

    for (index = 0U; index < state->evidence_count; ++index) {
        shakti_evidence_t *entry;

        entry = &state->evidence[index];

        if (entry->in_use &&
            strcmp(entry->question, question) == 0 &&
            same_answer(entry->answer, answer)) {
            return entry;
        }
    }

    return NULL;
}

static int apply_evidence_event(
    shakti_reason_state_t *state,
    const shakti_tick_t *tick,
    const char *question,
    const char *answer,
    shakti_source_t source,
    int contradiction
)
{
    char question_key[SHAKTI_QUESTION_CAPACITY];
    char answer_key[SHAKTI_ANSWER_CAPACITY];
    shakti_evidence_t *entry;
    shakti_decision_t decision;

    if (!normalize_basic(
            question,
            question_key,
            sizeof(question_key)) ||
        !normalize_basic(
            answer,
            answer_key,
            sizeof(answer_key))) {
        return 0;
    }

    entry = find_evidence(state, question_key, answer_key);

    if (entry == NULL) {
        if (state->evidence_count >= SHAKTI_MAX_EVIDENCE) {
            return 0;
        }

        entry = &state->evidence[state->evidence_count];
        memset(entry, 0, sizeof(*entry));
        entry->in_use = 1U;
        entry->source = (unsigned char)source;
        entry->first_seen_epoch = tick->epoch_seconds;
        entry->first_seen_frame = tick->frame;

        if (!copy_checked(
                entry->question,
                sizeof(entry->question),
                question_key) ||
            !copy_checked(
                entry->answer,
                sizeof(entry->answer),
                answer)) {
            memset(entry, 0, sizeof(*entry));
            return 0;
        }

        state->evidence_count++;
    } else if ((unsigned char)source > entry->source) {
        entry->source = (unsigned char)source;
    }

    if (contradiction) {
        if (entry->contradictions < 1000000U) {
            entry->contradictions++;
        }
    } else if (entry->confirmations < 1000000U) {
        entry->confirmations++;
    }

    state->teaching_steps++;
    decision = evaluate_exact_question(state, question_key);

    entry->last_score = evidence_score(entry);
    entry->currently_stable =
        decision.kind == SHAKTI_DECISION_KNOWN &&
        same_answer(decision.answer, entry->answer);

    if (entry->currently_stable && !entry->converged_once) {
        entry->converged_once = 1U;
        entry->convergence_step = state->teaching_steps;
        entry->converged_epoch = tick->epoch_seconds;
        entry->converged_frame = tick->frame;
    }

    return 1;
}

static int append_evidence_event(
    const char *path,
    const shakti_tick_t *tick,
    const char *question,
    const char *answer,
    shakti_source_t source,
    int contradiction
)
{
    FILE *file;
    char tick_text[SHAKTI_TICK_CAPACITY];
    char question_key[SHAKTI_QUESTION_CAPACITY];
    char answer_key[SHAKTI_ANSWER_CAPACITY];

    if (strchr(question, '|') != NULL ||
        strchr(answer, '|') != NULL ||
        !shakti_tick_format(tick, tick_text, sizeof(tick_text)) ||
        !normalize_basic(
            question,
            question_key,
            sizeof(question_key)) ||
        !normalize_basic(
            answer,
            answer_key,
            sizeof(answer_key))) {
        return 0;
    }

    file = fopen(path, "a");

    if (file == NULL) {
        return 0;
    }

    {
        int success;

        success = fprintf(
            file,
            "EV1|%s|%s|%s|%s|%s\n",
            tick_text,
            contradiction ? "REJECT" : "ADD",
            shakti_source_name(source),
            question_key,
            answer
        ) >= 0;

        if (success) {
            success = fflush(file) == 0;
        }

        if (fclose(file) != 0) {
            success = 0;
        }

        return success;
    }
}

static int parse_source_name(
    const char *text,
    shakti_source_t *source
)
{
    if (strcmp(text, "observed") == 0) {
        *source = SHAKTI_SOURCE_OBSERVED;
        return 1;
    }

    if (strcmp(text, "curriculum") == 0) {
        *source = SHAKTI_SOURCE_CURRICULUM;
        return 1;
    }

    if (strcmp(text, "verified") == 0) {
        *source = SHAKTI_SOURCE_VERIFIED;
        return 1;
    }

    return 0;
}

static int parse_tick_text(
    const char *text,
    shakti_tick_t *tick
)
{
    long epoch;
    unsigned int frame;
    char tail;

    if (sscanf(text, "%ld:%u%c", &epoch, &frame, &tail) != 2 ||
        frame >= SHAKTI_FRAME_LIMIT) {
        return 0;
    }

    tick->epoch_seconds = (time_t)epoch;
    tick->frame = frame;

    return 1;
}

static int load_facts(
    shakti_reason_state_t *state,
    const char *path
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[4];
        char normalized_question[SHAKTI_QUESTION_CAPACITY];
        char *trimmed;

        trimmed = trim_text__reason_h(line);

        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }

        if (!split_fields(trimmed, fields, 4U) ||
            !normalize_basic(
                fields[0],
                normalized_question,
                sizeof(normalized_question))) {
            fclose(file);
            return 0;
        }

        if (state->fact_count >= SHAKTI_MAX_FACTS) {
            fclose(file);
            return 0;
        }

        if (!copy_checked(
                state->facts[state->fact_count].question,
                sizeof(state->facts[state->fact_count].question),
                normalized_question) ||
            !copy_checked(
                state->facts[state->fact_count].answer,
                sizeof(state->facts[state->fact_count].answer),
                fields[1]) ||
            !copy_checked(
                state->facts[state->fact_count].relation,
                sizeof(state->facts[state->fact_count].relation),
                fields[2]) ||
            !copy_checked(
                state->facts[state->fact_count].source_id,
                sizeof(state->facts[state->fact_count].source_id),
                fields[3])) {
            fclose(file);
            return 0;
        }

        state->fact_count++;
    }

    fclose(file);

    return 1;
}

static int load_synonyms(
    shakti_reason_state_t *state,
    const char *path
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[2];
        char variant[SHAKTI_TOKEN_CAPACITY];
        char canonical[SHAKTI_TOKEN_CAPACITY];
        char *trimmed;

        trimmed = trim_text__reason_h(line);

        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }

        if (!split_fields(trimmed, fields, 2U) ||
            !normalize_basic(fields[0], variant, sizeof(variant)) ||
            !normalize_basic(fields[1], canonical, sizeof(canonical)) ||
            strchr(variant, ' ') != NULL ||
            strchr(canonical, ' ') != NULL) {
            fclose(file);
            return 0;
        }

        if (state->synonym_count >= SHAKTI_MAX_SYNONYMS) {
            fclose(file);
            return 0;
        }

        copy_checked(
            state->synonyms[state->synonym_count].variant,
            sizeof(state->synonyms[state->synonym_count].variant),
            variant
        );

        copy_checked(
            state->synonyms[state->synonym_count].canonical,
            sizeof(state->synonyms[state->synonym_count].canonical),
            canonical
        );

        state->synonym_count++;
    }

    fclose(file);

    return 1;
}

static int load_evidence(
    shakti_reason_state_t *state,
    const char *path
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[6];
        char *trimmed;
        shakti_tick_t tick;
        shakti_source_t source;

        trimmed = trim_text__reason_h(line);

        if (trimmed[0] == '\0' || trimmed[0] == '#') {
            continue;
        }

        if (!split_fields(trimmed, fields, 6U) ||
            strcmp(fields[0], "EV1") != 0 ||
            !parse_tick_text(fields[1], &tick) ||
            !parse_source_name(fields[3], &source) ||
            (strcmp(fields[2], "ADD") != 0 &&
             strcmp(fields[2], "REJECT") != 0) ||
            !apply_evidence_event(
                state,
                &tick,
                fields[4],
                fields[5],
                source,
                strcmp(fields[2], "REJECT") == 0)) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);

    return 1;
}

void shakti_reason_init(shakti_reason_state_t *state)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
}

int shakti_reason_load(
    shakti_reason_state_t *state,
    const char *facts_path,
    const char *thesaurus_path,
    const char *evidence_path
)
{
    if (state == NULL) {
        return 0;
    }

    shakti_reason_init(state);

    if (!load_facts(state, facts_path) ||
        !load_synonyms(state, thesaurus_path) ||
        !load_evidence(state, evidence_path)) {
        shakti_reason_init(state);
        return 0;
    }

    return 1;
}

int shakti_reason_record_evidence(
    shakti_reason_state_t *state,
    const char *evidence_path,
    const shakti_tick_t *tick,
    const char *question,
    const char *answer,
    shakti_source_t source,
    int contradiction
)
{
    if (state == NULL ||
        evidence_path == NULL ||
        tick == NULL ||
        question == NULL ||
        answer == NULL ||
        source == SHAKTI_SOURCE_CREATIVE ||
        source == SHAKTI_SOURCE_EDEN) {
        return 0;
    }

    if (!append_evidence_event(
            evidence_path,
            tick,
            question,
            answer,
            source,
            contradiction)) {
        return 0;
    }

    return apply_evidence_event(
        state,
        tick,
        question,
        answer,
        source,
        contradiction
    );
}

shakti_decision_t shakti_reason_answer(
    const shakti_reason_state_t *state,
    const char *question
)
{
    char basic_question[SHAKTI_QUESTION_CAPACITY];
    char rewritten[SHAKTI_QUESTION_CAPACITY];
    shakti_decision_t decision;

    memset(&decision, 0, sizeof(decision));

    if (state == NULL ||
        !normalize_basic(
            question,
            basic_question,
            sizeof(basic_question))) {
        decision.kind = SHAKTI_DECISION_UNKNOWN;
        copy_checked(
            decision.reason,
            sizeof(decision.reason),
            "The question could not be normalized."
        );
        return decision;
    }

    decision = evaluate_exact_question(state, basic_question);

    if (decision.kind != SHAKTI_DECISION_UNKNOWN) {
        return decision;
    }

    if (creative_rewrite_question(
            state,
            basic_question,
            rewritten,
            sizeof(rewritten))) {
        shakti_decision_t rewritten_decision;

        rewritten_decision =
            evaluate_exact_question(state, rewritten);
        rewritten_decision.used_creative_rewrite = 1;

        if (rewritten_decision.kind != SHAKTI_DECISION_UNKNOWN) {
            copy_checked(
                rewritten_decision.reason,
                sizeof(rewritten_decision.reason),
                "Creative rewrote approved words; logic cross-validated "
                "the exact grounded answer."
            );
            return rewritten_decision;
        }
    } else {
        copy_checked(rewritten, sizeof(rewritten), basic_question);
    }

    decision.creative_candidates_examined =
        count_creative_candidates(state, rewritten);

    if (decision.creative_candidates_examined > 0U) {
        copy_checked(
            decision.reason,
            sizeof(decision.reason),
            "Creative found related candidates, but logic could not "
            "cross-validate any answer."
        );
    }

    return decision;
}

const char *shakti_source_name(shakti_source_t source)
{
    switch (source) {
        case SHAKTI_SOURCE_OBSERVED:
            return "observed";

        case SHAKTI_SOURCE_CURRICULUM:
            return "curriculum";

        case SHAKTI_SOURCE_VERIFIED:
            return "verified";

        case SHAKTI_SOURCE_EDEN:
            return "eden";

        case SHAKTI_SOURCE_CREATIVE:
            return "creative";

        default:
            return "unknown";
    }
}
