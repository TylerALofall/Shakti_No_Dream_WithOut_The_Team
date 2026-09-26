/* shakti_call_scan v1.0, 2026-09-26.  Build: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_call_scan_v1_0.c -o shakti_call_scan
 *
 * GAPS / CONTRACT (READ FIRST): This is a lexical C99 source scanner, not a C
 * compiler. It does not expand #include, macros, or conditional compilation.
 * Thus inactive branches are scanned and a declaration supplied only by an
 * included header has visibility DECLARATION_UNSEEN. Calls through pointers,
 * shadowed names, macro invocations, compiler attributes, and nonordinary
 * function declarators, trigraphs, and universal-character identifiers cannot
 * be resolved to a C symbol without more parsing and type analysis;
 * each ordinary identifier followed by '(' is retained as a lexical call
 * unless it is a recognized declaration or a C keyword. No edge is silently
 * dropped because its callee is absent from the definitions index. The line
 * for caller_line is the physical call-site line; callee_line is the definition
 * line (0 for EXTERNAL). Function order is 1-based within its source file.
 * Output adds *_order and visibility to the requested fixed CALL block because
 * the work order separately requires identity and prototype reliance.
 *
 * Platform: Linux renameat2(RENAME_NOREPLACE) provides atomic no-clobber
 * publication after the complete .tmp file is closed. Unsupported platforms
 * BENCH rather than falling back to a clobbering rename.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_FILES 1024U
#define MAX_PATH 1024U
#define MAX_SOURCE (16U * 1024U * 1024U)
#define MAX_TOKENS 500000U
#define MAX_NAME 128U
#define MAX_NEST 4096U
#define MAX_DEFS 8192U
#define MAX_PROTOS 16384U
#define MAX_CALLS 250000U
#define MAX_MANIFEST_LINE 4096U
#define MAX_MANIFEST_BYTES (2U * 1024U * 1024U)

enum { TK_ID = 1, TK_OTHER = 2, TK_LITERAL = 3, TK_NUMBER = 4 };
enum { MARK_NONE = 0, MARK_DECL = 1 };
enum { SAME_FILE = 1, CROSS_FILE = 2, EXTERNAL = 3 };
enum { DEFINITION_BEFORE_CALL = 1, PROTOTYPE_RELIANT = 2,
       DECLARATION_UNSEEN = 3, EXTERNAL_VISIBILITY = 4 };

typedef struct {
    char display[MAX_PATH];
    char disk[MAX_PATH];
    char section;
    unsigned def_count;
    size_t bytes;
    uint64_t fingerprint;
    unsigned scanned;
} SourceFile;

typedef struct {
    char name[MAX_NAME];
    unsigned line, mate, scope_open, scope_close;
    unsigned char kind, punct, mark;
} Token;

typedef struct {
    char name[MAX_NAME];
    unsigned file, line, order, name_tok, body_open, body_close;
    unsigned char is_static;
} Definition;

typedef struct {
    char name[MAX_NAME];
    unsigned file, line, name_tok, scope_open, scope_close;
} Prototype;

typedef struct {
    char callee[MAX_NAME];
    unsigned callee_def, caller_def, file, line, token_index;
    unsigned char kind, visibility;
} Call;

static SourceFile files[MAX_FILES];
static Token tok[MAX_TOKENS];
static Definition defs[MAX_DEFS];
static Prototype protos[MAX_PROTOS];
static Call calls[MAX_CALLS];
static char source[MAX_SOURCE + 1U];
static char manifest_text[MAX_MANIFEST_BYTES + 1U];
static unsigned file_n, tok_n, def_n, proto_n, call_n;
static unsigned same_n, cross_n, external_n;
static unsigned failed;

static int full_write(int fd, const char *data, size_t count) {
    size_t sent = 0U;
    while (sent < count) {
        ssize_t n = write(fd, data + sent, count - sent);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { if (n == 0) errno = EIO; return 0; }
        sent += (size_t)n;
    }
    return 1;
}

static int bench(const char *fmt, ...) {
    va_list ap;
    if (!failed) {
        char message[2048];
        int n;
        memcpy(message, "BENCH: ", 7U);
        va_start(ap, fmt);
        n = vsnprintf(message + 7U, sizeof(message) - 8U, fmt, ap);
        va_end(ap);
        if (n < 0) n = 0;
        if ((size_t)n > sizeof(message) - 9U)
            n = (int)(sizeof(message) - 9U);
        message[7U + (size_t)n] = '\n';
        (void)full_write(STDERR_FILENO, message, 8U + (size_t)n);
    }
    failed = 1U;
    return 0;
}

static int read_bounded(int fd, char *buffer, size_t cap, size_t *bytes,
                        const char *path) {
    size_t total = 0U;
    while (total < cap + 1U) {
        ssize_t n = read(fd, buffer + total, cap + 1U - total);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return bench("read %s: %s", path, strerror(errno));
        if (n == 0) break;
        total += (size_t)n;
    }
    if (total > cap) return bench("input cap %lu bytes in %s",
                                   (unsigned long)cap, path);
    *bytes = total;
    buffer[total] = '\0';
    return 1;
}

static int copy_bounded(char *dst, size_t cap, const char *src,
                        const char *what) {
    size_t n = strlen(src);
    if (n >= cap) return bench("%s exceeds %lu bytes", what,
                               (unsigned long)(cap - 1U));
    memcpy(dst, src, n + 1U);
    return 1;
}

static int is_ident_first(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_more(int c) {
    return is_ident_first(c) || (c >= '0' && c <= '9');
}

typedef struct {
    size_t at, bytes;
    unsigned line, bol;
    const char *path;
} Lexer;

/* Phase-2 backslash/newline splicing, including CRLF, with physical lines. */
static int peek(Lexer *lx) {
    while (lx->at < lx->bytes && source[lx->at] == '\\') {
        size_t p = lx->at + 1U;
        if (p < lx->bytes && source[p] == '\r') ++p;
        if (p < lx->bytes && source[p] == '\n') {
            lx->at = p + 1U;
            ++lx->line;
        } else break;
    }
    if (lx->at == lx->bytes) return -1;
    if (source[lx->at] == '\r') return '\n';
    return (unsigned char)source[lx->at];
}

static int take(Lexer *lx) {
    int c = peek(lx);
    if (c < 0) return -1;
    if (source[lx->at] == '\r' && lx->at + 1U < lx->bytes &&
        source[lx->at + 1U] == '\n') lx->at += 2U;
    else ++lx->at;
    if (c == '\n') ++lx->line;
    return c;
}

static int next_is(Lexer *lx, int c) {
    Lexer look = *lx;
    (void)take(&look);
    return peek(&look) == c;
}

static int add_token(unsigned kind, int punct, unsigned line,
                     const char *name, const char *path) {
    Token *t;
    if (tok_n >= MAX_TOKENS)
        return bench("token cap %u in %s", MAX_TOKENS, path);
    t = &tok[tok_n++];
    memset(t, 0, sizeof(*t));
    t->kind = (unsigned char)kind;
    t->punct = (unsigned char)punct;
    t->line = line;
    if (name && !copy_bounded(t->name, sizeof(t->name), name, "identifier"))
        return 0;
    return 1;
}

static int tokenize(unsigned file) {
    Lexer lx;
    int fd = open(files[file].disk, O_RDONLY);
    unsigned stack[MAX_NEST], depth = 0U;
    uint64_t fingerprint = UINT64_C(14695981039346656037);
    size_t i;
    if (fd < 0) return bench("open %s: %s", files[file].disk, strerror(errno));
    if (!read_bounded(fd, source, MAX_SOURCE, &lx.bytes, files[file].disk)) {
        (void)close(fd); return 0;
    }
    if (close(fd) != 0) return bench("close %s: %s", files[file].disk, strerror(errno));
    if (memchr(source, '\0', lx.bytes) != NULL)
        return bench("NUL byte in source %s", files[file].display);
    for (i = 0U; i < lx.bytes; ++i) {
        fingerprint ^= (unsigned char)source[i];
        fingerprint *= UINT64_C(1099511628211);
    }
    if (files[file].scanned) {
        if (files[file].bytes != lx.bytes ||
            files[file].fingerprint != fingerprint)
            return bench("source changed between passes: %s", files[file].display);
    } else {
        files[file].bytes = lx.bytes;
        files[file].fingerprint = fingerprint;
        files[file].scanned = 1U;
    }
    lx.at = 0U;
    lx.line = 1U;
    lx.bol = 1U;
    lx.path = files[file].display;
    tok_n = 0U;
    while (peek(&lx) >= 0) {
        int c = peek(&lx);
        unsigned line = lx.line;
        if (c == '\n') { (void)take(&lx); lx.bol = 1U; continue; }
        if (c == ' ' || c == '\t' || c == '\v' || c == '\f') {
            (void)take(&lx); continue;
        }
        if (c == '#' && lx.bol) {
            while ((c = peek(&lx)) >= 0 && c != '\n') (void)take(&lx);
            continue;
        }
        if (c == '/' && next_is(&lx, '/')) {
            (void)take(&lx); (void)take(&lx);
            while ((c = peek(&lx)) >= 0 && c != '\n') (void)take(&lx);
            continue;
        }
        if (c == '/' && next_is(&lx, '*')) {
            int closed = 0, saw_newline = 0;
            (void)take(&lx); (void)take(&lx);
            while ((c = peek(&lx)) >= 0) {
                if (c == '*' && next_is(&lx, '/')) {
                    (void)take(&lx); (void)take(&lx); closed = 1; break;
                }
                if (take(&lx) == '\n') saw_newline = 1;
            }
            if (!closed) return bench("unterminated comment %s:%u", lx.path, line);
            if (saw_newline) lx.bol = 1U;
            continue;
        }
        lx.bol = 0U;
        if (is_ident_first(c)) {
            char name[MAX_NAME];
            size_t n = 0U;
            while (is_ident_more(peek(&lx))) {
                if (n + 1U >= sizeof(name))
                    return bench("identifier cap %u at %s:%u", MAX_NAME - 1U, lx.path, line);
                name[n++] = (char)take(&lx);
            }
            name[n] = '\0';
            if (!add_token(TK_ID, 0, line, name, lx.path)) return 0;
        } else if (c == '"' || c == '\'') {
            int quote = take(&lx), closed = 0;
            while ((c = peek(&lx)) >= 0) {
                if (c == '\n') break;
                c = take(&lx);
                if (c == '\\') {
                    if (peek(&lx) < 0) break;
                    (void)take(&lx);
                } else if (c == quote) { closed = 1; break; }
            }
            if (!closed) return bench("unterminated literal %s:%u", lx.path, line);
            if (!add_token(TK_LITERAL, 0, line, NULL, lx.path)) return 0;
        } else if (c >= '0' && c <= '9') {
            (void)take(&lx);
            while ((c = peek(&lx)) >= 0 &&
                   (is_ident_more(c) || c == '.' || c == '\'')) (void)take(&lx);
            if (!add_token(TK_NUMBER, 0, line, NULL, lx.path)) return 0;
        } else {
            (void)take(&lx);
            if (c == '-' && peek(&lx) == '>') {
                (void)take(&lx); c = '@'; /* member access, distinct from '-' */
            }
            if (!add_token(TK_OTHER, c, line, NULL, lx.path)) return 0;
        }
    }
    for (i = 0U; i < tok_n; ++i) {
        int c = tok[i].punct;
        if (c == '(' || c == '[' || c == '{') {
            if (depth >= MAX_NEST)
                return bench("nesting cap %u at %s:%u", MAX_NEST, lx.path, tok[i].line);
            stack[depth++] = (unsigned)i;
        } else if (c == ')' || c == ']' || c == '}') {
            unsigned open;
            if (!depth) return bench("unbalanced delimiter %s:%u", lx.path, tok[i].line);
            open = stack[--depth];
            if ((c == ')' && tok[open].punct != '(') ||
                (c == ']' && tok[open].punct != '[') ||
                (c == '}' && tok[open].punct != '{'))
                return bench("mismatched delimiter %s:%u", lx.path, tok[i].line);
            tok[open].mate = (unsigned)i;
            tok[i].mate = open;
        }
    }
    if (depth) return bench("unclosed delimiter %s:%u", lx.path, tok[stack[depth - 1U]].line);
    return 1;
}

static int ident(unsigned i, const char *s) {
    return tok[i].kind == TK_ID && strcmp(tok[i].name, s) == 0;
}

static int keyword(unsigned i) {
    static const char *const words[] = {
        "if", "else", "for", "while", "do", "switch", "case", "default",
        "return", "sizeof", "goto", "break", "continue", "typedef",
        "struct", "union", "enum", "_Alignof", "_Generic", "_Static_assert"
    };
    size_t j;
    for (j = 0U; j < sizeof(words) / sizeof(words[0]); ++j)
        if (ident(i, words[j])) return 1;
    return 0;
}

/* A declaration needs a type/specifier before the ordinary name(...). */
static int prefix_ok(unsigned start, unsigned name, int *is_static) {
    unsigned i;
    int seen_id = 0;
    *is_static = 0;
    if (name <= start) return 0;
    for (i = start; i < name; ++i) {
        if (tok[i].punct == '=' || tok[i].punct == '.' || tok[i].punct == '@')
            return 0;
        if (ident(i, "typedef") || ident(i, "return")) return 0;
        if (ident(i, "static")) *is_static = 1;
        if (tok[i].kind == TK_ID) seen_id = 1;
    }
    if (tok[name - 1U].punct == '*' || tok[name - 1U].kind == TK_ID)
        return seen_id;
    return 0;
}

static int add_definition(unsigned file, unsigned name, unsigned body,
                          int is_static) {
    unsigned j;
    Definition *d;
    for (j = 0U; j < def_n; ++j)
        if (defs[j].file == file && strcmp(defs[j].name, tok[name].name) == 0)
            return bench("duplicate definition %s in %s", tok[name].name,
                         files[file].display);
    if (def_n >= MAX_DEFS) return bench("definition cap %u", MAX_DEFS);
    d = &defs[def_n++];
    memset(d, 0, sizeof(*d));
    (void)copy_bounded(d->name, sizeof(d->name), tok[name].name, "definition");
    d->file = file;
    d->line = tok[name].line;
    d->order = ++files[file].def_count;
    d->name_tok = name;
    d->body_open = body;
    d->body_close = tok[body].mate;
    d->is_static = (unsigned char)is_static;
    tok[name].mark = MARK_DECL;
    return 1;
}

static int add_prototype(unsigned file, unsigned name, unsigned scope_open,
                         unsigned scope_close) {
    Prototype *p;
    if (proto_n >= MAX_PROTOS) return bench("prototype cap %u", MAX_PROTOS);
    p = &protos[proto_n++];
    memset(p, 0, sizeof(*p));
    (void)copy_bounded(p->name, sizeof(p->name), tok[name].name, "prototype");
    p->file = file;
    p->line = tok[name].line;
    p->name_tok = name;
    p->scope_open = scope_open;
    p->scope_close = scope_close;
    tok[name].mark = MARK_DECL;
    return 1;
}

static void mark_signature(unsigned begin, unsigned end) {
    unsigned i;
    for (i = begin; i < end; ++i)
        if (tok[i].kind == TK_ID && i + 1U < end && tok[i + 1U].punct == '(')
            tok[i].mark = MARK_DECL;
}

static int index_file(unsigned file) {
    unsigned i, depth = 0U, parens = 0U, scope[MAX_NEST];
    unsigned start[MAX_NEST + 1U];
    start[0] = 0U;
    /* Function bodies: an ordinary declarator immediately before top brace. */
    for (i = 0U; i < tok_n; ++i) {
        if (tok[i].punct == '{') {
            if (depth == 0U && i > 2U && tok[i - 1U].punct == ')') {
                unsigned open = tok[i - 1U].mate;
                int is_static = 0;
                if (open > start[0] && tok[open - 1U].kind == TK_ID &&
                    !keyword(open - 1U) &&
                    prefix_ok(start[0], open - 1U, &is_static)) {
                    if (!add_definition(file, open - 1U, i, is_static)) return 0;
                    mark_signature(open, i);
                }
            }
            ++depth;
        } else if (tok[i].punct == '}') {
            if (--depth == 0U) start[0] = i + 1U;
        } else if (tok[i].punct == ';' && depth == 0U) start[0] = i + 1U;
    }
    /* Every simple declaration statement, at file or block scope. */
    depth = 0U;
    start[0] = 0U;
    for (i = 0U; i < tok_n; ++i) {
        unsigned c = tok[i].punct;
        if (c == '{') {
            scope[depth++] = i;
            start[depth] = i + 1U;
        } else if (c == '}') {
            --depth;
            start[depth] = i + 1U;
        } else if (c == '(') ++parens;
        else if (c == ')') --parens;
        else if (c == ';' && parens == 0U) {
            unsigned j, in_parens = 0U;
            int dummy;
            for (j = start[depth]; j < i; ++j) {
                if (tok[j].punct == '(') ++in_parens;
                else if (tok[j].punct == ')') --in_parens;
                if (in_parens || tok[j].kind != TK_ID || keyword(j) ||
                    j + 1U >= i || tok[j + 1U].punct != '(' ||
                    tok[j + 1U].mate >= i || tok[j].mark) continue;
                if (prefix_ok(start[depth], j, &dummy)) {
                    if (!add_prototype(file, j, depth ? scope[depth - 1U] : 0U,
                                       depth ? tok[scope[depth - 1U]].mate : tok_n))
                        return 0;
                    mark_signature(j + 1U, tok[j + 1U].mate + 1U);
                }
            }
            start[depth] = i + 1U;
        }
    }
    return 1;
}

static int restore_marks(unsigned file) {
    unsigned j;
    for (j = 0U; j < def_n; ++j) {
        const Definition *d = &defs[j];
        if (d->file != file) continue;
        if (d->name_tok >= tok_n || d->body_open >= tok_n ||
            !ident(d->name_tok, d->name) ||
            tok[d->body_open].mate != d->body_close ||
            tok[d->name_tok].line != d->line)
            return bench("source changed between passes: %s", files[file].display);
        tok[d->name_tok].mark = MARK_DECL;
        mark_signature(d->name_tok + 1U, d->body_open);
    }
    for (j = 0U; j < proto_n; ++j) {
        const Prototype *p = &protos[j];
        if (p->file != file) continue;
        if (p->name_tok >= tok_n || !ident(p->name_tok, p->name) ||
            tok[p->name_tok].line != p->line ||
            p->name_tok + 1U >= tok_n || tok[p->name_tok + 1U].punct != '(')
            return bench("source changed between passes: %s", files[file].display);
        tok[p->name_tok].mark = MARK_DECL;
        mark_signature(p->name_tok + 1U, tok[p->name_tok + 1U].mate + 1U);
    }
    return 1;
}

static unsigned enclosing(unsigned file, unsigned token_index) {
    unsigned j;
    for (j = 0U; j < def_n; ++j)
        if (defs[j].file == file && token_index > defs[j].body_open &&
            token_index < defs[j].body_close) return j + 1U;
    return 0U;
}

static unsigned resolve(unsigned file, const char *name) {
    unsigned j, global = 0U;
    for (j = 0U; j < def_n; ++j) {
        if (strcmp(defs[j].name, name) != 0) continue;
        if (defs[j].file == file) return j + 1U;
        if (!defs[j].is_static) {
            if (global) {
                (void)bench("ambiguous global definition %s in %s and %s",
                            name, files[defs[global - 1U].file].display,
                            files[defs[j].file].display);
                return 0U;
            }
            global = j + 1U;
        }
    }
    return global;
}

static unsigned visibility_for(unsigned file, unsigned token_index,
                               const char *name, unsigned def) {
    unsigned j;
    if (!def) return EXTERNAL_VISIBILITY;
    if (defs[def - 1U].file == file &&
        defs[def - 1U].name_tok < token_index) return DEFINITION_BEFORE_CALL;
    for (j = 0U; j < proto_n; ++j)
        if (protos[j].file == file &&
            strcmp(protos[j].name, name) == 0 &&
            protos[j].name_tok < token_index &&
            (protos[j].scope_open == 0U ||
             (protos[j].scope_open < token_index &&
              token_index < protos[j].scope_close))) return PROTOTYPE_RELIANT;
    return DECLARATION_UNSEEN;
}

static int calls_file(unsigned file) {
    unsigned i;
    if (!tokenize(file)) return 0;
    if (!restore_marks(file)) return 0;
    for (i = 0U; i + 1U < tok_n; ++i) {
        unsigned d;
        Call *c;
        if (tok[i].kind != TK_ID || tok[i + 1U].punct != '(' ||
            tok[i].mark || keyword(i)) continue;
        if (call_n >= MAX_CALLS) return bench("call cap %u", MAX_CALLS);
        d = resolve(file, tok[i].name);
        if (failed) return 0;
        c = &calls[call_n++];
        memset(c, 0, sizeof(*c));
        (void)copy_bounded(c->callee, sizeof(c->callee), tok[i].name, "callee");
        c->file = file;
        c->line = tok[i].line;
        c->token_index = i;
        c->callee_def = d;
        c->caller_def = enclosing(file, i);
        c->kind = (unsigned char)(!d ? EXTERNAL :
                    defs[d - 1U].file == file ? SAME_FILE : CROSS_FILE);
        c->visibility = (unsigned char)visibility_for(file, i, c->callee, d);
        if (c->kind == SAME_FILE) ++same_n;
        else if (c->kind == CROSS_FILE) ++cross_n;
        else ++external_n;
    }
    return 1;
}

static const char *callee_file(const Call *c) {
    return c->callee_def ? files[defs[c->callee_def - 1U].file].display : "EXTERNAL";
}

static int compare_calls(const void *pa, const void *pb) {
    const Call *a = pa, *b = pb;
    unsigned al = a->callee_def ? defs[a->callee_def - 1U].line : 0U;
    unsigned bl = b->callee_def ? defs[b->callee_def - 1U].line : 0U;
    int v = strcmp(callee_file(a), callee_file(b));
    if (v) return v;
    if (al != bl) return al < bl ? -1 : 1;
    v = strcmp(files[a->file].display, files[b->file].display);
    if (v) return v;
    if (a->line != b->line) return a->line < b->line ? -1 : 1;
    v = strcmp(a->callee, b->callee);
    if (v) return v;
    if (a->token_index != b->token_index)
        return a->token_index < b->token_index ? -1 : 1;
    return 0;
}

/* In-place heapsort: deterministic and no allocation inside a library sort. */
static void sift_calls(unsigned root, unsigned count) {
    for (;;) {
        unsigned child;
        Call swap;
        if (root > (count - 1U) / 2U) return;
        child = root * 2U + 1U;
        if (child >= count) return;
        if (child + 1U < count &&
            compare_calls(&calls[child], &calls[child + 1U]) < 0) ++child;
        if (compare_calls(&calls[root], &calls[child]) >= 0) return;
        swap = calls[root]; calls[root] = calls[child]; calls[child] = swap;
        root = child;
    }
}

static void sort_calls(void) {
    unsigned i;
    if (call_n < 2U) return;
    for (i = call_n / 2U; i > 0U; --i) sift_calls(i - 1U, call_n);
    for (i = call_n - 1U; i > 0U; --i) {
        Call swap = calls[0];
        calls[0] = calls[i]; calls[i] = swap;
        sift_calls(0U, i);
    }
}

static const char *visibility_name(unsigned v) {
    switch (v) {
        case DEFINITION_BEFORE_CALL: return "DEFINITION_BEFORE_CALL";
        case PROTOTYPE_RELIANT: return "PROTOTYPE_RELIANT";
        case DECLARATION_UNSEEN: return "DECLARATION_UNSEEN";
        default: return "EXTERNAL";
    }
}

static int write_records(const char *output) {
    char temporary[MAX_PATH];
    char record[4096];
    int fd, saved;
    unsigned i;
    size_t n = strlen(output);
    if (n + 4U >= sizeof(temporary)) return bench("output path cap %u", MAX_PATH - 5U);
    memcpy(temporary, output, n);
    memcpy(temporary + n, ".tmp", 5U);
    fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return bench("reserve %s: %s", temporary, strerror(errno));
    for (i = 0U; i < call_n; ++i) {
        const Call *c = &calls[i];
        const Definition *d = c->callee_def ? &defs[c->callee_def - 1U] : NULL;
        const Definition *r = c->caller_def ? &defs[c->caller_def - 1U] : NULL;
        int length = snprintf(record, sizeof(record),
            "CALL\ncallee=%s\ncallee_file=%s\ncallee_line=%u\ncallee_order=%u\n"
            "caller=%s\ncaller_file=%s\ncaller_line=%u\ncaller_order=%u\n"
            "kind=%s\nvisibility=%s\nEND\n",
            c->callee, callee_file(c), d ? d->line : 0U, d ? d->order : 0U,
            r ? r->name : "GLOBAL_SCOPE", files[c->file].display, c->line,
            r ? r->order : 0U,
            c->kind == SAME_FILE ? "SAME_FILE" :
            c->kind == CROSS_FILE ? "CROSS_FILE" : "EXTERNAL",
            visibility_name(c->visibility));
        if (length < 0 || (size_t)length >= sizeof(record)) {
            (void)close(fd); (void)unlink(temporary);
            return bench("record buffer cap %lu", (unsigned long)sizeof(record));
        }
        if (!full_write(fd, record, (size_t)length)) {
            saved = errno; (void)close(fd); (void)unlink(temporary);
            return bench("write %s: %s", temporary, strerror(saved));
        }
    }
    {
        int length = snprintf(record, sizeof(record),
        "SCANNED: %u files, %u definitions, %u prototypes, %u call sites "
        "(%u same-file, %u cross-file, %u external)\n",
        file_n, def_n, proto_n, call_n, same_n, cross_n, external_n);
        if (length < 0 || (size_t)length >= sizeof(record)) {
            (void)close(fd); (void)unlink(temporary);
            return bench("summary buffer cap %lu", (unsigned long)sizeof(record));
        }
        if (!full_write(fd, record, (size_t)length)) {
            saved = errno; (void)close(fd); (void)unlink(temporary);
            return bench("write %s: %s", temporary, strerror(saved));
        }
    }
    if (fsync(fd) != 0) {
        saved = errno; (void)close(fd); (void)unlink(temporary);
        return bench("sync %s: %s", temporary, strerror(saved));
    }
    if (close(fd) != 0) {
        (void)unlink(temporary);
        return bench("close %s: %s", temporary, strerror(saved));
    }
    if (renameat2(AT_FDCWD, temporary, AT_FDCWD, output,
                  RENAME_NOREPLACE) == 0) return 1;
    saved = errno;
    (void)unlink(temporary);
    return bench("publish %s without overwrite: %s", output, strerror(saved));
}

static int read_manifest(const char *manifest) {
    int fd = open(manifest, O_RDONLY);
    char line[MAX_MANIFEST_LINE], directory[MAX_PATH];
    const char *slash = strrchr(manifest, '/');
    unsigned number = 0U;
    char previous = 'A';
    size_t bytes, at = 0U;
    if (fd < 0) return bench("open manifest %s: %s", manifest, strerror(errno));
    if (slash) {
        size_t len = (size_t)(slash - manifest) + 1U;
        if (len >= sizeof(directory)) {
            (void)close(fd); return bench("manifest directory cap %u", MAX_PATH - 1U);
        }
        memcpy(directory, manifest, len);
        directory[len] = '\0';
    } else directory[0] = '\0';
    if (!read_bounded(fd, manifest_text, MAX_MANIFEST_BYTES, &bytes, manifest)) {
        (void)close(fd); return 0;
    }
    if (close(fd) != 0) return bench("close manifest: %s", strerror(errno));
    if (memchr(manifest_text, '\0', bytes)) return bench("NUL byte in manifest");
    while (at < bytes) {
        char *p = line, *end;
        size_t n, begin = at;
        unsigned j;
        while (at < bytes && manifest_text[at] != '\n') ++at;
        ++number;
        n = at - begin;
        if (n >= sizeof(line)) {
            return bench("manifest line cap %u at line %u",
                         MAX_MANIFEST_LINE - 1U, number);
        }
        memcpy(line, manifest_text + begin, n);
        line[n] = '\0';
        if (at < bytes) ++at;
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p || *p == '\r' || *p == '\n' || *p == '#') continue;
        if (*p < 'A' || *p > 'O' || (p[1] != ' ' && p[1] != '\t')) {
            return bench("manifest line %u: expected SECTION_LETTER PATH", number);
        }
        if (*p < previous) {
            return bench("manifest line %u: sections out of A..O order", number);
        }
        previous = *p++;
        while (*p == ' ' || *p == '\t') ++p;
        end = p + strlen(p);
        while (end > p && isspace((unsigned char)end[-1])) --end;
        *end = '\0';
        n = strlen(p);
        if (n < 3U || strcmp(p + n - 2U, ".c") != 0) {
            return bench("manifest line %u: expected .c path", number);
        }
        for (j = 0U; j < n; ++j) {
            if ((unsigned char)p[j] < 32U || (unsigned char)p[j] == 127U) {
                return bench("manifest line %u: control byte in path", number);
            }
        }
        if (file_n >= MAX_FILES) return bench("file cap %u", MAX_FILES);
        for (j = 0U; j < file_n; ++j) {
            if (strcmp(files[j].display, p) == 0) {
                return bench("duplicate manifest path %s", p);
            }
        }
        files[file_n].section = previous;
        if (!copy_bounded(files[file_n].display, MAX_PATH, p, "display path")) return 0;
        if (p[0] == '/') {
            if (!copy_bounded(files[file_n].disk, MAX_PATH, p, "disk path")) return 0;
        } else {
            size_t prefix = strlen(directory);
            if (prefix + n >= MAX_PATH) return bench("resolved path cap %u", MAX_PATH - 1U);
            memcpy(files[file_n].disk, directory, prefix);
            memcpy(files[file_n].disk + prefix, p, n + 1U);
        }
        ++file_n;
    }
    if (!file_n) return bench("empty manifest");
    return 1;
}

int main(int argc, char **argv) {
    unsigned i;
    if (argc != 3) {
        const char *usage = "usage: shakti_call_scan MANIFEST NEW_OUTPUT\n";
        (void)full_write(STDERR_FILENO, usage, strlen(usage));
        return 2;
    }
    if (!read_manifest(argv[1])) return 2;
    for (i = 0U; i < file_n; ++i) {
        if (!tokenize(i) || !index_file(i)) return 2;
    }
    /* Pass 2 retokenizes; index_file here marks declarations, without adding
       duplicate entries. An independent marking function handles this below. */
    for (i = 0U; i < file_n; ++i)
        if (!calls_file(i)) return 2;
    sort_calls();
    if (!write_records(argv[2])) return 2;
    {
        char summary[256];
        int length = snprintf(summary, sizeof(summary),
                              "SCANNED: %u files, %u definitions, %u prototypes, "
                              "%u call sites (%u same-file, %u cross-file, %u external)\n",
                              file_n, def_n, proto_n, call_n,
                              same_n, cross_n, external_n);
        if (length < 0 || (size_t)length >= sizeof(summary) ||
            !full_write(STDOUT_FILENO, summary, (size_t)length)) {
            (void)bench("write summary to stdout: %s", strerror(errno));
            return 2;
        }
    }
    return 0;
}
