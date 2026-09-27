/* pin_hunt.c — bind's compute_pin() as written does not reproduce the pin
 * frozen in its own header comment. This prints every structurally plausible
 * variant so the disagreement can be judged instead of guessed at.
 *
 * "Structurally plausible" means: it mirrors how eden_pulse.c computes its
 * pin (8-byte big-endian per value, FNV-1a 64), or it is the code as pasted.
 * Nothing here is tuned to hit a target. If none match, none match.
 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define BIND_MAX_BINDINGS 256U
#define BIND_HASH_BYTES     8U
#define BIND_MOMENT_BYTES  16U
#define TET_CELLS          80U
#define TET_WHEEL       15120UL

static uint64_t fnv(uint64_t h, const unsigned char *d, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) { h ^= (uint64_t)d[i]; h *= FNV_PRIME; }
    return h;
}

static void be8(unsigned char *o, uint64_t v)
{
    int k;
    for (k = 0; k < 8; ++k) { o[k] = (unsigned char)((v >> (56 - 8 * k)) & 0xFFULL); }
}

static void be4(unsigned char *o, uint64_t v)
{
    int k;
    for (k = 0; k < 4; ++k) { o[k] = (unsigned char)((v >> (24 - 8 * k)) & 0xFFULL); }
}

static void show(const char *what, uint64_t got)
{
    printf("  %-58s %016llX  %s\n", what, (unsigned long long)got,
           got == 0xCD621D2B4B9E96CCULL ? "<== MATCHES FROZEN PIN" : "");
}

int main(void)
{
    unsigned char b[8];
    uint64_t h;

    printf("bind frozen pin (from Kimi's own header comment): CD621D2B4B9E96CC\n\n");

    /* A: exactly as pasted — 4 big-endian bytes of BIND_MAX_BINDINGS */
    be4(b, BIND_MAX_BINDINGS); show("A  be4(MAX_BINDINGS), len 4   [the code as pasted]",
                                    fnv(FNV_BASIS, b, 4));

    /* B: 8 big-endian bytes — the shape eden_pulse.c uses, and the shape the
     *    unused `unsigned char be[8]` declaration hints the original had */
    be8(b, BIND_MAX_BINDINGS); show("B  be8(MAX_BINDINGS), len 8",
                                    fnv(FNV_BASIS, b, 8));

    /* C: the three header constants, 8 bytes each */
    h = FNV_BASIS;
    be8(b, BIND_MAX_BINDINGS); h = fnv(h, b, 8);
    be8(b, BIND_HASH_BYTES);   h = fnv(h, b, 8);
    be8(b, BIND_MOMENT_BYTES); h = fnv(h, b, 8);
    show("C  be8(MAX, HASH_BYTES, MOMENT_BYTES)", h);

    /* D: the three header constants, 4 bytes each */
    h = FNV_BASIS;
    be4(b, BIND_MAX_BINDINGS); h = fnv(h, b, 4);
    be4(b, BIND_HASH_BYTES);   h = fnv(h, b, 4);
    be4(b, BIND_MOMENT_BYTES); h = fnv(h, b, 4);
    show("D  be4(MAX, HASH_BYTES, MOMENT_BYTES)", h);

    /* E: capacity bound to the geometry it lives in */
    h = FNV_BASIS;
    be8(b, BIND_MAX_BINDINGS); h = fnv(h, b, 8);
    be8(b, TET_CELLS);         h = fnv(h, b, 8);
    show("E  be8(MAX_BINDINGS, TET_CELLS)", h);

    h = FNV_BASIS;
    be8(b, BIND_MAX_BINDINGS); h = fnv(h, b, 8);
    be8(b, TET_CELLS);         h = fnv(h, b, 8);
    be8(b, TET_WHEEL);         h = fnv(h, b, 8);
    show("F  be8(MAX_BINDINGS, TET_CELLS, TET_WHEEL)", h);

    /* G: the literal name, the way a manifest hashes a path */
    show("G  fnv over the ASCII text \"eden_bind\"",
         fnv(FNV_BASIS, (const unsigned char *)"eden_bind", 9));
    show("H  fnv over the ASCII text \"bind\"",
         fnv(FNV_BASIS, (const unsigned char *)"bind", 4));

    printf("\nIf no line above matches, the frozen pin cannot be reproduced\n"
           "from the constants in the file, and one of the two is wrong.\n");
    return 0;
}
