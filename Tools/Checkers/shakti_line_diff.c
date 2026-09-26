/*
 * shakti-line-diff - Part 3 of 7: THE LINE FLAGGER (v2)
 *
 * Logic (THE LOGIC SS7):
 *   "Memory only fills the lines that are wrong, flags them, then moves on."
 *   - Matching lines are dropped the instant they match; a differing line is
 *     written and forgotten. Nothing accumulates.
 *   - It FLAGS. It never fixes and never picks a side; the reviewer decides.
 *   - Prints line number, column, excerpt. Log opened for append.
 *   - The rejected v3 two-edge bracket stays rejected: this is v2, two files,
 *     one pass, flags only.
 *
 * Usage: shakti-line-diff <old> <new> [logfile]
 * stdout, one flag per differing line:
 *   FLAG line=<n> col=<c>
 *     - <old excerpt>
 *     + <new excerpt>
 * A missing line on either side is flagged as EMPTY. Exit code is always 0
 * when both files could be read: flagging is not failure, it is the work list.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *msg)
{
    fprintf(stderr, "shakti-line-diff: %s\n", msg);
    exit(1);
}

/* Read one line (without the newline) into a growable buffer.
 * Returns 1 if a line was read, 0 at clean EOF. */
static int read_line(FILE *f, char **buf, size_t *cap)
{
    int c;
    size_t n = 0;
    if (*buf == NULL) {
        *cap = 256;
        *buf = malloc(*cap);
        if (!*buf) die("out of memory");
    }
    while ((c = fgetc(f)) != EOF) {
        if (n + 1 >= *cap) {
            char *nb;
            *cap *= 2;
            nb = realloc(*buf, *cap);
            if (!nb) die("out of memory");
            *buf = nb;
        }
        if (c == '\n') break;
        (*buf)[n++] = (char)c;
    }
    if (c == EOF && n == 0) return 0;
    (*buf)[n] = '\0';
    return 1;
}

static void excerpt(const char *line, char *out, size_t outsz)
{
    size_t n = strlen(line);
    if (n > outsz - 4) {
        memcpy(out, line, outsz - 4);
        strcpy(out + (outsz - 4), "...");
    } else {
        strcpy(out, line);
    }
}

int main(int argc, char **argv)
{
    FILE *fo, *fn, *log = NULL;
    char *lo = NULL, *ln = NULL;
    size_t co = 0, cn = 0;
    unsigned long lineno = 0, flags = 0;
    int moreo, moren;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: shakti-line-diff <old> <new> [logfile]\n");
        return 2;
    }
    fo = fopen(argv[1], "rb");
    if (!fo) { fprintf(stderr, "shakti-line-diff: cannot open %s\n", argv[1]); return 1; }
    fn = fopen(argv[2], "rb");
    if (!fn) { fprintf(stderr, "shakti-line-diff: cannot open %s\n", argv[2]); fclose(fo); return 1; }
    if (argc == 4) {
        log = fopen(argv[3], "ab");
        if (!log) { fprintf(stderr, "shakti-line-diff: cannot append %s\n", argv[3]); fclose(fo); fclose(fn); return 1; }
    }

    for (;;) {
        moreo = read_line(fo, &lo, &co);
        moren = read_line(fn, &ln, &cn);
        if (!moreo && !moren) break;
        lineno++;
        if (moreo && moren && strcmp(lo, ln) == 0)
            continue; /* matching line dropped the instant it matches */
        {
            unsigned long col = 1;
            const char *a = moreo ? lo : "";
            const char *b = moren ? ln : "";
            char ea[120], eb[120];
            while (a[col - 1] && b[col - 1] && a[col - 1] == b[col - 1]) col++;
            excerpt(a, ea, sizeof ea);
            excerpt(b, eb, sizeof eb);
            printf("FLAG line=%lu col=%lu\n  - %s\n  + %s\n", lineno, col, ea, eb);
            if (log)
                fprintf(log, "FLAG line=%lu col=%lu\n  - %s\n  + %s\n", lineno, col, ea, eb);
            flags++;
        }
    }

    fclose(fo);
    fclose(fn);
    if (log) fclose(log);
    free(lo);
    free(ln);
    fprintf(stderr, "shakti-line-diff: %lu lines compared, %lu flagged\n", lineno, flags);
    return 0;
}
