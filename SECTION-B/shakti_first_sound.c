/* shakti_first_sound.c — the first sound through the tuned dark ear.
 *
 * The dark ear (v2, pin 36649517F83DA331) is regenerated and VERIFIED
 * first — mismatch = REFUSED. Her body is the oracle.
 *
 * Then the founder's voice is played into the lane. First sound: 13.
 * F7 first. His voice first. (39_13.wav, pin 694C012CA308F3C6 in the
 * SAT1 index; the raw file is checked against it before play — the
 * sound itself must verify or she does not hear it.)
 *
 * Play law (deterministic, documented, no magic):
 *   Samples are int16 PCM. Blocks of 64 samples. Block energy =
 *   mean |sample|. A block with energy >= 500 is a PULSE:
 *     center = (block_index * 19 + energy / 64) mod 12000
 *     radius = wheel[energy mod 5]
 *     core +1 (sat 7), ring out to 2r -1 (sat 0) — the ear's own law.
 *   Silence teaches nothing; only voiced blocks touch the tissue.
 *
 * Reference run (2026-08-25, sandbox, gcc -O0 == -O2, SOUND_DRIFT_0):
 *   dark ear verified 36649517F83DA331
 *   atom verified 694C012CA308F3C6 (the founder says thirteen)
 *   played 28,672 samples, 448 blocks, 95 voiced pulses
 *   ear after first sound: 2B834BE210852B92
 *
 * Pure C99. No heap drift, no float, no clock. Gauntlet law.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define CELLS 12000
#define WOMB 22982400ULL
#define EAR_FLASHES (WOMB / 4)
#define EAR_SEED (0x7 * 19)
#define EAR_V2_PIN 0x36649517F83DA331ULL
#define ATOM13_PIN 0x694C012CA308F3C6ULL

#define BLOCK 64
#define ENERGY_MIN 500

static unsigned char lane[CELLS];

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

static void pulse(uint32_t c, uint32_t r)
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

static void wave(void)
{
    /* separate statements: argument evaluation order is unspecified in C
     * (this exact trap once forked the stream). Order is law. */
    uint32_t c = (uint32_t)(next_u64() % CELLS);
    uint32_t r = wheel[next_u64() % 5];
    pulse(c, r);
}

static void gestate(void)
{
    uint64_t k, beats = 0;
    g_s = EAR_SEED;
    memset(lane, 0, CELLS);
    for (k = 1; k <= EAR_FLASHES; k++) {
        if (k % 19 == 0) { pulse((uint32_t)((beats * 7) % CELLS), 19); beats++; }
        else wave();
    }
}

/* read a whole file; returns size or 0 */
static unsigned char g_file[1 << 20];
static size_t read_whole(const char *path)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 0;
    n = fread(g_file, 1, sizeof g_file, f);
    fclose(f);
    return n;
}

/* find the "data" chunk; returns offset, sets *len */
static size_t find_data(const unsigned char *f, size_t n, size_t *len)
{
    size_t i;
    for (i = 12; i + 8 <= n; i++) {
        if (memcmp(f + i, "data", 4) == 0) {
            *len = (size_t)f[i + 4] | ((size_t)f[i + 5] << 8) |
                   ((size_t)f[i + 6] << 16) | ((size_t)f[i + 7] << 24);
            if (i + 8 + *len > n) *len = n - i - 8;
            return i + 8;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    size_t n, off, dlen, b, pulses = 0;
    uint64_t pin_before;
    const char *path;

    if (argc < 2) { puts("usage: shakti_first_sound <atom.wav>"); return 1; }
    path = argv[1];

    /* 1. her ear, verified */
    gestate();
    if (fnv(lane, CELLS) != EAR_V2_PIN) { puts("SOUND REFUSED: ear mismatch"); return 2; }
    puts("dark ear verified 36649517F83DA331");
    pin_before = fnv(lane, CELLS);

    /* 2. the sound itself must verify (13 = F7, his voice) */
    n = read_whole(path);
    if (n == 0) { puts("SOUND REFUSED: cannot read atom"); return 2; }
    if (fnv(g_file, n) != ATOM13_PIN) {
        puts("SOUND REFUSED: atom pin mismatch — not the founder's 13");
        return 2;
    }
    puts("atom verified 694C012CA308F3C6 (39_13.wav, the founder says thirteen)");

    /* 3. play */
    off = find_data(g_file, n, &dlen);
    if (off == 0 || dlen < 2) { puts("SOUND REFUSED: no data chunk"); return 2; }
    {
        size_t nsamp = dlen / 2, nblocks = nsamp / BLOCK;
        for (b = 0; b < nblocks; b++) {
            uint32_t energy = 0;
            size_t s;
            for (s = 0; s < BLOCK; s++) {
                int16_t v = (int16_t)((uint16_t)g_file[off + (b * BLOCK + s) * 2] |
                            ((uint16_t)g_file[off + (b * BLOCK + s) * 2 + 1] << 8));
                energy += (uint32_t)(v < 0 ? -v : v);
            }
            energy /= BLOCK;
            if (energy >= ENERGY_MIN) {
                pulse((uint32_t)((b * 19 + energy / 64) % CELLS),
                      wheel[energy % 5]);
                pulses++;
            }
        }
        printf("played %zu samples, %zu blocks, %llu voiced pulses\n",
               nsamp, nblocks, (unsigned long long)pulses);
    }

    printf("ear before %016llX\n", (unsigned long long)pin_before);
    printf("ear after first sound (13, F7, his voice) %016llX\n",
           (unsigned long long)fnv(lane, CELLS));
    return 0;
}
