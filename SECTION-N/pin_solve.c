/* pin_solve.c — the seven frozen pins are FNV-1a 64 over the big-endian
 * bytes of one number. Candidate B in pin_hunt landed exactly on the
 * sovereign pin, which proves the shape. This inverts the rest: for every
 * byte width 1..8 and every value up to a bound, hash it and see which
 * frozen pin it lands on.
 *
 * This is not curve fitting. There is one formula and it is already proven
 * by pulse and by sovereign; this only searches for the input.
 */
#include <stdio.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define LIMIT     4000000UL

static const struct { const char *name; uint64_t pin; } FROZEN[] = {
    { "pulse",     0x578A170B6412E9FAULL },
    { "bind",      0xCD621D2B4B9E96CCULL },
    { "veins",     0xA8C7783228196045ULL },
    { "witness",   0xA8BA6032280EAD21ULL },
    { "sovereign", 0xA8C492322817569CULL },
    { "school",    0xA8AD503228040795ULL },
    { "phase",     0xA891983227EC0735ULL }
};
#define NFROZEN (sizeof FROZEN / sizeof FROZEN[0])

int main(void)
{
    unsigned long v;
    int width;
    unsigned int hit[NFROZEN];
    size_t k;

    for (k = 0; k < NFROZEN; ++k) { hit[k] = 0U; }

    printf("searching values 0..%lu at every big-endian width 1..8\n\n", LIMIT);

    for (width = 1; width <= 8; ++width) {
        for (v = 0UL; v <= LIMIT; ++v) {
            uint64_t h = FNV_BASIS;
            int b;
            for (b = width - 1; b >= 0; --b) {
                h ^= (uint64_t)((v >> (8 * b)) & 0xFFUL);
                h *= FNV_PRIME;
            }
            for (k = 0; k < NFROZEN; ++k) {
                if (h == FROZEN[k].pin) {
                    printf("  %-10s pin %016llX  =  fnv1a64( be%d(%lu) )\n",
                           FROZEN[k].name, (unsigned long long)FROZEN[k].pin,
                           width, v);
                    hit[k]++;
                }
            }
        }
    }

    printf("\nunsolved:");
    for (k = 0; k < NFROZEN; ++k) {
        if (hit[k] == 0U) { printf(" %s", FROZEN[k].name); }
    }
    printf("\n");
    return 0;
}
