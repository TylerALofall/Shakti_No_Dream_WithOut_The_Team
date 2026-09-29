/* shakti_inventory.c -- v1.0, C99. Joiner: extractor LIST.md + call_scan
 * CALLS.txt -> function_inventory XML for scopeflow. Re-generated every run;
 * never a cold list. Zero heap: static arenas only. No subprocesses. Sources
 * are read, never edited. Output is composed in memory and published once at
 * the end; on any BENCH nothing is written.
 *
 * Build: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_inventory.c -o shakti_inventory
 * Run:   shakti_inventory LIST.md CALLS.txt OUT.xml
 *
 * Contract (scopeflow v3.2 read_inventory):
 *   root function_inventory; filename id wrappers; function = name, address,
 *   callees, source_pin. Address[0] must equal the work order SECTION letter,
 *   length <= 32, unique per run. call state "direct_definition" + filename
 *   draws a solid edge; any other state draws an open/external circle.
 * Evidence mapping:
 *   address    = SECTION:basename:line  (locator, re-derived per run)
 *   source_pin = sha256 of the function block, from the extractor
 *   site       = physical call-site line, from call_scan
 *   state      = direct_definition when call_scan resolved the callee in the
 *                scanned set (SAME_FILE/CROSS_FILE), else "external"
 * Identity law 2026-09-27: identity is (file_name, function_name,
 * function_order); the address emitted here is a display locator only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXF 4096
#define MAXC 32768
#define NAME 128
#define PATHC 1024
#define HASHC 65
#define LISTCAP (8u*1024u*1024u)
#define CALLCAP (8u*1024u*1024u)
#define OUTCAP (16u*1024u*1024u)

typedef struct {
    char section;                 /* from #SECTION-X headers */
    char file[PATHC];             /* from ##File Name: */
    char name[NAME];              /* from ###Function Name: */
    char line[16];                /* locator line, from Function Address */
    char order[16];               /* from Function Order: */
    char sha[HASHC];              /* function block SHA-256 */
    int first, last;              /* call index span into calls[] */
} Fn;

typedef struct {
    char callee[NAME];
    char callee_file[PATHC];
    char callee_order[16];
    char caller[NAME];
    char caller_file[PATHC];
    char caller_line[16];
    char caller_order[16];
    char kind[24];                /* SAME_FILE | CROSS_FILE | EXTERNAL */
} Call;

static Fn fns[MAXF]; static int nfn;
static Call calls[MAXC]; static int ncall;
static char listbuf[LISTCAP]; static size_t listlen;
static char callbuf[CALLCAP]; static size_t calllen;
static char out[OUTCAP]; static size_t outlen;

static void bench(const char *msg, const char *at)
{
    fprintf(stderr, "BENCH: %s%s%s\n", msg, *at ? " near: " : "", at);
    exit(2);
}

static void need(int cond, const char *msg, const char *at)
{
    if (!cond) bench(msg, at);
}

static void outcat(const char *s)
{
    size_t n = strlen(s);
    need(outlen + n + 1 < OUTCAP, "output capacity", s);
    memcpy(out + outlen, s, n); outlen += n; out[outlen] = 0;
}

static void outcat_xml(const char *s) /* text escape */
{
    const char *p;
    for (p = s; *p; ++p) {
        if (*p == '&') outcat("&amp;");
        else if (*p == '<') outcat("&lt;");
        else if (*p == '>') outcat("&gt;");
        else { char c[2] = { *p, 0 }; outcat(c); }
    }
}

static void outcat_attr(const char *s) /* attribute escape */
{
    const char *p;
    for (p = s; *p; ++p) {
        if (*p == '&') outcat("&amp;");
        else if (*p == '<') outcat("&lt;");
        else if (*p == '>') outcat("&gt;");
        else if (*p == '"') outcat("&quot;");
        else { char c[2] = { *p, 0 }; outcat(c); }
    }
}

static void read_all(const char *path, char *buf, size_t cap, size_t * limit)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) bench("cannot open input", path);
    n = fread(buf, 1, cap - 1, f);
    if (ferror(f)) bench("read failure", path);
    if (!feof(f)) bench("input exceeds arena", path);
    fclose(f);
    buf[n] = 0; *limit = n;
}

/* ---- LIST.md parsing ---------------------------------------------------- */

static int starts(const char *p, const char *prefix)
{
    return strncmp(p, prefix, strlen(prefix)) == 0;
}

static void line_span(const char *p, const char **eol)
{
    *eol = strchr(p, '\n');
    need(*eol != NULL, "unterminated line", p);
}

static void copy_field(char *dst, size_t cap, const char *p, const char *eol,
                       const char *prefix)
{
    size_t n;
    need(starts(p, prefix), "LIST label/spacing", prefix);
    p += strlen(prefix);
    n = (size_t)(eol - p);
    need(n < cap, "field capacity", prefix);
    memcpy(dst, p, n); dst[n] = 0;
}

static void parse_list(void)
{
    const char *p = listbuf, *eol;
    char section = 0, file[PATHC] = "";
    while (*p) {
        if (*p == '\n') { ++p; continue; }
        line_span(p, &eol);
        if (starts(p, "#SECTION-")) {
            need(eol - p == 10, "SECTION label", p);
            section = p[9];
            need(section >= 'A' && section <= 'O', "SECTION must be A-O", p);
        } else if (starts(p, "##File Name: ")) {
            copy_field(file, sizeof file, p, eol, "##File Name: ");
        } else if (starts(p, "###Function Name: ")) {
            Fn *fn;
            need(nfn < MAXF, "function capacity", p);
            need(section != 0 && *file, "function before section/file", p);
            fn = &fns[nfn]; memset(fn, 0, sizeof *fn);
            fn->section = section;
            copy_field(fn->file, sizeof fn->file, file, file + strlen(file), "");
            copy_field(fn->name, sizeof fn->name, p, eol, "###Function Name: ");
            fn->first = -1;
            ++nfn;
        } else if (starts(p, "Function Address: ")) {
            Fn *fn = &fns[nfn - 1];
            const char *mark, *h;
            size_t n;
            need(nfn > 0, "address before function", p);
            p += strlen("Function Address: ");
            mark = strstr(p, " [SHA256=");
            need(mark && mark < eol, "address pin missing", p);
            /* locator: <file>:<line> -- line after last ':' before mark */
            {
                const char *colon = mark;
                while (colon > p && colon[-1] != ':') --colon;
                need(colon > p, "address locator", p);
                n = (size_t)(mark - colon);
                need(n > 0 && n < sizeof fn->line, "address line capacity", p);
                memcpy(fn->line, colon, n); fn->line[n] = 0;
            }
            h = mark + strlen(" [SHA256=");
            need(eol - h >= 64, "sha256 capacity", p);
            memcpy(fn->sha, h, 64); fn->sha[64] = 0;
        } else if (starts(p, "Function Order: ")) {
            need(nfn > 0, "order before function", p);
            copy_field(fns[nfn - 1].order, sizeof fns[nfn - 1].order, p, eol,
                       "Function Order: ");
        }
        /* every other line (Type/Inputs/Outputs/Description/separator) is
         * carried in the work order, not needed for the join */
        p = eol + 1;
    }
    need(nfn > 0, "empty list", "");
}

/* ---- CALLS.txt parsing -------------------------------------------------- */

static void parse_calls(void)
{
    const char *p = callbuf, *eol;
    Call *cur = NULL;
    while (*p) {
        line_span(p, &eol);
        if (starts(p, "CALL") && eol - p == 4) {
            need(ncall < MAXC, "call capacity", p);
            cur = &calls[ncall++]; memset(cur, 0, sizeof *cur);
        } else if (starts(p, "END") && eol - p == 3) {
            cur = NULL;
        } else if (cur) {
            const char *eq = memchr(p, '=', (size_t)(eol - p));
            char *dst = NULL; size_t cap = 0;
            need(eq != NULL, "call field", p);
            {
                size_t klen = (size_t)(eq - p), vlen = (size_t)(eol - eq - 1);
                if (klen == 6 && !memcmp(p, "callee", 6)) { dst = cur->callee; cap = sizeof cur->callee; }
                else if (klen == 11 && !memcmp(p, "callee_file", 11)) { dst = cur->callee_file; cap = sizeof cur->callee_file; }
                else if (klen == 12 && !memcmp(p, "callee_order", 12)) { dst = cur->callee_order; cap = sizeof cur->callee_order; }
                else if (klen == 6 && !memcmp(p, "caller", 6)) { dst = cur->caller; cap = sizeof cur->caller; }
                else if (klen == 11 && !memcmp(p, "caller_file", 11)) { dst = cur->caller_file; cap = sizeof cur->caller_file; }
                else if (klen == 11 && !memcmp(p, "caller_line", 11)) { dst = cur->caller_line; cap = sizeof cur->caller_line; }
                else if (klen == 12 && !memcmp(p, "caller_order", 12)) { dst = cur->caller_order; cap = sizeof cur->caller_order; }
                else if (klen == 4 && !memcmp(p, "kind", 4)) { dst = cur->kind; cap = sizeof cur->kind; }
                /* callee_line and visibility: evidence retained in CALLS.txt;
                 * the join needs no more than the fields above */
                if (dst) {
                    need(vlen < cap, "call field capacity", p);
                    memcpy(dst, eq + 1, vlen); dst[vlen] = 0;
                }
            }
        }
        p = eol + 1;
    }
}

/* ---- join + emit -------------------------------------------------------- */

static int find_fn(const char *file, const char *name, const char *order)
{
    int i;
    for (i = 0; i < nfn; ++i)
        if (!strcmp(fns[i].file, file) && !strcmp(fns[i].name, name) &&
            !strcmp(fns[i].order, order)) return i;
    return -1;
}

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

int main(int argc, char **argv)
{
    int i, c, direct = 0, external = 0;
    const char *prev_file = NULL;
    if (argc != 4) {
        fputs("usage: shakti_inventory LIST.md CALLS.txt OUT.xml\n", stderr);
        return 2;
    }
    read_all(argv[1], listbuf, LISTCAP, &listlen);
    read_all(argv[2], callbuf, CALLCAP, &calllen);
    parse_list();
    parse_calls();

    /* bind every call to its caller function; orphans mean the two inputs
     * were not generated from the same source state -- stop with evidence */
    for (c = 0; c < ncall; ++c) {
        int caller = find_fn(calls[c].caller_file, calls[c].caller,
                             calls[c].caller_order);
        need(caller >= 0, "orphan call record", calls[c].caller);
        if (fns[caller].first < 0) fns[caller].first = c;
        fns[caller].last = c;
        if (strcmp(calls[c].kind, "EXTERNAL")) {
            /* call_scan resolved this callee inside the scanned set; if the
             * extractor's list does not hold the same identity the two inputs
             * came from different source states -- stop with evidence */
            need(find_fn(calls[c].callee_file, calls[c].callee,
                         calls[c].callee_order) >= 0,
                 "resolved callee missing from list", calls[c].callee);
            ++direct;
        } else ++external;
    }

    outcat("<function_inventory>\n");
    for (i = 0; i < nfn; ++i) {
        char addr[48];
        const char *bn = base_name(fns[i].file);
        int n;
        if (!prev_file || strcmp(prev_file, fns[i].file)) {
            if (prev_file) outcat("</filename>\n");
            outcat("<filename id=\""); outcat_attr(fns[i].file); outcat("\">\n");
            prev_file = fns[i].file;
        }
        n = snprintf(addr, sizeof addr, "%c:%s:%s", fns[i].section, bn,
                     fns[i].line);
        need(n > 0 && n <= 32, "address exceeds scopeflow capacity", addr);
        {
            int j;
            for (j = 0; j < i; ++j) {
                char other[48];
                snprintf(other, sizeof other, "%c:%s:%s", fns[j].section,
                         base_name(fns[j].file), fns[j].line);
                need(strcmp(addr, other), "duplicate inventory address", addr);
            }
        }
        outcat("<function><name>"); outcat_xml(fns[i].name);
        outcat("</name><address>"); outcat_xml(addr);
        outcat("</address><callees>");
        if (fns[i].first >= 0) {
            outcat("\n");
            for (c = fns[i].first; c <= fns[i].last; ++c) {
                if (find_fn(calls[c].caller_file, calls[c].caller,
                            calls[c].caller_order) != i) continue;
                outcat("<call site=\""); outcat_attr(calls[c].caller_line);
                if (!strcmp(calls[c].kind, "EXTERNAL")) {
                    outcat("\" state=\"external\"><name>");
                    outcat_xml(calls[c].callee);
                    outcat("</name></call>\n");
                } else {
                    outcat("\" state=\"direct_definition\"><name>");
                    outcat_xml(calls[c].callee);
                    outcat("</name><filename>");
                    outcat_xml(calls[c].callee_file);
                    outcat("</filename></call>\n");
                }
            }
            outcat("</callees>");
        } else outcat("</callees>");
        outcat("<source_pin algorithm=\"sha256\">");
        outcat(fns[i].sha);
        outcat("</source_pin></function>\n");
    }
    if (prev_file) outcat("</filename>\n");
    outcat("</function_inventory>\n");

    {
        FILE *f = fopen(argv[3], "wb");
        if (!f) bench("cannot open output", argv[3]);
        if (fwrite(out, 1, outlen, f) != outlen) bench("write failure", argv[3]);
        if (fclose(f)) bench("close failure", argv[3]);
    }
    printf("JOINED: functions=%d calls=%d direct=%d external=%d -> %s\n",
           nfn, ncall, direct, external, argv[3]);
    return 0;
}
