/* Tool.Submit.h -- public data contract, v1.0, 2026-10-03.
 * Inputs: explicit form/files and manually returned HTTPS response bodies.
 * Outputs: immutable request text and audit records, never network traffic.
 * Two C translation units share this contract; only Tool.Submit.c has main.
 */
#ifndef TOOL_SUBMIT_H
#define TOOL_SUBMIT_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#define TS_VERSION "1.0"
#define TS_FILES 40
#define TS_FAMILIES 8
#define TS_FILE_BYTES (128u * 1024u)
#define TS_POOL_BYTES (1024u * 1024u)
#define TS_BUFFER (4u * 1024u * 1024u)
#define TS_TOKENS 16384
#define TS_PATH 512
#define TS_DIRS 128
#define TS_QUERY 65536
/* File bytes live once in the pool. Dates are declared, not OS timestamps. */
typedef struct {
    char source[TS_PATH], path[TS_PATH], date[32], sha[65];
    size_t offset, bytes;
    int family;
} SubmitFile;
typedef struct {
    char name[96], version[64], date[32], style[16], readme[TS_PATH];
    char description[1024], related[1024], runs[1024], changed[1024];
    char requirements[1024], example[1024], expected[1024], references[1024];
} SubmitFamily;
typedef struct {
    char mode[16], batch[64], owner[96], repo[96], base[128];
    char branch[128], root[256], hasher[32], logger[32], check[32];
    char form_sha[65], plan_sha[65], response_sha[65], approval_sha[65];
    char repository_id[256], base_oid[65], commit_oid[65], committed[64];
    SubmitFamily family[TS_FAMILIES];
    SubmitFile files[TS_FILES];
    int family_count, count, state[TS_FILES];
    char old_sha[TS_FILES][65], dirs[TS_DIRS][TS_PATH];
    int dir_count;
} SubmitPlan;
/* JSON nodes form a bounded preorder tree. next skips a whole value. */
typedef struct { int type, child, next; size_t start, len; } SubmitJson;
extern SubmitPlan ts;
extern char ts_pool[TS_POOL_BYTES + 1], ts_input[TS_BUFFER + 1];
extern char ts_output[TS_BUFFER + 1];
extern size_t ts_used, ts_length;
extern SubmitJson ts_json[TS_TOKENS];
void ts_die(const char *message);
void ts_need(int condition, const char *message);
void ts_copy(char *to, size_t capacity, const char *from);
void ts_sha256(const void *bytes, size_t n, char hex[65]);
void ts_reset(void);
void ts_add(const char *text);
void ts_printf(const char *format, ...);
void ts_quote(const char *text);
void ts_base64(const unsigned char *bytes, size_t n);
size_t ts_read(const char *path, char *buffer, size_t capacity);
void ts_save(const char *path, const char *bytes, size_t n);
void ts_name(char path[TS_PATH], const char *suffix);
void ts_save_output(const char *suffix);
void ts_probe(void);
void ts_utf8(const char *text, size_t n);
void ts_path(const char *path);
int ts_json_parse(void);
int ts_json_read(const char *path);
int ts_get(int object, const char *key);
int ts_optional(int object, const char *key);
const char *ts_string(int node);
int ts_null(int node);
int ts_number(int node);
void ts_expect(int node, const char *value);
void ts_form(const char *path);
void ts_collect(void);
void ts_plan_write(void);
void ts_plan_load(const char *batch);
void ts_inventory(void);
void ts_query(int readback);
int ts_remote(int readback);
void ts_branch_request(void);
void ts_branch_response(void);
void ts_upload_request(void);
void ts_upload_response(void);
void ts_approval_write(void);
void ts_approval_load(void);
void ts_receipt(void);
void ts_demo(void);
void ts_hash_checks(void);
/* Future reviewed C checks register here, never as shell-command strings. */
typedef int (*SubmitCheck)(const SubmitPlan *, const char *, size_t);
void ts_register_check(const char *name, SubmitCheck check);
void ts_run_check(void);
int shakti_submit_action(const char *action, const char *argument);
#endif
