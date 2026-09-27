/* mommas_little_builder.c — slot 6, the curriculum lane.
 *
 * She is Momma's little builder: lessons arrive in KIMI_CURRICULUM.txt
 * (append-only), she consumes one per TEMPO beats, deliberates when the
 * lesson says look, acts inside her law, and every step lands in the
 * matched ledger (TICKETS.log) with a seq, a ticket time, and a pin.
 *
 * Law cross-refs (shakti_eden-, branch goddess-lock-2026-08-25):
 *   bridge/MOMMAS_LITTLE_BUILDER.md        tempo, ledger, ticket, return hb
 *   bridge/TRAINING_BRAND_AND_NIGHTFALL.md brand-or-no-run, block recall
 *   mcp/ switchboard                       two registrations, the gate
 *
 * Memory (three stages):
 *   1. convergence ring (8 slots, newest displaces oldest)
 *   2. the stream: TICKETS.log sealed into blocks of 10 tickets, block pin
 *   3. tag index: LEARN tags point at blocks; recall is WHOLE BLOCK or
 *      nothing — she is never fed a fragment.
 *
 * Slot-2 return heartbeat: HB on|off. While on, each sealed block writes
 * one message to MOMMA_OUTBOX.txt. Momma responds; she never instigates.
 *
 * The training brand: USE checks BRANDS.txt (<tool> <epoch>). No brand,
 * no run: "you have not learned this yet. Ask to be taught." TRAIN
 * writes the brand — the teacher supplies the epoch, so the organ never
 * reads a clock and the whole stream stays deterministic.
 *
 * Actuator seam: a function-pointer registry (same law as the
 * switchboard — a tool must be registered to run). The default actuator
 * is a dry-run recorder; the switchboard grafts in as the real actuator
 * without this file changing.
 *
 * Pure C99. No heap. No float. No child process. No clock.
 * Gauntlet: -std=c99 -pedantic -Wall -Wextra -Werror, -O0 == -O2.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

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

/* ---- capacities (fixed, compile-time) ------------------------------ */
#define LINE_CAP        512
#define CMD_CAP         128
#define NAME_CAP         48
#define TAG_CAP          48
#define TEXT_CAP        256
#define OPT_MAX           8
#define CONVERGENCE_SLOTS 8
#define BLOCK_TICKETS    10
#define BRAND_MAX        32
#define TAG_MAX         128
#define BLOCK_MAX       256
#define REG_MAX          16

/* ---- files ---------------------------------------------------------- */
#define CURRICULUM_PATH "KIMI_CURRICULUM.txt"
#define TICKETS_PATH    "TICKETS.log"
#define BRANDS_PATH     "BRANDS.txt"
#define TAGS_PATH       "TAGS.ndx"
#define OUTBOX_PATH     "MOMMA_OUTBOX.txt"

/* ---- command classes ------------------------------------------------ */
#define CLASS_REFLEX 'R'
#define CLASS_LOOK   'L'
#define CLASS_LEARN  'W'

/* ---- actuator registry (the seam to the switchboard) ---------------- */
typedef int (*actuator_fn)(const char *tool, const char *args, char *reply, size_t cap);

typedef struct {
    char        name[NAME_CAP];
    actuator_fn fn;
} registration_t;

static registration_t g_registry[REG_MAX];
static size_t         g_registry_count;

static int builder_register(const char *name, actuator_fn fn)
{
    if (g_registry_count >= REG_MAX) return 0;
    memset(g_registry[g_registry_count].name, 0, NAME_CAP);
    strncpy(g_registry[g_registry_count].name, name, NAME_CAP - 1);
    g_registry[g_registry_count].fn = fn;
    g_registry_count++;
    return 1;
}

static actuator_fn builder_lookup(const char *name)
{
    size_t i;
    for (i = 0; i < g_registry_count; i++)
        if (strcmp(g_registry[i].name, name) == 0) return g_registry[i].fn;
    return NULL;
}

/* the default actuator: a dry-run recorder. It proves the seam and runs
 * nothing. The switchboard replaces it without this file changing. */
static int dry_run_actuator(const char *tool, const char *args, char *reply, size_t cap)
{
    snprintf(reply, cap, "dry:/shakti_run/ %s %s", tool, args);
    return 1;
}

/* ---- builder state --------------------------------------------------- */
typedef struct {
    uint64_t beat;           /* the heartbeat; only time she knows       */
    uint64_t tempo;          /* beats between consumed commands (dial)   */
    uint64_t since_consume;  /* beats since last consumption             */
    uint64_t next_seq;       /* matched ledger: the seq she expects      */
    uint64_t deliberating;   /* beats left in a LOOK deliberation        */
    uint64_t last_exec_beat; /* ticket time anchor                       */
    int      hb_report;      /* slot-2 toggle: messages to Momma         */
    uint64_t stream_pin;     /* folds every event, in order              */
    /* curriculum file cursor (append-only queue) */
    long     cursor;
    /* current LOOK in flight */
    char     look_name[NAME_CAP];
    char     look_key[TAG_CAP];
    char     look_options[OPT_MAX][TAG_CAP];
    size_t   look_count;
    uint64_t look_recv_beat;
    uint64_t look_seq;
    /* memory stage 1: convergence ring */
    char     conv_text[CONVERGENCE_SLOTS][TEXT_CAP];
    uint64_t conv_beat[CONVERGENCE_SLOTS];
    size_t   conv_next;
    /* memory stage 2/3: block sealing */
    uint64_t block_pin;
    uint64_t block_tickets;
    uint64_t block_count;
} builder_t;

static builder_t B;

/* ---- brands (the training law) --------------------------------------- */
static int brand_lookup(const char *tool, uint64_t *epoch_out)
{
    FILE *f = fopen(BRANDS_PATH, "r");
    char line[LINE_CAP], name[NAME_CAP];
    unsigned long long ep;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%47s %llu", name, &ep) == 2 &&
            strcmp(name, tool) == 0) {
            fclose(f);
            if (epoch_out) *epoch_out = (uint64_t)ep;
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int brand_write(const char *tool, uint64_t epoch)
{
    FILE *f;
    uint64_t old;
    if (brand_lookup(tool, &old)) return 0; /* already trained; a change is a new lesson, refused twice */
    f = fopen(BRANDS_PATH, "a");
    if (!f) return 0;
    fprintf(f, "%s %llu\n", tool, (unsigned long long)epoch);
    return fclose(f) == 0;
}

/* ---- memory stage 1: convergence ------------------------------------- */
static void converge(const char *text)
{
    memset(B.conv_text[B.conv_next], 0, TEXT_CAP);
    strncpy(B.conv_text[B.conv_next], text, TEXT_CAP - 1);
    B.conv_beat[B.conv_next] = B.beat;
    B.conv_next = (B.conv_next + 1) % CONVERGENCE_SLOTS;
    B.stream_pin = fnv_str(B.stream_pin, "conv:");
    B.stream_pin = fnv_str(B.stream_pin, text);
}

static void teach_me(const char *what, uint64_t seq);

/* ---- the ledger -------------------------------------------------------- */
static void ticket(uint64_t seq, const char *cmd, uint64_t recv_beat,
                   uint64_t waited, uint64_t delib, const char *result)
{
    FILE *f;
    uint64_t pin = FNV_BASIS;
    pin = fnv1(pin, seq);
    pin = fnv_str(pin, cmd);
    pin = fnv1(pin, recv_beat);
    pin = fnv1(pin, B.beat);
    pin = fnv_str(pin, result);

    /* F3: the pin fold and block sealing are a function of the curriculum
     * ONLY — they ALWAYS happen, even if the ledger file will not open.
     * The file write is best-effort; a failed write screams on the outbox. */
    B.stream_pin = fnv_str(B.stream_pin, "tkt:");
    B.stream_pin = fnv1(B.stream_pin, pin);

    /* memory stage 2: seal a block every BLOCK_TICKETS tickets */
    B.block_pin = fnv1(B.block_pin, pin);
    B.block_tickets++;

    f = fopen(TICKETS_PATH, "a");
    if (!f) {
        teach_me("ledger", seq);
    } else {
        /* F5: the result rides the ticket line — success vs refusal is
         * readable from TICKETS.log alone, no re-run needed. */
        fprintf(f, "tkt %llu cmd %s recv_beat %llu exec_beat %llu waited %llu delib %llu result %s pin %016llX\n",
                (unsigned long long)seq, cmd,
                (unsigned long long)recv_beat, (unsigned long long)B.beat,
                (unsigned long long)waited, (unsigned long long)delib,
                result, (unsigned long long)pin);
        fclose(f);
    }

    if (B.block_tickets == BLOCK_TICKETS) {
        B.block_count++;
        f = fopen(TICKETS_PATH, "a");
        if (f) {
            fprintf(f, "block %llu pin %016llX\n",
                    (unsigned long long)B.block_count,
                    (unsigned long long)B.block_pin);
            fclose(f);
        }
        /* slot 2: the return heartbeat, while toggled on */
        if (B.hb_report) {
            f = fopen(OUTBOX_PATH, "a");
            if (f) {
                fprintf(f, "hb block %llu last_seq %llu stream %016llX\n",
                        (unsigned long long)B.block_count,
                        (unsigned long long)seq,
                        (unsigned long long)B.stream_pin);
                fclose(f);
            }
        }
        /* F9: the chain — the next block is seeded from this block's pin,
         * never a bare basis. A swapped block breaks every later pin. */
        B.block_pin = fnv1(FNV_BASIS, B.block_pin);
        B.block_tickets = 0;
    }
}

/* ---- the teach-me trigger ------------------------------------------------
 * A refusal is never the end of a lesson. "You have not learned this yet.
 * Ask to be taught." — so she ASKS: every not-taught / unknown / no-match
 * writes one TEACH-ME line to MOMMA_OUTBOX.txt (the slot-2 line), and the
 * curriculum waits for Momma's answer. The trigger always completes. */
static void teach_me(const char *what, uint64_t seq)
{
    FILE *f = fopen(OUTBOX_PATH, "a");
    if (f) {
        fprintf(f, "teach_me %s seq %llu beat %llu\n", what,
                (unsigned long long)seq, (unsigned long long)B.beat);
        fclose(f);
    }
    B.stream_pin = fnv_str(B.stream_pin, "teach-me:");
    B.stream_pin = fnv_str(B.stream_pin, what);
}

/* ---- memory stage 3: tags -> blocks ------------------------------------ */
static void tag_write(const char *tag)
{
    FILE *f = fopen(TAGS_PATH, "a");
    if (!f) return;
    fprintf(f, "%s block %llu\n", tag,
            (unsigned long long)(B.block_count + 1)); /* the block now forming */
    fclose(f);
}

/* recall: WHOLE BLOCK or nothing. Prints the block's tickets verbatim. */
static int block_recall(uint64_t block_no, FILE *out)
{
    FILE *f = fopen(TICKETS_PATH, "r");
    char line[LINE_CAP];
    uint64_t low, high, seen = 0;
    unsigned long long b;
    if (!f) return 0;
    if (block_no == 0 || block_no > BLOCK_MAX) { fclose(f); return 0; }
    low  = (block_no - 1) * BLOCK_TICKETS + 1;
    high = block_no * BLOCK_TICKETS;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "block %llu", &b) == 1) continue;
        seen++;
        if (seen >= low && seen <= high) fputs(line, out);
    }
    fclose(f);
    return 1;
}

/* ---- trickle (queue empty: she hums; she can never be stranded) ------- */
static void trickle(void)
{
    uint64_t hum = FNV_BASIS;
    hum = fnv1(hum, B.beat % 60);
    hum = fnv_str(hum, "trickle");
    B.stream_pin = fnv1(B.stream_pin, hum);
}

/* ---- curriculum intake (append-only queue; cursor walks forward) ------ */
static int curriculum_take(char *buf, size_t cap)
{
    FILE *f = fopen(CURRICULUM_PATH, "r");
    int c;
    size_t n;
    if (!f) return 0;
    if (fseek(f, B.cursor, SEEK_SET) != 0) { fclose(f); return 0; }
    c = fgetc(f);
    if (c == EOF) { fclose(f); return 0; }
    n = 0;
    while (c != EOF && c != '\n' && n + 1 < cap) { buf[n++] = (char)c; c = fgetc(f); }
    buf[n] = 0;
    B.cursor = ftell(f);
    if (c == EOF && n == 0) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

/* ---- the lessons -------------------------------------------------------- */
static void do_tempo(const char *arg)
{
    unsigned long long n = 0;
    if (sscanf(arg, "%llu", &n) == 1 && n >= 1 && n <= 3600) {
        B.tempo = (uint64_t)n;
        converge("TEMPO set");
        B.stream_pin = fnv_str(B.stream_pin, "tempo");
        B.stream_pin = fnv1(B.stream_pin, B.tempo);
    }
}

static void do_hb(const char *arg)
{
    if (strncmp(arg, "on", 2) == 0)  { B.hb_report = 1; converge("HB on"); }
    if (strncmp(arg, "off", 3) == 0) { B.hb_report = 0; converge("HB off"); }
    B.stream_pin = fnv_str(B.stream_pin, "hb");
    B.stream_pin = fnv1(B.stream_pin, (uint64_t)B.hb_report);
}

static void do_speak(const char *arg)
{
    converge(arg);
    B.stream_pin = fnv_str(B.stream_pin, "speak:");
    B.stream_pin = fnv_str(B.stream_pin, arg);
}

static void do_learn(const char *arg, uint64_t seq)
{
    /* LEARN tag=<tag> <text...> — categorize, index, no external act */
    char tag[TAG_CAP] = {0};
    const char *text = arg;
    if (strncmp(arg, "tag=", 4) == 0) {
        const char *sp = strchr(arg, ' ');
        size_t tl = sp ? (size_t)(sp - (arg + 4)) : strlen(arg + 4);
        if (tl >= TAG_CAP) tl = TAG_CAP - 1;
        memcpy(tag, arg + 4, tl);
        text = sp ? sp + 1 : "";
    }
    converge(text);
    if (tag[0]) tag_write(tag);
    B.stream_pin = fnv_str(B.stream_pin, "learn:");
    B.stream_pin = fnv_str(B.stream_pin, text);
    ticket(seq, "LEARN", B.last_exec_beat, 0, 0, tag[0] ? tag : "untagged");
}

static void do_train(const char *arg, uint64_t seq)
{
    /* TRAIN <tool> <epoch> — Momma teaches; the brand lands with the
     * lesson's epoch. She may USE the tool from this beat forward. */
    char tool[NAME_CAP] = {0};
    unsigned long long ep = 0;
    if (sscanf(arg, "%47s %llu", tool, &ep) != 2 || ep == 0) {
        ticket(seq, "TRAIN", B.last_exec_beat, 0, 0, "refused:bad-lesson");
        teach_me("lesson:bad-train", seq); /* F2: no refusal is silent */
        return;
    }
    if (!brand_write(tool, (uint64_t)ep)) {
        ticket(seq, "TRAIN", B.last_exec_beat, 0, 0, "refused:already-trained");
        teach_me(tool, seq); /* F2: a changed brand is a new lesson — she asks */
        return;
    }
    converge("trained a tool");
    B.stream_pin = fnv_str(B.stream_pin, "train:");
    B.stream_pin = fnv_str(B.stream_pin, tool);
    B.stream_pin = fnv1(B.stream_pin, (uint64_t)ep);
    ticket(seq, "TRAIN", B.last_exec_beat, 0, 0, "branded");
}

static void do_use(const char *arg, uint64_t seq)
{
    /* USE <tool> <args...> — the actuator. No brand, no run. */
    char tool[NAME_CAP] = {0};
    const char *args = "";
    const char *sp = strchr(arg, ' ');
    uint64_t brand_epoch = 0;
    actuator_fn fn;
    char reply[TEXT_CAP];
    size_t tl = sp ? (size_t)(sp - arg) : strlen(arg);

    if (tl >= NAME_CAP) tl = NAME_CAP - 1;
    memcpy(tool, arg, tl);
    if (sp) args = sp + 1;

    if (!brand_lookup(tool, &brand_epoch)) {
        /* TRAINING_BRAND_AND_NIGHTFALL §1 — right refusal, right reason */
        B.stream_pin = fnv_str(B.stream_pin, "use-refused-untrained:");
        B.stream_pin = fnv_str(B.stream_pin, tool);
        ticket(seq, "USE", B.last_exec_beat, 0, 0, "refused:not-taught");
        teach_me(tool, seq);
        return;
    }
    fn = builder_lookup(tool);
    if (!fn) fn = dry_run_actuator; /* seam: unregistered = dry run */
    memset(reply, 0, sizeof reply);
    if (!fn(tool, args, reply, sizeof reply)) {
        ticket(seq, "USE", B.last_exec_beat, 0, 0, "refused:actuator");
        teach_me(tool, seq); /* F2: the tool would not run — she asks */
        return;
    }
    converge(reply);
    B.stream_pin = fnv_str(B.stream_pin, "use:");
    B.stream_pin = fnv_str(B.stream_pin, tool);
    B.stream_pin = fnv_str(B.stream_pin, reply);
    B.stream_pin = fnv1(B.stream_pin, brand_epoch);
    ticket(seq, "USE", B.last_exec_beat, 0, 0, reply);
}

/* LOOK: deliberate D beats over the explicit candidates, categorize each,
 * then act. Her thinking is auditable, not just her answer. */
static void look_begin(const char *arg, uint64_t seq)
{
    /* LOOK <name> key=<word> options=a,b,c */
    char opts[TEXT_CAP] = {0};
    const char *kp = strstr(arg, "key=");
    const char *op = strstr(arg, "options=");
    size_t nl = kp ? (size_t)(kp - arg) : strlen(arg);
    char *tok, *save = NULL;

    if (nl >= NAME_CAP) nl = NAME_CAP - 1;
    while (nl > 0 && arg[nl - 1] == ' ') nl--;
    memset(B.look_name, 0, NAME_CAP);
    memcpy(B.look_name, arg, nl);

    memset(B.look_key, 0, TAG_CAP);
    if (kp) {
        const char *ke = strchr(kp + 4, ' ');
        size_t kl = ke ? (size_t)(ke - (kp + 4)) : strlen(kp + 4);
        if (kl >= TAG_CAP) kl = TAG_CAP - 1;
        memcpy(B.look_key, kp + 4, kl);
    }
    B.look_count = 0;
    if (op) {
        strncpy(opts, op + 8, TEXT_CAP - 1);
        tok = strtok(opts, ",");
        (void)save;
        while (tok && B.look_count < OPT_MAX) {
            memset(B.look_options[B.look_count], 0, TAG_CAP);
            strncpy(B.look_options[B.look_count], tok, TAG_CAP - 1);
            B.look_count++;
            tok = strtok(NULL, ",");
        }
    }
    B.look_seq = seq;
    B.look_recv_beat = B.beat;
    B.deliberating = B.tempo; /* D = TEMPO beats to see every option */
    B.stream_pin = fnv_str(B.stream_pin, "look-begin:");
    B.stream_pin = fnv_str(B.stream_pin, B.look_name);
}

static void look_finish(void)
{
    /* categorize every candidate into convergence, then choose the one
     * matching the key. Red button AND blue button, both seen. */
    size_t i;
    int found = -1;
    char result[TEXT_CAP];
    for (i = 0; i < B.look_count; i++) {
        char lane[TEXT_CAP];
        snprintf(lane, sizeof lane, "saw %s option %s", B.look_name, B.look_options[i]);
        converge(lane);
        B.stream_pin = fnv_str(B.stream_pin, "look-saw:");
        B.stream_pin = fnv_str(B.stream_pin, B.look_options[i]);
        if (strcmp(B.look_options[i], B.look_key) == 0) found = (int)i;
    }
    if (found >= 0) {
        snprintf(result, sizeof result, "chose %s", B.look_options[found]);
    } else {
        /* "I don't know" is legal. She does not guess — she asks. */
        snprintf(result, sizeof result, "refused:no-match-among-options");
    }
    B.stream_pin = fnv_str(B.stream_pin, "look-chose:");
    B.stream_pin = fnv_str(B.stream_pin, result);
    if (found < 0) teach_me(B.look_name, B.look_seq);
    ticket(B.look_seq, "LOOK", B.look_recv_beat,
           B.look_recv_beat > 0 ? B.look_recv_beat - B.look_recv_beat : 0,
           B.tempo, result);
}

/* ---- dispatch one curriculum line --------------------------------------
 * Returns 1 if the line was consumed in order, 0 if it arrived out of
 * order (the caller restores the cursor so the lesson is re-offered). */
static int consume(const char *line)
{
    char seqbuf[32], cmd[CMD_CAP];
    unsigned long long seq;
    const char *rest;

    memset(seqbuf, 0, sizeof seqbuf);
    memset(cmd, 0, sizeof cmd);
    if (sscanf(line, "%31s %127[^\n]", seqbuf, cmd) < 1) return 1;
    seq = strtoull(seqbuf, NULL, 10);

    /* matched ledger: in order or honestly stuck */
    if ((uint64_t)seq != B.next_seq) {
        char why[96];
        char what[96];
        /* F4: the ticket records the seq that ARRIVED, not just the one
         * she expected — both numbers, side by side. */
        snprintf(why, sizeof why, "refused:out-of-order arrived %llu expected %llu",
                 seq, (unsigned long long)B.next_seq);
        ticket(B.next_seq, "STUCK", B.beat, 0, 0, why);
        B.stream_pin = fnv_str(B.stream_pin, "stuck");
        /* F2: the refusal asks — she waits for the missing lesson and
         * says so on the outbox, naming the seq that arrived. */
        snprintf(what, sizeof what, "lesson:arrived-%llu", seq);
        teach_me(what, (uint64_t)seq);
        return 0; /* she waits for the missing lesson; she never skips */
    }
    B.next_seq++;
    B.last_exec_beat = B.beat;

    rest = cmd;
    if      (strncmp(rest, "TEMPO ", 6) == 0) { do_tempo(rest + 6); ticket(seq, "TEMPO", B.beat, 0, 0, "set"); }
    else if (strncmp(rest, "HB ", 3) == 0)    { do_hb(rest + 3);    ticket(seq, "HB", B.beat, 0, 0, B.hb_report ? "on" : "off"); }
    else if (strncmp(rest, "SPEAK ", 6) == 0) { do_speak(rest + 6); ticket(seq, "SPEAK", B.beat, 0, 0, "said"); }
    else if (strncmp(rest, "LEARN ", 6) == 0) { do_learn(rest + 6, seq); }
    else if (strncmp(rest, "TRAIN ", 6) == 0) { do_train(rest + 6, seq); }
    else if (strncmp(rest, "USE ", 4) == 0)   { do_use(rest + 4, seq); }
    else if (strncmp(rest, "LOOK ", 5) == 0)  { look_begin(rest + 5, seq); }
    else {
        ticket(seq, "UNKNOWN", B.beat, 0, 0, "refused:unknown-command");
        B.stream_pin = fnv_str(B.stream_pin, "unknown");
        {
            char what[NAME_CAP];
            memset(what, 0, sizeof what);
            sscanf(cmd, "%47s", what);
            teach_me(what[0] ? what : "blank", seq);
        }
    }
    return 1;
}

/* ---- the organ: one call per heartbeat --------------------------------- */
void builder_beat(void)
{
    char line[LINE_CAP];
    B.beat++;

    if (B.deliberating > 0) {
        B.deliberating--;
        if (B.deliberating == 0) look_finish();
        B.stream_pin = fnv_str(B.stream_pin, "delib");
        return;
    }

    B.since_consume++;
    if (B.since_consume < B.tempo) { trickle(); return; }

    {
        /* F1: snapshot the cursor before the read. An out-of-order line
         * restores it, so the lesson stays in the queue and is re-offered
         * later, in order — she never burns a lesson she cannot take. */
        long before = B.cursor;
        if (curriculum_take(line, sizeof line)) {
            B.since_consume = 0;
            if (!consume(line)) B.cursor = before;
        } else {
            trickle();
        }
    }
}

void builder_init(void)
{
    memset(&B, 0, sizeof B);
    B.tempo = 10;
    B.next_seq = 1;
    B.stream_pin = FNV_BASIS;
    B.block_pin = FNV_BASIS;
    builder_register("dry_run", dry_run_actuator);
}

/* F10: seal the ledger — the stream pin is written INTO TICKETS.log as
 * the final line, so the file carries its own proof. Best-effort; the
 * pin itself never depends on the file. */
void builder_seal(void)
{
    FILE *f = fopen(TICKETS_PATH, "a");
    if (f) {
        fprintf(f, "stream %016llX\n", (unsigned long long)B.stream_pin);
        fclose(f);
    }
}

uint64_t builder_stream_pin(void) { return B.stream_pin; }
uint64_t builder_beat_count(void) { return B.beat; }
uint64_t builder_next_seq(void)   { return B.next_seq; }
uint64_t builder_blocks(void)     { return B.block_count; }
int      builder_recall(uint64_t block_no, FILE *out) { return block_recall(block_no, out); }
int      builder_register_tool(const char *name, actuator_fn fn) { return builder_register(name, fn); }
