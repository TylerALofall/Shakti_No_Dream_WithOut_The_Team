#include "shakti_senses.h"
#include <string.h>

static uint32_t little(const unsigned char *p, size_t count)
{
    uint32_t value = 0U;
    while (count) value = (value << 8) | p[--count];
    return value;
}

int shakti_ears_wave(const unsigned char *file, size_t bytes, shakti_pcm_wave_t *wave)
{
    size_t at = 12U;
    unsigned int format_seen = 0U, data_seen = 0U, align = 0U;
    shakti_pcm_wave_t candidate;
    memset(&candidate, 0, sizeof(candidate));
    if (!file || !wave || bytes < 12U || memcmp(file, "RIFF", 4U) ||
        memcmp(file + 8U, "WAVE", 4U) || (uint64_t)little(file + 4U, 4U) + 8U != bytes) return 0;
    while (at < bytes) {
        size_t n, padded;
        const unsigned char *chunk;
        if (bytes - at < 8U) return 0;
        n = little(file + at + 4U, 4U); chunk = file + at; at += 8U;
        padded = n + (n & 1U);
        if (padded < n || padded > bytes - at) return 0;
        if (!memcmp(chunk, "fmt ", 4U)) {
            if (format_seen++ || n < 16U || little(file + at, 2U) != 1U ||
                little(file + at + 14U, 2U) != 16U) return 0;
            candidate.channels = little(file + at + 2U, 2U);
            candidate.rate = little(file + at + 4U, 4U);
            align = little(file + at + 12U, 2U);
            if (!candidate.channels || candidate.channels > 8U || !candidate.rate ||
                candidate.rate > 384000U || align != candidate.channels * 2U ||
                little(file + at + 8U, 4U) != candidate.rate * align) return 0;
        } else if (!memcmp(chunk, "data", 4U)) {
            if (data_seen++) return 0;
            candidate.pcm = file + at; candidate.bytes = n;
        }
        at += padded;
    }
    if (!format_seen || !data_seen || !align || candidate.bytes % align) return 0;
    candidate.frames = candidate.bytes / align;
    *wave = candidate;
    return 1;
}

int shakti_ears_slice(const shakti_pcm_wave_t *wave, uint64_t position64,
    unsigned char *pcm, size_t capacity, size_t *bytes, size_t *recorded_frames)
{
    uint64_t start, end, count, recorded;
    size_t align;
    if (!wave || !wave->pcm || (!pcm && capacity) || !bytes || !recorded_frames ||
        !wave->rate || wave->rate > 384000U || !wave->channels || wave->channels > 8U ||
        position64 == UINT64_MAX || position64 + 1U > UINT64_MAX / wave->rate) return 0;
    align = wave->channels * 2U;
    if (wave->frames > SIZE_MAX / align || wave->bytes != wave->frames * align) return 0;
    start = position64 * wave->rate / 64U;
    end = (position64 + 1U) * wave->rate / 64U;
    count = end - start;
    if (!count || count > SIZE_MAX / align || (pcm && count > capacity / align)) return 0;
    recorded = start < wave->frames ? wave->frames - start : 0U;
    if (recorded > count) recorded = count;
    if (pcm) {
        uintptr_t a = (uintptr_t)pcm, b = (uintptr_t)wave->pcm;
        size_t n = (size_t)count * align;
        if (n > UINTPTR_MAX - a || wave->bytes > UINTPTR_MAX - b ||
            (a < b + wave->bytes && b < a + n)) return 0;
        memset(pcm, 0, n);
        if (recorded) memcpy(pcm, wave->pcm + (size_t)start * align, (size_t)recorded * align);
    }
    *bytes = (size_t)count * align; *recorded_frames = (size_t)recorded;
    return 1;
}
