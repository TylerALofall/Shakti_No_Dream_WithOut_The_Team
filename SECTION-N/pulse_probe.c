/* pulse_probe.c — run the pulse module and print what it actually says.
 * Not a test harness with expectations baked in; it prints the machine's
 * own answers so the pin can be compared to Kimi's frozen value by eye. */
#include "eden_pulse.h"
#include <stdio.h>

int main(void)
{
    int rc;
    unsigned int i;
    pulse_firing_t f;

    if (!pulse_init()) { printf("pulse_init FAILED\n"); return 1; }

    rc = pulse_verify();
    printf("pulse_verify() rc = %d   (0 = pass)\n", rc);
    printf("pulse_pin()        = %016llX\n", (unsigned long long)pulse_pin());
    printf("KIMI FROZEN PIN    = 578A170B6412E9FA\n");
    printf("MATCH              = %s\n",
           pulse_pin() == 0x578A170B6412E9FAULL ? "YES" : "NO");

    printf("\norgan schedule, beats 0..15:\n");
    for (i = 0U; i < 16U; ++i) {
        printf("  beat %2u -> %-8s mesh=%u\n", i,
               PULSE_ORGAN_NAMES[pulse_organ_at((uint64_t)i)],
               pulse_mesh_count((uint64_t)i));
    }

    if (pulse_firing_at(0ULL, &f)) {
        printf("\nfiring at beat 0: count=%u depth=%lu\n", f.count, f.depth);
    }
    if (pulse_firing_at(210ULL, &f)) {
        printf("firing at beat 210: count=%u\n", f.count);
    }
    pulse_print_state();
    return rc;
}
