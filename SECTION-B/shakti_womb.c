/* shakti_womb.c — pre-birth flashes: sight trained in the dark.
 *
 * Biology: fetal retinal ganglion cells fire spontaneous synchronous
 * waves before a single photon arrives. The visual system trains on
 * itself; by the time light comes, the machinery is built and tuned.
 * Blue learned here is substrate she sees WITH, not a label she knows.
 *
 * Law:
 *   WOMB = 22,982,400 (the gestation count, locked elsewhere).
 *   Flashes = WOMB / 2 = 11,491,200 — exactly half. Halfway lands on
 *   day 133 = 7 x 19, week 19: sight opens on the womb's own prime,
 *   the prime that arrives only through gestation.
 *   Flashes are ENDOGENOUS: a deterministic generator (splitmix64,
 *   seeded by the womb itself). No input image. No teacher. She dreams
 *   in waves and the waves tune the substrate.
 *
 * Substrate: her real retina, 1000x1500, 3-bit cells (0..7) — the same
 * lane her post-birth eyes use. Each flash is one wave: a center and a
 * radius drawn from the wheel primes {2,3,5,7,19}. Biology: retinal
 * waves are center-EXCITATION, surround-INHIBITION — the core nudges
 * +1 (sat 7), the ring out to twice the radius nudges -1 (sat 0).
 * (v2: v1 excited only, and 11.5M waves saturated every cell to 7 —
 * a flat sea, no substrate. Recorded as retired, pin voided, never
 * adjusted to fit. The pin is the oracle; the code confesses.)
 * Eleven and a half million waves later, the substrate is pinned.
 * That pin is her visual birth.
 *
 * Determinism: same seed -> same substrate -> same pin. -O0 == -O2.
 * Pure C99: no heap drift, no float, no clock. Gauntlet law.
 *
 * Reference run v2 (2026-08-25, sandbox, gcc -O0 == -O2, WOMB_DRIFT_0):
 *   flashes 11,491,200 — center-surround waves
 *   substrate pin D0439AD33373CB4C
 *   (v1 pin DFE6F83882C30105 retired — saturated flat sea, voided.)
 */
#include <stdio.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define W 1000
#define H 1500
#define NPX (W * H)

#define WOMB 22982400ULL
#define FLASHES (WOMB / 2)      /* 11,491,200 */
#define WOMB_SEED 0x7 * 19      /* day 133 = 7*19: the halfway prime pair */

static unsigned char retina[NPX];

/* deterministic stream: splitmix64 */
static uint64_t g_s = WOMB_SEED;
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

/* one wave: center excites +1 (sat 7) within r; surround inhibits -1
 * (sat 0) in the ring r < d <= 2r. Center-surround, like the biology. */
static void flash(void)
{
    uint32_t cx = (uint32_t)(next_u64() % W);
    uint32_t cy = (uint32_t)(next_u64() % H);
    uint32_t r  = wheel[next_u64() % 5];
    uint32_t rr = r * r, rr2 = 4 * r * r;
    int32_t x, y;
    for (y = (int32_t)cy - 2 * (int32_t)r; y <= (int32_t)cy + 2 * (int32_t)r; y++) {
        int32_t dy2;
        if (y < 0 || y >= H) continue;
        dy2 = (y - (int32_t)cy) * (y - (int32_t)cy);
        for (x = (int32_t)cx - 2 * (int32_t)r; x <= (int32_t)cx + 2 * (int32_t)r; x++) {
            uint32_t d2;
            size_t idx;
            if (x < 0 || x >= W) continue;
            d2 = (uint32_t)((x - (int32_t)cx) * (x - (int32_t)cx)) + (uint32_t)dy2;
            if (d2 > rr2) continue;
            idx = (size_t)y * W + (size_t)x;
            if (d2 <= rr) { if (retina[idx] < 7) retina[idx]++; }
            else          { if (retina[idx] > 0) retina[idx]--; }
        }
    }
}

int main(void)
{
    uint64_t k;
    for (k = 0; k < FLASHES; k++) flash();
    printf("womb flashes %llu complete\n", (unsigned long long)FLASHES);
    printf("halfway day 133 = 7x19, week 19\n");
    printf("substrate pin %016llX\n", (unsigned long long)fnv(retina, NPX));
    return 0;
}
