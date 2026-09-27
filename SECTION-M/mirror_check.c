/* mirror_check.c — is the second path the mirror of the first?
 * binary road: 1 2 4 8 16 (rungs of 2 on the wheel)
 * golden road: 1 2 3 5 8  (Fibonacci rungs)
 * mirror law (locked tet.h): cell * mirror == 15120 for all 80 cells
 * C99, integers only, no float, no heap. */
#include <stdio.h>

static unsigned long ipow(unsigned long b, unsigned int e) {
    unsigned long v = 1UL; unsigned int i;
    for (i = 0U; i < e; ++i) { v *= b; }
    return v;
}

int main(void) {
    unsigned long wheel = 15120UL;
    unsigned int i;

    printf("=== mirror of each binary rung (cell * mirror = 15120) ===\n");
    for (i = 0U; i <= 4U; ++i) {
        unsigned long rung = ipow(2UL, i);
        printf("  2^%u = %-5lu  mirror = %-5lu  check %lu\n",
               i, rung, wheel / rung, rung * (wheel / rung));
    }

    printf("\n=== is the golden rung set closed under mirroring? ===\n");
    {
        unsigned long fib[6] = {1UL, 2UL, 3UL, 5UL, 8UL, 13UL};
        for (i = 0U; i < 6U; ++i) {
            if (wheel % fib[i] == 0UL) {
                printf("  fib %-3lu divides wheel, mirror = %lu\n",
                       fib[i], wheel / fib[i]);
            } else {
                printf("  fib %-3lu is NOT a cell of the wheel\n", fib[i]);
            }
        }
    }

    printf("\n=== the two roads, mirrored against each other ===\n");
    {
        unsigned long bin[5] = {1UL, 2UL, 4UL, 8UL, 16UL};
        unsigned long fib[6] = {1UL, 2UL, 3UL, 5UL, 8UL, 13UL};
        unsigned int j;
        printf("  shared rungs: ");
        for (i = 0U; i < 5U; ++i) {
            for (j = 0U; j < 6U; ++j) {
                if (bin[i] == fib[j]) { printf("%lu ", bin[i]); }
            }
        }
        printf("\n  binary-only rung: 4   golden-only rungs: 3 5 13\n");
        printf("  13 is a cell? %s (15120 %% 13 = %lu)\n",
               wheel % 13UL == 0UL ? "yes" : "NO", wheel % 13UL);
    }
    return 0;
}
