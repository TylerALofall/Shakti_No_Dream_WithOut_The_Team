/* womb.c — Tyler: "Look to 2^8 x 3^3 x 5^2 x 7 x 19"
 *
 * Compute it, factor what it touches, and see what is already locked in
 * tet.h. Integer arithmetic only. Nothing asserted that is not printed.
 */
#include <stdio.h>

#define TET_WHEEL       15120UL
#define TET_WOMB     22982400UL
#define TET_SLICE_GRID 302400UL
#define TET_PYTHAGOREAN   432UL
#define TET_JUST           35UL

static unsigned long ipow(unsigned long b, unsigned int e)
{
    unsigned long v = 1UL;
    unsigned int i;
    for (i = 0U; i < e; ++i) { v *= b; }
    return v;
}

int main(void)
{
    unsigned long n;
    unsigned long d;
    unsigned long divisors = 0UL;
    unsigned long best_lo = 0UL, best_hi = 0UL;

    n = ipow(2UL,8U) * ipow(3UL,3U) * ipow(5UL,2U) * 7UL * 19UL;

    printf("=== 2^8 x 3^3 x 5^2 x 7 x 19 ===\n\n");
    printf("    = %lu\n", n);
    printf("    TET_WOMB in the locked tet.h = %lu\n", TET_WOMB);
    printf("    SAME NUMBER: %s\n\n", n == TET_WOMB ? "YES" : "NO");

    printf("=== where it comes from ===\n\n");
    printf("    seconds in a day        86400 = 2^7 x 3^3 x 5^2\n");
    printf("    gestation days            266 = 2 x 7 x 19\n");
    printf("    86400 x 266             = %lu   %s\n",
           86400UL * 266UL, (86400UL * 266UL == n) ? "= the womb" : "MISMATCH");
    printf("    266 days as weeks         266 / 7 = %lu weeks exactly\n",
           266UL / 7UL);
    printf("    so 19 enters through the gestation, not through the wheel.\n");
    printf("    the wheel's primes are 2 3 5 7. The womb adds ONE new prime: 19.\n\n");

    printf("=== how it tiles ===\n\n");
    printf("    womb / wheel        = %lu  remainder %lu\n",
           n / TET_WHEEL, n % TET_WHEEL);
    printf("    slice grid / wheel  = %lu  remainder %lu\n",
           TET_SLICE_GRID / TET_WHEEL, TET_SLICE_GRID % TET_WHEEL);
    printf("    tet.h claims 1520 and 20. agree: %s\n\n",
           (n / TET_WHEEL == 1520UL && TET_SLICE_GRID / TET_WHEEL == 20UL)
               ? "YES" : "NO");

    printf("=== how many cells does the womb lattice have ===\n\n");
    printf("    exponents        8   3   2   1   1\n");
    printf("    exponent+1       9 x 4 x 3 x 2 x 2 = %lu\n",
           9UL * 4UL * 3UL * 2UL * 2UL);
    for (d = 1UL; d <= n; ++d) {
        if (d * d > n) { break; }
        if (n % d == 0UL) { divisors += 2UL; best_lo = d; }
    }
    if (best_lo * best_lo == n) { divisors -= 1UL; }
    best_hi = n / best_lo;
    printf("    counted by division: %lu divisors\n", divisors);
    printf("    TET_PYTHAGOREAN in tet.h = %lu\n", TET_PYTHAGOREAN);
    printf("    the womb has exactly as many divisors as the Pythagorean\n");
    printf("    half of the wheel is large: %s\n\n",
           divisors == TET_PYTHAGOREAN ? "YES" : "NO");

    printf("=== the mirror axis of the womb ===\n\n");
    printf("    innermost divisor pair: %lu x %lu = %lu\n",
           best_lo, best_hi, best_lo * best_hi);
    printf("    they differ by %lu\n", best_hi - best_lo);
    printf("    %lu = 2^6 x 3 x 5^2      %lu = 2^2 x 3^2 x 7 x 19\n",
           best_hi, best_lo);
    printf("    one side carries the day, the other carries the gestation.\n");
    return 0;
}
