/* Tool.README_builder.c v7.1. Native package I/O, metadata and README.
 * Required files are declared by the caller. No tool is built or run.
 * Fixed storage; no allocator, subprocess, network or external SDK calls.
 * Worker: Codex. Independent supervisor acceptance is not asserted. */
#define _XOPEN_SOURCE 700
#include "Tool.Assembler.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include "Tool.Hash.h"
static unsigned char bytes[TA_FILE];
static char prior[TA_README], metadata[TA_README];
static size_t out_len;
static char *out_text;
size_t ta_read(const char *path, void *out, size_t cap)
{
    FILE *f; struct stat st; size_t n; int extra, bad;
    if (lstat(path, &st) || !S_ISREG(st.st_mode)) ta_bench("not a regular file", path);
    f = fopen(path, "rb"); if (!f) ta_bench("cannot read file", path);
    n = fread(out, 1, cap, f); extra = fgetc(f);
    bad = ferror(f); bad |= fclose(f) != 0;
    if (bad || extra != EOF) ta_bench("read failure or file over capacity", path);
    return n;
}
void ta_join(char out[TA_PATH], const char *a, const char *b)
{
    int n = snprintf(out, TA_PATH, "%s/%s", a, b);
    if (n < 0 || n >= TA_PATH) ta_bench("path capacity", a);
}
int ta_exists(const char *path)
{
    struct stat st;
    if (!lstat(path, &st)) return 1;
    if (errno != ENOENT) ta_bench("cannot inspect path", path);
    return 0;
}
void ta_real(const char *path, char out[TA_PATH])
{
    char full[PATH_MAX];
    if (!realpath(path, full) || strlen(full) >= TA_PATH) ta_bench("invalid input path", path);
    strcpy(out, full);
}
void ta_hash_data(const void *data, size_t n, char out[65]) { ta_sha256(data, n, out); }
void ta_hash_selftest(void)
{
    static const char *msg[3] = {"", "abc",
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"};
    static const char *want[3] = {
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"};
    int i; char got[65];
    for (i = 0; i < 3; i++) {
        ta_hash_data(msg[i], strlen(msg[i]), got);
        if (strcmp(got, want[i])) ta_bench("SHA-256 self-test failed", msg[i]);
    }
    puts("HASH: three standard vectors PASS");
}
void ta_hash_file(const char *path, char out[65])
{
    size_t n = ta_read(path, bytes, sizeof bytes); ta_hash_data(bytes, n, out);
}
static void walk(const char *root, const char *rel, TaFile *out, int *n, int depth)
{
    char dir[TA_PATH]; DIR *d; struct dirent *e;
    if (depth > 12) ta_bench("directory depth capacity", rel);
    if (*rel) ta_join(dir, root, rel); else strcpy(dir, root);
    d = opendir(dir); if (!d) ta_bench("cannot list directory", dir);
    for (;;) {
        char r[TA_PATH], p[TA_PATH]; struct stat st; TaFile *v;
        errno = 0; e = readdir(d);
        if (!e) { if (errno) ta_bench("directory read error", dir); break; }
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (strpbrk(e->d_name, "\r\n\t\\") || strstr(e->d_name, ".tmp"))
            ta_bench("invalid name or stale temporary", e->d_name);
        if (*rel) ta_join(r, rel, e->d_name); else {
            if (strlen(e->d_name) >= sizeof r) ta_bench("name capacity", e->d_name);
            strcpy(r, e->d_name);
        }
        ta_join(p, root, r);
        if (lstat(p, &st) || (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode)))
            ta_bench("link or unsupported file", p);
        if (*n == TA_COUNT) ta_bench("package entry capacity", dir);
        v = &out[(*n)++]; memset(v, 0, sizeof *v); strcpy(v->rel, r);
        v->mode = st.st_mode & 0777; v->dir = S_ISDIR(st.st_mode);
        if (v->dir) walk(root, r, out, n, depth+1); else ta_hash_file(p, v->hash);
    }
    if (closedir(d)) ta_bench("directory close failure", dir);
}
void ta_sort(TaFile *files, int count)
{
    int i, j;
    for (i = 1; i < count; i++) {
        TaFile key = files[i];
        for (j = i; j && strcmp(files[j-1].rel, key.rel) > 0; j--) files[j] = files[j-1];
        files[j] = key;
    }
}
int ta_scan(const char *root, TaFile files[TA_COUNT])
{
    struct stat st; int n = 0;
    if (lstat(root, &st) || !S_ISDIR(st.st_mode)) ta_bench("not a directory", root);
    walk(root, "", files, &n, 0); ta_sort(files, n); return n;
}
void ta_copy(const char *from, const char *to, const TaFile *expected)
{
    size_t n = ta_read(from, bytes, sizeof bytes); char hash[65]; FILE *f; int fd, bad;
    ta_hash_data(bytes, n, hash);
    if (strcmp(hash, expected->hash)) ta_bench("supplied file changed after inspection", from);
    fd = open(to, O_WRONLY|O_CREAT|O_EXCL, 0600);
    if (fd < 0) ta_bench("cannot exclusively store file", to);
    f = fdopen(fd, "wb"); if (!f) { close(fd); ta_bench("stream open failure", to); }
    bad = fchmod(fd, expected->mode) != 0;
    bad |= fwrite(bytes, 1, n, f) != n; bad |= fflush(f) != 0;
    bad |= fsync(fd) != 0; bad |= fclose(f) != 0;
    if (bad) ta_bench("tool copy failed", to);
}
void ta_write(const char *path, const char *data, size_t n)
{
    char temp[TA_PATH]; struct stat st; FILE *f; int fd, bad;
    if (snprintf(temp, sizeof temp, "%s.tmp", path) >= (int)sizeof temp)
        ta_bench("temporary path capacity", path);
    if (ta_exists(path) && (lstat(path, &st) || !S_ISREG(st.st_mode)))
        ta_bench("output is not a regular file", path);
    fd = open(temp, O_WRONLY|O_CREAT|O_EXCL, 0600);
    if (fd < 0) ta_bench("cannot exclusively create temporary", temp);
    f = fdopen(fd, "wb"); if (!f) { close(fd); unlink(temp); ta_bench("stream failure", temp); }
    bad = fwrite(data, 1, n, f) != n; bad |= fflush(f) != 0;
    bad |= fsync(fd) != 0; bad |= fclose(f) != 0;
    if (bad || rename(temp, path)) { unlink(temp); ta_bench("output publication failed", path); }
}
void ta_discard_stage(void)
{
    char p[TA_PATH]; int i;
    if (!*ta_stage) return;
    ta_join(p, ta_stage, "README.md"); if (unlink(p) && errno != ENOENT) goto leave;
    for (i = ta_source_count-1; i >= 0; i--) {
        if (!strcmp(ta_source[i].rel, "README.md")) continue;
        ta_join(p, ta_stage, ta_source[i].rel);
        if ((ta_source[i].dir ? rmdir(p) : unlink(p)) && errno != ENOENT) goto leave;
    }
    if (rmdir(ta_stage) && errno != ENOENT) goto leave;
    ta_stage[0] = 0; return;
leave:
    fprintf(stderr, "BENCH: owned staging directory retained for inspection: %s\n", ta_stage);
}
const char *ta_member(const char *rel, const char *prefix)
{
    size_t n = strlen(prefix);
    if (!n) return rel;
    return !strncmp(rel, prefix, n) && rel[n] == '/' ? rel+n+1 : NULL;
}
/* Seal bytes: sorted paths, F hash path and D path/ lines.
 * Permissions are omitted so Git checkouts retain the same content seal.
 * README is included. The package ledger lives outside each package. */
size_t ta_seal_index(TaFile *files, int count, char out[TA_INDEX])
{
    size_t used = 0; int i;
    for (i = 0; i < count; i++) {
        const char *s = files[i].rel; int n;
        if (files[i].dir) n = snprintf(out+used, TA_INDEX-used, "D %s/\n", s);
        else n = snprintf(out+used, TA_INDEX-used, "F %s %s\n", files[i].hash, s);
        if (n < 0 || (size_t)n >= TA_INDEX-used) ta_bench("package index capacity", s);
        used += (size_t)n;
    }
    return used;
}
static int component(const char *s)
{
    size_t i;
    if (*s < 'A' || *s > 'Z') return 0;
    for (i = 1; s[i]; i++) {
        char c = s[i];
        if (s[i-1] == '_' && !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return 0;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || (c == '_' && s[i+1] && s[i-1] != '_'))) return 0;
    }
    return i < 128;
}
static int date_ok(const char *s)
{
    static const int days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int y, m, d, cap, i;
    if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') return 0;
    for (i = 0; i < 10; i++) if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return 0;
    y = (s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+s[3]-'0';
    m = (s[5]-'0')*10+s[6]-'0'; d = (s[8]-'0')*10+s[9]-'0';
    if (y < 1 || m < 1 || m > 12) return 0;
    cap = days[m-1] + (m == 2 && y%4 == 0 && (y%100 != 0 || y%400 == 0));
    return d >= 1 && d <= cap;
}
static int relative_ok(const char *s)
{
    const char *a = s; size_t i;
    if (!*s || *s == '/') return 0;
    for (i = 0;; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c && !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || strchr("._-/", c))) return 0;
        if (c == '/' || !c) {
            size_t n = (size_t)(s+i-a);
            if (!n || (n == 1 && a[0] == '.') || (n == 2 && a[0] == '.' && a[1] == '.')) return 0;
            a = s+i+1; if (!c) break;
        }
    }
    return i < TA_PATH;
}
static size_t program_length(const char *s)
{
    size_t n = strcspn(s, " "), i, start = !strncmp(s, "./", 2) ? 2u : 0u;
    if (n <= start || (n == start+1u && s[start] == '.') ||
        (n == start+2u && !strncmp(s+start, "..", 2))) return 0;
    for (i = start; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return 0;
    }
    return n;
}
static int argument(const char *command, const char *entry)
{
    const char *s = command;
    while (*s) {
        size_t n;
        while (*s == ' ') s++;
        n = strcspn(s, " ");
        if ((n == strlen(entry) && !strncmp(s, entry, n)) ||
            (n == strlen(entry)+2u && !strncmp(s, "./", 2) && !strncmp(s+2, entry, n-2u))) return 1;
        s += n;
    }
    return 0;
}
void ta_package_read(const char *path, TaPackage *p)
{
    static const char *keys[10] = {"FAMILY", "TOOL", "VERSION", "DATE", "INTRODUCTION",
                                  "COMMAND", "EXAMPLE", "ENTRY_KIND", "ENTRY", "BUILD"};
    char *values[10]; size_t caps[10], n, i; char *line; unsigned seen = 0;
    memset(p, 0, sizeof *p);
    values[0]=p->family; caps[0]=sizeof p->family; values[1]=p->tool; caps[1]=sizeof p->tool;
    values[2]=p->version; caps[2]=sizeof p->version; values[3]=p->date; caps[3]=sizeof p->date;
    values[4]=p->introduction; caps[4]=sizeof p->introduction;
    values[5]=p->command; caps[5]=sizeof p->command; values[6]=p->example; caps[6]=sizeof p->example;
    values[7]=p->kind; caps[7]=sizeof p->kind; values[8]=p->entry; caps[8]=sizeof p->entry;
    values[9]=p->build; caps[9]=sizeof p->build;
    n = ta_read(path, metadata, sizeof metadata-1u); metadata[n] = 0;
    if (memchr(metadata, 0, n)) ta_bench("NUL in package record", path);
    ta_hash_data(metadata, n, p->record_hash);
    line = metadata;
    while (*line) {
        char *end = strchr(line, '\n'), *value, *colon; int k;
        if (end) *end = 0;
        for (i = 0; line[i]; i++) if ((unsigned char)line[i] < 32 || (unsigned char)line[i] == 127)
            ta_bench("control character in package record", path);
        if (*line && *line != '#') {
            colon = strchr(line, ':');
            if (!colon || colon[1] != ' ' || !colon[2]) ta_bench("expected KEY: value", path);
            *colon = 0; value = colon+2;
            if (!strcmp(line, "REQUIRED")) {
                if (p->required_count == TA_REQUIRED || !relative_ok(value)) ta_bench("required path/capacity", value);
                for (k = 0; k < p->required_count; k++) if (!strcmp(p->required[k], value)) ta_bench("duplicate required file", value);
                strcpy(p->required[p->required_count++], value);
            } else {
                for (k = 0; k < 10 && strcmp(keys[k], line); k++) { }
                if (k == 10 || (seen & (1u << k))) ta_bench("unknown/duplicate package field", line);
                if (strlen(value) >= caps[k]) ta_bench("package field capacity", line);
                strcpy(values[k], value); seen |= 1u << k;
            }
        }
        if (!end) break;
        line = end+1;
    }
    if ((seen & 511u) != 511u || !p->required_count) ta_bench("missing package field/required files", path);
    if (!component(p->family) || !component(p->tool) || !date_ok(p->date)) ta_bench("invalid family/tool/date", path);
    if (p->version[0] < '0' || p->version[0] > '9') ta_bench("version must begin with a digit", path);
    for (i = 0; p->version[i]; i++) {
        char c = p->version[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') || c == '.' || c == '_')) ta_bench("invalid version", path);
    }
    if (!relative_ok(p->entry)) ta_bench("invalid entry path", p->entry);
    if (strcmp(p->kind, "single") && strcmp(p->kind, "manifest") && strcmp(p->kind, "batch"))
        ta_bench("ENTRY_KIND must be single, manifest or batch", path);
    n = program_length(p->command);
    if (!n || n != program_length(p->example) || strncmp(p->command, p->example, n))
        ta_bench("command/example need the same explicit program", path);
    if (!strcmp(p->kind, "batch") && (!argument(p->command, p->entry) || !argument(p->example, p->entry)))
        ta_bench("batch command/example must name the supplied batch entry", p->entry);
    if (snprintf(p->folder, sizeof p->folder, "%s-%s-v%s-%s", p->family, p->tool, p->version, p->date) >= (int)sizeof p->folder ||
        snprintf(p->record, sizeof p->record, "%s.txt", p->folder) >= (int)sizeof p->record)
        ta_bench("package name capacity", path);
}
static int ends(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix); return n >= m && !strcmp(s+n-m, suffix);
}
void ta_package_validate(const TaPackage *p, TaFile *files, int count, const char *prefix)
{
    int i, j, scripts = 0, entry = 0;
    for (i = 0; i < p->required_count; i++) {
        const char *s = p->required[i];
        if (!strcmp(s, "README.md") || !strcmp(s, p->record)) ta_bench("generated metadata is not a required script", s);
        for (j = 0; j < count; j++) {
            const char *m = ta_member(files[j].rel, prefix);
            if (m && !strcmp(m, s) && !files[j].dir) break;
        }
        if (j == count) ta_bench("missing required file", s);
        if (ends(s, ".c") || ends(s, ".sh") || ends(s, ".cmd") || ends(s, ".bat")) scripts++;
        if (!strcmp(s, p->entry)) entry = 1;
    }
    if (!entry) ta_bench("entry must be listed as REQUIRED", p->entry);
    if (scripts > 1 && !strcmp(p->kind, "single")) ta_bench("multiple scripts require a supplied manifest/batch entry", p->folder);
    if (strcmp(p->kind, "single") && !(ends(p->entry, ".manifest") || ends(p->entry, ".batch") ||
        ends(p->entry, ".manifest.txt") || ends(p->entry, ".batch.txt")))
        ta_bench("entry needs a manifest/batch filename, optionally ending in .txt", p->entry);
}
static void add(const char *s, size_t n)
{
    if (n >= TA_README-out_len) ta_bench("README capacity", "");
    memcpy(out_text+out_len, s, n); out_len += n; out_text[out_len] = 0;
}
static void text(const char *s) { add(s, strlen(s)); }
size_t ta_readme(const char *notes, const TaPackage *p, char out[TA_README])
{
    size_t n = 0, at; int i, j, addons = 0, dirs = 0; char hash[65];
    out_text = out; out_len = 0;
    if (ta_exists(notes)) {
        n = ta_read(notes, prior, sizeof prior-1u); prior[n] = 0;
        if (memchr(prior, 0, n)) ta_bench("NUL in supplied README", notes);
        ta_hash_data(prior, n, hash);
        for (i = 0; i < ta_source_count; i++) if (!strcmp(ta_source[i].rel, "README.md") && strcmp(hash, ta_source[i].hash))
            ta_bench("supplied README changed after inspection", notes);
    }
    text("REQUIRED SCRIPTS: ");
    for (i = 0; i < p->required_count; i++) { if (i) text(" | "); text(p->required[i]); }
    text("\n# "); text(p->folder); text("\n\nFAMILY: "); text(p->family);
    text("\nTOOL: "); text(p->tool); text("\nVERSION: "); text(p->version); text("\nDATE: "); text(p->date);
    text("\nENTRY_KIND: "); text(p->kind); text("\nENTRY: "); text(p->entry);
    if (*p->build) { text("\nBUILD: "); text(p->build); }
    text("\nCOMMAND: "); text(p->command); text("\nEXAMPLE: "); text(p->example);
    text("\n\nINTRODUCTION:\n"); text(p->introduction);
    text("\n\nPACKAGE DETAILS: "); text(p->record); text("\n");
    for (i = 0; i < ta_source_count; i++) if (!ta_source[i].dir &&
        strcmp(ta_source[i].rel, "README.md") && strcmp(ta_source[i].rel, p->record)) {
        for (j = 0; j < p->required_count && strcmp(p->required[j], ta_source[i].rel); j++) { }
        if (j == p->required_count) {
            if (!addons++) text("\nADD-ON FILES:\n");
            text("  "); text(ta_source[i].rel); text("\n");
        }
    }
    text("\nFILE HASHES:\n");
    for (i = 0; i < ta_source_count; i++) if (!ta_source[i].dir && strcmp(ta_source[i].rel, "README.md")) {
        text(ta_source[i].hash); text("  "); text(ta_source[i].rel); text("\n");
    }
    for (i = 0; i < ta_source_count; i++) if (ta_source[i].dir) {
        if (!dirs++) text("\nDIRECTORIES:\n");
        text("  "); text(ta_source[i].rel); text("/\n");
    }
    if (n) {
        text("\nNOTES:\n");
        for (at = 0; at < n;) {
            size_t end = at; while (end < n && prior[end] != '\n') end++;
            text("  "); add(prior+at, end-at); text("\n"); at = end < n ? end+1u : n;
        }
    }
    return out_len;
}
