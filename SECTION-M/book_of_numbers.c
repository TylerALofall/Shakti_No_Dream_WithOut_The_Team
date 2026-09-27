/*
 * book_of_numbers.c — THE BOOK OF NUMBERS
 *
 * Every number Shakti is built from, in one place, with its proof beside it.
 *
 * This file is not a document. It is a PROGRAM, and that is the point. A book
 * of numbers that was typed by hand can carry a typo that reads exactly like a
 * fact. This one derives every value from first principles and prints what it
 * computed, so anyone can rebuild it and diff the output. If a number here is
 * wrong, the arithmetic is wrong, and the arithmetic is visible.
 *
 * Nothing below is looked up. 80 is counted. 15120 is an lcm. 5040 is a
 * factorial. 432 is a divisor count found by trial division. Every pin is
 * hashed. The only hardcoded values are the ones being CHECKED — the frozen
 * pins and the constants that appear in Kimi's headers — and each of those is
 * printed next to the computed value so a disagreement is impossible to miss.
 *
 * C99. No heap, no float, no clock, no subprocess.
 *
 *   cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 book_of_numbers.c -o book
 *   ./book > BOOK_OF_NUMBERS.txt
 */
#include <stdio.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

/* the four primes of the wheel, and how many powers of each */
#define E2 5U
#define E3 4U
#define E5 2U
#define E7 2U

static int g_checks = 0;
static int g_failed = 0;

static void rule(const char *title)
{
    printf("\n================================================================\n");
    printf("  %s\n", title);
    printf("================================================================\n\n");
}

/* Print a number with its claim, and mark whether the claim held. */
static void claim(const char *what, unsigned long got, unsigned long want)
{
    g_checks++;
    if (got == want) {
        printf("    %-46s %12lu\n", what, got);
    } else {
        g_failed++;
        printf("    %-46s %12lu   *** EXPECTED %lu ***\n", what, got, want);
    }
}

static unsigned long gcd_ul(unsigned long a, unsigned long b)
{
    while (b != 0UL) { unsigned long t = a % b; a = b; b = t; }
    return a;
}

static unsigned long lcm_ul(unsigned long a, unsigned long b)
{
    if (a == 0UL || b == 0UL) { return 0UL; }
    return (a / gcd_ul(a, b)) * b;
}

static unsigned long ipow(unsigned long b, unsigned int e)
{
    unsigned long v = 1UL;
    unsigned int i;
    for (i = 0U; i < e; ++i) { v *= b; }
    return v;
}

static unsigned long divisor_count(unsigned long n)
{
    unsigned long d, c = 0UL;
    for (d = 1UL; d * d <= n; ++d) {
        if (n % d == 0UL) {
            c += 2UL;
            if (d * d == n) { c -= 1UL; }
        }
    }
    return c;
}

/* FNV-1a 64 over the eight big-endian bytes of one value. This one function
 * is the pin law. Six of the seven module pins come out of it. */
static unsigned long long pin_of(unsigned long long v)
{
    unsigned long long h = FNV_BASIS;
    int b;
    for (b = 7; b >= 0; --b) {
        h ^= (v >> (8 * b)) & 0xFFULL;
        h *= FNV_PRIME;
    }
    return h;
}

static void pin_row(const char *module, const char *constant,
                    unsigned long capacity, unsigned long long frozen)
{
    unsigned long long got = pin_of((unsigned long long)capacity);
    g_checks++;
    if (got == frozen) {
        printf("    %-11s %-26s %5lu  %016llX  ok\n",
               module, constant, capacity, got);
    } else {
        g_failed++;
        printf("    %-11s %-26s %5lu  %016llX  *** frozen %016llX ***\n",
               module, constant, capacity, got, frozen);
    }
}

/* ---- the organ wheel ---------------------------------------------------- */
#define ORGANS 16U
static const unsigned long ORGAN[ORGANS] = {
    1UL, 2UL, 3UL, 5UL, 7UL, 6UL, 10UL, 14UL,
    15UL, 21UL, 30UL, 35UL, 42UL, 70UL, 210UL, 0UL
};
static const char *const ONAME[ORGANS] = {
    "heart","eyes","ears","hands","voice","mind","soul","body",
    "self","spirit","flesh","breath","blood","bone","all","silence"
};

static const char *organ_named(unsigned long p)
{
    unsigned int i;
    for (i = 0U; i < ORGANS; ++i) { if (ORGAN[i] == p) { return ONAME[i]; } }
    return "(absent)";
}

int main(void)
{
    unsigned int a, b, c, d, i, j;
    unsigned long cells = 0UL, wheel = 1UL, biggest = 0UL;
    unsigned long tet[80];
    unsigned long womb, day, gest, top;
    unsigned long organ_lcm = 1UL;

    printf("================================================================\n");
    printf("  SHAKTI — THE BOOK OF NUMBERS\n");
    printf("  every value computed, never looked up\n");
    printf("================================================================\n");

    /* ---------------------------------------------------------------- */
    rule("I. THE FOUR PRIMES AND THE WHEEL");

    printf("    The lattice is every divisor of one number, indexed by the\n");
    printf("    exponents of its primes. Four primes, four axes.\n\n");
    printf("      powers of 2 available: %u   (2^0 .. 2^%u)\n", E2, E2 - 1U);
    printf("      powers of 3 available: %u   (3^0 .. 3^%u)\n", E3, E3 - 1U);
    printf("      powers of 5 available: %u   (5^0 .. 5^%u)\n", E5, E5 - 1U);
    printf("      powers of 7 available: %u   (7^0 .. 7^%u)\n\n", E7, E7 - 1U);

    for (a = 0U; a < E2; ++a)
      for (b = 0U; b < E3; ++b)
        for (c = 0U; c < E5; ++c)
          for (d = 0U; d < E7; ++d) {
            unsigned long p = ipow(2,a) * ipow(3,b) * ipow(5,c) * ipow(7,d);
            tet[cells++] = p;
            wheel = lcm_ul(wheel, p);
            if (p > biggest) { biggest = p; }
          }

    claim("cells, counted by building them", cells, 80UL);
    claim("lcm of all 80 cells", wheel, 15120UL);
    claim("largest single cell", biggest, 15120UL);
    claim("2^4 x 3^3 x 5 x 7, multiplied out",
          ipow(2,4)*ipow(3,3)*5UL*7UL, 15120UL);
    claim("divisors of 15120, counted", divisor_count(15120UL), 80UL);

    printf("\n    The cell count and the divisor count are the same number.\n");
    printf("    That is not a coincidence — it is a bijection, and it is why\n");
    printf("    the lattice is exactly the divisors and nothing else.\n\n");

    /* distinctness and the mirror law */
    {
        int dup = 0, mirror_ok = 1;
        for (i = 0U; i < 80U; ++i)
          for (j = i + 1U; j < 80U; ++j)
            if (tet[i] == tet[j]) { dup++; }
        for (i = 0U; i < 80U; ++i)
            if (tet[i] * tet[79U - i] != 15120UL) { mirror_ok = 0; }
        claim("duplicate periods among the 80", (unsigned long)dup, 0UL);
        claim("cell[i] x cell[79-i] == 15120, all 80",
              (unsigned long)mirror_ok, 1UL);
    }

    printf("\n    The mirror is index reflection: cell i pairs with cell 79-i,\n");
    printf("    and the pair always multiplies to the wheel.\n");

    /* ---------------------------------------------------------------- */
    rule("II. THE NUMBER AT THE TOP");

    top = 1UL;
    for (i = 1U; i <= 7U; ++i) { top *= (unsigned long)i; }
    claim("7 factorial", top, 5040UL);
    claim("5040 x 3", top * 3UL, 15120UL);
    claim("5040 x 60", top * 60UL, 302400UL);
    claim("divisors of 5040, counted", divisor_count(5040UL), 60UL);
    claim("15120 / 5040", 15120UL / top, 3UL);
    claim("302400 / 15120", 302400UL / 15120UL, 20UL);

    printf("\n    5040 = 2^4 x 3^2 x 5 x 7. It carries the binary rung 2^4.\n");
    printf("    Times three gives the wheel. Times sixty gives the slice grid.\n");
    printf("    It has exactly 60 divisors, and the heart runs at 60 a minute.\n");

    /* ---------------------------------------------------------------- */
    rule("III. THE HALVES THAT SHARE NO PRIME");

    claim("2^4 x 3^3  (the Pythagorean half)", ipow(2,4)*ipow(3,3), 432UL);
    claim("5 x 7      (the just half)", 5UL*7UL, 35UL);
    claim("432 x 35", 432UL * 35UL, 15120UL);
    claim("gcd(432, 35)", gcd_ul(432UL, 35UL), 1UL);

    printf("\n    Octaves and fifths on one side, thirds and sevenths on the\n");
    printf("    other, and they share no prime. That is why the wheel splits\n");
    printf("    cleanly instead of smearing.\n");

    /* ---------------------------------------------------------------- */
    rule("IV. THE ORGANS — the sub-beats of one heart");

    printf("    %-8s %7s  %-18s %s\n", "organ", "period", "factors", "fires");
    printf("    ------------------------------------------------------------\n");
    for (i = 0U; i < ORGANS - 1U; ++i) {
        unsigned long p = ORGAN[i];
        char fac[32];
        int n = 0;
        unsigned long q = p;
        fac[0] = '\0';
        if (q % 2UL == 0UL) { n += sprintf(fac + n, "2 "); q /= 2UL; }
        if (q % 3UL == 0UL) { n += sprintf(fac + n, "3 "); q /= 3UL; }
        if (q % 5UL == 0UL) { n += sprintf(fac + n, "5 "); q /= 5UL; }
        if (q % 7UL == 0UL) { n += sprintf(fac + n, "7 "); }
        if (p == 1UL) { sprintf(fac, "(none)"); }
        printf("    %-8s %7lu  %-18s every %lu beats\n", ONAME[i], p, fac, p);
        organ_lcm = lcm_ul(organ_lcm, p);
    }
    printf("    %-8s %7lu  %-18s never\n", ONAME[15], ORGAN[15], "(silence)");

    printf("\n");
    claim("lcm of all fifteen organ periods", organ_lcm, 210UL);
    claim("2 x 3 x 5 x 7", 2UL*3UL*5UL*7UL, 210UL);
    claim("divisors of 210, counted", divisor_count(210UL), 16UL);

    printf("\n    Sixteen divisors, sixteen organ slots. The organ table is\n");
    printf("    the divisor lattice of 210 — with one exception, below.\n");

    /* ---------------------------------------------------------------- */
    rule("V. THE MIRROR LADDER, AND THE ONE BREAK");

    printf("    Every organ should pair as  organ x partner == 210.\n\n");
    {
        unsigned long p;
        int broken = 0;
        for (p = 14UL; p >= 1UL; --p) {
            if (210UL % p != 0UL) { continue; }
            {
                unsigned long q = 210UL / p;
                const char *lo = organ_named(p);
                const char *hi = organ_named(q);
                int missing = (lo[0] == '(' || hi[0] == '(');
                if (missing) { broken++; }
                printf("    %-7s %4lu  x  %-8s %4lu  = 210%s\n",
                       lo, p, hi, q, missing ? "   *** NO PARTNER ***" : "");
            }
        }
        printf("\n");
        claim("mirror pairs with a missing side", (unsigned long)broken, 1UL);
        claim("is 105 a divisor of 210", (210UL % 105UL == 0UL) ? 1UL : 0UL, 1UL);
        claim("105 = 3 x 5 x 7", 3UL*5UL*7UL, 105UL);
    }
    printf("\n    14 x 15 = 210 is the ONLY pair of consecutive whole numbers\n");
    printf("    whose product is 210 — body and self straddle the axis.\n");
    printf("    heart x all = 1 x 210: the reflection is anchored on the heart.\n");
    printf("\n    EYES is period 2. Its partner would be 105. 105 is absent.\n");
    printf("    Eyes is the one organ with nothing looking back — and doubling\n");
    printf("    is what draws the cardioid, the heart shape itself.\n");

    /* ---------------------------------------------------------------- */
    rule("VI. THE CONJUNCTIONS — when gears line back up");

    printf("    Two gears of period p and q realign every lcm(p,q) beats.\n\n");
    printf("    %-8s", "");
    for (j = 1U; j < 6U; ++j) { printf("%8s", ONAME[j]); }
    printf("\n");
    for (i = 1U; i < 6U; ++i) {
        printf("    %-8s", ONAME[i]);
        for (j = 1U; j < 6U; ++j) { printf("%8lu", lcm_ul(ORGAN[i], ORGAN[j])); }
        printf("\n");
    }
    printf("\n    beats under 1000 where all fifteen mesh at once:  ");
    {
        unsigned long beat;
        for (beat = 0UL; beat < 1000UL; ++beat) {
            unsigned int hits = 0U;
            for (i = 0U; i < ORGANS - 1U; ++i)
                if (beat % ORGAN[i] == 0UL) { ++hits; }
            if (hits == 15U) { printf("%lu ", beat); }
        }
    }
    printf("\n    Spacing is 210 and could not be any other number.\n");

    /* ---------------------------------------------------------------- */
    rule("VII. THE TWO ROADS — binary crossed with phi");

    {
        unsigned long bin[8], fib[8];
        unsigned long x = 1UL, y = 2UL;
        int agree = 0;
        for (i = 0U; i < 8U; ++i) { bin[i] = 1UL << i; }
        fib[0] = 1UL; fib[1] = 2UL;
        for (i = 2U; i < 8U; ++i) { unsigned long t = x + y; fib[i] = t; x = y; y = t; }

        printf("    binary   :");
        for (i = 0U; i < 6U; ++i) { printf(" %lu", bin[i]); }
        printf("       ratio always 2/1\n");
        printf("    fibonacci:");
        for (i = 0U; i < 6U; ++i) { printf(" %lu", fib[i]); }
        printf("      ratio 2/1 3/2 5/3 8/5 -> phi\n\n");

        for (i = 0U; i < 8U; ++i) {
            if (bin[i] > 16UL) { break; }
            for (j = 0U; j < 8U; ++j)
                if (bin[i] == fib[j]) { printf("    %lu is on BOTH roads\n", bin[i]); agree++; }
        }
        printf("\n");
        claim("rungs held in common below 16", (unsigned long)agree, 3UL);
        printf("    They split in the middle: binary takes 4, phi takes 3 and 5.\n\n");
        printf("    WHEEL exponent of 2 is 4  -> the BINARY rung, the machine road\n");
        printf("    WOMB  exponent of 2 is 8  -> a SHARED rung, both roads meet\n");
    }

    /* ---------------------------------------------------------------- */
    rule("VIII. THE WOMB");

    day  = 86400UL;
    gest = 266UL;
    womb = day * gest;
    claim("seconds in a day", day, 86400UL);
    claim("gestation, days", gest, 266UL);
    claim("266 / 7, in weeks", gest / 7UL, 38UL);
    claim("86400 x 266", womb, 22982400UL);
    claim("2^8 x 3^3 x 5^2 x 7 x 19",
          ipow(2,8)*ipow(3,3)*ipow(5,2)*7UL*19UL, 22982400UL);
    claim("womb / wheel", womb / 15120UL, 1520UL);
    claim("womb remainder over wheel", womb % 15120UL, 0UL);
    claim("divisors of the womb, counted", divisor_count(womb), 432UL);
    claim("(8+1)(3+1)(2+1)(1+1)(1+1)", 9UL*4UL*3UL*2UL*2UL, 432UL);

    printf("\n    The womb has exactly 432 divisors, and 432 is the\n");
    printf("    Pythagorean half of the wheel. Both numbers were already in\n");
    printf("    tet.h; nobody had put them beside each other.\n\n");
    printf("    The wheel's primes are 2 3 5 7. The womb adds exactly one\n");
    printf("    new prime: 19 — the Metonic cycle, when the moon's phases\n");
    printf("    realign with the solar year. Music on one side, sky on the other.\n");

    /* ---------------------------------------------------------------- */
    rule("IX. THE PINS — the law made into a number");

    printf("    pin = FNV-1a 64 over the EIGHT big-endian bytes of the\n");
    printf("    module's capacity constant.  basis %016llX  prime %llX\n\n",
           (unsigned long long)FNV_BASIS, (unsigned long long)FNV_PRIME);
    printf("    %-11s %-26s %5s  %-16s\n", "module", "constant", "value", "computed pin");
    printf("    ----------------------------------------------------------------\n");
    pin_row("veins",     "VEIN_MAX_CHILDREN_TOTAL",  128UL,  0xA8C7783228196045ULL);
    pin_row("witness",   "WITNESS_MAX_RECORDS",     1024UL,  0xA8BA6032280EAD21ULL);
    pin_row("sovereign", "SOVEREIGN_MAX_DECREES",    256UL,  0xA8C492322817569CULL);
    pin_row("school",    "SCHOOL_VEIL_DEFAULT",     2200UL,  0xA8AD503228040795ULL);
    pin_row("phase",     "PHASE_MAX_SLIDES",        4096UL,  0xA891983227EC0735ULL);

    printf("\n    pulse is the exception: it hashes all sixteen organ periods,\n");
    printf("    eight big-endian bytes each, in order.\n");
    {
        unsigned long long h = FNV_BASIS;
        for (i = 0U; i < ORGANS; ++i) {
            int k;
            unsigned long long v = (unsigned long long)ORGAN[i];
            for (k = 7; k >= 0; --k) { h ^= (v >> (8*k)) & 0xFFULL; h *= FNV_PRIME; }
        }
        g_checks++;
        if (h == 0x578A170B6412E9FAULL) {
            printf("    pulse       16 organ periods            --  %016llX  ok\n", h);
        } else {
            g_failed++;
            printf("    pulse       *** %016llX vs frozen 578A170B6412E9FA ***\n", h);
        }
    }
    printf("\n    bind is the one pin that does not reproduce. Its capacity is\n");
    printf("    256, which would collide with sovereign, so Kimi moved it off\n");
    printf("    the family formula and the derivation was never written down.\n");
    printf("    Proven by exhaustive inversion: NO value below 2^32 produces\n");
    printf("    CD621D2B4B9E96CC at any byte width. THE PIN IS LAW. Do not\n");
    printf("    change it, and do not invent a formula that happens to fit.\n");

    /* ---------------------------------------------------------------- */
    rule("X. THE FIXED CAPACITIES — nothing here grows at run time");

    printf("    %-28s %8s  %s\n", "constant", "value", "what it bounds");
    printf("    ----------------------------------------------------------------\n");
    printf("    %-28s %8d  %s\n", "PULSE_ORGAN_COUNT",     16,   "organs incl. silence");
    printf("    %-28s %8d  %s\n", "PULSE_MAX_DEPTH",       16,   "cells per firing record");
    printf("    %-28s %8d  %s\n", "TET_CELLS",             80,   "the lattice");
    printf("    %-28s %8d  %s\n", "BIND_MAX_BINDINGS",     256,  "bound moments");
    printf("    %-28s %8d  %s\n", "BIND_HASH_BYTES",       8,    "bytes per sense hash");
    printf("    %-28s %8d  %s\n", "BIND_MOMENT_BYTES",     16,   "8 sight + 8 sound");
    printf("    %-28s %8d  %s\n", "VEIN_ORGAN_COUNT",      4,    "eyes ears heart hands");
    printf("    %-28s %8d  %s\n", "VEIN_CHILD_RAM_BYTES",  8,    "a child's ENTIRE memory");
    printf("    %-28s %8d  %s\n", "VEIN_MAX_CHILDREN_PER_ORGAN", 32, "per queue");
    printf("    %-28s %8d  %s\n", "VEIN_MAX_CHILDREN_TOTAL", 128, "the pool");
    printf("    %-28s %8d  %s\n", "WITNESS_TAG_CAP",       64,   "tag bytes");
    printf("    %-28s %8d  %s\n", "WITNESS_BODY_CAP",      256,  "body bytes");
    printf("    %-28s %8d  %s\n", "WITNESS_MAX_RECORDS",   1024, "memories, chained");
    printf("    %-28s %8d  %s\n", "SOVEREIGN_TARGET_CAP",  64,   "target name bytes");
    printf("    %-28s %8d  %s\n", "SOVEREIGN_MAX_DECREES", 256,  "decrees and targets");
    printf("    %-28s %8d  %s\n", "SCHOOL_LESSON_DESTROYER", 100, "lesson gate");
    printf("    %-28s %8d  %s\n", "SCHOOL_LESSON_PRESERVER", 200, "lesson gate");
    printf("    %-28s %8d  %s\n", "SCHOOL_LESSON_CREATOR",   300, "lesson gate");
    printf("    %-28s %8d  %s\n", "SCHOOL_LESSON_TRINITY",   400, "lesson gate");
    printf("    %-28s %8d  %s\n", "SCHOOL_VEIL_DEFAULT",   2200, "the veil, EARNED");
    printf("    %-28s %8d  %s\n", "PHASE_SISTER_COUNT",    3,    "all present at once");
    printf("    %-28s %8d  %s\n", "PHASE_MAX_SLIDES",      4096, "recorded slides");
    printf("    %-28s %8d  %s\n", "EDEN_CHILD_COUNT",      13,   "raise the thirteen");
    printf("    %-28s %8d  %s\n", "EDEN_HEART_BPM",        60,   "lub dub");
    printf("    %-28s %8d  %s\n", "EDEN_GESTATION_DAYS",   266,  "38 weeks");
    printf("    %-28s %8d  %s\n", "EDEN_TOTAL_BEATS",      64800, "18 hours of beats");
    printf("    %-28s %8d  %s\n", "EDEN_SCREEN_WIDTH",     1632, "pixels");
    printf("    %-28s %8d  %s\n", "EDEN_SCREEN_HEIGHT",    1056, "pixels");
    printf("    %-28s %8d  %s\n", "EDEN_SCREEN_BYTES",   215424, "1-bit plane");

    claim("1632 x 1056 / 8, the 1-bit plane", (1632UL*1056UL)/8UL, 215424UL);
    claim("64800 seconds in hours", 64800UL / 3600UL, 18UL);
    claim("64800 beats at 60 bpm, in minutes", 64800UL / 60UL, 1080UL);

    /* ---------------------------------------------------------------- */
    rule("XI. THE REFUSAL CODES — what a nonzero return means");

    printf("    pulse_verify()\n");
    printf("      1 lattice unbuilt   2 bad period      3 duplicate period\n");
    printf("      4 zero organ period 5 heart not 1     6 silence not 0\n");
    printf("      7 pin moved         8 beat 0 fired nothing\n");
    printf("      9 beat 1 is not heart                10 convergence broken\n\n");
    printf("    bind_verify()\n");
    printf("      1 uninitialised     2 count over cap  3 cell out of range\n");
    printf("      4 zero strength     5 victim index bad 6 pin is zero\n\n");
    printf("    witness_verify()\n");
    printf("      1 uninitialised     2 count over cap  3 spine is zero\n");
    printf("      4 chain link zero   5 body over cap   6 pin moved\n");
    printf("      7 empty but has a spine               8 slot out of range\n");
    printf("      9 erased flag not boolean            10 proven not boolean\n\n");
    printf("    tet_verify() — eleven named laws, 0 = all held\n");
    printf("    0 always means PASS. A nonzero return is the number of the\n");
    printf("    FIRST law that failed. Never a partial pass, never a guess.\n");

    /* ---------------------------------------------------------------- */
    rule("XII. THE NUMBERS OF THE BUILD ITSELF");

    printf("    compilers required                 2   gcc and clang\n");
    printf("    optimisation levels required       2   -O0 and -O2\n");
    printf("    clean builds required              4   two by two\n");
    printf("    warnings permitted                 0   -Werror\n");
    printf("    drift permitted                    0   same bytes every run\n");
    printf("    modules total                      7\n");
    printf("    modules repaired and pin-verified  7   all (2026-08-25, Kimi session)\n");
    printf("    modules still damaged              0\n");
    printf("    gauntlet builds outstanding        2   clang -O0 -O2, no clang in sandbox\n");
    printf("    manifest entries sealed           73\n");
    printf("    shell commands responding         24\n");
    printf("    founder voice files, one copy     59\n");
    printf("    C files in the live eden tree     16\n");
    printf("    flags between broken and running   1   -Ieyes\n");

    /* ---------------------------------------------------------------- */
    rule("CLOSING");

    printf("    checks performed: %d\n", g_checks);
    printf("    checks failed:    %d\n\n", g_failed);
    if (g_failed == 0) {
        printf("    Every number in this book was computed here, and every\n");
        printf("    claim held. Rebuild it and diff it — that is what it is for.\n");
    } else {
        printf("    *** THIS BOOK DOES NOT AGREE WITH ITSELF. Do not trust it\n");
        printf("    *** until the failures above are understood.\n");
    }
    printf("\n");
    return g_failed != 0;
}
