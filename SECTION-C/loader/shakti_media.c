#include "shakti_media.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int overlap(const void *a, size_t na, const void *b, size_t nb);

static int overlap(const void *a, size_t na, const void *b, size_t nb)
{
    uintptr_t aa = (uintptr_t)a, bb = (uintptr_t)b;
    if (na > UINTPTR_MAX - aa || nb > UINTPTR_MAX - bb) return 1;
    return aa < bb + nb && bb < aa + na;
}

static int frame_conflicts(const shakti_media_frame_t *a, const shakti_media_frame_t *b)
{
    const void *ap[4] = { a->input, a->reference, a->binary, a->rebuilt };
    const void *bp[4] = { b->input, b->reference, b->binary, b->rebuilt };
    size_t an[4] = { a->bytes, a->bytes, shakti_binary_capacity(a->bytes), a->bytes };
    size_t bn[4] = { b->bytes, b->bytes, shakti_binary_capacity(b->bytes), b->bytes };
    size_t i, j;
    for (i = 0U; i < 4U; ++i) for (j = 0U; j < 4U; ++j)
        if ((i || j) && overlap(ap[i], an[i], bp[j], bn[j])) return 1;
    return 0;
}

static int reject(shakti_media_state_t *state, const char *reason)
{
    ++state->rejected;
    fprintf(stderr, "BENCH media: %s\n", reason);
    return 0;
}

static int frame_valid(const shakti_media_frame_t *frame)
{
    uint64_t expected, count;
    size_t i, size;
    const void *buffers[4];
    size_t sizes[4];
    if (frame == NULL || !memchr(frame->source_id, 0, sizeof(frame->source_id)) ||
        !frame->source_id[0] || !memchr(frame->context, 0, sizeof(frame->context)) ||
        frame->lane > 2U || frame->retention_policy > SHAKTI_MEDIA_MOMENT ||
        frame->format > SHAKTI_FORMAT_PCM_S16LE || frame->sample_bytes == 0U || frame->sample_bytes > 8U ||
        (frame->required_mask != SHAKTI_MEDIA_SIGHT && frame->required_mask != SHAKTI_MEDIA_AUDIO_PAIR &&
         frame->required_mask != SHAKTI_MEDIA_MOVIE) || !(frame->required_mask & (1U << frame->lane))) return 0;
    for (i = 0U; frame->source_id[i]; ++i)
        if (!isalnum((unsigned char)frame->source_id[i]) && frame->source_id[i] != '_' &&
            frame->source_id[i] != '-' && frame->source_id[i] != '.') return 0;
    if (frame->lane == 0U) {
        if ((frame->position64 & 1U) || !frame->width || !frame->height ||
            (frame->channels != 3U && frame->channels != 4U)) return 0;
        count = (uint64_t)frame->width * frame->height;
        if (count > UINT64_MAX / frame->channels / frame->sample_bytes) return 0;
        expected = count * frame->channels * frame->sample_bytes;
    } else {
        uint64_t part = frame->position64 % 64U;
        if ((frame->lane == 1U && (frame->position64 & 1U)) ||
            (frame->lane == 2U && !(frame->position64 & 1U)) ||
            !frame->sample_rate || frame->sample_rate > 384000U ||
            !frame->channels || frame->channels > 8U || frame->width || frame->height) return 0;
        count = ((uint64_t)frame->sample_rate * (part + 1U)) / 64U -
            ((uint64_t)frame->sample_rate * part) / 64U;
        expected = count * frame->channels * frame->sample_bytes;
    }
    if (expected == 0U || expected > SIZE_MAX || frame->bytes != (size_t)expected) return 0;
    if (frame->format && (frame->lane ?
        (frame->format != SHAKTI_FORMAT_PCM_S16LE || frame->sample_bytes != 2U) :
        (frame->sample_bytes != 1U || frame->format !=
            (frame->channels == 3U ? SHAKTI_FORMAT_RGB8 : SHAKTI_FORMAT_RGBA8)))) return 0;
    size = shakti_binary_capacity(frame->bytes);
    if (!size || !frame->input || !frame->reference || !frame->binary || !frame->rebuilt ||
        frame->reference_capacity < frame->bytes || frame->binary_capacity < size ||
        frame->rebuilt_capacity < frame->bytes) return 0;
    buffers[0] = frame->input; buffers[1] = frame->reference;
    buffers[2] = frame->binary; buffers[3] = frame->rebuilt;
    sizes[0] = sizes[1] = sizes[3] = frame->bytes; sizes[2] = size;
    for (i = 0U; i < 4U; ++i) {
        size_t j;
        for (j = i + 1U; j < 4U; ++j)
            if (overlap(buffers[i], sizes[i], buffers[j], sizes[j])) return 0;
    }
    return 1;
}

void shakti_media_init(shakti_media_state_t *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->active = SHAKTI_MEDIA_GROUPS;
}

int shakti_media_queue(shakti_media_state_t *state, const shakti_media_frame_t *frame)
{
    size_t i, lane, target = SHAKTI_MEDIA_GROUPS, available = SHAKTI_MEDIA_GROUPS;
    uint64_t window;
    if (state == NULL) return 0;
    if (!frame_valid(frame)) return reject(state, "invalid dimensions, position, byte count, or buffer ownership");
    window = frame->position64 / 2U;
    for (i = 0U; i < SHAKTI_MEDIA_GROUPS; ++i) {
        shakti_media_group_t *group = &state->pending[i];
        if (!group->in_use) { if (available == SHAKTI_MEDIA_GROUPS) available = i; continue; }
        for (lane = 0U; lane < 3U; ++lane) {
            if (!(group->queued_mask & (1U << lane))) continue;
            if (frame_conflicts(frame, &group->parts[lane])) return reject(state, "queued buffers overlap an owned frame");
            if (group->parts[lane].position64 / 2U == window &&
                strcmp(group->parts[lane].source_id, frame->source_id) == 0) target = i;
        }
    }
    if (state->current_ready) {
        for (lane = 0U; lane < 3U; ++lane)
            if ((state->current.checked_mask & (1U << lane)) &&
                frame_conflicts(frame, &state->current.parts[lane])) return reject(state, "current view must be released before its buffers are reused");
    }
    if (target == SHAKTI_MEDIA_GROUPS) target = available;
    if (target == SHAKTI_MEDIA_GROUPS) return reject(state, "loading zone full; item not taken");
    {
        shakti_media_group_t *group = &state->pending[target];
        if (!group->in_use) {
            if (state->next_order == UINT64_MAX) return reject(state, "loading sequence exhausted");
            memset(group, 0, sizeof(*group));
            group->order = ++state->next_order;
            group->required_mask = frame->required_mask;
            group->retention_policy = frame->retention_policy;
            group->in_use = 1U;
        }
        if (group->required_mask != frame->required_mask || group->retention_policy != frame->retention_policy || group->taken || group->held ||
            (group->queued_mask & (1U << frame->lane))) return reject(state, "duplicate or incompatible convergence lane");
        for (lane = 0U; lane < 3U; ++lane) if (group->queued_mask & (1U << lane)) {
            const shakti_media_frame_t *other = &group->parts[lane];
            if (other->context[0] && frame->context[0] && strcmp(other->context, frame->context) != 0)
                return reject(state, "context differs inside one source window");
            if (lane && frame->lane && (other->sample_rate != frame->sample_rate ||
                other->channels != frame->channels || other->sample_bytes != frame->sample_bytes))
                return reject(state, "audio format differs inside one source window");
        }
        group->parts[frame->lane] = *frame;
        group->queued_mask |= 1U << frame->lane;
    }
    ++state->submitted;
    return 1;
}

static int decompose(shakti_media_state_t *state, shakti_media_group_t *group)
{
    size_t lane, byte;
    for (lane = 0U; lane < 3U; ++lane) if (group->required_mask & (1U << lane)) {
        shakti_media_frame_t *frame = &group->parts[lane];
        memcpy(frame->reference, frame->input, frame->bytes);
        if (!shakti_binary_encode(frame->input, frame->bytes, frame->binary, frame->binary_capacity)) {
            group->held = 1U;
            (void)reject(state, "binary breakdown failed; item held");
            return 0;
        }
        for (byte = 0U; byte < frame->bytes; ++byte)
            frame->rebuilt[byte] = (unsigned char)~frame->reference[byte];
    }
    group->taken = 1U;
    return 1;
}

static int validate_lane(shakti_media_state_t *state, shakti_media_group_t *group, size_t lane)
{
    shakti_media_frame_t *frame = &group->parts[lane];
    uint64_t hash = shakti_media_hash(frame->reference, frame->bytes);
    if (!shakti_binary_rebuild(frame->binary, frame->bytes, frame->rebuilt, frame->rebuilt_capacity) ||
        memcmp(frame->reference, frame->rebuilt, frame->bytes) != 0 ||
        (frame->expected_hash && frame->expected_hash != hash)) {
        group->held = 1U;
        (void)reject(state, "reconstruction mismatch; whole source window held");
        return 0;
    }
    frame->validated_hash = hash;
    frame->prepared_binary_hash = shakti_media_hash(frame->binary, shakti_binary_capacity(frame->bytes));
    group->checked_mask |= 1U << lane;
    ++state->validated_parts;
    return 1;
}

int shakti_media_prepare(shakti_media_state_t *state)
{
    size_t i, lane;
    int count = 0;
    if (!state) return -1;
    for (i = 0U; i < SHAKTI_MEDIA_GROUPS; ++i) {
        shakti_media_group_t *group = &state->pending[i];
        if (!group->in_use || group->held || group->taken ||
            group->queued_mask != group->required_mask) continue;
        if (!decompose(state, group)) return -1;
        for (lane = 0U; lane < 3U; ++lane)
            if ((group->required_mask & (1U << lane)) && !validate_lane(state, group, lane)) return -1;
        group->prepared = 1U;
        ++count;
    }
    return count;
}

int shakti_media_take(shakti_media_state_t *state)
{
    size_t i, chosen = SHAKTI_MEDIA_GROUPS;
    uint64_t oldest = UINT64_MAX;
    if (!state || state->current_ready || state->active != SHAKTI_MEDIA_GROUPS) return 0;
    for (i = 0U; i < SHAKTI_MEDIA_GROUPS; ++i)
        if (state->pending[i].in_use && !state->pending[i].held && state->pending[i].order < oldest) {
            oldest = state->pending[i].order;
            chosen = i;
        }
    if (chosen == SHAKTI_MEDIA_GROUPS ||
        state->pending[chosen].queued_mask != state->pending[chosen].required_mask) return 0;
    if (!state->pending[chosen].taken && !decompose(state, &state->pending[chosen])) return -1;
    state->active = (unsigned int)chosen;
    return 1;
}

int shakti_media_step(shakti_media_state_t *state, unsigned int heartbeat_slot)
{
    shakti_media_group_t *group;
    size_t lane;
    if (state == NULL || heartbeat_slot >= 64U) return -1;
    if (state->current_ready) return 1;
    if (state->active == SHAKTI_MEDIA_GROUPS) {
        if (heartbeat_slot & 1U) return 1;
        int taken = shakti_media_take(state);
        if (taken < 0) return -1;
        if (taken == 0) return 1;
    }
    group = &state->pending[state->active];
    if (group->prepared) {
        if (!(heartbeat_slot & 1U)) group->gate_seen = 1U;
        if (!group->gate_seen || ((group->required_mask == SHAKTI_MEDIA_SIGHT) ?
            (heartbeat_slot & 1U) : !(heartbeat_slot & 1U))) return 1;
        for (lane = 0U; lane < 3U; ++lane) if (group->required_mask & (1U << lane)) {
            const shakti_media_frame_t *frame = &group->parts[lane];
            if (shakti_media_hash(frame->reference, frame->bytes) != frame->validated_hash ||
                shakti_media_hash(frame->binary, shakti_binary_capacity(frame->bytes)) != frame->prepared_binary_hash ||
                memcmp(frame->reference, frame->rebuilt, frame->bytes)) {
                group->held = 1U; state->active = SHAKTI_MEDIA_GROUPS;
                (void)reject(state, "prepared window changed before heartbeat publication");
                return -1;
            }
        }
    }
    for (lane = 0U; lane < 3U; ++lane) {
        if (!(group->required_mask & (1U << lane)) || (group->checked_mask & (1U << lane)) ||
            ((heartbeat_slot & 1U) != (lane == 2U ? 1U : 0U))) continue;
        if (!validate_lane(state, group, lane)) {
            state->active = SHAKTI_MEDIA_GROUPS;
            return -1;
        }
    }
    if (group->checked_mask == group->required_mask) {
        state->current = *group;
        memset(&state->sensory, 0, sizeof(state->sensory));
        state->sensory.checked_mask = group->checked_mask;
        for (lane = 0U; lane < 3U; ++lane) if (group->checked_mask & (1U << lane)) {
            const shakti_media_frame_t *from = &group->parts[lane];
            shakti_sensory_frame_t *to = &state->sensory.parts[lane];
            memcpy(to->source_id, from->source_id, strlen(from->source_id) + 1U);
            memcpy(to->context, from->context, strlen(from->context) + 1U);
            to->position64 = from->position64;
            to->lane = from->lane;
            to->width = from->width;
            to->height = from->height;
            to->channels = from->channels;
            to->sample_bytes = from->sample_bytes;
            to->sample_rate = from->sample_rate;
            to->format = from->format;
            to->data = from->rebuilt;
            to->bytes = from->bytes;
            to->validated_hash = from->validated_hash;
        }
        state->current_ready = 1U;
        group->in_use = 0U;
        state->active = SHAKTI_MEDIA_GROUPS;
        ++state->published;
        return 2;
    }
    return 1;
}

const shakti_media_group_t *shakti_media_current(const shakti_media_state_t *state)
{
    return state != NULL && state->current_ready ? &state->current : NULL;
}

void shakti_media_release(shakti_media_state_t *state)
{
    if (state != NULL) {
        state->current_ready = 0U;
        memset(&state->sensory, 0, sizeof(state->sensory));
    }
}

int shakti_media_cancel(shakti_media_state_t *state, const char *source_id, uint64_t window)
{
    size_t i, lane;
    if (state == NULL || source_id == NULL) return 0;
    for (i = 0U; i < SHAKTI_MEDIA_GROUPS; ++i) {
        shakti_media_group_t *group = &state->pending[i];
        if (!group->in_use) continue;
        for (lane = 0U; lane < 3U; ++lane) if (group->queued_mask & (1U << lane)) {
            if (group->parts[lane].position64 / 2U == window && strcmp(group->parts[lane].source_id, source_id) == 0) {
                memset(group, 0, sizeof(*group));
                if (state->active == i) state->active = SHAKTI_MEDIA_GROUPS;
                return 1;
            }
        }
    }
    return 0;
}

/* Every consumer receives pointers to these same rebuilt shared buffers.
 * The view exposes no input, reference, or binary-work pointer. */
const shakti_sensory_group_t *shakti_media_perception(const shakti_media_state_t *state)
{
    return state != NULL && state->current_ready ? &state->sensory : NULL;
}

