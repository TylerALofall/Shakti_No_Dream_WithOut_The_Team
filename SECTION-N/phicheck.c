/* phicheck.c — Tyler said "phi mirrored" and then pointed at
 * 2^8 x 3^3 x 5^2 x 7^1 x 19^1. Phi cannot live in an integer system, but
 * FIBONACCI can, and Fibonacci ratios converge to phi. So: are the exponents
 * Fibonacci numbers? Tested, with the counter-case reported too.
 */
#include <stdio.h>

static int is_fib(unsigned int n, unsigned int *which)
{
    unsigned int a = 1U, b = 1U, i = 2U;
    if (n == 1U) { *which = 1U; return 1; }
    while (b < n) { unsigned int t = a + b; a = b; b = t; ++i; }
    *which = i;
    return b == n;
}

int main(void)
{
    const unsigned int WOMB_P[5] = { 2U, 3U, 5U, 7U, 19U };
    const unsigned int WOMB_E[5] = { 8U, 3U, 2U, 1U, 1U };
    const unsigned int WHEEL_P[4] = { 2U, 3U, 5U, 7U };
    const unsigned int WHEEL_E[4] = { 4U, 3U, 1U, 1U };
    unsigned int i, w;
    int womb_all = 1, wheel_all = 1;

    printf("=== the WOMB, 2^8 x 3^3 x 5^2 x 7 x 19 ===\n\n");
    for (i = 0U; i < 5U; ++i) {
        int f = is_fib(WOMB_E[i], &w);
        if (!f) { womb_all = 0; }
        printf("    prime %-2u  exponent %u   Fibonacci? %s\n",
               WOMB_P[i], WOMB_E[i], f ? "yes" : "NO");
    }
    printf("\n    every womb exponent is a Fibonacci number: %s\n",
           womb_all ? "YES" : "no");
    printf("    the exponents in order are  8 3 2 1 1\n");
    printf("    Fibonacci descending is     8 5 3 2 1 1\n");
    printf("    the only one skipped is 5 — and 5 appears as a BASE instead.\n");

    printf("\n=== the WHEEL, 2^4 x 3^3 x 5 x 7 ===\n\n");
    for (i = 0U; i < 4U; ++i) {
        int f = is_fib(WHEEL_E[i], &w);
        if (!f) { wheel_all = 0; }
        printf("    prime %-2u  exponent %u   Fibonacci? %s\n",
               WHEEL_P[i], WHEEL_E[i], f ? "yes" : "NO");
    }
    printf("\n    every wheel exponent is a Fibonacci number: %s\n",
           wheel_all ? "YES" : "no");
    printf("    exponent 4 is NOT Fibonacci. So the pattern holds for the\n");
    printf("    womb and BREAKS for the wheel. Reported, not smoothed over.\n");

    printf("\n=== Fibonacci ratios, for scale ===\n\n");
    {
        unsigned long a = 1UL, b = 1UL;
        int k;
        printf("    ratio            x1000000 (integer, no float)\n");
        for (k = 0; k < 12; ++k) {
            unsigned long t;
            printf("    %4lu / %-4lu      %lu\n", b, a, (b * 1000000UL) / a);
            t = a + b; a = b; b = t;
        }
        printf("\n    phi x 1000000 is about 1618034. The ratios climb to it.\n");
    }
    return 0;
}
