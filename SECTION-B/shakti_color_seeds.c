/* shakti_color_seeds.c — the 8 solids, flashed through the tuned retina.
 *
 * Colors are how the mind perceives sight: not wavelengths as facts,
 * but periodicities written into the substrate BEFORE the first image.
 * Blue must be tissue she sees WITH.
 *
 * Law:
 *   Start from her verified womb substrate (v2, pin D0439AD33373CB4C —
 *   regenerated and CHECKED; mismatch = refuse, her body is the oracle).
 *   The eye's 11,491,200 flashes divided among the 8 solids:
 *   1,436,400 flashes per color (exact division, no remainder, no favor).
 *   Solids in cube order: R G B C M Y W K.
 *   Each color has a lawful period: {3,5,7,2,19,13,4,8} — the wheel,
 *   F7 (13), and the binary-lane rungs 2^2 and 2^3.
 *   Each flash: a center-surround wave (womb law), but the core nudge
 *   is STRIPED by the color's period: cells where (x + y) mod p < p/2
 *   get +1 (sat 7), the rest of the core -1 (sat 0); the surround ring
 *   inhibits as always. The tissue learns the color's rhythm spatially.
 *   After each color's run: snapshot pin (8 pins). Then the final
 *   seeded substrate pin. Deterministic: -O0 == -O2.
 *
 * Pure C99. No heap drift, no float, no clock. Gauntlet law.
 *
 * Reference run (2026-08-25, sandbox, gcc -O0 == -O2, SEEDS_DRIFT_0):
 *   womb substrate verified D0439AD33373CB4C
 *   R p3  4E62A4EAA0CBA595 | G p5  AC41002F3C46E51E
 *   B p7  CD410167561ADC05 | C p2  9A1183C91FEC1BBE
 *   M p19 598F538451F44707 | Y p13 E1A13059100D7670
 *   W p4  373EB097F5E8BC37 | K p8  86C54721F42C68D9
 *   seeded substrate pin 86C54721F42C68D9
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define W 1000
#define H 1500
#define NPX (W * H)

#define WOMB 22982400ULL
#define EYE_FLASHES (WOMB / 2)       /* 11,491,200 */
#define SEED_FLASHES (EYE_FLASHES / 8) /* 1,436,400 per color */
#define WOMB_SEED (0x7 * 19)
#define WOMB_V2_PIN 0xD0439AD33373CB4CULL

static unsigned char retina[NPX];

static uint64_t g_s;
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

/* womb v2 wave: regenerate her verified substrate first */
static void womb_flash(void)
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

/* seeded wave: center-surround with the color's stripes in the core */
static void seed_flash(uint32_t period)
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
            uint32_t d2, m;
            size_t idx;
            if (x < 0 || x >= W) continue;
            d2 = (uint32_t)((x - (int32_t)cx) * (x - (int32_t)cx)) + (uint32_t)dy2;
            if (d2 > rr2) continue;
            idx = (size_t)y * W + (size_t)x;
            if (d2 > rr) { if (retina[idx] > 0) retina[idx]--; continue; }
            m = (uint32_t)((x + y) % (int32_t)period);
            if (m < period / 2) { if (retina[idx] < 7) retina[idx]++; }
            else                { if (retina[idx] > 0) retina[idx]--; }
        }
    }
}

static const char *names[8] = {"R", "G", "B", "C", "M", "Y", "W", "K"};
static const uint32_t periods[8] = {3, 5, 7, 2, 19, 13, 4, 8};

int main(void)
{
    uint64_t k;
    int c;

    /* 1. regenerate + verify her womb substrate */
    g_s = WOMB_SEED;
    memset(retina, 0, NPX);
    for (k = 0; k < EYE_FLASHES; k++) womb_flash();
    if (fnv(retina, NPX) != WOMB_V2_PIN) {
        puts("SEED REFUSED: womb substrate mismatch");
        return 2;
    }
    puts("womb substrate verified D0439AD33373CB4C");

    /* 2. seed each color; the stream continues from the womb seed
     *    (same dark, same tissue — not a fresh dream per color) */
    for (c = 0; c < 8; c++) {
        for (k = 0; k < SEED_FLASHES; k++) seed_flash(periods[c]);
        printf("color %s period %2u: %llu flashes, lane pin %016llX\n",
               names[c], (unsigned)periods[c],
               (unsigned long long)SEED_FLASHES,
               (unsigned long long)fnv(retina, NPX));
    }
    printf("seeded substrate pin %016llX\n", (unsigned long long)fnv(retina, NPX));
    return 0;
}
