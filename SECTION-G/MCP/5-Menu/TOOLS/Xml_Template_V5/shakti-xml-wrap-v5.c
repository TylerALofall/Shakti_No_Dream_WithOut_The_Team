/* SCRIPT ONE v5: C99 XML-like wrap collector. MODIFIED: 2026-09-26.
 * QA SPECIALIST: GPT-6 (Codex); LOCATION: ChatGPT Work workspace.
 * SUPERVISOR QA: pending different model; LOCATION: pending inspection.
 * INSTRUCTIONS: cc -std=c99 -Wall -Wextra -Werror -pedantic
 * shakti-xml-wrap-v5.c shakti-xml-template-v5.c -o shakti-xml-template-v5
 * Scan each opener to its exact closer, then recurse inside those bounds.
 * fgetc reads only; unclosed roots and stray closers refuse the input.
 */
#include "shakti-xml-wrap-v5.h"
#include <string.h>
typedef struct {
    FILE *file;
    long at, limit;
    unsigned line;
    int indent, line_start;
} Reader;
typedef struct {
    XmlNode node;
    long end;
    unsigned end_line;
    int closing;
} Tag;
static int take(Reader *r)
{
    int c;
    if (r->at >= r->limit || (c = fgetc(r->file)) == EOF) return EOF;
    r->at++;
    if (c == '\n') { r->line++; r->indent = 0; r->line_start = 1; }
    else if (r->line_start && c == ' ') r->indent++;
    else if (r->line_start && c == '\t') r->indent += 4;
    else r->line_start = 0;
    return c;
}
static int peek(Reader *r)
{
    int c;
    if (r->at >= r->limit || (c = fgetc(r->file)) == EOF) return EOF;
    ungetc(c, r->file);
    return c;
}
static int reset(Reader *r, FILE *f, long start, unsigned line, long limit)
{
    r->file = f; r->at = start; r->limit = limit;
    r->line = line; r->indent = 0; r->line_start = 0;
    return fseek(f, start, SEEK_SET) == 0;
}
static int space(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static int first(int c) { return (c >= 'a' && c <= 'z') ||
    (c >= 'A' && c <= 'Z') || c == '_'; }
static int in_name(int c) { return first(c) || (c >= '0' && c <= '9') ||
    c == '-' || c == ':' || c == '.'; }
static void blanks(Reader *r) { while (space(peek(r))) take(r); }
static int problem(XmlScan *s, Reader *r, const char *why)
{
    snprintf(s->error, sizeof s->error, "line %u, byte %ld: %s", r->line, r->at, why);
    return -1;
}
static int name(Reader *r, char *dst, size_t cap, XmlScan *s)
{
    size_t n = 0;
    if (!first(peek(r))) return problem(s, r, "expected tag or attribute name");
    while (in_name(peek(r))) {
        if (n + 1 >= cap) return problem(s, r, "name exceeds limit");
        dst[n++] = (char)take(r);
    }
    dst[n] = 0;
    return 1;
}
static int until(Reader *r, XmlScan *s, const char *end)
{
    int c, a = 0, b = 0;
    size_t n = strlen(end);
    while ((c = take(r)) != EOF) {
        if ((n == 2 && b == end[0] && c == end[1]) ||
            (n == 3 && a == end[0] && b == end[1] && c == end[2])) return 1;
        a = b; b = c;
    }
    return problem(s, r, "unfinished comment, CDATA, or instruction");
}
static int special(Reader *r, XmlScan *s)
{
    int c = take(r), d, quote = 0, brackets = 0;
    if (c == '?') return until(r, s, "?>");
    if (c != '!') return problem(s, r, "invalid markup");
    d = peek(r);
    if (d == '-') {
        take(r);
        if (take(r) != '-') return problem(s, r, "invalid comment");
        return until(r, s, "-->");
    }
    if (d == '[') {
        const char *p = "[CDATA[";
        while (*p) if (take(r) != *p++) return problem(s, r, "invalid CDATA");
        return until(r, s, "]]>");
    }
    while ((c = take(r)) != EOF) {
        if (quote) { if (c == quote) quote = 0; continue; }
        if (c == '\'' || c == '"') quote = c;
        else if (c == '[') brackets++;
        else if (c == ']' && brackets) brackets--;
        else if (c == '>' && !brackets) return 1;
    }
    return problem(s, r, "unfinished declaration");
}
static int tag_end(Reader *r, XmlScan *s, XmlNode *n)
{
    size_t used = 0, value_used = 0, k;
    char key[XML_NAME];
    int c, quote, store;
    for (;;) {
        blanks(r); c = peek(r);
        if (c == '>') { take(r); return 1; }
        if (c == '/') {
            take(r);
            if (take(r) != '>') return problem(s, r, "expected />");
            n->self = 1; return 1;
        }
        if (c == EOF) return problem(s, r, "unfinished opening tag");
        if (name(r, key, sizeof key, s) < 0) return -1;
        blanks(r);
        if (take(r) != '=') return problem(s, r, "attribute needs =");
        blanks(r); quote = take(r);
        if (quote != '\'' && quote != '"') return problem(s, r, "attribute needs quotes");
        store = strcmp(key, "lvl") != 0;
        while ((c = take(r)) != EOF && c != quote) {
            if (c == '<' || c == 0) return problem(s, r, "invalid attribute value: BENCH");
            if (store) {
                if (value_used + 1 >= sizeof n->values)
                    return problem(s, r, "attribute values exceed limit: BENCH");
                n->values[value_used++] = (char)c;
            }
        }
        if (c == EOF) return problem(s, r, "unfinished attribute value");
        if (!store) continue;
        n->values[value_used++] = 0; n->value_bytes = (unsigned)value_used;
        k = strlen(key);
        if (used + k + 2 > sizeof n->attrs) return problem(s, r, "attributes exceed limit");
        if (used) n->attrs[used++] = ' ';
        memcpy(n->attrs + used, key, k + 1); used += k;
    }
}
static int next_tag(Reader *r, Tag *t, XmlScan *s)
{
    int c;
    while ((c = take(r)) != EOF) {
        if (c == '<') {
            long start = r->at - 1;
            unsigned line = r->line;
            int indent = r->indent;
            c = peek(r);
            if (c == '!' || c == '?') { if (special(r, s) < 0) return -1; continue; }
            if (c != '/' && !first(c)) return problem(s, r, "invalid markup: BENCH");
            memset(t, 0, sizeof *t);
            t->node.offset = start; t->node.line = line; t->node.indent = indent;
            if (c == '/') { take(r); t->closing = 1; }
            if (name(r, t->node.name, sizeof t->node.name, s) < 0) return -1;
            if (t->closing) {
                blanks(r);
                if (take(r) != '>') return problem(s, r, "invalid closing tag");
            } else if (tag_end(r, s, &t->node) < 0) return -1;
            t->end = r->at; t->end_line = r->line;
            return 1;
        }
    }
    return ferror(r->file) ? problem(s, r, "read failed") : 0;
}
/* Rescan from a start tag to its same-name closer, bounded by its parent. */
static int match(FILE *in, long from, unsigned line, long limit,
                 const char *target, Tag *close, XmlScan *s)
{
    Reader r;
    Tag t;
    int rc, nested = 0;
    if (!reset(&r, in, from, line, limit)) return problem(s, &r, "seek failed");
    while ((rc = next_tag(&r, &t, s)) == 1) {
        if (strcmp(t.node.name, target)) continue;
        if (t.closing) {
            if (!nested) { *close = t; return 1; }
            nested--;
        } else if (!t.node.self) nested++;
    }
    return rc;
}
/* Claim parent interval, then restart within it to discover its children. */
static int region(FILE *in, long start, unsigned line, long limit,
                  int parent, int depth, XmlScan *s)
{
    Reader r;
    Tag t, close = {0};
    int rc;
    if (depth == XML_MAX_DEPTH)
        { snprintf(s->error, sizeof s->error, "nesting exceeds fixed depth"); return -1; }
    if (!reset(&r, in, start, line, limit)) return problem(s, &r, "seek failed");
    while ((rc = next_tag(&r, &t, s)) == 1) {
        int j, found, nested = 0;
        long cursor;
        unsigned cursor_line;
        if (t.closing) {
            snprintf(s->error, sizeof s->error, "line %u: unowned closer </%s>: BENCH",
                     t.node.line, t.node.name);
            return -1;
        }
        if (s->count == XML_MAX_NODES) return problem(s, &r, "node table full");
        cursor = t.end; cursor_line = t.end_line;
        found = t.node.self ? 0 : match(in, cursor, cursor_line, limit, t.node.name, &close, s);
        if (found < 0) return -1;
        if (!found && !t.node.self) {
            snprintf(s->error, sizeof s->error, "line %u: no matching closer for <%s>: BENCH",
                     t.node.line, t.node.name);
            return -1;
        }
        if (found) {
            Reader probe;
            Tag ahead;
            int other;
            if (!reset(&probe, in, cursor, cursor_line, close.node.offset))
                return problem(s, &r, "seek failed");
            other = next_tag(&probe, &ahead, s);
            if (other < 0) return -1;
            nested = other > 0;
        }
        j = s->count++;
        s->node[j] = t.node;
        s->node[j].parent = parent; s->node[j].depth = depth;
        s->node[j].close_line = found ? close.end_line : t.end_line;
        if (found) {
            if (nested && region(in, cursor, cursor_line,
                                 close.node.offset, j, depth + 1, s) < 0)
                return -1;
            if (parent < 0) s->roots++;
            cursor = close.end; cursor_line = close.end_line;
        } else if (parent < 0) s->roots++;
        s->node[j].after = s->count;
        if (!reset(&r, in, cursor, cursor_line, limit)) return problem(s, &r, "seek failed");
    }
    return rc;
}
int shakti_xml_scan(FILE *input, XmlScan *s)
{
    long limit;
    s->count = s->roots = 0; s->error[0] = 0;
    if (fseek(input, 0, SEEK_END) || (limit = ftell(input)) < 0)
        { snprintf(s->error, sizeof s->error, "input must be seekable"); return 0; }
    if (region(input, 0, 1, limit, -1, 0, s) < 0) return 0;
    if (!s->roots) { snprintf(s->error, sizeof s->error, "no complete wrap found"); return 0; }
    return 1;
}
static int same_attrs(const char *a, const char *b)
{
    int count_a = 0, count_b = 0;
    const char *p;
    for (p = a; *p; ) {
        const char *end = strchr(p, ' '), *q = b;
        size_t len = end ? (size_t)(end - p) : strlen(p);
        int found = 0;
        while (*q) {
            const char *next = strchr(q, ' ');
            size_t n = next ? (size_t)(next - q) : strlen(q);
            if (n == len && !memcmp(p, q, len)) found = 1;
            q = next ? next + 1 : q + n;
        }
        if (!found) return 0;
        count_a++; p = end ? end + 1 : p + len;
    }
    for (p = b; *p; p++) if (p == b || p[-1] == ' ') count_b++;
    return count_a == count_b;
}
int shakti_xml_same_shape(const XmlScan *s, int a, int b)
{
    int k, n = s->node[a].after - a;
    if (n != s->node[b].after - b) return 0;
    for (k = 0; k < n; k++) {
        const XmlNode *x = &s->node[a + k], *y = &s->node[b + k];
        if (x->depth - s->node[a].depth != y->depth - s->node[b].depth ||
            x->self != y->self || strcmp(x->name, y->name) ||
            !same_attrs(x->attrs, y->attrs)) return 0;
    }
    return 1;
}
int shakti_xml_confirmed(const XmlScan *s)
{
    int parent, child, prev, root, prev_root = -1;
    for (root = 0; root < s->count; root = s->node[root].after) {
        if (prev_root >= 0 && s->node[root].after > root + 1 &&
            shakti_xml_same_shape(s, prev_root, root)) return 1;
        prev_root = root;
    }
    for (parent = 0; parent < s->count; parent++) {
        prev = -1;
        for (child = parent + 1; child < s->node[parent].after;
             child = s->node[child].after) {
            if (prev >= 0 && s->node[child].after > child + 1 &&
                shakti_xml_same_shape(s, prev, child)) return 1;
            prev = child;
        }
    }
    return 0;
}
