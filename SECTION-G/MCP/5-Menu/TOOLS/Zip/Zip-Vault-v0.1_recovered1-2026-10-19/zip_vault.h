/*
 * zip_vault.h -- Shakti loading-zone vault. SPEC v0.1 (B chamber only).
 * ISO C99. Zero allocation. No clock. No recursion. Fixed memory map.
 * Built on junzip_nomalloc (patched P1-P3), stored entries only.
 *
 * LOADING ZONE: one static block, fixed order (C99 struct layout):
 *
 *     A | B | C      each ZV_CHAMBER bytes (default 10 MiB)
 *
 *   B    : the vault. Holds its own header, index, names and file bytes.
 *          Nothing outside B holds B's names.
 *   A, C : reserved; role not decided yet (empty walls, or B's mirrors).
 *          In v0.1 the vault never writes them and every seal and verify
 *          checks they are still all zero. Any change = breach.
 *
 * B LAYOUT (all fields little-endian, written byte by byte):
 *     [0..8)    magic "SHKVB001"
 *     [8..12)   entry count
 *     [12..16)  data bytes used
 *     [16..20)  fingerprint: CRC-32 of all of B except these 4 bytes
 *     [64 .. 64 + ZV_MAX_ENTRIES*20)   index, 20 bytes per entry:
 *               name offset, name length, data offset, data length, CRC-32
 *     [ZV_DATA_BASE .. end)            per entry: name, NUL, file bytes
 *
 * STATES:  EMPTY --fill--> SEALED --wipe--> EMPTY
 *   fill measures the whole archive first (B untouched), then writes.
 *   Any failure while writing wipes B and returns to EMPTY: partial
 *   content is never visible. A breach wipes A, B and C.
 *
 * REFUSED AT THE DOOR (before any byte is written to B):
 *   compressed entries, encryption, ZIP64, multi-disk, unsafe names,
 *   more than ZV_MAX_ENTRIES entries, total that does not fit B.
 * REFUSED WHILE WRITING (B wiped): duplicate names, local/central
 *   mismatch, CRC mismatch, archive changed between the two passes.
 *
 * WORST CASES (fixed ceilings):
 *   memory      3 * ZV_CHAMBER + one JZScratch, static; never grows
 *   end search  65 reads of JZ_SCAN_CHUNK bytes
 *   fill        2 passes over the central directory + one copy of each
 *               file, at most ZV_DATA_CAP bytes in total
 *   duplicates  at most ZV_MAX_ENTRIES^2 / 2 name comparisons
 *   seal/verify one CRC pass over B + one zero check over A and C
 *   get         index search (<= ZV_MAX_ENTRIES) + one CRC pass over
 *               that file only
 *
 * NOT IN v0.1: roles for A and C, the airlock, persistence across
 * launches, the 10 and 100 tanks. Single-threaded only.
 *
 * Pointers handed out by zv_get/zv_entry point INTO B and are read-only
 * by contract. C99 cannot make the CPU enforce that; the fingerprint and
 * per-file CRC catch any write through them (detected, then wiped).
 */

#ifndef ZIP_VAULT_H
#define ZIP_VAULT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ZV_CHAMBER
#define ZV_CHAMBER (10u * 1024u * 1024u)   /* bytes per chamber */
#endif
#ifndef ZV_MAX_ENTRIES
#define ZV_MAX_ENTRIES 1024u
#endif

/* Status codes. junzip's own codes pass through unchanged:
 * JZ_ERR (-1), JZ_ERR_UNSUPPORTED (-2), JZ_ERR_CORRUPT (-3), JZ_ERR_SIZE (-4) */
#define ZV_OK              0
#define ZV_ERR_STATE     (-10) /* call not allowed in the current state */
#define ZV_ERR_NAME      (-11) /* unsafe entry name */
#define ZV_ERR_DUPLICATE (-12) /* same name twice */
#define ZV_ERR_FULL      (-13) /* does not fit B, or too many entries */
#define ZV_ERR_NOTFOUND  (-14)
#define ZV_ERR_BREACH    (-15) /* fingerprint or wall check failed; wiped */
#define ZV_ERR_CHANGED   (-16) /* archive differed between the two passes */

#define ZV_EMPTY  0
#define ZV_OPEN   1            /* only while zv_fill is writing */
#define ZV_SEALED 2

/* Touch every byte of A, B and C once (all zero), so the OS commits the
 * memory now: if it is not there, startup fails, not a reconstruction. */
void zv_init(void);

int zv_state(void);

/* EMPTY -> SEALED. `archive` is the whole zip, handed in by the caller
 * (no file I/O here). It must not change during the call. */
int zv_fill(const unsigned char *archive, size_t size);

/* SEALED only. Number of entries (0 when not sealed). */
unsigned long zv_count(void);

/* SEALED only. Entry `index`: its NUL-terminated name and its size. */
int zv_entry(unsigned long index, const char **name, size_t *size);

/* SEALED only. Find a file by exact name; its CRC is re-checked first. */
int zv_get(const char *name, const unsigned char **data, size_t *size);

/* SEALED only. Re-check the whole of B and the walls A and C. */
int zv_verify(void);

/* Any state. Zero B and the working scratch; back to EMPTY. */
void zv_wipe(void);

#ifdef ZV_TEST_HOOKS
/* TEST BUILDS ONLY: simulate damage and inspect chambers. 0=A 1=B 2=C */
void zv_test_flip(int chamber, size_t offset, unsigned char mask); /* XOR, mask != 0 */
int zv_test_is_zero(int chamber);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZIP_VAULT_H */
