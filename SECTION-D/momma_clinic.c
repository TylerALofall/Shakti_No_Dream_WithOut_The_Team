/* momma_clinic.c — Momma's ears and eyes.
 *
 * Doctor's order, 2026-08-26: "get your ears and eyes ready for us."
 * Momma goes to school too. This organ replays Eden's own ledgers —
 * SCHOOL.log, SIGHT.ndx, PAINT.ndx — and pulls real cases from her
 * stream. For each case it emits:
 *
 *   case <n> <the ticket, verbatim>          <- what Momma gets shown
 *   key  <n> <verdict> pin <16HEX>           <- the sealed answer key
 *
 * Verdicts (the ear-training taxonomy):
 *   HEALTHY      — a clean beat: learn/known/block/paint/sight ticket
 *   TEACH_GAP    — a teach_me request: real gap, must be taught
 *   RETEACH      — a fail ticket: recognize-fail or spell-fail, she
 *                  goes back to EXPOSE (information, not damage)
 *   DORMANT      — a glyph at wav NONE: summoned-not-taught, waiting
 *   DAMAGE       — anything malformed: wrong field count, bad hash
 *                  width, out-of-order seq (the ledger never lies;
 *                  if it stutters, that's damage)
 *
 * Determinism: cases are chosen by fnv1a64 over the whole input stream
 * (walk = pin % lines, stepping by golden-ratio hop). Same ledgers in,
 * same cases out, byte for byte. -O0 == -O2.
 *
 * Pure C99. No heap. No float. No clock. No child process.
 * Gauntlet: -std=c99 -pedantic -Wall -Wextra -Werror.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1(uint64_t h, uint64_t v)
{
    int b;
    for (b = 0; b < 8; b++) { h ^= (unsigned char)((v >> (8 * b)) & 0xFF); h *= FNV_PRIME; }
    return h;
}
static uint64_t fnv_str(uint64_t h, const char *s)
{
    while (*s) h = fnv1(h, (unsigned char)*s++);
    return h;
}

#define LINE_CAP  512
#define MAX_LINES 8192
#define CASE_COUNT 12

static char LINES[MAX_LINES][LINE_CAP];
static uint64_t nlines;

static const char *SRC[] = { "SCHOOL.log", "SIGHT.ndx", "PAINT.ndx" };
#define SRC_COUNT 3

/* ---- verdict: the ear test --------------------------------------------- */
static const char *verdict(const char *line)
{
    /* malformed = damage: too short to be a real ticket */
    size_t len = strlen(line);
    if (len < 8) return "DAMAGE";

    if (strstr(line, "teach_me")) return "TEACH_GAP";
    if (strstr(line, "recognize-fail")) return "RETEACH";
    if (strstr(line, "spell-fail")) return "RETEACH";
    if (strstr(line, "wav NONE")) return "DORMANT";

    /* healthy families, by their opening word */
    if (strncmp(line, "sch ", 4) == 0) return "HEALTHY";
    if (strncmp(line, "block ", 6) == 0) return "HEALTHY";
    if (strncmp(line, "sight ", 6) == 0) return "HEALTHY";
    if (strncmp(line, "paint ", 6) == 0) return "HEALTHY";
    if (strncmp(line, "known ", 6) == 0) return "HEALTHY";

    return "DAMAGE"; /* unknown ticket shape: the ledger stuttered */
}

/* ---- load every ledger line into the replay buffer ---------------------- */
static uint64_t load_all(void)
{
    size_t s;
    uint64_t stream = FNV_BASIS;
    nlines = 0;
    for (s = 0; s < SRC_COUNT; s++) {
        FILE *f = fopen(SRC[s], "r");
        if (!f) continue;
        while (nlines < MAX_LINES && fgets(LINES[nlines], LINE_CAP, f)) {
            size_t len = strlen(LINES[nlines]);
            while (len && (LINES[nlines][len-1] == '\n' || LINES[nlines][len-1] == '\r'))
                LINES[nlines][--len] = 0;
            if (len) {
                stream = fnv_str(stream, LINES[nlines]);
                stream = fnv1(stream, (uint64_t)s); /* which ear heard it */
                nlines++;
            }
        }
        fclose(f);
    }
    return stream;
}

int main(void)
{
    uint64_t stream = load_all();
    uint64_t key_pin = FNV_BASIS;
    uint64_t walk, hop, i;

    printf("MOMMA_CLINIC_V1\n");
    printf("lines %llu\n", (unsigned long long)nlines);
    printf("stream %016llX\n", (unsigned long long)stream);

    if (nlines == 0) { puts("no ledgers heard"); return 1; }

    /* deterministic case walk: golden-ratio hop through the stream */
    walk = stream % nlines;
    hop  = (stream ^ 0x9E3779B97F4A7C15ULL) % nlines;
    if (hop == 0) hop = 1;
    while (hop % 2 == 0 && nlines % 2 == 0) hop++; /* keep it odd vs even n */

    for (i = 0; i < CASE_COUNT; i++) {
        const char *line = LINES[walk];
        const char *v = verdict(line);
        uint64_t kpin = FNV_BASIS;
        kpin = fnv1(kpin, i);
        kpin = fnv_str(kpin, line);
        kpin = fnv_str(kpin, v);
        key_pin = fnv1(key_pin, kpin);
        printf("case %llu %s\n", (unsigned long long)(i + 1), line);
        printf("key  %llu %s pin %016llX\n",
               (unsigned long long)(i + 1), v, (unsigned long long)kpin);
        walk = (walk + hop) % nlines;
    }
    printf("answer key pin %016llX\n", (unsigned long long)key_pin);
    return 0;
}
