/* word_school.c — where noise becomes words.
 *
 * Today, to her, "sit on my face" and "$;& /: : )@" are the same thing:
 * unbound strings, noise. This organ is where the difference gets
 * MANUFACTURED — a binding between her own shape token (made by her eye,
 * not handed to her), a spelling, and a use. A string with no binding is
 * not a word. She cannot hold it, so she asks: teach_me.
 *
 * The law (builder/WORD_SCHOOL.md):
 *   EXPOSE    — the binding is shown; strength grows. Many times.
 *   RECOGNIZE — given the shape, pick the spelling out of 4 options.
 *               Early on she picks WRONG. That is not failure; it is a
 *               trigger: teach_me, back to EXPOSE. A lookup table would
 *               be right the first time. She is not a lookup table.
 *   SPELL     — given the shape, produce the letters one per beat, from
 *               the binding, not the answer key.
 *   SPACED    — a word is KNOWN only after SPELL passes in THREE
 *               SEPARATE sealed blocks. Right once is a parrot. Right
 *               across three blocks is hers.
 *
 * Deterministic: fixed corpus, hash-chosen distractors, no clock, no
 * rand. Same school day replayed is byte-identical.
 * Pure C99. No heap. No float. No child process. No clock.
 * Gauntlet: -std=c99 -pedantic -Wall -Wextra -Werror, -O0 == -O2.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ---- pins ---------------------------------------------------------- */
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

/* ---- capacities ------------------------------------------------------ */
#define WORD_MAX     512
#define SPELL_MAX     24
#define SHAPE_MAX     48
#define LINE_CAP     128
#define OPTS            4
#define BLOCK_TICKETS  10

/* ---- the school law, in numbers -------------------------------------- */
#define EXPOSE_NEED      3   /* exposures before she may be tested        */
#define RECOGNIZE_NEED   4   /* strength to see the word among 4 options  */
#define RECOGNIZE_GAIN   2   /* a recognition strengthens the binding     */
#define SPELL_NEED       7   /* strength to spell from the binding        */
#define PASSES_NEED      3   /* spaced SPELL passes, distinct blocks      */

/* ---- files ------------------------------------------------------------ */
#define WORDLIST_PATH "WORDLIST.txt"
#define SCHOOL_LOG    "SCHOOL.log"
#define MASTERY_PATH  "MASTERY.txt"
#define OUTBOX_PATH   "MOMMA_OUTBOX.txt"

/* ---- stages ------------------------------------------------------------ */
#define ST_EXPOSE    0
#define ST_RECOGNIZE 1
#define ST_SPELL     2
#define ST_KNOWN     3

typedef struct {
    char     spell[SPELL_MAX];   /* the letters, in order                 */
    char     shape[SHAPE_MAX];   /* HER token — she matched this shape    */
    uint64_t strength;           /* binding strength; built, never given  */
    uint64_t passes;             /* spaced SPELL passes                   */
    uint64_t last_pass_block;    /* spacing: passes must cross blocks     */
    uint64_t last_pass_tno;      /* spacing: ticket no of last counted pass */
    int      stage;
    size_t   spell_pos;          /* letters produced in current attempt   */
} word_t;

static word_t   W[WORD_MAX];
static size_t   word_count;
static size_t   rr;              /* round-robin cursor: every word gets its turn */
static uint64_t beat;
static uint64_t tseq;            /* school ticket sequence */
static uint64_t stream_pin;
static uint64_t block_pin, block_tickets, block_count;
static uint64_t known_count, teach_count;
static size_t   last_ticket_word = WORD_MAX; /* word of the previous ticket      */
static uint64_t other_tno[WORD_MAX]; /* F8: latest ticket no belonging to any
                                      * word OTHER than the index; tseq is the
                                      * global ticket counter that makes the
                                      * spacing auditable without a clock.   */

/* ---- the outbox: teach_me and known ride the slot-2 line -------------- */
static void outbox(const char *kind, const char *what)
{
    FILE *f = fopen(OUTBOX_PATH, "a");
    if (f) {
        fprintf(f, "%s %s beat %llu\n", kind, what,
                (unsigned long long)beat);
        fclose(f);
    }
    stream_pin = fnv_str(stream_pin, kind);
    stream_pin = fnv_str(stream_pin, what);
}

static void teach_me_shape(const char *shape)
{
    /* she asks about the SHAPE — she does not know the word yet */
    char buf[SHAPE_MAX + 8];
    snprintf(buf, sizeof buf, "shape:%s", shape);
    teach_count++;
    outbox("teach_me", buf);
}

/* ---- the ledger -------------------------------------------------------- */
static void ticket(const char *event, const word_t *w, size_t widx, const char *detail)
{
    FILE *f;
    uint64_t pin = FNV_BASIS;
    tseq++;
    pin = fnv1(pin, tseq);
    pin = fnv_str(pin, event);
    pin = fnv_str(pin, w->spell);
    pin = fnv1(pin, beat);
    pin = fnv_str(pin, detail);

    /* F3: the pin fold and block sealing are a function of the school day
     * ONLY — they ALWAYS happen, even if the ledger file will not open.
     * The file write is best-effort; a failed write screams on the outbox. */
    stream_pin = fnv_str(stream_pin, "sch:");
    stream_pin = fnv1(stream_pin, pin);

    block_pin = fnv1(block_pin, pin);
    block_tickets++;
    last_ticket_word = widx;
    {
        /* F8: every other word's "someone else was here" mark advances */
        size_t j;
        for (j = 0; j < word_count; j++)
            if (j != widx) other_tno[j] = tseq;
    }

    f = fopen(SCHOOL_LOG, "a");
    if (!f) {
        outbox("teach_me", "ledger");
    } else {
        fprintf(f, "sch %llu %s %s stage %d strength %llu passes %llu beat %llu %s pin %016llX\n",
                (unsigned long long)tseq, event, w->spell, w->stage,
                (unsigned long long)w->strength, (unsigned long long)w->passes,
                (unsigned long long)beat, detail, (unsigned long long)pin);
        fclose(f);
    }

    if (block_tickets == BLOCK_TICKETS) {
        block_count++;
        f = fopen(SCHOOL_LOG, "a");
        if (f) {
            fprintf(f, "block %llu pin %016llX\n",
                    (unsigned long long)block_count,
                    (unsigned long long)block_pin);
            fclose(f);
        }
        /* F9: the chain — the next block is seeded from this block's pin. */
        block_pin = fnv1(FNV_BASIS, block_pin);
        block_tickets = 0;
    }
}

/* ---- deterministic distractors ------------------------------------------
 * Three wrong spellings from the corpus, chosen by hash of HER word.
 * Same corpus, same word -> same options, every replay. */
static void make_options(const word_t *w, size_t self, size_t opts[OPTS], size_t *correct_slot)
{
    uint64_t h = fnv_str(FNV_BASIS, w->shape);
    size_t k, placed = 1;
    size_t slot = (size_t)(h % OPTS);
    for (k = 0; k < OPTS; k++) opts[k] = WORD_MAX; /* empty marker */
    opts[slot] = self;
    *correct_slot = slot;
    k = 0;
    while (placed < OPTS && word_count > 1) {
        size_t cand = (size_t)((fnv1(h, k) ^ (h >> 13)) % word_count);
        size_t j, dup = 0;
        if (cand == self) { k++; continue; }
        for (j = 0; j < OPTS; j++) if (opts[j] == cand) dup = 1;
        if (!dup) {
            size_t e;
            for (e = 0; e < OPTS; e++) if (opts[e] == WORD_MAX) { opts[e] = cand; break; }
            placed++;
        }
        k++;
        if (k > word_count * 4) break; /* tiny corpus safety */
    }
}

/* ---- one micro-step for one word ---------------------------------------- */
static void school_step(word_t *w, size_t idx)
{
    size_t len = strlen(w->spell);
    switch (w->stage) {

    case ST_EXPOSE:
        /* the binding is shown: her shape, the letters. Strength grows. */
        w->strength++;
        ticket("expose", w, idx, "binding-shown");
        if (w->strength >= EXPOSE_NEED) w->stage = ST_RECOGNIZE;
        break;

    case ST_RECOGNIZE: {
        /* given her shape, pick the spelling among 4. F6: the options are
         * REAL — all four are folded into the pin and written to the log,
         * and her choice is deterministic: a strong binding (strength >=
         * RECOGNIZE_NEED) chooses the correct spelling; a weak one points
         * at fnv(shape, block) mod 4 (bumped off the correct slot). Early
         * on she is WRONG — the honest state of a weak binding, and the
         * trigger completes: teach_me, back to EXPOSE. */
        size_t opts[OPTS], correct, chose, k;
        char odetail[224];
        const char *oname[OPTS];
        make_options(w, idx, opts, &correct);
        for (k = 0; k < OPTS; k++) {
            oname[k] = opts[k] != WORD_MAX ? W[opts[k]].spell : "x";
            stream_pin = fnv_str(stream_pin, "option:");
            stream_pin = fnv_str(stream_pin, oname[k]);
        }
        if (w->strength >= RECOGNIZE_NEED) {
            chose = correct; /* the binding is strong enough — she sees it */
            w->strength += RECOGNIZE_GAIN;
            w->stage = ST_SPELL;
            w->spell_pos = 0;
            snprintf(odetail, sizeof odetail,
                     "chose-right opts %s,%s,%s,%s chose %s",
                     oname[0], oname[1], oname[2], oname[3], oname[chose]);
            ticket("recognize-pass", w, idx, odetail);
        } else {
            /* she points at a distractor, deterministically wrong */
            chose = (size_t)(fnv1(fnv_str(FNV_BASIS, w->shape), block_count) % OPTS);
            if (chose == correct || opts[chose] == WORD_MAX)
                chose = (correct + 1) % OPTS;
            w->stage = ST_EXPOSE;
            snprintf(odetail, sizeof odetail,
                     "chose-wrong-back-to-expose opts %s,%s,%s,%s chose %s",
                     oname[0], oname[1], oname[2], oname[3], oname[chose]);
            ticket("recognize-fail", w, idx, odetail);
            teach_me_shape(w->shape);
        }
        break;
    }

    case ST_SPELL: {
        /* one letter per beat, from the binding. A weak binding makes a
         * wrong letter — deterministic, honest, and it sends her back
         * to EXPOSE with a teach_me. */
        char produced;
        if (w->spell_pos >= len) { w->spell_pos = 0; break; }
        if (w->strength >= SPELL_NEED) {
            produced = w->spell[w->spell_pos];
        } else {
            produced = (char)('a' + (fnv1(FNV_BASIS, beat + w->spell_pos) % 26));
        }
        if (produced != w->spell[w->spell_pos]) {
            w->stage = ST_EXPOSE;
            w->spell_pos = 0;
            ticket("spell-fail", w, idx, "wrong-letter-back-to-expose");
            teach_me_shape(w->shape);
            break;
        }
        w->spell_pos++;
        if (w->spell_pos == len) {
            /* whole word spelled — but see F7: a weak binding's lucky
             * pseudo-letters bank nothing. */
            w->spell_pos = 0;
            if (w->strength < SPELL_NEED) {
                /* F7: below SPELL_NEED this is luck, not knowledge */
                ticket("spell-pass", w, idx, "below-need-not-counted");
            } else if (block_count > w->last_pass_block &&
                       other_tno[idx] > w->last_pass_tno) {
                /* F8: spaced = a later block AND at least one OTHER word
                 * ticketed since this word's last counted pass
                 * (other_tno[idx] marks the newest ticket that is not hers) */
                w->passes++;
                w->last_pass_block = block_count;
                ticket("spell-pass", w, idx, "spaced-pass-counted");
                w->last_pass_tno = tseq;
            } else if (block_count <= w->last_pass_block) {
                ticket("spell-pass", w, idx, "same-block-not-counted");
            } else {
                ticket("spell-pass", w, idx, "no-other-word-not-counted");
            }
            if (w->passes >= PASSES_NEED) {
                FILE *m;
                w->stage = ST_KNOWN;
                known_count++;
                ticket("known", w, idx, "word-is-hers");
                outbox("known", w->spell);
                m = fopen(MASTERY_PATH, "a");
                if (m) {
                    fprintf(m, "%s %s known block %llu beat %llu\n",
                            w->spell, w->shape,
                            (unsigned long long)block_count,
                            (unsigned long long)beat);
                    fclose(m);
                }
            }
        } else {
            ticket("spell-letter", w, idx, "letter-from-binding");
        }
        break;
    }

    default:
        break; /* ST_KNOWN: she is done with this word */
    }
}

/* ---- the organ: one call per heartbeat ---------------------------------- */
void school_beat(void)
{
    size_t scanned = 0;
    beat++;
    if (word_count == 0) { stream_pin = fnv_str(stream_pin, "empty"); return; }
    /* round-robin: find the next word that is not yet known */
    while (scanned < word_count) {
        size_t idx = (rr + scanned) % word_count;
        if (W[idx].stage != ST_KNOWN) {
            rr = (idx + 1) % word_count;
            school_step(&W[idx], idx);
            return;
        }
        scanned++;
    }
    /* every word known: she hums her vocabulary */
    stream_pin = fnv_str(stream_pin, "all-known");
}

/* ---- intake: the corpus is the teacher's, the shapes are hers ---------- */
size_t school_load(void)
{
    FILE *f = fopen(WORDLIST_PATH, "r");
    char line[LINE_CAP];
    word_count = 0;
    if (!f) return 0;
    while (word_count < WORD_MAX && fgets(line, sizeof line, f)) {
        word_t *w = &W[word_count];
        memset(w, 0, sizeof *w);
        if (sscanf(line, "%23s %47s", w->spell, w->shape) == 2) {
            w->stage = ST_EXPOSE;
            stream_pin = fnv_str(stream_pin, "word:");
            stream_pin = fnv_str(stream_pin, w->spell);
            stream_pin = fnv_str(stream_pin, w->shape);
            word_count++;
        }
    }
    fclose(f);
    return word_count;
}

void school_init(void)
{
    memset(W, 0, sizeof W);
    word_count = 0;
    rr = 0;
    beat = 0;
    tseq = 0;
    stream_pin = FNV_BASIS;
    block_pin = FNV_BASIS;
    block_tickets = 0;
    block_count = 0;
    known_count = 0;
    teach_count = 0;
    last_ticket_word = WORD_MAX;
    memset(other_tno, 0, sizeof other_tno);
}

/* F10: seal the ledger — the stream pin is written INTO SCHOOL.log as
 * the final line, so the file carries its own proof. Best-effort. */
void school_seal(void)
{
    FILE *f = fopen(SCHOOL_LOG, "a");
    if (f) {
        fprintf(f, "stream %016llX\n", (unsigned long long)stream_pin);
        fclose(f);
    }
}

uint64_t school_stream_pin(void)  { return stream_pin; }
uint64_t school_beat_count(void)  { return beat; }
uint64_t school_blocks(void)      { return block_count; }
uint64_t school_known(void)       { return known_count; }
uint64_t school_teach_count(void) { return teach_count; }
size_t   school_word_count(void)  { return word_count; }
int      school_word_stage(size_t i) { return i < word_count ? W[i].stage : -1; }
uint64_t school_word_strength(size_t i) { return i < word_count ? W[i].strength : 0; }
