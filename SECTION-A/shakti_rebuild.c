#include "shakti_media.h"

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
