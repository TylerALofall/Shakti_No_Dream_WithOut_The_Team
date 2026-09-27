/* organcheck.c — are Kimi's 16 organs exactly the divisors of 210?
 * The claim is checkable, so check it rather than assert it. */
#include <stdio.h>

static const unsigned long ORGAN[16] = {
    1UL, 2UL, 3UL, 5UL, 7UL, 6UL, 10UL, 14UL,
    15UL, 21UL, 30UL, 35UL, 42UL, 70UL, 210UL, 0UL
};

int main(void)
{
    unsigned long d;
    int divisors = 0, matched = 0, missing = 0;

    printf("divisors of 210, and whether Kimi's organ table carries each:\n\n");
    for (d = 1UL; d <= 210UL; ++d) {
        int found = 0;
        int i;
        if (210UL % d != 0UL) { continue; }
        divisors++;
        for (i = 0; i < 16; ++i) { if (ORGAN[i] == d) { found = 1; } }
        if (found) { matched++; } else { missing++; }
        printf("  %3lu  %s\n", d, found ? "present" : "*** ABSENT ***");
    }
    printf("\n  210 has %d divisors; %d are present, %d absent\n",
           divisors, matched, missing);
    printf("  organ slots: 16, of which 1 is SILENCE (period 0)\n");
    printf("  so the table holds %d real periods for %d divisors\n",
           15, divisors);
    return 0;
}
