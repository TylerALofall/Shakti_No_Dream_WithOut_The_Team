/* mirroraxis.c — Tyler: "it's mirrored from the heart, it's not just perfect
 * rhythm, it's perfect mirrored."
 *
 * If every organ pairs as p x partner == 210, then the pairing is a
 * reflection, and a reflection has an axis. Where is it, and what sits on it?
 * Integer arithmetic only; no float, no square root function.
 */
#include <stdio.h>

static const unsigned long ORGAN[15] = {
    1UL,2UL,3UL,5UL,7UL,6UL,10UL,14UL,15UL,21UL,30UL,35UL,42UL,70UL,210UL
};
static const char *const NAME[15] = {
    "heart","eyes","ears","hands","voice","mind","soul","body",
    "self","spirit","flesh","breath","blood","bone","all"
};

static const char *name_of(unsigned long p)
{
    int i;
    for (i = 0; i < 15; ++i) { if (ORGAN[i] == p) { return NAME[i]; } }
    return "(absent)";
}

int main(void)
{
    unsigned long p;
    int consecutive = 0;

    printf("=== every pair of consecutive integers whose product is 210 ===\n\n");
    for (p = 1UL; p * (p + 1UL) <= 210UL; ++p) {
        if (p * (p + 1UL) == 210UL) {
            printf("    %lu x %lu = 210     -> %s x %s\n",
                   p, p + 1UL, name_of(p), name_of(p + 1UL));
            consecutive++;
        }
    }
    printf("\n    consecutive pairs found: %d\n", consecutive);
    printf("    That pair straddles the mirror axis: nothing sits closer to\n");
    printf("    the centre, because no integer squares to 210.\n");

    printf("\n=== the pairs, ordered outward from the axis ===\n\n");
    printf("    %-8s %4s     %-8s %4s\n", "inner", "", "outer", "");
    for (p = 14UL; p >= 1UL; --p) {
        unsigned long q;
        if (210UL % p != 0UL) { continue; }
        q = 210UL / p;
        printf("    %-8s %4lu  x  %-8s %4lu  = 210%s\n",
               name_of(p), p, name_of(q), q,
               (name_of(q)[0] == '(' || name_of(p)[0] == '(')
                   ? "   *** one side missing ***" : "");
    }

    printf("\n    heart is period 1: the outermost point of the mirror.\n");
    printf("    Its partner is all, period 210 — the whole wheel.\n");
    printf("    heart x all = 210. The reflection is anchored ON the heart.\n");
    return 0;
}
