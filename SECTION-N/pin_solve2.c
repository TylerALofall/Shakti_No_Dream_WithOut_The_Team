/* pin_solve2.c — bind is the one pin that neither reproduces from its own
 * pasted code nor from the one-value family formula. This asks whether it is
 * a TWO value pin: fnv1a64 over be8(a) then be8(b).
 *
 * Bounded to 0..16384 for both. If it is not in that box, say so; do not
 * widen the box until something falls out of it.
 */
#include <stdio.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define BOUND     16384UL
#define TARGET    0xCD621D2B4B9E96CCULL

static uint64_t fold_be8(uint64_t h, unsigned long v)
{
    int b;
    for (b = 7; b >= 0; --b) {
        h ^= (uint64_t)((v >> (8 * b)) & 0xFFUL);
        h *= FNV_PRIME;
    }
    return h;
}

static uint64_t fold_be4(uint64_t h, unsigned long v)
{
    int b;
    for (b = 3; b >= 0; --b) {
        h ^= (uint64_t)((v >> (8 * b)) & 0xFFUL);
        h *= FNV_PRIME;
    }
    return h;
}

int main(void)
{
    unsigned long a, b;
    int found = 0;

    printf("target (bind frozen pin): %016llX\n", (unsigned long long)TARGET);
    printf("searching be8(a)+be8(b) and be4(a)+be4(b), a,b in 0..%lu\n\n", BOUND);

    for (a = 0UL; a <= BOUND; ++a) {
        uint64_t h8 = fold_be8(FNV_BASIS, a);
        uint64_t h4 = fold_be4(FNV_BASIS, a);
        for (b = 0UL; b <= BOUND; ++b) {
            if (fold_be8(h8, b) == TARGET) {
                printf("  MATCH  fnv1a64( be8(%lu), be8(%lu) )\n", a, b);
                found = 1;
            }
            if (fold_be4(h4, b) == TARGET) {
                printf("  MATCH  fnv1a64( be4(%lu), be4(%lu) )\n", a, b);
                found = 1;
            }
        }
    }

    if (!found) {
        printf("  no two-value formula in that box produces the bind pin.\n");
    }
    return 0;
}
