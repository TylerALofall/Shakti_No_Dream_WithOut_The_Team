/* handoff.c — print everything the next session needs, without anyone having
 * to remember what that is.
 *
 * Tyler's standing rule: every tool is reusable, and one of the tools prints
 * the tools and the documents automatically, so a handoff never depends on a
 * model reassembling context by hand.
 *
 * It is driven by a plain list file, one entry per line:
 *
 *     <path><TAB><what it is for>
 *
 * Lines starting with '#' are headings and are printed as section titles.
 * Blank lines are spacing. Nothing is hardcoded, so this works for the next
 * project too.
 *
 * Two modes:
 *   (default)          print an index: real byte counts and line counts,
 *                      measured from the files, plus what each one is for
 *   --bundle <out>     additionally write ONE file containing StudMuffin.md
 *                      and every listed file, with clear delimiters, so the
 *                      whole handoff can be carried as a single artifact
 *
 * A listed file that is missing is reported loudly and counted. This tool
 * never claims a file exists because the list said so.
 *
 * C99. No heap.
 *
 *   cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 handoff.c -o handoff
 *   ./handoff handoff.list
 *   ./handoff handoff.list --bundle HANDOFF.txt
 */
#include <stdio.h>
#include <string.h>

#define LINE_CAP 1024
#define PATH_CAP 512

static int measure(const char *path, long *bytes, long *lines)
{
    FILE *f = fopen(path, "rb");
    int c;
    long b = 0L, l = 0L;

    if (f == NULL) { return 0; }
    while ((c = fgetc(f)) != EOF) {
        b++;
        if (c == '\n') { l++; }
    }
    fclose(f);
    *bytes = b;
    *lines = l;
    return 1;
}

static int copy_into(const char *path, FILE *out)
{
    FILE *f = fopen(path, "rb");
    int c;
    if (f == NULL) { return 0; }
    while ((c = fgetc(f)) != EOF) { fputc(c, out); }
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    FILE *list;
    FILE *bundle = NULL;
    char line[LINE_CAP];
    int present = 0, missing = 0;
    long total_bytes = 0L, total_lines = 0L;

    if (argc != 2 && argc != 4) {
        fprintf(stderr, "usage: handoff <list-file> [--bundle <out-file>]\n");
        return 2;
    }
    if (argc == 4) {
        if (strcmp(argv[2], "--bundle") != 0) {
            fprintf(stderr, "second option must be --bundle\n");
            return 2;
        }
        bundle = fopen(argv[3], "w");
        if (bundle == NULL) {
            fprintf(stderr, "cannot write %s\n", argv[3]);
            return 3;
        }
    }

    list = fopen(argv[1], "r");
    if (list == NULL) { fprintf(stderr, "cannot read %s\n", argv[1]); return 3; }

    printf("\n");
    printf("  %-42s %8s %7s  %s\n", "FILE", "BYTES", "LINES", "WHAT IT IS FOR");
    printf("  %-42s %8s %7s  %s\n",
           "------------------------------------------",
           "--------", "-------", "--------------");

    while (fgets(line, (int)sizeof line, list) != NULL) {
        char path[PATH_CAP];
        const char *why = "";
        char *tab;
        char *nl;
        long bytes = 0L, lines = 0L;

        nl = strchr(line, '\n');
        if (nl != NULL) { *nl = '\0'; }

        if (line[0] == '\0') { printf("\n"); continue; }
        if (line[0] == '#') {
            printf("\n  %s\n", line + 1);
            continue;
        }

        tab = strchr(line, '\t');
        if (tab != NULL) {
            *tab = '\0';
            why = tab + 1;
            while (*why == '\t' || *why == ' ') { why++; }
        }
        if (strlen(line) >= sizeof path) {
            printf("  PATH TOO LONG, skipped\n");
            missing++;
            continue;
        }
        strcpy(path, line);

        if (!measure(path, &bytes, &lines)) {
            printf("  %-42s %8s %7s  *** MISSING ***  %s\n", path, "-", "-", why);
            missing++;
            continue;
        }
        present++;
        total_bytes += bytes;
        total_lines += lines;
        printf("  %-42s %8ld %7ld  %s\n", path, bytes, lines, why);

        if (bundle != NULL) {
            fprintf(bundle,
                "\n\n===============================================================\n");
            fprintf(bundle, "=== %s\n", path);
            fprintf(bundle, "=== %s\n", why);
            fprintf(bundle, "=== %ld bytes, %ld lines\n", bytes, lines);
            fprintf(bundle,
                "===============================================================\n\n");
            if (!copy_into(path, bundle)) {
                fprintf(bundle, "[could not be read back]\n");
            }
        }
    }
    fclose(list);

    printf("\n  present: %d   missing: %d   total: %ld bytes, %ld lines\n",
           present, missing, total_bytes, total_lines);

    if (bundle != NULL) {
        if (fclose(bundle) != 0) {
            printf("  BUNDLE WRITE FAILED\n");
            return 3;
        }
        printf("  bundle written: %s\n", argv[3]);
    }
    if (missing > 0) {
        printf("\n  %d listed file(s) are missing. The handoff is INCOMPLETE.\n",
               missing);
        return 1;
    }
    printf("\n  handoff complete: every listed file is present.\n");
    return 0;
}
