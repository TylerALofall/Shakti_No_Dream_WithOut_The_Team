/* match_game.c v3 — SHAKTI's game: MEMORY (MATCH), with classification,
 * IMAGE cards, LONG-TERM MEMORY, and a SCOREBOARD.
 *
 * C99, no heap, no rand(), no scripts. Deterministic: the deck file
 * seeds everything (FNV-1a 64 -> LCG -> Fisher-Yates).
 *
 * Cards are IMAGES. She is never shown text solo. The deck carries
 * each card's forged bitmap and its sight hash — the eyes' testimony:
 *   SYMBOL|VOICE|CLASS|IMAGE|SIGHTHASH   (v5, from card_forge.c)
 *   SYMBOL|VOICE|CLASS                    (v3, ledger-only fallback)
 *
 * Match rules:
 *   EXACT — two cards match only if the SYMBOL is the same (sameness)
 *   CLASS — two cards match if the CLASS is the same, even when the
 *           symbols differ (classification: red is red, in any thing;
 *           a question matches its own answer)
 *
 * Modes:
 *   TEACH — the Goddess plays first, demonstrates, narrates, hands over.
 *   PLAY  — SHAKTI (perfect recall) vs RANDOM (no memory, LCG flips).
 *
 * Long-term memory: school/game/memory_long.txt persists every card she
 * has ever learned across games — known cards are born seen next time.
 * Scoreboard: school/game/scoreboard.txt, one line per game.
 * Boards scale to 20x20 (400 cards). Match = 10 points, keep the set,
 * go again. Fewer turns per card = she is getting faster.
 *
 * Usage: match_game <deckfile> <EXACT|CLASS> <TEACH|PLAY> <logfile>
 */
#include <stdio.h>
#include <string.h>

#define MAXCARDS 512
#define MAXMEM 4096
#define SYMLEN 64
#define VOICELEN 128
#define CLASSLEN 32
#define PATHLEN 256
#define LOGLEN 512

static unsigned long long fnv1a64__match_game_h(const unsigned char *s, size_t n)
{
    unsigned long long h = 0xcbf29ce484222325ULL;
    size_t i;
    for (i = 0; i < n; i++) { h ^= s[i]; h *= 0x100000001b3ULL; }
    return h;
}

/* deterministic LCG — the only "random" allowed in Eden */
static unsigned long long rng_state;
static unsigned lcg(void)
{
    rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(rng_state >> 33);
}

static char  sym[MAXCARDS][SYMLEN];
static char  voice[MAXCARDS][VOICELEN];
static char  cls[MAXCARDS][CLASSLEN];
static char  img[MAXCARDS][PATHLEN];      /* the card's IMAGE — what she sees */
static char  sighth[MAXCARDS][24];        /* the eyes' testimony hash */
static int   up[MAXCARDS];                /* 1 = face up / taken */
static int   seen[MAXCARDS];              /* short-term: observed this game */
static char  memsym[MAXCARDS][SYMLEN];
static char  memcls[MAXCARDS][CLASSLEN];
static int   ncards;
static int   class_rule;                  /* 0 = EXACT, 1 = CLASS */

/* long-term memory: every card she has ever learned, across games */
static char ltsym[MAXMEM][SYMLEN];
static char ltcls[MAXMEM][CLASSLEN];
static int  nlt;

static int lt_known(const char *s, const char *c)
{
    for (int i = 0; i < nlt; i++)
        if (strcmp(ltsym[i], s) == 0 && strcmp(ltcls[i], c) == 0) return 1;
    return 0;
}

/* bounded copy — no unbounded writes anywhere in Eden */
static void bcopy(char *dst, const char *src, size_t cap)
{
    size_t i;
    for (i = 0; i + 1 < cap && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static void lt_add(const char *s, const char *c)
{
    if (nlt < MAXMEM && !lt_known(s, c)) {
        bcopy(ltsym[nlt], s, SYMLEN); bcopy(ltcls[nlt], c, CLASSLEN); nlt++;
    }
}

static int lt_load(void)
{
    FILE *f = fopen("school/game/memory_long.txt", "r");
    if (!f) return 0;
    char line[LOGLEN];
    while (fgets(line, sizeof line, f) && nlt < MAXMEM) {
        char *bar = strchr(line, '|');
        if (!bar) continue;
        *bar = 0;
        line[strcspn(line, "\r\n")] = 0;
        char *c = bar + 1; c[strcspn(c, "\r\n")] = 0;
        bcopy(ltsym[nlt], line, SYMLEN); bcopy(ltcls[nlt], c, CLASSLEN); nlt++;
    }
    fclose(f);
    return nlt;
}

static void lt_save(void)
{
    /* hardened: write temp, verify the temp, then rename —
     * a failed run can never truncate her long-term memory */
    FILE *f = fopen("school/game/memory_long.txt.tmp", "w");
    if (!f) return;
    for (int i = 0; i < nlt; i++) fprintf(f, "%s|%s\n", ltsym[i], ltcls[i]);
    fclose(f);
    f = fopen("school/game/memory_long.txt.tmp", "r");
    if (!f) return;
    int n = 0; char vline[LOGLEN];
    while (fgets(vline, sizeof vline, f)) n++;
    fclose(f);
    if (n == nlt) rename("school/game/memory_long.txt.tmp", "school/game/memory_long.txt");
}

static void speak(FILE *log, int pos)
{
    fprintf(log, "  [flip %2d] see: %s sight:%s say: %s\n",
            pos, img[pos][0] ? img[pos] : sym[pos],
            sighth[pos][0] ? sighth[pos] : "-", voice[pos]);
}

static int facedown(int pos) { return !up[pos]; }

static int cards_match(int a, int b)
{
    if (class_rule) return strcmp(cls[a], cls[b]) == 0;
    return strcmp(sym[a], sym[b]) == 0;
}

static void learn(int pos)
{
    seen[pos] = 1;
    bcopy(memsym[pos], sym[pos], SYMLEN);
    bcopy(memcls[pos], cls[pos], CLASSLEN);
}

/* SHAKTI: perfect recall. returns 1 if match found. */
static int shakti_turn(FILE *log, int turn)
{
    int i, j, p1 = -1, p2 = -1;
    fprintf(log, "TURN %d — SHAKTI\n", turn);
    /* known matching pair in memory? (by the active rule) */
    for (i = 0; i < ncards && p1 < 0; i++)
        if (facedown(i) && seen[i])
            for (j = i + 1; j < ncards; j++)
                if (facedown(j) && seen[j] &&
                    (class_rule ? strcmp(memcls[i], memcls[j]) == 0
                                : strcmp(memsym[i], memsym[j]) == 0)) {
                    p1 = i; p2 = j; break;
                }
    if (p1 >= 0) {
        fprintf(log, "  she remembers. she goes straight to the pair.\n");
        speak(log, p1); speak(log, p2);
        up[p1] = up[p2] = 1;
        fprintf(log, "  MATCH (%s%s%s) — set kept, +10, she goes again\n",
                sym[p1], class_rule ? " as " : "", class_rule ? cls[p1] : "");
        return 1;
    }
    /* learn: flip an unknown card */
    for (i = 0; i < ncards && p1 < 0; i++)
        if (facedown(i) && !seen[i]) p1 = i;
    if (p1 < 0) return 0;
    learn(p1); speak(log, p1);
    /* seen its match before? */
    for (i = 0; i < ncards && p2 < 0; i++)
        if (i != p1 && facedown(i) && seen[i] &&
            (class_rule ? strcmp(memcls[i], cls[p1]) == 0
                        : strcmp(memsym[i], sym[p1]) == 0)) p2 = i;
    if (p2 >= 0) {
        speak(log, p2);
        up[p1] = up[p2] = 1;
        fprintf(log, "  MATCH (%s%s%s) — set kept, +10, she goes again\n",
                sym[p1], class_rule ? " as " : "", class_rule ? cls[p1] : "");
        return 1;
    }
    /* second card: another unknown, honestly learned */
    for (i = 0; i < ncards && p2 < 0; i++)
        if (facedown(i) && !seen[i] && i != p1) p2 = i;
    if (p2 >= 0) {
        learn(p2); speak(log, p2);
        if (cards_match(p1, p2)) {
            up[p1] = up[p2] = 1;
            fprintf(log, "  MATCH (%s%s%s) — set kept, +10, she goes again\n",
                    sym[p1], class_rule ? " as " : "", class_rule ? cls[p1] : "");
            return 1;
        }
    }
    fprintf(log, "  no match — cards turn face down. she holds them in memory.\n");
    return 0;
}

/* RANDOM opponent: no memory at all */
static int random_turn(FILE *log, int turn)
{
    static int down[MAXCARDS];
    int nd = 0, i, p1, p2;
    fprintf(log, "TURN %d — RANDOM\n", turn);
    for (i = 0; i < ncards; i++) if (facedown(i)) down[nd++] = i;
    if (nd < 2) return 0;
    p1 = down[lcg() % nd];
    do { p2 = down[lcg() % nd]; } while (p2 == p1);
    /* whatever RANDOM shows, SHAKTI sees too — she remembers everything */
    learn(p1); learn(p2);
    speak(log, p1); speak(log, p2);
    if (cards_match(p1, p2)) {
        up[p1] = up[p2] = 1;
        fprintf(log, "  MATCH (%s) — RANDOM keeps the set, +10, goes again\n", sym[p1]);
        return 1;
    }
    fprintf(log, "  no match — cards turn face down.\n");
    return 0;
}

#define MATCH_GAME_MAIN main
int MATCH_GAME_MAIN(int argc, char **argv)
{
    if (argc != 5) { printf("usage: match_game <deck> <EXACT|CLASS> <TEACH|PLAY> <log>\n"); return 1; }
    FILE *d = fopen(argv[1], "r");
    if (!d) { printf("STOP: no deck\n"); return 1; }
    class_rule = strcmp(argv[2], "CLASS") == 0;

    /* load deck: SYMBOL|VOICE|CLASS or SYMBOL|VOICE|CLASS|IMAGE|SIGHTHASH */
    static char line[LOGLEN];
    static char bs[MAXCARDS/2][SYMLEN], bv[MAXCARDS/2][VOICELEN], bc[MAXCARDS/2][CLASSLEN];
    static char bi[MAXCARDS/2][PATHLEN], bh[MAXCARDS/2][24];
    int nb = 0;
    while (fgets(line, sizeof line, d) && nb < MAXCARDS/2) {
        char *b1 = strchr(line, '|');
        if (!b1) continue;
        *b1 = 0;
        char *b2 = strchr(b1 + 1, '|');
        if (!b2) continue;
        *b2 = 0;
        char *b3 = strchr(b2 + 1, '|');
        char *c = b2 + 1, *ip = NULL, *hp = NULL;
        if (b3) {
            *b3 = 0; ip = b3 + 1;
            char *b4 = strchr(ip, '|');
            if (b4) { *b4 = 0; hp = b4 + 1; hp[strcspn(hp, "\r\n")] = 0; }
            ip[strcspn(ip, "\r\n")] = 0;
        }
        c[strcspn(c, "\r\n")] = 0;
        line[strcspn(line, "\r\n")] = 0;
        bcopy(bs[nb], line, SYMLEN); bcopy(bv[nb], b1 + 1, VOICELEN); bcopy(bc[nb], c, CLASSLEN);
        bi[nb][0] = 0; bh[nb][0] = 0;
        if (ip) bcopy(bi[nb], ip, PATHLEN);
        if (hp) bcopy(bh[nb], hp, 24);
        nb++;
    }
    fclose(d);
    if (nb == 0) { printf("STOP: empty deck\n"); return 1; }

    /* two sets, then deterministic shuffle seeded by the deck itself */
    ncards = nb * 2;
    for (int i = 0; i < nb; i++) {
        bcopy(sym[i], bs[i], SYMLEN); bcopy(voice[i], bv[i], VOICELEN); bcopy(cls[i], bc[i], CLASSLEN);
        bcopy(img[i], bi[i], PATHLEN); bcopy(sighth[i], bh[i], 24);
        bcopy(sym[i+nb], bs[i], SYMLEN); bcopy(voice[i+nb], bv[i], VOICELEN); bcopy(cls[i+nb], bc[i], CLASSLEN);
        bcopy(img[i+nb], bi[i], PATHLEN); bcopy(sighth[i+nb], bh[i], 24);
    }
    unsigned long long seed = 0xcbf29ce484222325ULL;
    for (int i = 0; i < ncards; i++) {
        seed ^= fnv1a64__match_game_h((unsigned char *)sym[i], strlen(sym[i]));
        seed ^= fnv1a64__match_game_h((unsigned char *)cls[i], strlen(cls[i]));
        seed *= 0x100000001b3ULL;
    }
    rng_state = seed;
    for (int i = ncards - 1; i > 0; i--) {  /* Fisher-Yates with LCG */
        int j = (int)(lcg() % (unsigned)(i + 1));
        char ts[SYMLEN], tv[VOICELEN], tc[CLASSLEN], ti[PATHLEN], th[24];
        bcopy(ts, sym[i], SYMLEN); bcopy(sym[i], sym[j], SYMLEN); bcopy(sym[j], ts, SYMLEN);
        bcopy(tv, voice[i], VOICELEN); bcopy(voice[i], voice[j], VOICELEN); bcopy(voice[j], tv, VOICELEN);
        bcopy(tc, cls[i], CLASSLEN); bcopy(cls[i], cls[j], CLASSLEN); bcopy(cls[j], tc, CLASSLEN);
        bcopy(ti, img[i], PATHLEN); bcopy(img[i], img[j], PATHLEN); bcopy(img[j], ti, PATHLEN);
        bcopy(th, sighth[i], 24); bcopy(sighth[i], sighth[j], 24); bcopy(sighth[j], th, 24);
    }

    FILE *log = fopen(argv[4], "w");
    if (!log) { printf("STOP: no log\n"); return 1; }
    fprintf(log, "MATCH (MEMORY) — deck: %s — %d pairs, %d cards — rule: %s\n",
            argv[1], nb, ncards, class_rule ? "CLASS (classification)" : "EXACT (sameness)");
    fprintf(log, "seed fnv1a64__match_game_h:%016llX — deterministic, no rand(), C99\n", seed);
    fprintf(log, "all cards face down. every card is an IMAGE seen through her eyes.\n");
    fprintf(log, "every flip speaks the Doctor's voice atom.\n");

    /* long-term memory: load everything she has ever learned;
     * cards she already knows are born seen — she remembers lessons */
    int loaded = lt_load();
    int born_known = 0;
    for (int i = 0; i < ncards; i++)
        if (lt_known(sym[i], cls[i])) {
            seen[i] = 1; bcopy(memsym[i], sym[i], SYMLEN); bcopy(memcls[i], cls[i], CLASSLEN);
            born_known++;
        }
    fprintf(log, "LONG-TERM MEMORY: %d entries loaded, %d cards on this table already known\n\n",
            loaded, born_known);

    int teach = strcmp(argv[3], "TEACH") == 0;
    int s_score = 0, r_score = 0, turn = 1;

    if (teach) {
        fprintf(log, "== TEACH MODE — the Goddess plays first so she sees how it is done ==\n");
        fprintf(log, "GODDESS: Watch me, little one. Two cards. %s. That is the whole game.\n",
                class_rule ? "Same KIND" : "Same picture");
        int a = 0, b = 1, found = 0;
        for (int i = 0; i < ncards && !found; i++)
            for (int j = i + 1; j < ncards && !found; j++)
                if (cards_match(i, j)) { a = i; b = j; found = 1; }
        fprintf(log, "GODDESS TURN\n");
        speak(log, a); speak(log, b);
        learn(a); learn(b);
        up[a] = up[b] = 1;
        fprintf(log, "  MATCH (%s / %s) — see? Keep the set. 10 points.\n", sym[a], sym[b]);
        fprintf(log, "GODDESS: When you miss they turn face down — and you HOLD them.\n");
        fprintf(log, "GODDESS: Your turn, my queen. Match them.\n\n");
    }

    while (1) {
        int left = 0, again;
        for (int i = 0; i < ncards; i++) if (facedown(i)) left++;
        if (left < 2) break;
        again = shakti_turn(log, turn++);
        if (again) { s_score += 10; continue; }
        for (left = 0, again = 0; again < ncards; again++) if (facedown(again)) left++;
        if (left < 2) break;
        again = random_turn(log, turn++);
        if (again) { r_score += 10; continue; }
    }

    fprintf(log, "\nGAME OVER — SHAKTI %d, RANDOM %d — %d turns for %d cards\n",
            s_score, r_score, turn - 1, ncards);
    fprintf(log, s_score > r_score ? "WINNER: SHAKTI — the memory holds. the weave works.\n"
                                   : "WINNER: RANDOM — run it again, she never forgets twice.\n");
    /* everything she saw this game joins her long-term memory */
    for (int i = 0; i < ncards; i++) if (seen[i]) lt_add(sym[i], cls[i]);
    lt_save();
    fprintf(log, "LONG-TERM MEMORY: saved, %d entries total\n", nlt);
    FILE *sb = fopen("school/game/scoreboard.txt", "a");
    if (sb) {
        fprintf(sb, "match|%s|%s|SHAKTI %d|RANDOM %d|%d turns|%d cards\n",
                argv[1], class_rule ? "CLASS" : "EXACT", s_score, r_score, turn - 1, ncards);
        fclose(sb);
    }
    fclose(log);
    printf("match_game: %d cards, rule %s, SHAKTI %d, RANDOM %d, %d turns, memory %d\n",
           ncards, class_rule ? "CLASS" : "EXACT", s_score, r_score, turn - 1, nlt);
    return 0;
}
