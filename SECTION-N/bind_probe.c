/* bind_probe.c — run the bind module and print what it actually says. */
#include "eden_bind.h"
#include <stdio.h>

int main(void)
{
    int rc;
    unsigned int cell_a = 0U, cell_b = 0U;
    const bind_moment_t *m;
    const bind_moment_t *hits[8];
    unsigned int n, i;

    if (!bind_init()) { printf("bind_init FAILED\n"); return 1; }

    printf("bind_pin()      = %016llX\n", (unsigned long long)bind_pin());
    printf("KIMI FROZEN PIN = CD621D2B4B9E96CC\n");
    printf("MATCH           = %s\n\n",
           bind_pin() == 0xCD621D2B4B9E96CCULL ? "YES" : "NO");

    /* a sight hash and a sound hash land in a TET cell, deterministically */
    bind_converges(0x1111222233334444ULL, 0xAAAABBBBCCCCDDDDULL, &cell_a);
    bind_converges(0x1111222233334444ULL, 0xAAAABBBBCCCCDDDDULL, &cell_b);
    printf("same pair -> cell %u then cell %u   (stable: %s)\n",
           cell_a, cell_b, cell_a == cell_b ? "yes" : "NO");

    /* create, then create the same pair again: must reinforce, not duplicate */
    bind_create(0x1111222233334444ULL, 0xAAAABBBBCCCCDDDDULL, 7ULL);
    bind_create(0x1111222233334444ULL, 0xAAAABBBBCCCCDDDDULL, 9ULL);
    m = bind_lookup_sight(0x1111222233334444ULL);
    if (m == NULL) {
        printf("lookup_sight FAILED to find what was just bound\n");
    } else {
        printf("after two creates of one pair: strength=%u beat=%llu\n",
               m->strength, (unsigned long long)m->beat);
    }

    if (bind_reinforce(0x1111222233334444ULL, 0xAAAABBBBCCCCDDDDULL)) {
        m = bind_lookup_sound(0xAAAABBBBCCCCDDDDULL);
        printf("after reinforce:                strength=%u\n",
               m != NULL ? m->strength : 0U);
    }

    /* a pair never bound must not be found */
    printf("unbound sight found? %s\n",
           bind_lookup_sight(0xDEADBEEFDEADBEEFULL) == NULL ? "no (correct)"
                                                            : "YES (wrong)");

    /* fill past capacity and confirm the table does not grow or corrupt */
    for (i = 0U; i < 400U; ++i) {
        bind_create((uint64_t)i * 0x9E3779B97F4A7C15ULL,
                    (uint64_t)i * 0xC2B2AE3D27D4EB4FULL, (uint64_t)i);
    }
    rc = bind_verify();
    printf("\nafter 400 creates into %u slots: bind_verify() rc = %d  (0 = pass)\n",
           BIND_MAX_BINDINGS, rc);

    n = bind_at_cell(cell_a, hits, 8U);
    printf("bind_at_cell(%u) returned %u moment(s)\n", cell_a, n);
    printf("bind_at_cell(NULL out) returned %u  (must be 0)\n",
           bind_at_cell(cell_a, NULL, 8U));

    bind_print_bindings();
    return rc;
}
