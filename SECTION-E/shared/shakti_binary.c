#include "shakti_media.h"

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

