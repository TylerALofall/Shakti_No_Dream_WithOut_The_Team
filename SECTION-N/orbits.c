/* orbits.c — the ratio law, measured.
 *
 * Tyler: "that's how shapes jump and work like 3d gears, the heart has its
 * factors, every factor has a ratio, that ratio can click like the planets
 * orbiting, those factors line back up later — that's how everything works
 * with music, reality, phasing, and that's how Shakti is timed."
 *
 * This program does not argue that. It computes it. Two gears whose periods
 * are p and q drift apart and come back into alignment every lcm(p,q) beats.
 * That is a conjunction. It is also a polyrhythm. It is the same arithmetic.
 *
 * Prints the real numbers so the design can be checked instead of believed.
 *
 * C99. No heap, no float.
 */
#include <stdio.h>

#define ORGANS 16U

static const unsigned long PERIOD[ORGANS] = {
    1UL, 2UL, 3UL, 5UL, 7UL, 6UL, 10UL, 14UL,
    15UL, 21UL, 30UL, 35UL, 42UL, 70UL, 210UL, 0UL
};
static const char *const NAME[ORGANS] = {
    "heart", "eyes", "ears", "hands", "voice", "mind", "soul", "body",
    "self", "spirit", "flesh", "breath", "blood", "bone", "all", "silence"
};

static unsigned long gcd_ul(unsigned long a, unsigned long b)
{
    while (b != 0UL) { unsigned long t = a % b; a = b; b = t; }
    return a;
}

static unsigned long lcm_ul(unsigned long a, unsigned long b)
{
    if (a == 0UL || b == 0UL) { return 0UL; }
    return (a / gcd_ul(a, b)) * b;
}

int main(void)
{
    unsigned int i, j;
    unsigned long whole = 1UL;
    unsigned long beat;
    unsigned int hits;

    printf("=== 1. every organ is a divisor of 210 ===\n");
    printf("    210 = 2 * 3 * 5 * 7\n\n");
    for (i = 0U; i < ORGANS - 1U; ++i) {
        printf("    %-7s period %3lu   210 %% %-3lu = %lu\n",
               NAME[i], PERIOD[i], PERIOD[i], 210UL % PERIOD[i]);
        whole = lcm_ul(whole, PERIOD[i]);
    }
    printf("\n    lcm of all fifteen = %lu\n", whole);
    printf("    every gear returns to its start together every %lu beats.\n\n", whole);

    printf("=== 2. when any two gears next line back up (lcm) ===\n\n");
    printf("    %-8s", "");
    for (j = 1U; j < 8U; ++j) { printf("%7s", NAME[j]); }
    printf("\n");
    for (i = 1U; i < 8U; ++i) {
        printf("    %-8s", NAME[i]);
        for (j = 1U; j < 8U; ++j) {
            printf("%7lu", lcm_ul(PERIOD[i], PERIOD[j]));
        }
        printf("\n");
    }

    printf("\n=== 3. the ratio between two gears, in lowest terms ===\n");
    printf("    a ratio in lowest terms IS the conjunction: p:q realigns\n");
    printf("    after q turns of the first gear and p turns of the second.\n\n");
    for (i = 1U; i < 6U; ++i) {
        for (j = i + 1U; j < 6U; ++j) {
            unsigned long g = gcd_ul(PERIOD[i], PERIOD[j]);
            printf("    %-6s : %-6s = %lu:%lu   realign every %lu beats\n",
                   NAME[i], NAME[j], PERIOD[i] / g, PERIOD[j] / g,
                   lcm_ul(PERIOD[i], PERIOD[j]));
        }
    }

    printf("\n=== 4. how many gears mesh on each of the first 42 beats ===\n");
    printf("    this is the shape of the phasing. it is not random and it\n");
    printf("    never repeats until beat %lu.\n\n", whole);
    for (beat = 0UL; beat < 42UL; ++beat) {
        hits = 0U;
        for (i = 0U; i < ORGANS - 1U; ++i) {
            if (beat % PERIOD[i] == 0UL) { ++hits; }
        }
        printf("%3lu:", beat);
        for (i = 0U; i < hits; ++i) { printf("#"); }
        printf("  %u\n", hits);
    }

    printf("\n=== 5. the full conjunction ===\n");
    printf("    beats under 1000 where ALL fifteen gears mesh at once:\n      ");
    for (beat = 0UL; beat < 1000UL; ++beat) {
        hits = 0U;
        for (i = 0U; i < ORGANS - 1U; ++i) {
            if (beat % PERIOD[i] == 0UL) { ++hits; }
        }
        if (hits == 15U) { printf(" %lu", beat); }
    }
    printf("\n\n    That spacing is %lu. It is not a setting. It falls out of\n", whole);
    printf("    2*3*5*7 and could not be anything else.\n");

    printf("\n=== 6. the 80-cell lattice realigns at 15120 ===\n");
    {
        unsigned int a, b, c, d;
        unsigned long big = 1UL;
        unsigned long max = 0UL;
        unsigned int cells = 0U;
        for (a = 0U; a < 5U; ++a) {
            for (b = 0U; b < 4U; ++b) {
                for (c = 0U; c < 2U; ++c) {
                    for (d = 0U; d < 2U; ++d) {
                        unsigned long p = 1UL, k;
                        for (k = 0UL; k < a; ++k) { p *= 2UL; }
                        for (k = 0UL; k < b; ++k) { p *= 3UL; }
                        for (k = 0UL; k < c; ++k) { p *= 5UL; }
                        for (k = 0UL; k < d; ++k) { p *= 7UL; }
                        big = lcm_ul(big, p);
                        if (p > max) { max = p; }
                        ++cells;
                    }
                }
            }
        }
        printf("    cells built          %u\n", cells);
        printf("    largest cell period  %lu\n", max);
        printf("    lcm of all cells     %lu\n", big);
        printf("    TET_WHEEL in source  15120\n");
        printf("    agree: %s\n", (big == 15120UL && max == 15120UL) ? "yes" : "NO");
    }
    return 0;
}
