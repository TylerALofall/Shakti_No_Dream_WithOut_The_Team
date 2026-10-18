/* Tool-Assembler v7.1. C99 package librarian; one main, no tool execution. */
#ifndef TOOL_ASSEMBLER_H
#define TOOL_ASSEMBLER_H
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#define TA_PATH 1024
#define TA_COUNT 1536
#define TA_REQUIRED 64
#define TA_PACKAGES 128
#define TA_README (128u * 1024u)
#define TA_INDEX (2u * 1024u * 1024u)
#define TA_LEDGER (1024u * 1024u)
#define TA_FILE (8u * 1024u * 1024u)
#define TA_BOOK "TOOLS_LEDGER.txt"
typedef struct {
    char rel[TA_PATH], hash[65];
    mode_t mode;
    int dir;
} TaFile;
typedef struct {
    char family[128], tool[128], version[64], date[11];
    char introduction[2048], command[TA_PATH], example[TA_PATH], build[TA_PATH];
    char kind[16], entry[TA_PATH], required[TA_REQUIRED][TA_PATH];
    char folder[384], record[512], record_hash[65];
    int required_count;
} TaPackage;
extern TaFile ta_source[TA_COUNT];
extern int ta_source_count, ta_stored;
extern char ta_stage[TA_PATH];
void ta_bench(const char *why, const char *path);
void ta_join(char out[TA_PATH], const char *a, const char *b);
int ta_exists(const char *path);
void ta_real(const char *path, char out[TA_PATH]);
size_t ta_read(const char *path, void *out, size_t cap);
void ta_hash_selftest(void);
void ta_hash_data(const void *data, size_t n, char out[65]);
void ta_hash_file(const char *path, char out[65]);
void ta_sort(TaFile *files, int count);
int ta_scan(const char *root, TaFile files[TA_COUNT]);
void ta_copy(const char *from, const char *to, const TaFile *expected);
void ta_write(const char *path, const char *data, size_t n);
void ta_discard_stage(void);
const char *ta_member(const char *rel, const char *prefix);
size_t ta_seal_index(TaFile *files, int count, char out[TA_INDEX]);
void ta_package_read(const char *path, TaPackage *p);
void ta_package_validate(const TaPackage *p, TaFile *files, int count,
                         const char *prefix);
size_t ta_readme(const char *notes, const TaPackage *p, char out[TA_README]);
#endif
