/* Ticket.Dealer.c -- shakti-ticket-dealer v2
 * Spec v2.0, Tyler Allen Lofall, 2026-09-30.
 *
 * v2 removes the FNV-1a run ID and the hash self-test. No opaque
 * identifiers are emitted: the log records only exact counts and byte
 * sizes that a reader can recompute by hand against the inputs.
 *
 * Deals the records of one or more work orders into tickets.
 *
 *   Record  : a block between blank lines whose first line starts with
 *             RECORD_STAMP (work orders: <SECTION). Two or more blank
 *             lines count as one break. Empty blocks do not exist.
 *   Cluster : consecutive records with the same section id + CHECKPOINT.
 *   Piece   : a cluster of n units is cut by position into letters:
 *             1..X = A, X+1..2X = B, ... 9X+1..10X = J. More = refused.
 *   Deal    : pieces of exactly X get their own ticket first. The rest
 *             go largest first into the first ticket they fit in without
 *             passing X; if none fits, a new ticket opens. Ties go to
 *             the earlier piece in the document.
 *   Floor   : the fewest tickets possible is at least
 *             full pieces + max(ceil(leftover units / X),
 *                               leftover pieces bigger than X/2).
 *             If tickets == floor, no deal can use fewer.
 *
 * STRICT checks. Any failure: REFUSED, exit 2, nothing left behind.
 *   - when the checkpoint changes, the unit marker must be UNIT_START
 *   - UNIT_START may only appear when the checkpoint changes
 *   - a numeric unit marker goes up by exactly 1
 *   - a checkpoint may not appear twice
 *   - a non-record block may not sit between records
 *   - records found must equal TOTAL_MARK (when TOTAL_MARK is set)
 *   - every output name must be unique
 *
 * Output name: PREFIX-section-checkpoint_name-letter.txt, with every '.'
 * in the checkpoint name turned into '_'. Example: WO-C-shakti_choice_c-B.txt
 *
 * Writes TICKET-NN.txt and DATE-log-ticket-dealer.txt into OUT_DIR.
 * Never overwrites. Writes .tmp files first and renames only after every
 * one succeeded. Zero heap. No subprocess. No clock. Same input, same bytes.
 *
 * Build: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 \
 *           -o shakti-ticket-dealer Ticket.Dealer.c
 * Usage: shakti-ticket-dealer <config>
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUTS      16
#define MAX_INPUT_BYTES (8UL * 1024UL * 1024UL)
#define MAX_RECORDS     8192
#define MAX_CLUSTERS    4096
#define MAX_PIECES      8192
#define MAX_TICKETS     8192
#define MAX_PATH        1024
#define MAX_VALUE       256
#define MAX_KEY         64
#define MAX_CONFIG      65536
#define MAX_TEMPLATE    262144
#define MAX_LETTERS     10
#define MAX_X           100000L

typedef struct {
    char   path[MAX_PATH];
    size_t off, len;
    long   found;
    long   total;
} Input;

typedef struct {
    int    input;
    int    num;                 /* record number inside its input, from 1 */
    size_t off, len;            /* block bytes inside g_data */
    long   line_first, line_last;
    char   section[16];
    char   check[MAX_VALUE];
    char   unit[MAX_VALUE];
    long   unum;
    int    cluster;
    int    pos;                 /* position inside its cluster, from 1 */
} Record;

typedef struct {
    int  first_rec;
    int  count;
    char base[MAX_VALUE];       /* checkpoint after the last slash */
    char name[MAX_VALUE];       /* base with '.' turned into '_' */
} Cluster;

typedef struct {
    int  cluster;
    int  letter;
    int  first_pos, last_pos, size;
    int  first_rec;
    int  ticket;
    char out[MAX_VALUE + 64];
} Piece;

static struct {
    long x;
    char checkpoint[MAX_KEY], unit_marker[MAX_KEY], total_mark[MAX_KEY];
    char stamp[MAX_KEY], closer[MAX_KEY + 2];
    char unit_start[MAX_VALUE];
    char instructions[MAX_PATH], tpl_path[MAX_PATH], out_dir[MAX_PATH];
    char prefix[32], source_prefix[MAX_PATH], date[16];
    int  has_total_mark;
    int  numeric;
    long start_num;
} g_cfg;

static char          g_data[MAX_INPUT_BYTES];
static size_t        g_data_used;
static char          g_cfgbuf[MAX_CONFIG];
static char          g_tpl[MAX_TEMPLATE];
static size_t        g_tpl_len;
static unsigned char g_chunk[65536];

static Input   g_in[MAX_INPUTS];   static int g_nin;
static Record  g_rec[MAX_RECORDS]; static int g_nrec;
static Cluster g_cl[MAX_CLUSTERS]; static int g_ncl;
static Piece   g_pc[MAX_PIECES];   static int g_npc;
static int     g_order[MAX_PIECES];
static int     g_tk_size[MAX_TICKETS];
static int     g_tk_pieces[MAX_TICKETS];
static int     g_ntk, g_nfull;
static long    g_floor;
static size_t  g_instr_bytes;

static int g_tmp_made, g_final_made;

/* ---------------------------------------------------------------- paths */

static int pad_width(void)
{
    int w = 0, n = g_ntk;
    while (n > 0) { w++; n /= 10; }
    return w < 2 ? 2 : w;
}

/* idx 0..g_ntk-1 = tickets, idx g_ntk = the log. Returns 0 if cut short. */
static int file_path(int idx, int tmp, char *buf, size_t cap)
{
    int n;
    if (idx < g_ntk)
        n = snprintf(buf, cap, "%s/TICKET-%0*d.txt%s", g_cfg.out_dir,
                     pad_width(), idx + 1, tmp ? ".tmp" : "");
    else
        n = snprintf(buf, cap, "%s/%s-log-ticket-dealer.txt%s", g_cfg.out_dir,
                     g_cfg.date, tmp ? ".tmp" : "");
    return n >= 0 && (size_t)n < cap;
}

static void cleanup(void)
{
    char p[MAX_PATH * 2];
    int i;
    for (i = 0; i < g_final_made; i++)
        if (file_path(i, 0, p, sizeof p)) remove(p);
    for (i = g_final_made; i < g_tmp_made; i++)
        if (file_path(i, 1, p, sizeof p)) remove(p);
}

static void die(const char *fmt, ...)
{
    va_list ap;
    fflush(stdout);
    fputs("REFUSED: ", stderr);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    cleanup();
    exit(2);
}

/* -------------------------------------------------------------- helpers */

static int is_ws(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

static int is_alnum(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static char *trim(char *s)
{
    char *e;
    while (*s && is_ws(*s)) s++;
    e = s + strlen(s);
    while (e > s && is_ws(e[-1])) e--;
    *e = '\0';
    return s;
}

static const char *memfind(const char *h, size_t hn, const char *nd, size_t nn)
{
    size_t i;
    if (nn == 0 || nn > hn) return NULL;
    for (i = 0; i + nn <= hn; i++)
        if (h[i] == nd[0] && memcmp(h + i, nd, nn) == 0) return h + i;
    return NULL;
}

static int parse_long(const char *s, long *out)
{
    long v = 0;
    int n = 0;
    if (!*s) return 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return 0;
        if (++n > 9) return 0;
        v = v * 10 + (*s - '0');
    }
    *out = v;
    return 1;
}

static int valid_tag(const char *s)
{
    size_t n = strlen(s);
    if (n == 0 || n >= MAX_KEY) return 0;
    for (; *s; s++)
        if (!(is_alnum(*s) || *s == '_' || *s == '-')) return 0;
    return 1;
}

/* 0 ok, 1 cannot open, 2 too large, 3 read error */
static int read_file(const char *path, char *dst, size_t cap, size_t *len)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return 1;
    n = fread(dst, 1, cap, f);
    if (ferror(f)) { fclose(f); return 3; }
    if (n == cap && fgetc(f) != EOF) { fclose(f); return 2; }
    fclose(f);
    *len = n;
    return 0;
}

static const char *read_err(int r)
{
    return r == 1 ? "cannot open" : r == 2 ? "is larger than this tool's fixed limit"
                                           : "read error";
}

static int exists(const char *p)
{
    FILE *f = fopen(p, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* --------------------------------------------------------------- config */

enum { K_MAX_UNITS, K_CHECKPOINT, K_UNIT_MARKER, K_UNIT_START, K_RECORD_STAMP,
       K_TOTAL_MARK, K_INPUT, K_INSTRUCTIONS, K_TEMPLATE, K_OUT_DIR, K_PREFIX,
       K_SOURCE_PREFIX, K_DATE, K_COUNT };

static const char *const k_names[K_COUNT] = {
    "MAX_UNITS", "CHECKPOINT", "UNIT_MARKER", "UNIT_START", "RECORD_STAMP",
    "TOTAL_MARK", "INPUT", "INSTRUCTIONS", "TEMPLATE", "OUT_DIR", "PREFIX",
    "SOURCE_PREFIX", "DATE"
};

static void set_str(char *dst, size_t cap, const char *v, const char *key)
{
    if (strlen(v) >= cap) die("config %s is too long (limit %lu characters)",
                              key, (unsigned long)(cap - 1));
    strcpy(dst, v);
}

static void load_config(const char *path)
{
    size_t len = 0;
    int r, k, seen[K_COUNT];
    long lineno = 0;
    char *p;

    memset(seen, 0, sizeof seen);
    r = read_file(path, g_cfgbuf, sizeof g_cfgbuf - 1, &len);
    if (r) die("config %s %s", path, read_err(r));
    if (memchr(g_cfgbuf, '\0', len)) die("config %s contains a NUL byte", path);
    g_cfgbuf[len] = '\0';

    p = g_cfgbuf;
    while (*p) {
        char *line = p, *nl = strchr(p, '\n'), *eq, *key, *val;
        if (nl) { *nl = '\0'; p = nl + 1; } else { p = line + strlen(line); }
        lineno++;
        line = trim(line);
        if (!*line || *line == '#') continue;
        eq = strchr(line, '=');
        if (!eq) die("config line %ld has no '='", lineno);
        *eq = '\0';
        key = trim(line);
        val = trim(eq + 1);
        for (k = 0; k < K_COUNT; k++)
            if (strcmp(key, k_names[k]) == 0) break;
        if (k == K_COUNT) die("config line %ld: unknown key '%s'", lineno, key);
        if (k != K_INPUT && seen[k]) die("config line %ld: %s is set twice", lineno, key);
        if (!*val) die("config line %ld: %s is empty", lineno, key);
        seen[k]++;

        switch (k) {
        case K_MAX_UNITS:
            if (!parse_long(val, &g_cfg.x) || g_cfg.x < 1 || g_cfg.x > MAX_X)
                die("config line %ld: MAX_UNITS must be a whole number from 1 to %ld",
                    lineno, MAX_X);
            break;
        case K_CHECKPOINT:
        case K_UNIT_MARKER:
        case K_TOTAL_MARK:
            if (!valid_tag(val))
                die("config line %ld: %s must be a tag name (letters, digits, _ or -)",
                    lineno, key);
            set_str(k == K_CHECKPOINT ? g_cfg.checkpoint
                    : k == K_UNIT_MARKER ? g_cfg.unit_marker : g_cfg.total_mark,
                    MAX_KEY, val, key);
            break;
        case K_UNIT_START:
            set_str(g_cfg.unit_start, sizeof g_cfg.unit_start, val, key);
            break;
        case K_RECORD_STAMP:
            if (val[0] != '<' || !valid_tag(val + 1))
                die("config line %ld: RECORD_STAMP must be '<' followed by a tag name",
                    lineno);
            set_str(g_cfg.stamp, sizeof g_cfg.stamp, val, key);
            {   /* stamp is at most MAX_KEY-1 chars, so "</" + stamp+1 fits closer */
                size_t sn = strlen(g_cfg.stamp + 1);
                memcpy(g_cfg.closer, "</", 2);
                memcpy(g_cfg.closer + 2, g_cfg.stamp + 1, sn + 1);
            }
            break;
        case K_INPUT:
            if (g_nin >= MAX_INPUTS) die("config: more than %d INPUT lines", MAX_INPUTS);
            set_str(g_in[g_nin].path, MAX_PATH, val, key);
            g_nin++;
            break;
        case K_INSTRUCTIONS:
            set_str(g_cfg.instructions, MAX_PATH, val, key);
            break;
        case K_TEMPLATE:
            set_str(g_cfg.tpl_path, MAX_PATH, val, key);
            break;
        case K_OUT_DIR:
            set_str(g_cfg.out_dir, MAX_PATH, val, key);
            break;
        case K_SOURCE_PREFIX:
            set_str(g_cfg.source_prefix, MAX_PATH, val, key);
            break;
        case K_PREFIX: {
            const char *t;
            for (t = val; *t; t++)
                if (!(is_alnum(*t) || *t == '_'))
                    die("config line %ld: PREFIX may only hold letters, digits and _", lineno);
            set_str(g_cfg.prefix, sizeof g_cfg.prefix, val, key);
            break;
        }
        case K_DATE: {
            int i, good = strlen(val) == 10;
            for (i = 0; good && i < 10; i++)
                good = (i == 4 || i == 7) ? val[i] == '-' : (val[i] >= '0' && val[i] <= '9');
            if (!good) die("config line %ld: DATE must be YYYY-MM-DD", lineno);
            set_str(g_cfg.date, sizeof g_cfg.date, val, key);
            break;
        }
        default:
            break;
        }
    }
    for (k = 0; k < K_COUNT; k++)
        if (!seen[k] && k != K_TOTAL_MARK) die("config %s is missing %s", path, k_names[k]);
    g_cfg.has_total_mark = seen[K_TOTAL_MARK];
    g_cfg.numeric = parse_long(g_cfg.unit_start, &g_cfg.start_num);
}

/* ------------------------------------------------------------- template */

static const char *const k_ph[] = {
    "TICKET", "TICKETS", "UNITS", "PIECE_COUNT", "PIECES", "OUTPUTS", "RECORDS"
};
enum { P_TICKET, P_TICKETS, P_UNITS, P_PIECE_COUNT, P_PIECES, P_OUTPUTS, P_RECORDS, P_COUNT };

/* -1 not a placeholder, -2 unknown placeholder, else its index */
static int placeholder_at(size_t i, size_t *plen)
{
    size_t j = i + 1, n;
    int k;
    if (g_tpl[i] != '{') return -1;
    while (j < g_tpl_len && ((g_tpl[j] >= 'A' && g_tpl[j] <= 'Z') || g_tpl[j] == '_')) j++;
    if (j == i + 1 || j >= g_tpl_len || g_tpl[j] != '}') return -1;
    n = j - i - 1;
    *plen = j - i + 1;
    for (k = 0; k < P_COUNT; k++)
        if (strlen(k_ph[k]) == n && memcmp(g_tpl + i + 1, k_ph[k], n) == 0) return k;
    return -2;
}

static void load_template(void)
{
    size_t i, plen = 0;
    int r, ph, have_pieces = 0, have_records = 0;
    r = read_file(g_cfg.tpl_path, g_tpl, sizeof g_tpl, &g_tpl_len);
    if (r) die("TEMPLATE %s %s", g_cfg.tpl_path, read_err(r));
    for (i = 0; i < g_tpl_len; i++) {
        ph = placeholder_at(i, &plen);
        if (ph == -2) die("TEMPLATE has an unknown placeholder %.*s", (int)plen, g_tpl + i);
        if (ph == P_PIECES) have_pieces = 1;
        if (ph == P_RECORDS) have_records = 1;
    }
    if (!have_pieces || !have_records)
        die("TEMPLATE must contain {PIECES} and {RECORDS}");
}

/* ----------------------------------------------------------------- scan */

static void extract_value(const Input *in, const char *b, size_t n, long line,
                          const char *tag, char *out, size_t cap)
{
    char open[MAX_KEY + 4], close[MAX_KEY + 4];
    const char *o, *c, *vs, *ve;
    size_t on, cn, rest;

    snprintf(open, sizeof open, "<%s>", tag);
    snprintf(close, sizeof close, "</%s>", tag);
    on = strlen(open);
    cn = strlen(close);
    o = memfind(b, n, open, on);
    if (!o) die("%s: record at line %ld has no %s", in->path, line, open);
    rest = n - (size_t)(o + on - b);
    if (memfind(o + on, rest, open, on))
        die("%s: record at line %ld has %s twice", in->path, line, open);
    c = memfind(o + on, rest, close, cn);
    if (!c) die("%s: record at line %ld opens %s but never closes it", in->path, line, open);
    vs = o + on;
    ve = c;
    while (vs < ve && is_ws(*vs)) vs++;
    while (ve > vs && is_ws(ve[-1])) ve--;
    if (vs == ve) die("%s: record at line %ld has an empty %s", in->path, line, open);
    if ((size_t)(ve - vs) >= cap)
        die("%s: record at line %ld: %s value is longer than %lu characters",
            in->path, line, open, (unsigned long)(cap - 1));
    if (memchr(vs, '<', (size_t)(ve - vs)))
        die("%s: record at line %ld: %s value contains '<'", in->path, line, open);
    memcpy(out, vs, (size_t)(ve - vs));
    out[ve - vs] = '\0';
}

static void new_cluster(const Input *in, Record *r)
{
    Cluster *cl;
    const char *b = r->check, *t;
    char *s;
    if (g_ncl >= MAX_CLUSTERS) die("more than %d checkpoints", MAX_CLUSTERS);
    for (t = r->check; *t; t++)
        if (*t == '/' || *t == '\\') b = t + 1;
    if (!*b) die("%s record %d (line %ld): %s '%s' has no name after its last slash",
                 in->path, r->num, r->line_first, g_cfg.checkpoint, r->check);
    for (t = b; *t; t++)
        if (!(is_alnum(*t) || *t == '_' || *t == '.' || *t == '-'))
            die("%s record %d (line %ld): name '%s' holds '%c', which is not allowed "
                "in an output file name", in->path, r->num, r->line_first, b, *t);
    cl = &g_cl[g_ncl];
    cl->first_rec = g_nrec;
    cl->count = 1;
    strcpy(cl->base, b);
    strcpy(cl->name, b);
    for (s = cl->name; *s; s++)
        if (*s == '.') *s = '_';
    r->cluster = g_ncl;
    r->pos = 1;
    g_ncl++;
}

static void strict_check(const Input *in, Record *r, int prev)
{
    int same, at_start, c;

    same = prev >= 0 && strcmp(g_rec[prev].section, r->section) == 0 &&
           strcmp(g_rec[prev].check, r->check) == 0;
    if (g_cfg.numeric) {
        if (!parse_long(r->unit, &r->unum))
            die("%s record %d (line %ld): %s '%s' is not a whole number",
                in->path, r->num, r->line_first, g_cfg.unit_marker, r->unit);
        at_start = r->unum == g_cfg.start_num;
    } else {
        at_start = strcmp(r->unit, g_cfg.unit_start) == 0;
    }

    if (!same) {
        if (!at_start)
            die("%s record %d (line %ld): %s changed to '%s' but %s is '%s', not the "
                "starting value '%s'", in->path, r->num, r->line_first, g_cfg.checkpoint,
                r->check, g_cfg.unit_marker, r->unit, g_cfg.unit_start);
        for (c = 0; c < g_ncl; c++) {
            const Record *f = &g_rec[g_cl[c].first_rec];
            if (strcmp(f->section, r->section) == 0 && strcmp(f->check, r->check) == 0)
                die("%s record %d (line %ld): %s '%s' in section %s already appeared at "
                    "record %d of %s -- a checkpoint may not appear twice",
                    in->path, r->num, r->line_first, g_cfg.checkpoint, r->check,
                    r->section, f->num, g_in[f->input].path);
        }
        new_cluster(in, r);
    } else {
        if (at_start)
            die("%s record %d (line %ld): %s went back to '%s' but %s did not change",
                in->path, r->num, r->line_first, g_cfg.unit_marker, r->unit,
                g_cfg.checkpoint);
        if (g_cfg.numeric && r->unum != g_rec[prev].unum + 1)
            die("%s record %d (line %ld): %s jumps from %ld to %ld -- expected %ld",
                in->path, r->num, r->line_first, g_cfg.unit_marker, g_rec[prev].unum,
                r->unum, g_rec[prev].unum + 1);
        r->cluster = g_rec[prev].cluster;
        r->pos = g_rec[prev].pos + 1;
        g_cl[r->cluster].count++;
    }
}

static void handle_block(int ii, size_t bs, size_t be, long lf, long ll,
                         int *seen_rec, long *trail_line, int *prev)
{
    Input *in = &g_in[ii];
    const char *b = g_data + in->off + bs, *nl, *q;
    size_t n = be - bs, k = 0, sl = strlen(g_cfg.stamp), cl = strlen(g_cfg.closer);
    size_t fl, ls, le, m;
    Record *r;

    while (k < n && (b[k] == ' ' || b[k] == '\t')) k++;
    if (!(k + sl < n && memcmp(b + k, g_cfg.stamp, sl) == 0 &&
          (b[k + sl] == ' ' || b[k + sl] == '\t' || b[k + sl] == '>'))) {
        if (*seen_rec && *trail_line == 0) *trail_line = lf;
        return;
    }
    if (*trail_line)
        die("%s: the block at line %ld is not a record but sits between records",
            in->path, *trail_line);
    if (g_nrec >= MAX_RECORDS) die("more than %d records", MAX_RECORDS);

    r = &g_rec[g_nrec];
    memset(r, 0, sizeof *r);
    r->input = ii;
    r->num = (int)in->found + 1;
    r->off = in->off + bs;
    r->len = n;
    r->line_first = lf;
    r->line_last = ll;

    /* section id: id="..." on the stamp line */
    nl = memchr(b, '\n', n);
    fl = nl ? (size_t)(nl - b) : n;
    q = memfind(b, fl, "id=\"", 4);
    if (!q) die("%s: record at line %ld has no id=\"...\" on its %s line",
                in->path, lf, g_cfg.stamp);
    q += 4;
    for (m = 0; q + m < b + fl && q[m] != '"'; m++) {
        if (!is_alnum(q[m]) || m >= sizeof r->section - 1)
            die("%s: record at line %ld: section id must be 1 to 15 letters or digits",
                in->path, lf);
        r->section[m] = q[m];
    }
    if (m == 0 || q + m >= b + fl)
        die("%s: record at line %ld: section id is empty or never closed", in->path, lf);
    r->section[m] = '\0';

    /* the last line must be the closer */
    le = n;
    while (le > 0 && (b[le - 1] == '\n' || b[le - 1] == '\r')) le--;
    ls = le;
    while (ls > 0 && b[ls - 1] != '\n') ls--;
    while (ls < le && (b[ls] == ' ' || b[ls] == '\t')) ls++;
    if (le - ls < cl || memcmp(b + ls, g_cfg.closer, cl) != 0)
        die("%s: record at line %ld does not end with %s> (its last line is %ld) -- "
            "a blank line inside a record breaks it in two", in->path, lf, g_cfg.closer, ll);

    extract_value(in, b, n, lf, g_cfg.checkpoint, r->check, sizeof r->check);
    extract_value(in, b, n, lf, g_cfg.unit_marker, r->unit, sizeof r->unit);
    strict_check(in, r, *prev);

    *prev = g_nrec;
    g_nrec++;
    in->found++;
    *seen_rec = 1;
}

static int is_blank_line(const char *s, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r') return 0;
    return 1;
}

static void scan_input(int ii)
{
    Input *in = &g_in[ii];
    const char *base = g_data + in->off;
    size_t pos = 0, bstart = 0, bend = 0;
    long lineno = 0, bfirst = 0, blast = 0, trail_line = 0;
    int inblock = 0, seen_rec = 0, prev = -1;

    while (pos < in->len) {
        size_t s = pos, e = pos;
        while (e < in->len && base[e] != '\n') e++;
        lineno++;
        if (is_blank_line(base + s, e - s)) {
            if (inblock) {
                handle_block(ii, bstart, bend, bfirst, blast, &seen_rec, &trail_line, &prev);
                inblock = 0;
            }
        } else {
            if (!inblock) { inblock = 1; bstart = s; bfirst = lineno; }
            bend = e < in->len ? e + 1 : e;
            blast = lineno;
        }
        pos = e < in->len ? e + 1 : e;
    }
    if (inblock) handle_block(ii, bstart, bend, bfirst, blast, &seen_rec, &trail_line, &prev);

    if (in->found == 0) die("%s: no %s records found", in->path, g_cfg.stamp);
    if (g_cfg.has_total_mark) {
        char open[MAX_KEY + 4];
        const char *o, *v;
        size_t on;
        long t = 0;
        snprintf(open, sizeof open, "<%s>", g_cfg.total_mark);
        on = strlen(open);
        o = memfind(base, in->len, open, on);
        if (!o) die("%s: has no %s", in->path, open);
        for (v = o + on; v < base + in->len && is_ws(*v); v++) ;
        if (v >= base + in->len || *v < '0' || *v > '9')
            die("%s: %s does not hold a whole number", in->path, open);
        for (; v < base + in->len && *v >= '0' && *v <= '9'; v++) {
            if (t > 100000000L) die("%s: %s is too large", in->path, open);
            t = t * 10 + (*v - '0');
        }
        in->total = t;
        if (t != in->found)
            die("%s: %s says %ld but %ld records were found", in->path, open, t, in->found);
    }
}

/* ----------------------------------------------------------------- deal */

static void build_pieces(void)
{
    int c, i, j;
    long x = g_cfg.x;
    for (c = 0; c < g_ncl; c++) {
        const Cluster *cl = &g_cl[c];
        const Record *r0 = &g_rec[cl->first_rec];
        long n = cl->count;
        int np = (int)((n + x - 1) / x), k;
        if (np > MAX_LETTERS)
            die("%s record %d: %s '%s' has %ld units -- more than 10 x %ld (letter J) "
                "is refused", g_in[r0->input].path, r0->num, g_cfg.checkpoint, r0->check,
                n, x);
        for (k = 0; k < np; k++) {
            Piece *p;
            long last = (k + 1) * x < n ? (k + 1) * x : n;
            if (g_npc >= MAX_PIECES) die("more than %d pieces", MAX_PIECES);
            p = &g_pc[g_npc++];
            p->cluster = c;
            p->letter = k;
            p->first_pos = (int)(k * x + 1);
            p->last_pos = (int)last;
            p->size = p->last_pos - p->first_pos + 1;
            p->first_rec = cl->first_rec + p->first_pos - 1;
            p->ticket = -1;
            snprintf(p->out, sizeof p->out, "%s-%s-%s-%c.txt", g_cfg.prefix,
                     r0->section, cl->name, 'A' + k);
        }
    }
    for (i = 0; i < g_npc; i++)
        for (j = i + 1; j < g_npc; j++)
            if (strcmp(g_pc[i].out, g_pc[j].out) == 0)
                die("two pieces would both be named %s", g_pc[i].out);
}

/* larger first; on a tie, earlier in the document first */
static int before(int a, int b)
{
    if (g_pc[a].size != g_pc[b].size) return g_pc[a].size > g_pc[b].size;
    return g_pc[a].first_rec < g_pc[b].first_rec;
}

static void deal(void)
{
    int i, t, nl = 0, big = 0;
    long lsum = 0, x = g_cfg.x, need;

    g_ntk = 0;
    for (i = 0; i < g_npc; i++)
        if (g_pc[i].size == x) {
            if (g_ntk >= MAX_TICKETS) die("more than %d tickets", MAX_TICKETS);
            g_pc[i].ticket = g_ntk;
            g_tk_size[g_ntk] = g_pc[i].size;
            g_tk_pieces[g_ntk] = 1;
            g_ntk++;
        }
    g_nfull = g_ntk;

    for (i = 0; i < g_npc; i++)
        if (g_pc[i].ticket < 0) g_order[nl++] = i;
    for (i = 1; i < nl; i++) {
        int v = g_order[i], j = i - 1;
        while (j >= 0 && before(v, g_order[j])) { g_order[j + 1] = g_order[j]; j--; }
        g_order[j + 1] = v;
    }

    for (i = 0; i < nl; i++) {
        Piece *p = &g_pc[g_order[i]];
        lsum += p->size;
        if (2L * p->size > x) big++;
        for (t = g_nfull; t < g_ntk; t++)
            if (g_tk_size[t] + p->size <= x) break;
        if (t == g_ntk) {
            if (g_ntk >= MAX_TICKETS) die("more than %d tickets", MAX_TICKETS);
            g_tk_size[g_ntk] = 0;
            g_tk_pieces[g_ntk] = 0;
            g_ntk++;
        }
        g_tk_size[t] += p->size;
        g_tk_pieces[t]++;
        p->ticket = t;
    }
    need = (lsum + x - 1) / x;
    g_floor = g_nfull + (need > big ? need : big);
}

/* ----------------------------------------------------------------- write */

/* INSTRUCTIONS must exist and be readable; its exact byte count goes in
 * the log. Nothing is hashed -- the log holds counts, not digests. */
static void check_instructions(void)
{
    FILE *f = fopen(g_cfg.instructions, "rb");
    size_t n;
    if (!f) die("INSTRUCTIONS %s cannot open", g_cfg.instructions);
    while ((n = fread(g_chunk, 1, sizeof g_chunk, f)) > 0)
        g_instr_bytes += n;
    if (ferror(f)) { fclose(f); die("INSTRUCTIONS %s read error", g_cfg.instructions); }
    fclose(f);
}

static void emit_range(FILE *f, const Piece *p)
{
    if (g_cfg.numeric)
        fprintf(f, "%s %ld to %ld", g_cfg.unit_marker,
                g_cfg.start_num + p->first_pos - 1, g_cfg.start_num + p->last_pos - 1);
    else
        fprintf(f, "positions %d to %d", p->first_pos, p->last_pos);
}

static void emit_pieces(FILE *f, int t)
{
    int i, k = 0;
    for (i = 0; i < g_npc; i++) {
        const Piece *p = &g_pc[i];
        const Cluster *c;
        if (p->ticket != t) continue;
        c = &g_cl[p->cluster];
        k++;
        fprintf(f, "%d. %s%s/%s, ", k, g_cfg.source_prefix,
                g_rec[c->first_rec].section, c->base);
        emit_range(f, p);
        fprintf(f, " (%d record%s)\n   Write: %s\n", p->size, p->size == 1 ? "" : "s",
                p->out);
    }
}

static void emit_outputs(FILE *f, int t)
{
    int i;
    for (i = 0; i < g_npc; i++)
        if (g_pc[i].ticket == t) fprintf(f, "%s\n", g_pc[i].out);
}

static void emit_records(FILE *f, int t)
{
    int i, j, k = 0;
    for (i = 0; i < g_npc; i++) {
        const Piece *p = &g_pc[i];
        if (p->ticket != t) continue;
        k++;
        if (k > 1) fputc('\n', f);
        fprintf(f, "--- piece %d of %d: %s ---\n", k, g_tk_pieces[t], p->out);
        for (j = 0; j < p->size; j++) {
            const Record *r = &g_rec[p->first_rec + j];
            if (j > 0) fputc('\n', f);
            fwrite(g_data + r->off, 1, r->len, f);
            if (r->len == 0 || g_data[r->off + r->len - 1] != '\n') fputc('\n', f);
        }
    }
}

static void write_ticket(FILE *f, int t)
{
    size_t i = 0, plen = 0;
    while (i < g_tpl_len) {
        int ph = placeholder_at(i, &plen);
        if (ph < 0) { fputc(g_tpl[i], f); i++; continue; }
        switch (ph) {
        case P_TICKET:      fprintf(f, "%0*d", pad_width(), t + 1); break;
        case P_TICKETS:     fprintf(f, "%d", g_ntk); break;
        case P_UNITS:       fprintf(f, "%d", g_tk_size[t]); break;
        case P_PIECE_COUNT: fprintf(f, "%d", g_tk_pieces[t]); break;
        case P_PIECES:      emit_pieces(f, t); break;
        case P_OUTPUTS:     emit_outputs(f, t); break;
        default:            emit_records(f, t); break;
        }
        i += plen;
    }
}

static void emit_summary(FILE *f)
{
    int t, i, first;
    fprintf(f, "records: %d  checkpoints: %d  pieces: %d (full: %d)\n",
            g_nrec, g_ncl, g_npc, g_nfull);
    fprintf(f, "tickets: %d  floor: %ld  %s\n", g_ntk, g_floor,
            (long)g_ntk == g_floor ? "MINIMUM PROVEN"
                                   : "NOT PROVEN (the floor is a lower bound; the true minimum may be higher)");
    for (t = 0; t < g_ntk; t++) {
        fprintf(f, "TICKET-%0*d  %d units: ", pad_width(), t + 1, g_tk_size[t]);
        first = 1;
        for (i = 0; i < g_npc; i++) {
            if (g_pc[i].ticket != t) continue;
            fprintf(f, "%s%s (%d)", first ? "" : ", ", g_pc[i].out, g_pc[i].size);
            first = 0;
        }
        fputc('\n', f);
    }
}

static void write_log(FILE *f)
{
    int i, t;
    fprintf(f, "shakti-ticket-dealer v2 -- internal log, not for agents\n");
    fprintf(f, "date: %s\n", g_cfg.date);
    fprintf(f, "max_units: %ld\n", g_cfg.x);
    fprintf(f, "instructions: %s (%lu bytes)\n", g_cfg.instructions,
            (unsigned long)g_instr_bytes);
    for (i = 0; i < g_nin; i++) {
        fprintf(f, "input %d: %s (%lu bytes), records %ld", i + 1, g_in[i].path,
                (unsigned long)g_in[i].len, g_in[i].found);
        if (g_cfg.has_total_mark) fprintf(f, ", %s %ld", g_cfg.total_mark, g_in[i].total);
        fputc('\n', f);
    }
    emit_summary(f);
    fprintf(f, "pieces:\n");
    for (t = 0; t < g_ntk; t++)
        for (i = 0; i < g_npc; i++) {
            const Piece *p = &g_pc[i];
            const Record *a, *z;
            if (p->ticket != t) continue;
            a = &g_rec[p->first_rec];
            z = &g_rec[p->first_rec + p->size - 1];
            fprintf(f, "TICKET-%0*d | %s | section %s | %s | ", pad_width(), t + 1, p->out,
                    a->section, a->check);
            emit_range(f, p);
            fprintf(f, " | records %d-%d | input %d lines %ld-%ld\n", a->num, z->num,
                    a->input + 1, a->line_first, z->line_last);
        }
}

static void write_all(void)
{
    char p[MAX_PATH * 2], q[MAX_PATH * 2];
    int i, n = g_ntk + 1;
    FILE *f;

    for (i = 0; i < n; i++) {
        if (!file_path(i, 0, p, sizeof p) || !file_path(i, 1, q, sizeof q))
            die("OUT_DIR %s makes an output path too long", g_cfg.out_dir);
        if (exists(p)) die("%s already exists -- this tool never overwrites", p);
        file_path(i, 1, p, sizeof p);
        if (exists(p))
            die("%s is a .tmp left by an earlier run that died mid-write -- find out why "
                "before removing it", p);
    }
    for (i = 0; i < n; i++) {
        file_path(i, 1, p, sizeof p);
        f = fopen(p, "wb");
        if (!f) die("cannot create %s (does OUT_DIR exist?)", p);
        g_tmp_made++;
        if (i < g_ntk) write_ticket(f, i); else write_log(f);
        if (ferror(f)) { fclose(f); die("write error on %s", p); }
        if (fclose(f) != 0) die("write error on %s", p);
    }
    for (i = 0; i < n; i++) {
        file_path(i, 1, p, sizeof p);
        file_path(i, 0, q, sizeof q);
        if (rename(p, q) != 0) die("cannot rename %s to %s", p, q);
        g_final_made++;
    }
}

/* ----------------------------------------------------------------- main */

int main(int argc, char **argv)
{
    int i, r;

    if (argc != 2) {
        fputs("usage: shakti-ticket-dealer <config>\n", stderr);
        return 2;
    }

    load_config(argv[1]);
    load_template();
    for (i = 0; i < g_nin; i++) {
        size_t len = 0;
        r = read_file(g_in[i].path, g_data + g_data_used, sizeof g_data - g_data_used, &len);
        if (r == 2) die("INPUT files together are larger than %lu bytes",
                        (unsigned long)sizeof g_data);
        if (r) die("INPUT %s %s", g_in[i].path, read_err(r));
        g_in[i].off = g_data_used;
        g_in[i].len = len;
        g_data_used += len;
    }
    for (i = 0; i < g_nin; i++) scan_input(i);
    build_pieces();
    deal();
    check_instructions();
    write_all();

    printf("shakti-ticket-dealer v2\n");
    emit_summary(stdout);
    printf("wrote %d ticket%s and 1 log to %s\n", g_ntk, g_ntk == 1 ? "" : "s", g_cfg.out_dir);
    return 0;
}
