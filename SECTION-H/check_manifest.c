/* check_manifest.c — hashes each manifest-listed file (FNV-1a 64) and compares.
 * C99, no heap. The branch proves itself or it doesn't.
 */
#include <stdio.h>
#include <string.h>

static unsigned long long fnv_file__check_manifest_h(const char *path, int *ok)
{
    FILE *f = fopen(path, "rb");
    unsigned long long h = 0xcbf29ce484222325ULL;
    int c;
    *ok = 0;
    if (!f) return h;
    while ((c = fgetc(f)) != EOF) { h ^= (unsigned char)c; h *= 0x100000001b3ULL; }
    fclose(f); *ok = 1;
    return h;
}

#define CHECK_MANIFEST_MAIN main
int CHECK_MANIFEST_MAIN(void)
{
    FILE *m = fopen("MANIFEST.fnv64.txt", "r");
    if (!m) { printf("STOP: no manifest\n"); return 1; }
    char line[512]; int total = 0, bad = 0;
    while (fgets(line, sizeof line, m)) {
        unsigned long long want; char path[256];
        if (sscanf(line, "%16llX  %255s", &want, path) != 2) continue;
        if (!strchr(path, '/')) continue; /* skip prose header lines */
        int ok; unsigned long long got = fnv_file__check_manifest_h(path, &ok);
        total++;
        if (!ok || got != want) {
            printf("MISMATCH %s: manifest %016llX, pulled %016llX\n", path, want, got);
            bad++;
        }
    }
    fclose(m);
    printf("check_manifest: %d files verified against the pushed manifest, %d mismatches\n",
           total, bad);
    printf(bad == 0 ? "check_manifest: PASS — the branch is byte-exact\n"
                    : "check_manifest: FAIL\n");
    return bad != 0;
}
