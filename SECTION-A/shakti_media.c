#include "shakti_media.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int overlap(const void *a, size_t na, const void *b, size_t nb);

/* Portable bit transpose: each source byte contributes one bit to each of
 * eight planes. This groups bytes for packing; it never groups spatial pixels. */
static uint64_t transpose_bits(uint64_t value)
{
    uint64_t swap;
    swap = (value ^ (value >> 7)) & UINT64_C(0x00AA00AA00AA00AA);
    value ^= swap ^ (swap << 7);
    swap = (value ^ (value >> 14)) & UINT64_C(0x0000CCCC0000CCCC);
    value ^= swap ^ (swap << 14);
    swap = (value ^ (value >> 28)) & UINT64_C(0x00000000F0F0F0F0);
    return value ^ swap ^ (swap << 28);
}

size_t shakti_binary_capacity(size_t bytes)
{
    if (bytes > SIZE_MAX - 7U) return 0U;
    return ((bytes + 7U) / 8U) * 8U;
}

int shakti_binary_encode(const unsigned char *input, size_t bytes, unsigned char *binary, size_t capacity)
{
    size_t group, plane, stride, required = shakti_binary_capacity(bytes);
    if (input == NULL || binary == NULL || bytes == 0U || required == 0U || capacity < required ||
        overlap(input, bytes, binary, required)) return 0;
    stride = required / 8U;
    for (group = 0U; group < stride; ++group) {
        uint64_t packed = 0U;
        size_t i;
        for (i = 0U; i < 8U && group * 8U + i < bytes; ++i)
            packed |= (uint64_t)input[group * 8U + i] << (i * 8U);
        packed = transpose_bits(packed);
        for (plane = 0U; plane < 8U; ++plane)
            binary[plane * stride + group] = (unsigned char)(packed >> (plane * 8U));
    }
    return 1;
}

int shakti_binary_rebuild(const unsigned char *binary, size_t bytes, unsigned char *out, size_t capacity)
{
    size_t group, plane, stride, required = shakti_binary_capacity(bytes);
    if (binary == NULL || out == NULL || bytes == 0U || required == 0U || capacity < bytes ||
        overlap(binary, required, out, bytes)) return 0;
    stride = required / 8U;
    for (group = 0U; group < stride; ++group) {
        uint64_t packed = 0U;
        size_t i;
        for (plane = 0U; plane < 8U; ++plane)
            packed |= (uint64_t)binary[plane * stride + group] << (plane * 8U);
        packed = transpose_bits(packed);
        for (i = 0U; i < 8U && group * 8U + i < bytes; ++i)
            out[group * 8U + i] = (unsigned char)(packed >> (i * 8U));
    }
    return 1;
}

uint64_t shakti_media_hash(const unsigned char *bytes, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t i;
    for (i = 0U; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

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
        frame->lane > 2U || frame->sample_bytes == 0U || frame->sample_bytes > 8U ||
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
            group->in_use = 1U;
        }
        if (group->required_mask != frame->required_mask || group->taken || group->held ||
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

int shakti_media_take(shakti_media_state_t *state)
{
    size_t i, lane, chosen = SHAKTI_MEDIA_GROUPS;
    uint64_t oldest = UINT64_MAX;
    if (state == NULL || state->current_ready || state->active != SHAKTI_MEDIA_GROUPS) return 0;
    for (i = 0U; i < SHAKTI_MEDIA_GROUPS; ++i) {
        if (state->pending[i].in_use && !state->pending[i].held && state->pending[i].order < oldest) {
            oldest = state->pending[i].order;
            chosen = i;
        }
    }
    if (chosen == SHAKTI_MEDIA_GROUPS) return 0;
    if (state->pending[chosen].queued_mask != state->pending[chosen].required_mask) return 0;
    for (lane = 0U; lane < 3U; ++lane) if (state->pending[chosen].required_mask & (1U << lane)) {
        shakti_media_frame_t *frame = &state->pending[chosen].parts[lane];
        memcpy(frame->reference, frame->input, frame->bytes);
        memcpy(frame->rebuilt, frame->input, frame->bytes);
        if (!shakti_binary_encode(frame->rebuilt, frame->bytes, frame->binary, frame->binary_capacity)) {
            state->pending[chosen].held = 1U;
            (void)reject(state, "binary breakdown failed; item held");
            return -1;
        }
    }
    state->pending[chosen].taken = 1U;
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
    for (lane = 0U; lane < 3U; ++lane) {
        shakti_media_frame_t *frame;
        uint64_t hash;
        if (!(group->required_mask & (1U << lane)) || (group->checked_mask & (1U << lane)) ||
            ((heartbeat_slot & 1U) != (lane == 2U ? 1U : 0U))) continue;
        frame = &group->parts[lane];
        hash = shakti_media_hash(frame->reference, frame->bytes);
        if (!shakti_binary_rebuild(frame->binary, frame->bytes, frame->rebuilt, frame->rebuilt_capacity) ||
            memcmp(frame->reference, frame->rebuilt, frame->bytes) != 0 ||
            (frame->expected_hash && frame->expected_hash != hash)) {
            group->held = 1U;
            state->active = SHAKTI_MEDIA_GROUPS;
            (void)reject(state, "reconstruction mismatch; whole source window held");
            return -1;
        }
        frame->validated_hash = hash;
        group->checked_mask |= 1U << lane;
        ++state->validated_parts;
    }
    if (group->checked_mask == group->required_mask) {
        state->current = *group;
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
    if (state != NULL) state->current_ready = 0U;
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
