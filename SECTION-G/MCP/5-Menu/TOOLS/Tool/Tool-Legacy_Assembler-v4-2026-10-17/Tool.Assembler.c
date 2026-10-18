/*
 * Tool.Assembler.c  v4  ->  shakti-assembler
 *
 * The packager for the flat tool bank. It NEVER builds a tool.
 *
 * Six slots, one fixed shape per package, disk truth only:
 *
 *     FILES:        what is here      (real names, read from disk)
 *     FORMAT:       what kind         (suffixes present)
 *     NAMES MEAN:   the naming law    (one fixed line)
 *     RELATED TO:   what it eats / what eats it   (carried hand-written)
 *     RUN:          proven runs, verbatim         (carried hand-written)
 *     CHANGED:      the log                       (carried hand-written)
 *
 * CREATE: no README -> the skeleton, honest empties.
 * FIX:    README exists -> the disk-truth slots are replaced in place;
 *         every other byte is carried verbatim, in order, never
 *         duplicated. The fixer converges: two runs, same bytes.
 *
 * Flat law: the bank is ONE level of families. Any subdirectory of a
 * family is an attachment dir, listed as such — the assembler never
 * descends to build packages for grandchildren.
 *
 * No dynamic memory. No lexer. No tokens. No clock. No Python.
 * C99 strict: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2
 *
 * Usage: shakti-assembler TOOLS_DIR [--write]
 * Exit 0 clean; 2 on bench (caps, io).
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAXP    1024
#define MAXN    256
#define MAXF    4096
#define MAXFAM  512
#define MAXLINE 4096
#define MAXOUT  (128U * 1024U)

typedef struct { char name[MAXN]; int isdir; } ent_t;

static ent_t g_ent[MAXF];
static ent_t g_fam[MAXFAM];

static char  g_out[MAXOUT];
static size_t g_olen = 0;

static int g_write = 0;

static void bench(const char *why)
{
    fprintf(stderr, "BENCH: %s\n", why);
    exit(2);
}

static void out_add(const char *s)
{
    size_t n = strlen(s);
    if (g_olen + n >= MAXOUT) bench("package over MAXOUT bytes");
    memcpy(g_out + g_olen, s, n);
    g_olen += n;
    g_out[g_olen] = '\0';
}

static int join2(char *dst, size_t cap, const char *a, const char *b)
{
    int n = snprintf(dst, cap, "%s/%s", a, b);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

static int ent_cmp(const void *pa, const void *pb)
{
    return strcmp(((const ent_t *)pa)->name, ((const ent_t *)pb)->name);
}

static int path_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void write_whole(const char *path, const char *buf, size_t len)
{
    char tmp[MAXP];
    FILE *fp;
    int n = snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof tmp) bench("tmp path over cap");
    fp = fopen(tmp, "wb");
    if (!fp) bench("cannot open output .tmp");
    if (len && fwrite(buf, 1, len, fp) != len) { fclose(fp); bench("write failed"); }
    if (fclose(fp) != 0) bench("close failed");
    if (rename(tmp, path) != 0) bench("rename failed");
}

static int list_dir(const char *dir, ent_t *out, int cap)
{
    DIR *dp = opendir(dir);
    struct dirent *de;
    char p[MAXP];
    int n = 0;
    if (!dp) return -1;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        if (strlen(de->d_name) >= MAXN) { closedir(dp); bench("name over MAXN"); }
        if (join2(p, sizeof p, dir, de->d_name) != 0) { closedir(dp); bench("path over MAXP"); }
        if (stat(p, &st) != 0) { closedir(dp); bench("stat failed"); }
        if (n >= cap) { closedir(dp); bench("entries over cap"); }
        strcpy(out[n].name, de->d_name);
        out[n].isdir = S_ISDIR(st.st_mode) ? 1 : 0;
        n++;
    }
    closedir(dp);
    qsort(out, (size_t)n, sizeof(ent_t), ent_cmp);
    return n;
}

static int is_c_source(const char *s)
{
    size_t n = strlen(s);
    return n > 2 && !strcmp(s + n - 2, ".c");
}

static const char *suffix_of(const char *s)
{
    const char *dot = strrchr(s, '.');
    return dot ? dot + 1 : "";
}

static void emit_format(int m)
{
    static char seen[64][16];
    int nseen = 0, i, j;
    char line[MAXLINE];
    out_add("FORMAT: ");
    for (i = 0; i < m; i++) {
        const char *sx;
        if (g_ent[i].isdir) continue;
        sx = suffix_of(g_ent[i].name);
        if (!*sx) continue;
        for (j = 0; j < nseen; j++)
            if (!strcmp(seen[j], sx)) break;
        if (j < nseen) continue;
        if (nseen >= 64) bench("suffix count over cap");
        snprintf(seen[nseen], 16, "%s", sx);
        nseen++;
        if (nseen > 1) out_add(", ");
        if (snprintf(line, sizeof line, ".%s", sx) < 0) bench("line error");
        out_add(line);
    }
    if (!nseen) out_add("(no suffixed files)");
    out_add("\n");
}

static void emit_truth(int m)
{
    char line[MAXLINE];
    int i, any = 0;
    out_add("FILES:\n");
    for (i = 0; i < m; i++) {
        if (snprintf(line, sizeof line, "  %s%s\n",
                     g_ent[i].name, g_ent[i].isdir ? "/" : "") < 0) bench("line error");
        out_add(line);
        any = 1;
    }
    if (!any) out_add("  (none)\n");
    emit_format(m);
    out_add("NAMES MEAN: family dir First-Name; source First.Name.c; "
            "binary shakti-<name> — a file that breaks the law is drift, flag it.\n");
}

static int is_truth_slot(const char *line)
{
    return !strncmp(line, "FILES:", 6) ||
           !strncmp(line, "FORMAT:", 7) ||
           !strncmp(line, "NAMES MEAN:", 11);
}

static void assemble_family(const char *tools, const char *fam)
{
    char fd[MAXP], rp[MAXP];
    int m, exists, i, has_c = 0;

    if (join2(fd, sizeof fd, tools, fam) != 0) bench("path over MAXP");
    m = list_dir(fd, g_ent, MAXF);
    if (m < 0) { printf("SKIP %s (unreadable)\n", fam); return; }
    if (m == 0) { printf("SKIP %s (empty dir — nothing to package)\n", fam); return; }
    for (i = 0; i < m; i++)
        if (!g_ent[i].isdir && is_c_source(g_ent[i].name)) has_c = 1;
    if (!has_c) { printf("SKIP %s (no source — not a tool family)\n", fam); return; }

    if (join2(rp, sizeof rp, fd, "README.md") != 0) bench("path over MAXP");
    exists = path_exists(rp);

    g_olen = 0;

    if (!exists) {
        char line[MAXLINE];
        if (snprintf(line, sizeof line, "# %s\n\n", fam) < 0) bench("line error");
        out_add(line);
        emit_truth(m);
        out_add("\nRELATED TO: (empty)\n");
        out_add("\nRUN: (empty — no proven runs recorded)\n");
        out_add("\nCHANGED: (empty)\n");
    } else {
        FILE *fp = fopen(rp, "rb");
        char line[MAXLINE];
        int in_slot = 0, truth_done = 0;
        if (!fp) bench("cannot read existing README");
        while (fgets(line, sizeof line, fp)) {
            if (is_truth_slot(line)) {
                if (!truth_done) {
                    emit_truth(m);
                    truth_done = 1;
                }
                in_slot = 1;
                continue;
            }
            if (in_slot) {
                if (line[0] == ' ' || line[0] == '\t' ||
                    line[0] == '\n' || line[0] == '\r') {
                    continue;
                }
                in_slot = 0;
            }
            {
                size_t l = strlen(line);
                if (g_olen + l >= MAXOUT) { fclose(fp); bench("package over MAXOUT"); }
                memcpy(g_out + g_olen, line, l);
                g_olen += l; g_out[g_olen] = '\0';
            }
        }
        fclose(fp);
        if (!truth_done) {
            out_add("\n");
            emit_truth(m);
        }
    }

    if (g_write) {
        write_whole(rp, g_out, g_olen);
        printf("%s %s\n", exists ? "FIXED" : "CREATED", fam);
    } else {
        printf("--- %s: would %s (%lu bytes) ---\n",
               fam, exists ? "FIX" : "CREATE", g_olen);
        fwrite(g_out, 1, g_olen > 1200 ? 1200 : g_olen, stdout);
        if (g_olen > 1200) printf("...[preview ends]\n");
    }
}

int main(int argc, char **argv)
{
    int i, n;
    const char *tools = NULL;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--write")) g_write = 1;
        else if (!tools) tools = argv[i];
        else bench("unknown argument");
    }
    if (!tools) {
        fprintf(stderr, "usage: shakti-assembler TOOLS_DIR [--write]\n");
        return 2;
    }
    n = list_dir(tools, g_fam, MAXFAM);
    if (n < 0) bench("cannot list tools dir");
    for (i = 0; i < n; i++)
        if (g_fam[i].isdir)
            assemble_family(tools, g_fam[i].name);
    return 0;
}
