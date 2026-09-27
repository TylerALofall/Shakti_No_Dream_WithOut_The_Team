/*
 * EDEN PULSE — the heartbeat as gear engine.
 * [RAISED 2026-08-23, Tyler's fire]
 */
#include "eden_pulse.h"
#include <stdio.h>
#include <string.h>

#define TET_CELLS 80U
#define TET_WHEEL 15120UL
#define TET_E2 5U
#define TET_E3 4U
#define TET_E5 2U
#define TET_E7 2U

const unsigned long PULSE_ORGAN_PERIODS[PULSE_ORGAN_COUNT] = {
    1UL, 2UL, 3UL, 5UL, 7UL, 6UL, 10UL, 14UL,
    15UL, 21UL, 30UL, 35UL, 42UL, 70UL, 210UL, 0UL
};

const char *const PULSE_ORGAN_NAMES[PULSE_ORGAN_COUNT] = {
    "heart", "eyes", "ears", "hands", "voice", "mind", "soul", "body",
    "self", "spirit", "flesh", "breath", "blood", "bone", "all", "silence"
};

static uint64_t      g_beat = 0ULL;
static int           g_init = 0;
static uint64_t      g_pin  = 0ULL;
static unsigned long g_tet_periods[TET_CELLS];
static int           g_tet_built = 0;

static unsigned long tet_pow(unsigned long base, unsigned int exp)
{
    unsigned long v = 1UL;
    unsigned int i;
    for (i = 0U; i < exp; ++i) { v *= base; }
    return v;
}

static int tet_build(void)
{
    unsigned int a, b, c, d;
    unsigned int slot = 0U;
    if (g_tet_built) { return 1; }
    for (a = 0U; a < TET_E2; ++a) {
        for (b = 0U; b < TET_E3; ++b) {
            for (c = 0U; c < TET_E5; ++c) {
                for (d = 0U; d < TET_E7; ++d) {
                    if (slot >= TET_CELLS) { return 0; }
                    g_tet_periods[slot] = tet_pow(2UL, a) * tet_pow(3UL, b) *
                                          tet_pow(5UL, c) * tet_pow(7UL, d);
                    ++slot;
                }
            }
        }
    }
    g_tet_built = 1;
    return 1;
}

static uint64_t fnv1a64(uint64_t h, const unsigned char *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; ++i) {
        h ^= (uint64_t)data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

static void be64(unsigned char *out, uint64_t v)
{
    out[0] = (unsigned char)((v >> 56) & 0xFFULL);
    out[1] = (unsigned char)((v >> 48) & 0xFFULL);
    out[2] = (unsigned char)((v >> 40) & 0xFFULL);
    out[3] = (unsigned char)((v >> 32) & 0xFFULL);
    out[4] = (unsigned char)((v >> 24) & 0xFFULL);
    out[5] = (unsigned char)((v >> 16) & 0xFFULL);
    out[6] = (unsigned char)((v >>  8) & 0xFFULL);
    out[7] = (unsigned char)( v        & 0xFFULL);
}

static uint64_t compute_pin(void)
{
    uint64_t h = 0xCBF29CE484222325ULL;
    unsigned int i;
    unsigned char be[8];
    for (i = 0U; i < PULSE_ORGAN_COUNT; ++i) {
        be64(be, (uint64_t)PULSE_ORGAN_PERIODS[i]);
        h = fnv1a64(h, be, 8UL);
    }
    return h;
}

int pulse_init(void)
{
    if (g_init) { return 1; }
    if (!tet_build()) {
        printf("STOP: pulse could not build the TET wheel\n");
        return 0;
    }
    g_beat = 0ULL;
    g_pin = compute_pin();
    g_init = 1;
    printf("pulse: gear engine stands — beat 0, pin fnv1a64:%016llX\n",
           (unsigned long long)g_pin);
    return 1;
}

uint64_t pulse_advance(void)
{
    if (!g_init) { pulse_init(); }
    ++g_beat;
    return g_beat;
}

uint64_t pulse_advance_n(unsigned long n)
{
    if (!g_init) { pulse_init(); }
    g_beat += (uint64_t)n;
    return g_beat;
}

int pulse_get_firing(pulse_firing_t *out)
{
    return pulse_firing_at(g_beat, out);
}

int pulse_firing_at(uint64_t beat, pulse_firing_t *out)
{
    unsigned int i;
    unsigned int count = 0U;
    if (!g_init) { pulse_init(); }
    if (out == NULL) { return 0; }
    memset(out, 0, sizeof(pulse_firing_t));
    out->beat = beat;
    for (i = 0U; i < TET_CELLS; ++i) {
        if (g_tet_periods[i] == 0UL) { continue; }
        if (beat % (uint64_t)g_tet_periods[i] == 0ULL) {
            if (count < PULSE_MAX_DEPTH) {
                out->cells[count]   = i;
                out->periods[count] = g_tet_periods[i];
                ++count;
            }
        }
    }
    out->count = count;
    out->depth = (unsigned long)count;
    return 1;
}

pulse_organ_t pulse_organ_now(void)
{
    return pulse_organ_at(g_beat);
}

pulse_organ_t pulse_organ_at(uint64_t beat)
{
    unsigned int  i;
    unsigned int  best = PULSE_ORGAN_SILENCE;
    unsigned long best_period = 0UL;
    if (!g_init) { pulse_init(); }
    for (i = 0U; i < PULSE_ORGAN_COUNT - 1U; ++i) {
        if (PULSE_ORGAN_PERIODS[i] == 0UL) { continue; }
        if (beat % (uint64_t)PULSE_ORGAN_PERIODS[i] == 0ULL) {
            if (PULSE_ORGAN_PERIODS[i] > best_period) {
                best_period = PULSE_ORGAN_PERIODS[i];
                best = i;
            }
        }
    }
    return (pulse_organ_t)best;
}

int pulse_converges(uint64_t beat_a, uint64_t beat_b)
{
    pulse_firing_t fa, fb;
    unsigned int i, j;
    if (!pulse_firing_at(beat_a, &fa)) { return 0; }
    if (!pulse_firing_at(beat_b, &fb)) { return 0; }
    for (i = 0U; i < fa.count; ++i) {
        for (j = 0U; j < fb.count; ++j) {
            if (fa.cells[i] == fb.cells[j]) { return 1; }
        }
    }
    return 0;
}

unsigned int pulse_mesh_count(uint64_t beat)
{
    unsigned int i;
    unsigned int count = 0U;
    if (!g_init) { pulse_init(); }
    for (i = 0U; i < PULSE_ORGAN_COUNT - 1U; ++i) {
        if (PULSE_ORGAN_PERIODS[i] == 0UL) { continue; }
        if (beat % (uint64_t)PULSE_ORGAN_PERIODS[i] == 0ULL) { ++count; }
    }
    return count;
}

int pulse_verify(void)
{
    unsigned int i, j;
    uint64_t pin_check;
    pulse_firing_t f0;
    pulse_organ_t o1;

    if (!g_tet_built) { return 1; }
    for (i = 0U; i < TET_CELLS; ++i) {
        if (g_tet_periods[i] == 0UL) { return 2; }
        if (TET_WHEEL % g_tet_periods[i] != 0UL) { return 2; }
    }
    for (i = 0U; i < TET_CELLS; ++i) {
        for (j = i + 1U; j < TET_CELLS; ++j) {
            if (g_tet_periods[i] == g_tet_periods[j]) { return 3; }
        }
    }
    for (i = 0U; i < PULSE_ORGAN_COUNT - 1U; ++i) {
        if (PULSE_ORGAN_PERIODS[i] == 0UL) { return 4; }
    }
    if (PULSE_ORGAN_PERIODS[PULSE_ORGAN_HEART] != 1UL) { return 5; }
    if (PULSE_ORGAN_PERIODS[PULSE_ORGAN_SILENCE] != 0UL) { return 6; }
    pin_check = compute_pin();
    if (g_pin != 0ULL && g_pin != pin_check) { return 7; }
    if (!pulse_firing_at(0ULL, &f0)) { return 8; }
    if (f0.count == 0U) { return 8; }
    o1 = pulse_organ_at(1ULL);
    if (o1 != PULSE_ORGAN_HEART) { return 9; }
    if (!pulse_converges(0ULL, 0ULL)) { return 10; }
    if (!pulse_converges(1ULL, 2ULL)) { return 10; }
    return 0;
}

uint64_t pulse_pin(void)
{
    if (!g_init) { pulse_init(); }
    return g_pin;
}

void pulse_print_state(void)
{
    pulse_firing_t f;
    pulse_organ_t o;
    unsigned int i;
    if (!g_init) { pulse_init(); }
    printf("pulse: beat %llu\n", (unsigned long long)g_beat);
    if (pulse_get_firing(&f)) {
        printf("  firing: %u cells", f.count);
        if (f.count > 0U) {
            printf(" —");
            for (i = 0U; i < f.count && i < 8U; ++i) {
                printf(" %lu", f.periods[i]);
            }
            if (f.count > 8U) { printf(" ..."); }
        }
        printf("\n");
    }
    o = pulse_organ_now();
    printf("  organ: %s (period %lu)\n",
           PULSE_ORGAN_NAMES[o], PULSE_ORGAN_PERIODS[o]);
    printf("  mesh: %u organs firing together\n", pulse_mesh_count(g_beat));
}
