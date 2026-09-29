/*
 * Scope.Feed.c  (binary: shakti-feed)
 *
 * Feeder: filled Shakti work orders -> scope.xml for the Controlled Scope
 * Flow v2 renderer (scopeflow.c). This is the automation half of the flow
 * chart: no model hand-writes scope XML anymore.
 *
 * Shape law (Tyler, 2026-09-29):
 *   shape 0 circle   - function called from outside its zone (cross-section
 *                      or unresolved). Carries a note: who calls it and
 *                      "goes over there".
 *   shape 3 triangle - bulls: the entry functions (name exactly "main").
 *   shape 4 block    - everything else.
 *
 * Remote-call law (Tyler, 2026-09-29): <function_remote_call> holds DIRECT
 * calls only, one per line, exact form:
 *     file.c: function_name()
 *     libc: printf()            (library call: no .c suffix on the owner)
 *     NONE                      (no external calls)
 * Chains are never traced: each function lists only its own direct calls
 * and the renderer closes the graph.
 *
 * Resolution rules:
 *   - file known + name known  -> call edge; cross-section target turns
 *     into a circle with a "goes over there" note.
 *   - file known + name NOT in that file's work order -> BENCH loud
 *     (the order covers the whole file; a missing name is a typo).
 *   - file has .c suffix but is not in any loaded order -> circle in the
 *     EXTERNAL section, note "in tree, not yet mapped" (the parked files).
 *   - owner without .c suffix (libc etc.) -> circle, note "library call".
 *
 * Determinism: same WOs in the same argument order -> same bytes out.
 * No timestamps, sorted emission, fixed static arenas, zero heap, no
 * subprocesses. Every overflow or malformed record is a loud BENCH,
 * never a silent loss.
 *
 * Usage: shakti-feed OUT.scope.xml WO.xml [WO2.xml ...]
 *
 * C99 strict: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 \
 *     Scope.Feed.c -o shakti-feed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXF        1536            /* functions across all orders      */
#define MAXE        4096            /* resolved + unresolved edges      */
#define NAME        128             /* function / file base name        */
#define DESC       1200             /* note text cap                    */
#define WOCAP  (1024u*1024u)        /* one work-order file              */
#define OUTCAP (6u*1024u*1024u)     /* generated scope.xml              */
#define MAXREMOTE     64            /* remote lines per function        */
#define MAXSEC        26            /* A..Z                             */
#define MAXSUB        26            /* a..z per section                 */

typedef struct {
    char section;                   /* A..O from <SECTION id>           */
    char file[NAME];                /* base name only: shakti_loop.c    */
    char name[NAME];                /* function_name                    */
    char input[256];                /* verbatim parameter list          */
    char ret[96];                   /* function_return                  */
    char note[DESC];                /* description + generated notes    */
    int  order;
    int  complete;                  /* function_complete == yes         */
    int  has_remote;                /* remote_call field was present    */
    int  uid;                       /* emitted uid suffix on collision  */
    int  cross;                     /* called from another section      */
} Fn;

typedef struct {
    int  caller;                    /* index into fns[]                 */
    char file[NAME];                /* callee owner as written          */
    char name[NAME];                /* callee name                      */
    int  resolved;                  /* callee index or -1               */
    int  kind;                      /* 0 direct, 1 unmapped .c, 2 lib   */
} Edge;

static Fn   fns[MAXF];   static int nfn;
static Edge edges[MAXE]; static int nedge;

static char   wobuf[WOCAP];  static size_t wolen;
static char   out[OUTCAP];   static size_t outlen;

static void bench(const char *msg, const char *at)
{
    fprintf(stderr, "BENCH: %s%s%s\n", msg, *at ? " near: " : "", at);
    exit(2);
}

static void need(int cond, const char *msg, const char *at)
{
    if (!cond) bench(msg, at);
}

static void oadd(const char *s)
{
    size_t n = strlen(s);
    need(outlen + n + 1 < OUTCAP, "output capacity", s);
    memcpy(out + outlen, s, n);
    outlen += n;
    out[outlen] = 0;
}

static void oadd_attr(const char *s)        /* XML attribute escape */
{
    const char *p;
    for (p = s; *p; ++p) {
        if      (*p == '&')  oadd("&amp;");
        else if (*p == '<')  oadd("&lt;");
        else if (*p == '>')  oadd("&gt;");
        else if (*p == '"')  oadd("&quot;");
        else if (*p == '\n' || *p == '\r') oadd(" ");
        else { char c[2] = { *p, 0 }; oadd(c); }
    }
}

/* ---- work-order parsing (machine format, no lexer) ------------------ */

static const char *tag_val(const char *blk, const char *blkend,
                           const char *tag, char *dst, size_t cap,
                           int required)
{
    char open[160], close[160];
    const char *a, *b;
    size_t n;
    snprintf(open,  sizeof open,  "<%s>", tag);
    snprintf(close, sizeof close, "</%s>", tag);
    a = strstr(blk, open);
    if (!a || a >= blkend) {
        need(!required, "work-order tag missing", tag);
        dst[0] = 0;
        return NULL;
    }
    a += strlen(open);
    b = strstr(a, close);
    need(b != NULL && b < blkend, "work-order tag not closed", tag);
    n = (size_t)(b - a);
    need(n < cap, "work-order field over capacity", tag);
    memcpy(dst, a, n);
    dst[n] = 0;
    return dst;
}

static void base_name(const char *path, char *dst, size_t cap)
{
    const char *s = strrchr(path, '/');
    size_t n;
    s = s ? s + 1 : path;
    n = strlen(s);
    need(n > 0 && n < cap, "file base name capacity", path);
    memcpy(dst, s, n + 1);
}

static int is_ident(const char *s)
{
    size_t i;
    if (!*s) return 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

static void trim(char *s)
{
    size_t n = strlen(s);
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' ||
                 s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = 0;
}

/* Parse one remote_call line: "file.c: name()" or "libc: printf()". */
static void add_remote(int caller, char *line)
{
    char *colon, *paren, *dotc;
    Edge *e;
    trim(line);
    if (!*line) return;
    if (!strcmp(line, "NONE")) return;
    colon = strchr(line, ':');
    need(colon != NULL, "remote_call line missing ':'", line);
    *colon = 0;
    paren = strchr(colon + 1, '(');
    need(paren != NULL, "remote_call line missing '()'", line);
    *paren = 0;
    trim(line);
    trim(colon + 1);
    need(is_ident(colon + 1), "remote_call callee not a C identifier", line);
    need(strlen(line) < NAME && strlen(colon + 1) < NAME,
         "remote_call name capacity", line);
    dotc = strstr(line, ".c");
    need(dotc == NULL || dotc[2] == 0,
         "remote_call owner must be bare file.c or library", line);
    need(nedge < MAXE, "edge capacity", line);
    e = &edges[nedge++];
    memset(e, 0, sizeof *e);
    e->caller = caller;
    strcpy(e->file, line);
    strcpy(e->name, colon + 1);
    e->resolved = -1;
    e->kind = dotc ? 1 : 2;         /* refined after resolution */
}

static void parse_wo(const char *path)
{
    FILE *f = fopen(path, "rb");
    const char *p, *end;
    size_t n;
    if (!f) bench("cannot open work order", path);
    n = fread(wobuf, 1, WOCAP - 1, f);
    if (ferror(f)) bench("read failure", path);
    if (!feof(f)) bench("work order exceeds arena", path);
    fclose(f);
    wobuf[n] = 0;
    wolen = n;

    p = wobuf;
    end = wobuf + wolen;
    while ((p = strstr(p, "<SECTION id=\"")) != NULL) {
        const char *blkend, *next;
        Fn *fn;
        char sec[8], val[DESC], remote[8192], *rp, *line;
        need(p + 13 < end, "truncated SECTION tag", p);
        sec[0] = p[13];
        sec[1] = 0;
        need(sec[0] >= 'A' && sec[0] <= 'O' && p[14] == '"',
             "SECTION id must be A-O", p);
        blkend = strstr(p, "</SECTION>");
        need(blkend != NULL, "SECTION block not closed", p);
        need(nfn < MAXF, "function capacity", path);
        fn = &fns[nfn];
        memset(fn, 0, sizeof *fn);
        fn->section = sec[0];
        fn->uid = -1;

        tag_val(p, blkend, "file_name", val, sizeof val, 1);
        base_name(val, fn->file, sizeof fn->file);
        tag_val(p, blkend, "function_name", fn->name, sizeof fn->name, 1);
        need(is_ident(fn->name), "function_name not a C identifier", fn->name);
        tag_val(p, blkend, "function_order", val, sizeof val, 1);
        fn->order = atoi(val);
        tag_val(p, blkend, "function_type", val, sizeof val, 1);
        tag_val(p, blkend, "function_input", fn->input, sizeof fn->input, 1);
        tag_val(p, blkend, "function_return", fn->ret, sizeof fn->ret, 1);
        fn->has_remote =
            tag_val(p, blkend, "function_remote_call",
                    remote, sizeof remote, 0) != NULL;
        tag_val(p, blkend, "function_description", fn->note,
                sizeof fn->note, 0);
        tag_val(p, blkend, "function_complete", val, sizeof val, 1);
        fn->complete = !strcmp(val, "yes");

        nfn++;                              /* claim index before edges */
        if (fn->has_remote) {
            rp = remote;
            while ((line = strchr(rp, '\n')) != NULL) {
                *line = 0;
                add_remote(nfn - 1, rp);
                rp = line + 1;
            }
            add_remote(nfn - 1, rp);        /* last line, maybe no \n */
        }
        next = blkend + 10;
        p = next;
    }
    need(nfn > 0, "no functions parsed", path);
}

/* ---- resolution ------------------------------------------------------ */

static int find_fn(const char *file, const char *name)
{
    int i;
    for (i = 0; i < nfn; i++)
        if (!strcmp(fns[i].file, file) && !strcmp(fns[i].name, name))
            return i;
    return -1;
}

static int file_known(const char *file)
{
    int i;
    for (i = 0; i < nfn; i++)
        if (!strcmp(fns[i].file, file)) return 1;
    return 0;
}

static void resolve(void)
{
    int i;
    for (i = 0; i < nedge; i++) {
        Edge *e = &edges[i];
        if (e->kind == 2) continue;                 /* library call */
        e->resolved = find_fn(e->file, e->name);
        if (e->resolved >= 0) {
            e->kind = 0;
            if (fns[e->resolved].section != fns[e->caller].section)
                fns[e->resolved].cross = 1;         /* out-of-zone call */
        } else {
            need(!file_known(e->file),
                 "remote_call name not in that file's work order", e->name);
            e->kind = 1;                            /* in tree, unmapped */
        }
    }
}

/* ---- emission -------------------------------------------------------- */

static const char *uid_of(int i, char *buf)
{
    if (fns[i].uid == 0) return fns[i].name;
    snprintf(buf, NAME + 16, "%s_%d", fns[i].name, fns[i].uid + 1);
    return buf;
}

static void assign_uids(void)
{
    int i, j;
    for (i = 0; i < nfn; i++) {
        int suffix = 0;
        for (j = 0; j < i; j++)
            if (!strcmp(fns[j].name, fns[i].name))
                suffix++;
        fns[i].uid = suffix;        /* 0 = first claim keeps bare name */
    }
}

static int sub_letter(int fn)       /* a..z by sorted file in section */
{
    char files[MAXSUB][NAME];
    int count = 0, i, a, b;
    for (i = 0; i < nfn; i++) {
        int seen = 0, j;
        if (fns[i].section != fns[fn].section) continue;
        for (j = 0; j < count; j++)
            if (!strcmp(files[j], fns[i].file)) { seen = 1; break; }
        if (!seen) {
            need(count < MAXSUB, "over 26 files in one section", fns[i].file);
            memcpy(files[count], fns[i].file, NAME);
            count++;
        }
    }
    for (a = 0; a < count; a++)                     /* insertion sort */
        for (b = a; b > 0 && strcmp(files[b-1], files[b]) > 0; b--) {
            char t[NAME];
            memcpy(t, files[b-1], NAME);
            memcpy(files[b-1], files[b], NAME);
            memcpy(files[b], t, NAME);
        }
    for (i = 0; i < count; i++)
        if (!strcmp(files[i], fns[fn].file)) return i;
    bench("subsection letter lost", fns[fn].file);
    return 0;
}

static void emit_scope(void)
{
    char secseen[MAXSEC + 1];
    char num[64], uidbuf[NAME + 16];
    int s, i, first;

    memset(secseen, 0, sizeof secseen);
    for (i = 0; i < nfn; i++) secseen[fns[i].section - 'A'] = 1;

    oadd("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    oadd("<scope title=\"SHAKTI SYSTEM MAP\" layout=\"auto\">\n");
    oadd("  <families>\n");
    oadd("    <family name=\"core\" fill=\"#DCEBFF\" stroke=\"#28354A\" text=\"#111111\"/>\n");
    oadd("    <family name=\"ext\" fill=\"#FFF0CD\" stroke=\"#8A6D1A\" text=\"#111111\"/>\n");
    oadd("    <family name=\"bull\" fill=\"#DDF5DD\" stroke=\"#1A5C1A\" text=\"#111111\"/>\n");
    oadd("  </families>\n\n");

    for (s = 0; s < MAXSEC - 1; s++) {              /* A..Y used; Z reserved */
        char letter = (char)('A' + s);
        if (!secseen[s]) continue;
        oadd("  <section code=\"");
        { char c[2] = { letter, 0 }; oadd(c); }
        oadd("\" action=\"SECTION ");
        { char c[2] = { letter, 0 }; oadd(c); }
        oadd("\">\n");

        {
            int sub;
            for (sub = 0; sub < MAXSUB; sub++) {    /* probe a..z */
                int has = 0;
                for (i = 0; i < nfn; i++)
                    if (fns[i].section == letter &&
                        sub_letter(i) == sub) { has = 1; break; }
                if (!has) continue;
                oadd("    <subsection code=\"");
                { char c[2] = { (char)('a' + sub), 0 }; oadd(c); }
                oadd("\" action=\"");
                for (i = 0; i < nfn; i++)
                    if (fns[i].section == letter &&
                        sub_letter(i) == sub) {
                        oadd_attr(fns[i].file);
                        break;
                    }
                oadd("\">\n");
                for (i = 0; i < nfn; i++) {
                    const Fn *fn;
                    int shape, rank;
                    if (fns[i].section != letter ||
                        sub_letter(i) != sub) continue;
                    fn = &fns[i];
                    shape = fn->cross ? 0 :
                            (!strcmp(fn->name, "main") ? 3 : 4);
                    rank = !fn->has_remote ? 3 : (fn->complete ? 1 : 2);
                    oadd("      <function\n        uid=\"");
                    oadd_attr(uid_of(i, uidbuf));
                    oadd("\"\n        action=\"");
                    oadd_attr(uid_of(i, uidbuf));
                    oadd("\"\n        family=\"");
                    oadd(shape == 0 ? "ext" : (shape == 3 ? "bull" : "core"));
                    oadd("\"\n        shape=\"");
                    snprintf(num, sizeof num, "%d", shape);
                    oadd(num);
                    oadd("\"\n        rank=\"");
                    snprintf(num, sizeof num, "%d", rank);
                    oadd(num);
                    oadd("\"\n        checkpoint=\"0\"\n        x=\"\"\n        y=\"\"\n");
                    oadd("        inputs=\"call\"\n        outputs=\"call\"\n");
                    oadd("        enclosure=\"none\"\n        owner=\"\"\n");
                    oadd("        note=\"");
                    if (fn->cross) {
                        oadd_attr("CALLED FROM OUTSIDE THIS SECTION - goes over there. ");
                    }
                    if (!fn->has_remote)
                        oadd_attr("Remote calls not yet recorded. ");
                    oadd_attr(fn->note);
                    oadd("\"/>\n");
                }
                oadd("    </subsection>\n");
            }
        }
        oadd("  </section>\n\n");
    }

    /* Section Z: unresolved callees, one circle each, sorted by name. */
    {
        int extn = 0;
        for (i = 0; i < nedge; i++)
            if (edges[i].kind) extn++;
        if (extn) {
            int done = 0;
            oadd("  <section code=\"Z\" action=\"EXTERNAL\">\n");
            oadd("    <subsection code=\"z\" action=\"out-of-zone callees\">\n");
            while (done < extn) {
                int best = -1;
                for (i = 0; i < nedge; i++) {
                    if (!edges[i].kind || edges[i].resolved == -2) continue;
                    if (best < 0 ||
                        strcmp(edges[i].name, edges[best].name) < 0 ||
                        (!strcmp(edges[i].name, edges[best].name) &&
                         strcmp(edges[i].file, edges[best].file) < 0))
                        best = i;
                }
                if (best < 0) break;
                oadd("      <function\n        uid=\"ext_");
                oadd_attr(edges[best].name);
                oadd("\"\n        action=\"");
                oadd_attr(edges[best].name);
                oadd("\"\n        family=\"ext\"\n        shape=\"0\"\n");
                oadd("        rank=\"3\"\n        checkpoint=\"0\"\n");
                oadd("        x=\"\"\n        y=\"\"\n");
                oadd("        inputs=\"call\"\n        outputs=\"call\"\n");
                oadd("        enclosure=\"none\"\n        owner=\"\"\n");
                oadd("        note=\"");
                oadd_attr(edges[best].kind == 2
                          ? "Library call - goes over there."
                          : "In tree, not yet mapped - goes over there.");
                oadd(" Owner as written: ");
                oadd_attr(edges[best].file);
                oadd("\"/>\n");
                /* every duplicate of this callee resolves to this node */
                {
                    int j;
                    for (j = 0; j < nedge; j++)
                        if (edges[j].kind && edges[j].resolved != -2 &&
                            !strcmp(edges[j].name, edges[best].name) &&
                            !strcmp(edges[j].file, edges[best].file)) {
                            edges[j].resolved = -2;         /* consumed */
                            done++;
                        }
                }
            }
            oadd("    </subsection>\n  </section>\n\n");
        }
    }

    oadd("  <connections>\n");
    first = 1;
    (void)first;
    for (i = 0; i < nedge; i++) {
        const Edge *e = &edges[i];
        char caller[NAME + 16];
        oadd("    <connection\n      from=\"");
        oadd_attr(uid_of(e->caller, caller));
        oadd("\"\n      to=\"");
        if (e->kind) {
            oadd("ext_");
            oadd_attr(e->name);
        } else {
            char callee[NAME + 16];
            oadd_attr(uid_of(e->resolved, callee));
        }
        oadd("\"\n      fromPort=\"call\"\n      toPort=\"call\"\n");
        oadd("      kind=\"call\"\n      label=\"\"/>\n");
    }
    oadd("  </connections>\n</scope>\n");
}

int main(int argc, char **argv)
{
    int i, resolved = 0, unresolved = 0;
    FILE *f;
    if (argc < 3) {
        fputs("usage: shakti-feed OUT.scope.xml WO.xml [WO2.xml ...]\n",
              stderr);
        return 2;
    }
    for (i = 2; i < argc; i++)
        parse_wo(argv[i]);
    assign_uids();
    resolve();
    emit_scope();

    f = fopen(argv[1], "wb");
    if (!f) bench("cannot open output", argv[1]);
    if (fwrite(out, 1, outlen, f) != outlen)
        bench("write failure", argv[1]);
    if (fclose(f)) bench("close failure", argv[1]);

    for (i = 0; i < nedge; i++) {
        if (edges[i].kind) unresolved++;
        else resolved++;
    }
    printf("FEED: functions=%d edges=%d resolved=%d external=%d -> %s\n",
           nfn, nedge, resolved, unresolved, argv[1]);
    return 0;
}
