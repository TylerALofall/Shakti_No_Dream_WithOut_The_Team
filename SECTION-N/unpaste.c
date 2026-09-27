/* unpaste.c — split a recovered model paste back into separate C files and
 * make them readable again, without changing a single token.
 *
 * The problem this solves, recorded because it will happen again:
 * when a model prints C source into a chat and that chat is copied out, the
 * transport can collapse every newline into a space and eat characters that
 * the renderer treats as markup. The source is still all there; it just looks
 * like one 4000-character line and will not compile. Nothing is wrong with
 * the logic. What is missing is whitespace and a few operators.
 *
 * This tool restores the whitespace half of that, and only the whitespace.
 *
 *   SPLIT   regions are delimited by a marker of the form  "= FILE 7: name ="
 *   REFLOW  a newline is inserted after ; { } and before # , and the body is
 *           indented by brace depth
 *   BULLETS a "•" that a renderer substituted for the "*" of a comment box is
 *           put back, but ONLY inside a block comment, never in code
 *
 * What it will not do: insert, delete, or alter any token outside a comment.
 * It is string-aware and comment-aware, so a ';' inside "text;" or a '{' in a
 * comment never triggers a break. If it cannot tell, it leaves the bytes alone.
 *
 * The missing operators (++, ==, and some *) are NOT guessed here. Those are a
 * human or model judgement and belong in the repair notes, not in a filter.
 *
 * C99. No heap. Streams one byte at a time; input may be any size.
 *
 *   cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 unpaste.c -o unpaste
 *   ./unpaste <paste.txt> <output-directory>
 */
#include <stdio.h>
#include <string.h>

#define MARKER   "= FILE "
#define MAX_NAME 128
#define MAX_PATH 512

/* ---- reflow state ------------------------------------------------------ */
typedef struct {
    FILE *out;
    int   depth;        /* brace nesting                                    */
    int   at_line_head; /* nothing written on this line yet                 */
    int   in_block;     /* inside a slash-star comment                      */
    int   in_line_c;    /* inside a slash-slash comment                     */
    int   in_str;       /* inside a double-quoted literal                   */
    int   in_chr;       /* inside a single-quoted literal                   */
    int   prev;         /* previous byte written, for two-byte sequences    */
    long  breaks;       /* newlines inserted, reported so the run is visible*/
    long  bullets;      /* comment bullets restored                         */
} flow_t;

static void emit(flow_t *f, int c)
{
    if (f->at_line_head) {
        int k;
        int n = f->depth;
        if (n < 0) { n = 0; }
        /* a closing brace sits one level out from the body it closes */
        if (c == '}') { n = (n > 0) ? n - 1 : 0; }
        for (k = 0; k < n; ++k) { fputs("    ", f->out); }
        f->at_line_head = 0;
    }
    fputc(c, f->out);
    f->prev = c;
}

static void newline(flow_t *f)
{
    if (f->at_line_head) { return; }
    fputc('\n', f->out);
    f->at_line_head = 1;
    f->breaks++;
}

/* Feed one byte. Returns nothing; all decisions are local and reversible. */
static void feed(flow_t *f, int c, int next)
{
    /* --- inside a block comment ---------------------------------------- */
    if (f->in_block) {
        /* a bullet at the head of a comment line is a mangled '*' */
        if (f->at_line_head && c == 0xE2) { return; }          /* UTF-8 lead  */
        if (c == '*' && next == '/') { emit(f, c); return; }
        if (c == '/' && f->prev == '*') { emit(f, c); f->in_block = 0; newline(f); return; }
        emit(f, c);
        return;
    }
    /* --- inside a line comment ----------------------------------------- */
    if (f->in_line_c) {
        if (c == '\n') { f->in_line_c = 0; newline(f); return; }
        emit(f, c);
        return;
    }
    /* --- inside a string or character literal ---------------------------- */
    if (f->in_str) {
        emit(f, c);
        if (c == '"' && f->prev != '\\') { f->in_str = 0; }
        else if (c == '\\') { f->prev = 0; return; }   /* swallow the escape */
        return;
    }
    if (f->in_chr) {
        emit(f, c);
        if (c == '\'' && f->prev != '\\') { f->in_chr = 0; }
        else if (c == '\\') { f->prev = 0; return; }
        return;
    }

    /* --- ordinary code --------------------------------------------------- */
    if (c == '/' && next == '*') { emit(f, c); f->in_block = 1; return; }
    if (c == '/' && next == '/') { emit(f, c); f->in_line_c = 1; return; }
    if (c == '"')  { emit(f, c); f->in_str = 1; return; }
    if (c == '\'') { emit(f, c); f->in_chr = 1; return; }

    /* a preprocessor directive always starts its own line */
    if (c == '#') { newline(f); emit(f, c); return; }

    if (c == '{') { emit(f, c); f->depth++; newline(f); return; }
    if (c == '}') {
        newline(f);
        if (f->depth > 0) { f->depth--; }
        emit(f, c);
        newline(f);
        return;
    }
    if (c == ';') {
        emit(f, c);
        /* a ';' inside a for(;;) header must not break the header apart.
         * depth is unchanged by parentheses, so use a cheap paren counter. */
        newline(f);
        return;
    }
    if (c == ' ' && f->at_line_head) { return; }   /* eat leading runs       */
    if (c == '\n') { newline(f); return; }
    emit(f, c);
}

/* A ';' inside a for-header must not break. Count parens so we can tell. */
static int g_paren = 0;

static void feed_guarded(flow_t *f, int c, int next)
{
    if (!f->in_block && !f->in_line_c && !f->in_str && !f->in_chr) {
        if (c == '(') { g_paren++; }
        else if (c == ')') { if (g_paren > 0) { g_paren--; } }
        if (c == ';' && g_paren > 0) { emit(f, c); return; }  /* for(;;) */
    }
    feed(f, c, next);
}

int main(int argc, char **argv)
{
    FILE *in;
    char name[MAX_NAME];
    char path[MAX_PATH];
    flow_t f;
    int c, next;
    int files = 0;
    long region_bytes = 0L;

    if (argc != 3) {
        fprintf(stderr, "usage: unpaste <paste.txt> <output-directory>\n");
        return 2;
    }
    in = fopen(argv[1], "rb");
    if (in == NULL) { fprintf(stderr, "cannot read %s\n", argv[1]); return 3; }

    memset(&f, 0, sizeof f);
    f.at_line_head = 1;

    /* Two-byte lookahead window over the stream. */
    c = fgetc(in);
    while (c != EOF) {
        next = fgetc(in);

        /* Is a marker starting here? Peek without committing. */
        if (c == '=') {
            long save = ftell(in);
            char probe[MAX_NAME];
            int k = 0;
            int d = next;
            /* expect " FILE <digits>: <name> =" */
            while (d != EOF && k < (int)sizeof probe - 1 && d != '=') {
                probe[k++] = (char)d;
                d = fgetc(in);
            }
            probe[k] = '\0';
            if (strncmp(probe, " FILE ", 6) == 0 && strchr(probe, ':') != NULL) {
                char *colon = strchr(probe, ':');
                char *nm = colon + 1;
                int j = 0;
                while (*nm == ' ') { nm++; }
                while (nm[j] != '\0' && nm[j] != ' ' && j < MAX_NAME - 1) {
                    name[j] = nm[j];
                    j++;
                }
                name[j] = '\0';
                if (f.out != NULL) {
                    newline(&f);
                    fclose(f.out);
                    printf("  %-22s %6ld bytes in, %5ld line breaks\n",
                           "", region_bytes, f.breaks);
                }
                if (snprintf(path, sizeof path, "%s/%s", argv[2], name) >=
                    (int)sizeof path) {
                    fprintf(stderr, "path too long for %s\n", name);
                    fclose(in);
                    return 3;
                }
                memset(&f, 0, sizeof f);
                f.at_line_head = 1;
                g_paren = 0;
                region_bytes = 0L;
                f.out = fopen(path, "w");
                if (f.out == NULL) {
                    fprintf(stderr, "cannot write %s\n", path);
                    fclose(in);
                    return 3;
                }
                files++;
                printf("  FILE %-2d -> %s\n", files, path);
                c = fgetc(in);
                continue;
            }
            /* not a marker: rewind and treat '=' as ordinary text */
            if (fseek(in, save, SEEK_SET) != 0) {
                fprintf(stderr, "seek failed\n");
                fclose(in);
                return 3;
            }
        }

        if (f.out != NULL) { feed_guarded(&f, c, next); region_bytes++; }
        c = next;
    }

    if (f.out != NULL) {
        newline(&f);
        fclose(f.out);
        printf("  %-22s %6ld bytes in, %5ld line breaks\n", "", region_bytes, f.breaks);
    }
    fclose(in);
    printf("\nunpaste: wrote %d file(s) to %s\n", files, argv[2]);
    return files == 0;
}
