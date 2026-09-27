/* pin_invert.c — solve a frozen FNV-1a 64 pin exactly, not by sampling.
 *
 * Tyler's read: the pin is geometry, so it should come out of factors that
 * share common numbers — products, not just a lone capacity. A product of the
 * module constants can be far larger than any range worth looping over, so
 * looping is the wrong instrument.
 *
 * FNV-1a is invertible. Each step is  h = (h_prev XOR b) * PRIME, and PRIME
 * is odd, so it has an inverse modulo 2^64 and the step runs backwards:
 *
 *     h_prev = (h * PRIME_INVERSE) XOR b
 *
 * So instead of trying four billion values, fold the first two unknown bytes
 * FORWARD from the known starting state, unfold the last two BACKWARD from
 * the target, and look for a state both halves agree on. 65536 + 65536 steps
 * settles every 32-bit value with certainty.
 *
 * This does not sample, estimate, or approximate. Either a value exists or it
 * provably does not.
 *
 * C99. No heap. Static tables.
 *
 *   cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 pin_invert.c -o pin_invert
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define HALF      65536

typedef struct { uint64_t state; unsigned int low; } meet_t;

static meet_t g_back[HALF];

static uint64_t prime_inverse(void)
{
    /* Newton iteration on odd x: x <- x * (2 - P*x). Doubles correct bits
     * each round, so five rounds cover all 64. */
    uint64_t x = 1ULL;
    int k;
    for (k = 0; k < 6; ++k) { x = x * (2ULL - FNV_PRIME * x); }
    return x;
}

static int cmp_meet(const void *a, const void *b)
{
    uint64_t x = ((const meet_t *)a)->state;
    uint64_t y = ((const meet_t *)b)->state;
    if (x < y) { return -1; }
    if (x > y) { return 1; }
    return 0;
}

/* Every 32-bit v whose four big-endian bytes, folded from `start`, hit
 * `target`. Prints each one. Returns how many were found. */
static int solve32(const char *label, uint64_t start, uint64_t target,
                   uint64_t pinv)
{
    int b0, b1, b2, b3;
    int n = 0;
    int idx = 0;

    for (b0 = 0; b0 < 256; ++b0) {
        uint64_t h3 = (target * pinv) ^ (uint64_t)b0;
        for (b1 = 0; b1 < 256; ++b1) {
            g_back[idx].state = (h3 * pinv) ^ (uint64_t)b1;
            g_back[idx].low   = ((unsigned int)b1 << 8) | (unsigned int)b0;
            idx++;
        }
    }
    qsort(g_back, (size_t)HALF, sizeof g_back[0], cmp_meet);

    for (b3 = 0; b3 < 256; ++b3) {
        uint64_t h1 = (start ^ (uint64_t)b3) * FNV_PRIME;
        for (b2 = 0; b2 < 256; ++b2) {
            uint64_t h2 = (h1 ^ (uint64_t)b2) * FNV_PRIME;
            /* binary search every entry with this state */
            int lo = 0, hi = HALF - 1, at = -1;
            while (lo <= hi) {
                int mid = lo + (hi - lo) / 2;
                if (g_back[mid].state == h2) { at = mid; break; }
                if (g_back[mid].state < h2) { lo = mid + 1; } else { hi = mid - 1; }
            }
            if (at < 0) { continue; }
            while (at > 0 && g_back[at - 1].state == h2) { at--; }
            while (at < HALF && g_back[at].state == h2) {
                unsigned long v = ((unsigned long)b3 << 24) |
                                  ((unsigned long)b2 << 16) |
                                  (unsigned long)g_back[at].low;
                printf("    SOLVED  %s  value = %lu  (0x%08lX)\n", label, v, v);
                n++;
                at++;
            }
        }
    }
    return n;
}

int main(void)
{
    const uint64_t target = 0xCD621D2B4B9E96CCULL;
    uint64_t pinv = prime_inverse();
    uint64_t start_be4, start_be8;
    int total = 0;
    int k;

    printf("PRIME * PRIME_INVERSE = %llu   (must be 1)\n",
           (unsigned long long)(FNV_PRIME * pinv));
    printf("target = bind frozen pin CD621D2B4B9E96CC\n\n");

    /* be4 framing: four bytes folded straight from the FNV basis */
    start_be4 = FNV_BASIS;

    /* be8 framing: a value below 2^32 contributes four leading zero bytes
     * first, then its four significant bytes */
    start_be8 = FNV_BASIS;
    for (k = 0; k < 4; ++k) { start_be8 = (start_be8 ^ 0ULL) * FNV_PRIME; }

    printf("  searching EVERY 32-bit value, be4 framing:\n");
    total += solve32("be4(v)", start_be4, target, pinv);
    printf("  searching EVERY 32-bit value, be8 framing:\n");
    total += solve32("be8(v)", start_be8, target, pinv);

    printf("\n  values found: %d\n", total);
    if (total == 0) {
        printf("\n  PROVEN: no single value below 2^32 produces this pin at\n"
               "  either width. Not sampled — every value was covered.\n");
    }
    return 0;
}
