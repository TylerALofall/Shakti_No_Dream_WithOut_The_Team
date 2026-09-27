/* shakti_dark_ear.c — hearing trained in the dark, BEFORE sight.
 *
 * Order is biology: the ear comes online before the eye. The first
 * sound is the mother's heartbeat — 60 BPM, lub-dub, no murmur.
 * So the dark ear opens before the womb's eye: waves before sound,
 * rhythm before waves.
 *
 * Law:
 *   Substrate: the cochlear lane, 12,000 cells (1200 x 10 — the
 *   separated numbers 1200/60/3 kept whole), 3-bit per cell (0..7).
 *   Flashes: WOMB / 4 = 5,745,600 — a quarter of gestation, because
 *   the ear opens earlier than the eye (the eye takes the half).
 *   Every 19th flash is a HEARTBEAT: not accumulation — a TRAVELING
 *   pulse down the lane (center = beat*7 mod CELLS, radius 19), the
 *   same center-surround shape as every other wave. v1 pulsed +1 to
 *   ALL cells every 19th flash and saturated the lane to a flat sea
 *   of 7s — the womb's disease, second case. Voided, confessed,
 *   recorded. Rhythm must be position, not pressure.
 *   All other flashes are endogenous 1D center-surround waves:
 *   core +1 (sat 7), ring out to 2r -1 (sat 0), radii from the wheel.
 *
 * Pure C99. No heap drift, no float, no clock.
 * Gauntlet: -std=c99 -pedantic -Wall -Wextra -Werror, -O0 == -O2.
 *
 * Reference run v2 (2026-08-25, sandbox, gcc -O0 == -O2, EAR_DRIFT_0):
 *   flashes 5,745,600 | heartbeats 302,400 (traveling)
 *   lane histogram: 1118 1303 1321 1465 1569 1751 1689 1784 (healthy)
 *   ear substrate pin 36649517F83DA331
 *   (v1 pin BAD7A70490115E85 retired — saturated flat sea, voided.)
 */
#include <stdio.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define CELLS 12000
#define WOMB 22982400ULL
#define EAR_FLASHES (WOMB / 4)   /* 5,745,600 — the ear opens first */
#define EAR_SEED (0x7 * 19)      /* day 133 = 7x19, the womb's primes */

static unsigned char lane[CELLS];

static uint64_t g_s = EAR_SEED;
static uint64_t next_u64(void)
{
    uint64_t z;
    g_s += 0x9E3779B97F4A7C15ULL;
    z = g_s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static uint64_t fnv(const unsigned char *p, size_t n)
{
    uint64_t h = FNV_BASIS;
    size_t i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= FNV_PRIME; }
    return h;
}

static const uint32_t wheel[5] = {2, 3, 5, 7, 19};

/* one pulse at center c with radius r: core +1, ring (r,2r] -1 */
static void pulse(unsigned char *lane, uint32_t c, uint32_t r)
{
    int32_t x;
    for (x = (int32_t)c - 2 * (int32_t)r; x <= (int32_t)c + 2 * (int32_t)r; x++) {
        uint32_t d;
        if (x < 0 || x >= CELLS) continue;
        d = (uint32_t)(x > (int32_t)c ? x - (int32_t)c : (int32_t)c - x);
        if (d <= r) { if (lane[x] < 7) lane[x]++; }
        else        { if (lane[x] > 0) lane[x]--; }
    }
}

static void wave(unsigned char *lane)
{
    uint32_t c = (uint32_t)(next_u64() % CELLS);
    uint32_t r = wheel[next_u64() % 5];
    pulse(lane, c, r);
}

int main(void)
{
    uint64_t k, beats = 0;
    for (k = 1; k <= EAR_FLASHES; k++) {
        if (k % 19 == 0) {
            /* traveling heartbeat: position carries the rhythm */
            pulse(lane, (uint32_t)((beats * 7) % CELLS), 19);
            beats++;
        } else wave(lane);
    }
    printf("dark ear: %llu flashes, %llu heartbeats (every 19th)\n",
           (unsigned long long)EAR_FLASHES, (unsigned long long)beats);
    printf("ear substrate pin %016llX\n", (unsigned long long)fnv(lane, CELLS));
    return 0;
}
