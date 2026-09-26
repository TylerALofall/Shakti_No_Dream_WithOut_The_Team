/*
 * Spiral.Template_make.c  ->  shakti-template-make
 *
 * Whole-file template extractor, Tyler's spiral law.
 *
 * One file in, every design out. No other tools touch the input.
 *
 * What it accepts:
 *   - a pure XML document (one root): walked whole.
 *   - a mixed document with ```xml fenced blocks (e.g. the master
 *     architecture markdown): prose outside fences is NOT a record
 *     and is skipped; every fenced block is walked as a document.
 *
 * What it does:
 *   - Walks every document with the spiral law (same-line = neutral,
 *     new-line = descend, match enforced, bench loudly, never guess).
 *   - Merges repeated records everywhere: 1766 function_records
 *     collapse into their distinct FORMS; every form is written,
 *     in first-seen order, with its repeat count.
 *   - Document law: the first tag and its final match are the
 *     document frame, seated at lvl="0", never tied into the objects.
 *   - SECTION law: the first repeated record inside a document is
 *     wrapped in <SECTION lvl="N" id="...">, repeat written once;
 *     later variants unseen unless 'full'.
 *
 * No ceilings: line buffer, tag stack, child lists and the root
 * forest all grow on demand. Memory failure benches, never truncates.
 *
 * Usage: shakti-template-make <file> [full]
 * Exit:  0 clean, 1 usage/io/memory, 2 unbalanced record (bench).
 *
 * C99 strict: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- dynamic line reader ---------- */

static char *read_line(FILE *fp)
{
    size_t cap = 256, n = 0;
    char *buf = malloc(cap);
    int c;
    if (!buf) return NULL;
    while ((c = fgetc(fp)) != EOF) {
        if (n + 2 >= cap) {
            char *nb;
            cap *= 2;
            nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[n++] = (char)c;
        if (c == '\n') break;
    }
    if (n == 0 && c == EOF) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

/* ---------- tree ---------- */

typedef struct Node {
    char *tag;
    long count;                  /* records collapsed into this node */
    struct Node **child;
    int nchild;
    int cap;
} Node;

static Node *node_new(const char *tag)
{
    Node *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->tag = malloc(strlen(tag) + 1);
    if (!n->tag) { free(n); return NULL; }
    strcpy(n->tag, tag);
    n->count = 1;
    n->child = NULL;
    n->nchild = 0;
    n->cap = 0;
    return n;
}

static void node_free(Node *n)
{
    int i;
    if (!n) return;
    for (i = 0; i < n->nchild; i++) node_free(n->child[i]);
    free(n->child);
    free(n->tag);
    free(n);
}

static int same_signature(const Node *a, const Node *b)
{
    int i;
    if (strcmp(a->tag, b->tag) != 0) return 0;
    if (a->nchild != b->nchild) return 0;
    for (i = 0; i < a->nchild; i++)
        if (!same_signature(a->child[i], b->child[i])) return 0;
    return 1;
}

/* attach child under parent; collapse into an identical sibling shape */
static int attach(Node *parent, Node *child)
{
    int i;
    for (i = 0; i < parent->nchild; i++) {
        if (same_signature(parent->child[i], child)) {
            parent->child[i]->count += child->count;
            node_free(child);
            return 0;
        }
    }
    if (parent->nchild == parent->cap) {
        int ncap = parent->cap == 0 ? 8 : parent->cap * 2;
        Node **nc = realloc(parent->child, (size_t)ncap * sizeof *nc);
        if (!nc) return -1;
        parent->child = nc;
        parent->cap = ncap;
    }
    parent->child[parent->nchild++] = child;
    return 0;
}

/* ---------- forest: every root form the file carries ---------- */

static Node **g_roots = NULL;    /* distinct root forms, first-seen  */
static int g_nroots = 0, g_rootcap = 0;

static int forest_add(Node *root)
{
    int i;
    for (i = 0; i < g_nroots; i++) {
        if (same_signature(g_roots[i], root)) {
            g_roots[i]->count += root->count;
            node_free(root);
            return 0;
        }
    }
    if (g_nroots == g_rootcap) {
        int ncap = g_rootcap == 0 ? 16 : g_rootcap * 2;
        Node **nr = realloc(g_roots, (size_t)ncap * sizeof *nr);
        if (!nr) return -1;
        g_roots = nr;
        g_rootcap = ncap;
    }
    g_roots[g_nroots++] = root;
    return 0;
}

/* ---------- tag walking (spiral law, per tag, in order) ---------- */

static int next_tag(const char *line, size_t *pos,
                    char *tag, size_t cap, int *is_close, int *kind)
{
    size_t i = *pos, start, n;
    while (line[i] && line[i] != '<') i++;
    if (line[i] != '<') return 0;
    i++;
    *is_close = 0;
    *kind = 0;
    if (line[i] == '?') {
        *kind = 1;
        while (line[i] && !(line[i] == '?' && line[i + 1] == '>')) i++;
        if (line[i]) i += 2;
        *pos = i;
        return 1;
    }
    if (line[i] == '!') {
        *kind = (line[i + 1] == '-' && line[i + 2] == '-') ? 2 : 3;
        while (line[i] && line[i] != '>') i++;
        if (line[i] == '>') i++;
        *pos = i;
        return 1;
    }
    if (line[i] == '/') { *is_close = 1; i++; }
    start = i;
    while (line[i] && line[i] != '>' && !isspace((unsigned char)line[i])
           && line[i] != '/') i++;
    n = i - start;
    if (n == 0 || n >= cap) return 0;
    memcpy(tag, line + start, n);
    tag[n] = '\0';
    while (line[i] && line[i] != '>') i++;
    if (line[i] == '>') i++;
    *pos = i;
    return 1;
}

static int closes_on_line(const char *line, size_t *pos, const char *tag)
{
    size_t p = *pos;
    char t[256];
    int is_close = 0, kind = 0;
    long interim = 0;
    while (next_tag(line, &p, t, sizeof t, &is_close, &kind)) {
        if (kind != 0) continue;
        if (!is_close && strcmp(t, tag) == 0) {
            size_t q = p;
            if (q >= 2 && line[q - 2] == '/') continue;
            interim++;
        } else if (is_close && strcmp(t, tag) == 0) {
            if (interim == 0) { *pos = p; return 1; }
            interim--;
        }
    }
    return 0;
}

static int line_blank(const char *s)
{
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static int line_is(const char *s, const char *word)
{
    while (isspace((unsigned char)*s)) s++;
    while (*word) {
        if (*s != *word) return 0;
        s++; word++;
    }
    return *s == '\0' || *s == '\n' || isspace((unsigned char)*s);
}

/* ---------- emission, Tyler's template law ---------- */

static const Node *g_section = NULL;
static long g_repeats = 0;
static int g_variants = 0;
static int g_sec_lvl = 0;
static int g_full = 0;

static const Node *find_section(const Node *n, int is_root)
{
    int i;
    if (!is_root && n->count > 1) return n;
    for (i = 0; i < n->nchild; i++) {
        const Node *r = find_section(n->child[i], 0);
        if (r) return r;
    }
    return NULL;
}

static int count_same_tag(const Node *parent, const char *tag)
{
    int i, k = 0;
    for (i = 0; i < parent->nchild; i++)
        if (strcmp(parent->child[i]->tag, tag) == 0) k++;
    return k;
}

static const Node *find_parent(const Node *n, const Node *target, int lvl,
                               int *found_lvl)
{
    int i;
    for (i = 0; i < n->nchild; i++) {
        if (n->child[i] == target) { *found_lvl = lvl + 1; return n; }
        {
            const Node *r = find_parent(n->child[i], target, lvl + 1,
                                        found_lvl);
            if (r) return r;
        }
    }
    return NULL;
}

static void lower_copy(char *dst, size_t cap, const char *src)
{
    size_t i;
    for (i = 0; src[i] && i + 1 < cap; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static void join3(char *dst, size_t cap, const char *a, const char *b)
{
    size_t na = strlen(a), nb = strlen(b);
    if (na + 1 + nb >= cap) { dst[0] = '\0'; return; }
    memcpy(dst, a, na);
    dst[na] = '.';
    memcpy(dst + na + 1, b, nb + 1);
}

/* Placeholder law: TWO names, never more - the immediate parent
 * and the tag itself: [inputs.input], [function.name].
 * Chains are forbidden: the fourth level must not copy three
 * other names. Children of the SECTION content node read
 * [section.tag]; one deeper, the container's own name takes over.
 * `prefix` names n's children, not n.                            */
static void emit_node(const Node *n, int lvl, const char *prefix)
{
    int i;
    if (n->nchild == 0) {
        char p[512];
        join3(p, sizeof p, prefix, n->tag);
        printf("<%s lvl=\"%d\">[%s]</%s>\n", n->tag, lvl, p, n->tag);
        return;
    }
    printf("<%s lvl=\"%d\">\n", n->tag, lvl);
    for (i = 0; i < n->nchild; i++) {
        const Node *c = n->child[i];
        int wraps = (c == g_section) ||
                    (g_full && g_section &&
                     strcmp(c->tag, g_section->tag) == 0);
        if (g_section && !g_full && c != g_section &&
            strcmp(c->tag, g_section->tag) == 0)
            continue;
        if (wraps) {
            if (g_sec_lvl == 0) g_sec_lvl = lvl + 1;
            printf("<SECTION lvl=\"%d\" id=\"%s\">\n", lvl + 1, c->tag);
            if (c->nchild == 0) {
                emit_node(c, lvl + 2, "section");
            } else {
                int j;
                for (j = 0; j < c->nchild; j++) {
                    const Node *g = c->child[j];
                    if (g->nchild == 0)
                        emit_node(g, lvl + 2, "section");
                    else
                        emit_node(g, lvl + 2, g->tag);
                }
            }
            printf("</SECTION>\n");
            continue;
        }
        if (c->nchild == 0)
            emit_node(c, lvl + 1, prefix);    /* leaf: [parent.tag] */
        else
            emit_node(c, lvl + 1, c->tag);    /* container names its kids */
    }
    printf("</%s>\n", n->tag);
}

static void emit_template(const Node *root)
{
    char rootlow[256];
    lower_copy(rootlow, sizeof rootlow, root->tag);

    g_section = find_section(root, 1);
    g_repeats = 0;
    g_variants = 0;
    g_sec_lvl = 0;
    if (g_section) {
        int slvl = 0;
        const Node *par = find_parent(root, g_section, 1, &slvl);
        g_repeats = g_section->count;
        if (par) g_variants = count_same_tag(par, g_section->tag) - 1;
    }

    if (root->nchild == 0) {
        char p[512];
        join3(p, sizeof p, rootlow, root->tag);
        printf("<%s lvl=\"0\">[%s]</%s>\n", root->tag, p, root->tag);
        return;
    }
    emit_node(root, 0, rootlow);

    if (g_section) {
        printf("<!-- two-match CONFIRMED: <SECTION> level=%d repeats=%ld;"
               " repeat written once;", g_sec_lvl, g_repeats);
        if (g_variants > 0 && !g_full)
            printf(" %d later variant(s) unseen (run 'full' to override)",
                   g_variants);
        else
            printf(" no variants");
        printf(" -->\n");
    }
}

/* ---------- main walk ---------- */

int main(int argc, char **argv)
{
    FILE *fp;
    char *line;
    long lineno = 0;
    int in_comment = 0;
    int rc = 0;
    int fence_mode = -1;         /* -1 undecided, 0 pure xml, 1 fenced */
    int in_fence = 0;
    long documents = 0;

    Node **stack = NULL;
    int sp = 0, stackcap = 0;
    Node *cur_root = NULL;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: shakti-template-make <file> [full]\n");
        return 1;
    }
    if (argc == 3) {
        if (strcmp(argv[2], "full") != 0) {
            fprintf(stderr, "usage: shakti-template-make <file> [full]\n");
            return 1;
        }
        g_full = 1;
    }
    /* a markdown file is prose with fenced record blocks: fence mode.
     * anything else decides itself at its first record tag.        */
    {
        size_t L = strlen(argv[1]);
        if (L >= 3 && strcmp(argv[1] + L - 3, ".md") == 0)
            fence_mode = 1;
    }

    fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "cannot open: %s\n", argv[1]);
        return 1;
    }

    while ((line = read_line(fp)) != NULL) {
        size_t pos = 0;
        char tag[256];
        int is_close = 0, kind = 0;
        lineno++;

        /* fence detection: ```xml opens a record block, ``` closes it.
         * Decided before the first record tag is seen; a file with
         * no fences at all is walked as one pure XML document.      */
        if (fence_mode != 0 && line_is(line, "```xml")) {
            if (sp == 0 && !cur_root) { fence_mode = 1; in_fence = 1; }
            free(line);
            continue;
        }
        if (fence_mode == 1 && in_fence && line_is(line, "```")) {
            in_fence = 0;
            free(line);
            continue;
        }
        if (fence_mode == 1 && !in_fence) { free(line); continue; }
        if (fence_mode == -1) {
            /* no fence yet: prose lines carry no tags; only a real
             * tag decides pure-XML mode                           */
            size_t p0 = 0;
            char t0[256];
            int c0 = 0, k0 = 0;
            if (!next_tag(line, &p0, t0, sizeof t0, &c0, &k0)) {
                free(line);
                continue;
            }
            fence_mode = 0;
        }

        if (line_blank(line)) { free(line); continue; }

        if (in_comment) {
            char *e = strstr(line, "-->");
            if (!e) { free(line); continue; }
            in_comment = 0;
            pos = (size_t)(e - line) + 3;
        }

        while (next_tag(line, &pos, tag, sizeof tag, &is_close, &kind)) {
            if (kind == 1 || kind == 3) continue;
            if (kind == 2) {
                char *e = strstr(line + pos, "-->");
                if (e) pos = (size_t)(e - line) + 3;
                else { in_comment = 1; break; }
                continue;
            }
            if (!is_close && pos >= 2 && line[pos - 2] == '/') {
                Node *n = node_new(tag);
                if (!n || sp < 1 || attach(stack[sp - 1], n) != 0) {
                    fprintf(stderr, "BENCH: attach failed line %ld\n",
                            lineno);
                    node_free(n);
                    rc = 1; break;
                }
            } else if (!is_close && closes_on_line(line, &pos, tag)) {
                Node *n = node_new(tag);
                if (!n || sp < 1 || attach(stack[sp - 1], n) != 0) {
                    fprintf(stderr, "BENCH: attach failed line %ld\n",
                            lineno);
                    node_free(n);
                    rc = 1; break;
                }
            } else if (!is_close) {
                Node *n = node_new(tag);
                if (!n) { fprintf(stderr, "BENCH: memory\n"); rc = 1; break; }
                if (sp == stackcap) {
                    int ncap = stackcap == 0 ? 16 : stackcap * 2;
                    Node **ns = realloc(stack, (size_t)ncap * sizeof *ns);
                    if (!ns) {
                        fprintf(stderr, "BENCH: memory\n");
                        node_free(n); rc = 1; break;
                    }
                    stack = ns;
                    stackcap = ncap;
                }
                if (sp == 0) {
                    if (cur_root) {
                        fprintf(stderr,
                                "BENCH: unclosed root before line %ld\n",
                                lineno);
                        node_free(n); rc = 2; break;
                    }
                    cur_root = n;
                }
                stack[sp++] = n;
            } else {
                Node *done;
                if (sp <= 0 || strcmp(stack[sp - 1]->tag, tag) != 0) {
                    fprintf(stderr,
                            "BENCH: line %ld closes %s but expected %s\n",
                            lineno, tag,
                            sp > 0 ? stack[sp - 1]->tag
                                   : "(nothing open)");
                    rc = 2; break;
                }
                done = stack[--sp];
                if (sp == 0) {
                    cur_root = NULL;
                    documents++;
                    if (forest_add(done) != 0) {
                        fprintf(stderr, "BENCH: memory\n");
                        rc = 1; break;
                    }
                } else if (attach(stack[sp - 1], done) != 0) {
                    fprintf(stderr, "BENCH: attach failed line %ld\n",
                            lineno);
                    rc = 1; break;
                }
            }
        }
        free(line);
        if (rc != 0) break;
    }
    fclose(fp);

    if (rc == 0 && sp != 0) {
        fprintf(stderr, "BENCH: unbalanced, %d record(s) never closed\n", sp);
        rc = 2;
    }
    if (rc == 0 && g_nroots == 0) {
        fprintf(stderr, "BENCH: no records found\n");
        rc = 2;
    }

    if (rc == 0) {
        int i;
        for (i = 0; i < g_nroots; i++) {
            int same = 0, j, form = 0;
            for (j = 0; j < g_nroots; j++)
                if (strcmp(g_roots[j]->tag, g_roots[i]->tag) == 0) same++;
            for (j = 0; j < i; j++)
                if (strcmp(g_roots[j]->tag, g_roots[i]->tag) == 0) form++;
            if (same > 1)
                printf("<!-- <%s> form %d of %d: repeats=%ld -->\n",
                       g_roots[i]->tag, form + 1, same,
                       g_roots[i]->count);
            emit_template(g_roots[i]);
            putchar('\n');        /* space between templates */
        }
        printf("<!-- documents walked: %ld; distinct forms: %d -->\n",
               documents, g_nroots);
    }

    for (sp = 0; sp < g_nroots; sp++) node_free(g_roots[sp]);
    free(g_roots);
    free(stack);
    return rc;
}
