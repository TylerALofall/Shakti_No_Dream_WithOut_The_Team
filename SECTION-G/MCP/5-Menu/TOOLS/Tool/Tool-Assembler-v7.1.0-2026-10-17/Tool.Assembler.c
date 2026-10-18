/* Tool.Assembler.c v7.1. Package librarian; README last, then package seal.
 * Usage: shakti-assembler BANK INCOMING PACKAGE_RECORD
 * Repository change reporting belongs to the existing hash tools.
 * Native C99/POSIX I/O, fixed storage, no tool builds or execution.
 * Worker: Codex. Independent supervisor acceptance is not asserted. */
#define _XOPEN_SOURCE 700
#include "Tool.Assembler.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
TaFile ta_source[TA_COUNT];
int ta_source_count, ta_stored;
char ta_stage[TA_PATH];
static TaFile stored[TA_COUNT];
static char lock_path[TA_PATH], index_text[TA_INDEX], ledger[TA_LEDGER], readme[TA_README];
static size_t ledger_length;
static TaPackage desired;
typedef struct { char rel[512], hash[65]; } Registration;
static Registration registrations[TA_PACKAGES];
static int registered;
void ta_bench(const char *why, const char *path)
{
    fprintf(stderr, "BENCH: %s: %s\n", why, path);
    ta_discard_stage();
    if (*lock_path && rmdir(lock_path)) fprintf(stderr, "BENCH: lock retained: %s\n", lock_path);
    if (ta_stored) fputs("BENCH: package present; ledger registration incomplete. Inspect before retry.\n", stderr);
    exit(2);
}
static int inside(const char *path, const char *root)
{
    size_t n = strlen(root);
    return !strcmp(root, "/") || (!strncmp(path, root, n) && (!path[n] || path[n] == '/'));
}
static void ledger_load(const char *path)
{
    char *line; int i, j;
    registered = 0; ledger_length = 0;
    if (!ta_exists(path)) return;
    ledger_length = ta_read(path, ledger, sizeof ledger-1u); ledger[ledger_length] = 0;
    if (memchr(ledger, 0, ledger_length)) ta_bench("NUL in package ledger", path);
    line = ledger;
    while (*line) {
        char *end = strchr(line, '\n'); Registration *r; size_t n;
        if (!end) ta_bench("incomplete package ledger append", path);
        *end = 0; n = strlen(line);
        if (n < 74 || strncmp(line, "SHA256 ", 7) || line[71] != ' ' || line[72] != ' ' || n-73 >= sizeof registrations[0].rel)
            ta_bench("invalid package ledger record", path);
        for (i = 7; i < 71; i++) if (!strchr("0123456789abcdef", line[i])) ta_bench("invalid ledger hash", path);
        for (j = 0; j < registered && strcmp(registrations[j].rel, line+73); j++) { }
        if (j == registered) {
            if (registered == TA_PACKAGES) ta_bench("package ledger capacity", path);
            registered++;
        }
        r = &registrations[j]; memcpy(r->hash, line+7, 64); r->hash[64] = 0;
        strcpy(r->rel, line+73); *end = '\n'; line = end+1;
    }
}
static void same_files(int count)
{
    int i, j, hits = 0, expected = 0;
    for (i = 0; i < ta_source_count; i++) if (strcmp(ta_source[i].rel, "README.md")) expected++;
    for (i = 0; i < count; i++) {
        const char *s = stored[i].rel;
        if (!strcmp(s, "README.md")) continue;
        for (j = 0; j < ta_source_count && strcmp(ta_source[j].rel, s); j++) { }
        if (j == ta_source_count || stored[i].dir != ta_source[j].dir || strcmp(stored[i].hash, ta_source[j].hash) ||
            (!stored[i].dir && stored[i].mode != ta_source[j].mode)) ta_bench("stored payload differs from supplied files", s);
        hits++;
    }
    if (hits != expected) ta_bench("stored payload is missing entries", "");
}
static void ledger_append(const char *path, const char *line, size_t n)
{
    int fd, bad; struct stat st;
    if (ledger_length+n >= TA_LEDGER) ta_bench("package ledger capacity", path);
    fd = open(path, O_WRONLY|O_APPEND|O_CREAT|O_NOFOLLOW, 0600);
    if (fd < 0) ta_bench("cannot append package ledger", path);
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uintmax_t)st.st_size+n >= TA_LEDGER) { close(fd); ta_bench("invalid package ledger/capacity", path); }
    if (st.st_size != (off_t)ledger_length) fputs("NOTICE: package ledger changed during this run; appending to its current end.\n", stderr);
    bad = write(fd, line, n) != (ssize_t)n; bad |= fsync(fd) != 0; bad |= close(fd) != 0;
    if (bad) ta_bench("ledger append failed; original prefix was not rewritten", path);
}
static void unlock(void)
{
    if (rmdir(lock_path)) ta_bench("cannot release bank lock", lock_path);
    lock_path[0] = 0;
}
int main(int argc, char **argv)
{
    char bank[TA_PATH], incoming[TA_PATH], record_path[TA_PATH], book_path[TA_PATH];
    char destination[TA_PATH], relative[TA_PATH], family_dir[TA_PATH], notes[TA_PATH];
    char from[TA_PATH], to[TA_PATH], path[TA_PATH], seal[65], actual[65], line[TA_PATH];
    int m, i, found, exists, registration = -1; size_t length; struct stat st;
    if (argc != 4) {
        fputs("Usage: shakti-assembler BANK INCOMING PACKAGE_RECORD\n"
              "No submitted tool is compiled or executed. Use the existing hash tools for change reporting.\n", stderr); return 2;
    }
    ta_hash_selftest(); ta_real(argv[1], bank); ta_real(argv[2], incoming); ta_real(argv[3], record_path);
    ta_package_read(record_path, &desired);
    ta_join(relative, desired.family, desired.folder); ta_join(destination, bank, relative);
    ta_join(family_dir, bank, desired.family); ta_join(notes, incoming, "README.md");
    ta_join(book_path, bank, TA_BOOK);
    if (inside(bank, incoming) || inside(destination, incoming))
        ta_bench("input directory must not contain the output bank/package", incoming);
    ta_source_count = ta_scan(incoming, ta_source);
    found = 0;
    for (i = 0; i < ta_source_count; i++) {
        const char *s = ta_source[i].rel;
        if (!strcmp(s, TA_BOOK) || !strcmp(s, ".Tool.Assembler.lock")) ta_bench("reserved bank filename in package", s);
        if (!strcmp(s, desired.record)) {
            if (ta_source[i].dir || strcmp(ta_source[i].hash, desired.record_hash)) ta_bench("package record collision", s);
            found = 1;
        }
    }
    if (!found) {
        TaFile *f;
        if (ta_source_count == TA_COUNT) ta_bench("entry capacity for package record", desired.record);
        f = &ta_source[ta_source_count++]; memset(f, 0, sizeof *f);
        strcpy(f->rel, desired.record); strcpy(f->hash, desired.record_hash); f->mode = 0600;
        ta_sort(ta_source, ta_source_count);
    }
    ta_package_validate(&desired, ta_source, ta_source_count, "");
    ta_join(path, bank, ".Tool.Assembler.lock");
    if (mkdir(path, 0700)) ta_bench("bank busy or stale assembler lock", path);
    strcpy(lock_path, path); ledger_load(book_path);
    for (i = 0; i < registered; i++) if (!strcmp(relative, registrations[i].rel)) registration = i;
    if (registration < 0 && registered == TA_PACKAGES) ta_bench("package ledger capacity", book_path);
    exists = ta_exists(destination);
    ta_join(path, bank, ".Tool.Assembler.tmp");
    if (mkdir(path, 0700)) ta_bench("cannot exclusively create package staging", path);
    strcpy(ta_stage, path);
    for (i = 0; i < ta_source_count; i++) {
        if (!strcmp(ta_source[i].rel, "README.md")) continue;
        if (!strcmp(ta_source[i].rel, desired.record)) strcpy(from, record_path);
        else ta_join(from, incoming, ta_source[i].rel);
        ta_join(to, ta_stage, ta_source[i].rel);
        if (ta_source[i].dir) {
            if (mkdir(to, 0700)) ta_bench("cannot store attachment directory", to);
        } else ta_copy(from, to, &ta_source[i]);
    }
    length = ta_readme(notes, &desired, readme);
    ta_join(path, ta_stage, "README.md"); ta_write(path, readme, length);
    m = ta_scan(ta_stage, stored); same_files(m);
    length = ta_seal_index(stored, m, index_text); ta_hash_data(index_text, length, seal);
    if (exists) {
        m = ta_scan(destination, stored);
        length = ta_seal_index(stored, m, index_text); ta_hash_data(index_text, length, actual);
        if (strcmp(seal, actual)) ta_bench("destination collision; existing files retained. Use a new package version/date", relative);
        ta_discard_stage(); if (*ta_stage) ta_bench("staging cleanup incomplete", ta_stage);
        if (registration >= 0 && !strcmp(seal, registrations[registration].hash)) {
            unlock(); printf("PASS: already packaged %s; ledger unchanged\n", relative); return 0;
        }
        fputs("NOTICE: existing package matches supplied files; recording its current seal.\n", stderr);
    } else {
        if (ta_exists(family_dir)) {
            if (lstat(family_dir, &st) || !S_ISDIR(st.st_mode)) ta_bench("family path is not a directory", family_dir);
        } else if (mkdir(family_dir, 0700)) ta_bench("cannot create family directory", family_dir);
        if (lstat(destination, &st) == 0 || errno != ENOENT) ta_bench("destination appeared during assembly", destination);
        if (rename(ta_stage, destination)) ta_bench("cannot publish package", destination);
        ta_stage[0] = 0; ta_stored = 1;
        m = ta_scan(destination, stored); same_files(m);
        length = ta_seal_index(stored, m, index_text); ta_hash_data(index_text, length, actual);
        if (strcmp(seal, actual)) { ta_stored = 1; ta_bench("package changed while being saved", relative); }
    }
    ta_stored = 1;
    i = snprintf(line, sizeof line, "SHA256 %s  %s\n", seal, relative);
    if (i < 0 || i >= (int)sizeof line) ta_bench("ledger line capacity", relative);
    if (registration >= 0 && strcmp(seal, registrations[registration].hash))
        fprintf(stderr, "NOTICE: prior seal %s; current seal %s: %s\n", registrations[registration].hash, seal, relative);
    ledger_append(book_path, line, (size_t)i); ta_stored = 0; unlock();
    printf("PASS: packaged %s\nDIRECTORY SHA256: %s\nLEDGER: appended\n", relative, seal);
    return 0;
}
