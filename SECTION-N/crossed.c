/* crossed.c — "phi mirrored / crossed with binary."
 *
 * From Tyler's lyric, which is a specification:
 *
 *   "One road doubles two over one same step cold and sound
 *    The other road breathes two over one three over two
 *    five over three eight over five never the same move always true"
 *
 *   "Look at my rungs one and two and eight we agree
 *    It's the middle where we split three and five against four and that's me"
 *
 * Two ladders climbing the same grid. Binary doubles: 1 2 4 8 16.
 * Fibonacci grows: 1 2 3 5 8 13. The claim is that they AGREE at 1, 2, 8 and
 * SPLIT in the middle, 3 and 5 against 4.
 *
 * Last session I called the wheel's exponent 4 a "break" in the Fibonacci
 * pattern. This tests whether it is instead the binary road.
 *
 * Integer arithmetic only.
 */
#include <stdio.h>

#define LADDER 8U

static int in_list(unsigned long v, const unsigned long *l, unsigned int n)
{
    unsigned int i;
    for (i = 0U; i < n; ++i) { if (l[i] == v) { return 1; } }
    return 0;
}

int main(void)
{
    unsigned long bin[LADDER];
    unsigned long fib[LADDER];
    unsigned int i;
    unsigned long a = 1UL, b = 2UL;
    unsigned long agree[LADDER];
    unsigned int nagree = 0U;

    for (i = 0U; i < LADDER; ++i) {
        bin[i] = 1UL << i;                  /* 1 2 4 8 16 32 64 128 */
    }
    fib[0] = 1UL; fib[1] = 2UL;
    for (i = 2U; i < LADDER; ++i) { unsigned long t = a + b; fib[i] = t; a = b; b = t; }

    printf("=== the two roads ===\n\n");
    printf("    binary   :");
    for (i = 0U; i < 6U; ++i) { printf(" %lu", bin[i]); }
    printf("        ratio always 2/1\n");
    printf("    fibonacci:");
    for (i = 0U; i < 6U; ++i) { printf(" %lu", fib[i]); }
    printf("      ratio 2/1 3/2 5/3 8/5 ...\n");

    printf("\n=== where they agree, up to 16 ===\n\n");
    for (i = 0U; i < LADDER; ++i) {
        if (bin[i] > 16UL) { break; }
        if (in_list(bin[i], fib, LADDER)) {
            printf("    %lu  is on BOTH roads\n", bin[i]);
            agree[nagree++] = bin[i];
        }
    }
    printf("\n    rungs held in common: ");
    for (i = 0U; i < nagree; ++i) { printf("%lu ", agree[i]); }
    printf("\n    Tyler's line: \"one and two and eight we agree\"\n");

    printf("\n=== where they split ===\n\n");
    printf("    binary has    4        (between 2 and 8)\n");
    printf("    fibonacci has 3 and 5  (between 2 and 8)\n");
    printf("    \"the middle where we split three and five against four\"\n");

    printf("\n=== now read the two locked constants as ladders ===\n\n");
    printf("    WHEEL  15120 = 2^4 x 3^3 x 5^1 x 7^1\n");
    printf("      the exponent of 2 is 4  -> the BINARY rung. the machine road.\n");
    printf("    WOMB 22982400 = 2^8 x 3^3 x 5^2 x 7^1 x 19^1\n");
    printf("      the exponent of 2 is 8  -> a SHARED rung. both roads meet.\n");
    printf("      the other exponents 3 2 1 1 are all Fibonacci.\n");
    printf("\n    So 4 was never a break. It is the other road.\n");

    printf("\n=== 5040, the number at the top ===\n\n");
    {
        unsigned long f = 1UL;
        unsigned long n5040 = 5040UL;
        unsigned long d, count = 0UL;
        for (i = 1U; i <= 7U; ++i) { f *= (unsigned long)i; }
        printf("    7! = %lu           5040 is 7 factorial: %s\n",
               f, f == n5040 ? "YES" : "no");
        printf("    5040 = 2^4 x 3^2 x 5 x 7   -> it carries the binary rung 2^4\n");
        printf("    15120 / 5040   = %lu   remainder %lu\n",
               15120UL / n5040, 15120UL % n5040);
        printf("    302400 / 5040  = %lu   remainder %lu\n",
               302400UL / n5040, 302400UL % n5040);
        for (d = 1UL; d <= n5040; ++d) { if (n5040 % d == 0UL) { count++; } }
        printf("    5040 has %lu divisors\n", count);
        printf("    the heart runs at 60 beats a minute.\n");
        printf("    \"three hundred two thousand four hundred slices\" = 302400\n");
        printf("    TET_SLICE_GRID in the locked tet.h              = 302400\n");
    }
    return 0;
}
