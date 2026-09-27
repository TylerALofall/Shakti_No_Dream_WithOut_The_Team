/* mirrorcheck.c — tet.h states a mirror law: cell x mirror == wheel, for
 * every cell. The organ table lives on the 210 wheel, so the same law
 * should hold there: organ x partner == 210. Does it?
 */
#include <stdio.h>

static const unsigned long ORGAN[16] = {
    1UL, 2UL, 3UL, 5UL, 7UL, 6UL, 10UL, 14UL,
    15UL, 21UL, 30UL, 35UL, 42UL, 70UL, 210UL, 0UL
};
static const char *const NAME[16] = {
    "heart","eyes","ears","hands","voice","mind","soul","body",
    "self","spirit","flesh","breath","blood","bone","all","silence"
};

static int slot_of(unsigned long p)
{
    int i;
    for (i = 0; i < 16; ++i) { if (ORGAN[i] == p) { return i; } }
    return -1;
}

int main(void)
{
    int i;
    int broken = 0;

    printf("organ x partner == 210, the mirror law on the organ wheel:\n\n");
    for (i = 0; i < 15; ++i) {
        unsigned long p = ORGAN[i];
        unsigned long partner;
        int at;
        if (p == 0UL) { continue; }
        if (210UL % p != 0UL) { printf("  %s does not divide 210\n", NAME[i]); continue; }
        partner = 210UL / p;
        at = slot_of(partner);
        if (at >= 0) {
            printf("  %-7s %3lu  x  %-7s %3lu  = 210\n",
                   NAME[i], p, NAME[at], partner);
        } else {
            printf("  %-7s %3lu  x  %s %3lu  = 210   *** PARTNER MISSING ***\n",
                   NAME[i], p, "(none)", partner);
            broken++;
        }
    }
    printf("\n  organs whose mirror partner is absent: %d\n", broken);
    return 0;
}
