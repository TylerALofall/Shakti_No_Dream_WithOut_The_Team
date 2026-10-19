/*
 * junzip_nomalloc.h -- malloc-free, strict-C99 ZIP parsing core.
 * Refactor of jokkebk/JUnzip @ a935ea8 with all allocator use removed.
 *
 * Guarantees:
 *  - Zero dynamic allocation. All working memory lives in JZScratch,
 *    owned by the caller, sized at compile time.
 *  - No library writes to stderr. Errors are return codes only; the
 *    caller owns logging.
 *  - No packed structs, no endian/alignment assumptions. All ZIP format
 *    fields are decoded explicitly from little-endian bytes.
 *  - Same hardened security posture as the reconciled upstream tree:
 *    central-directory values are authoritative, local headers and data
 *    descriptors are verified (never adopted), exact production required,
 *    CRC32 enforced, ZIP64/encryption/multi-disk rejected, all offset
 *    arithmetic in 64 bits.
 *
 * Build (zlib backend):    cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -DHAVE_ZLIB ...
 * Build (puff backend):    cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -DHAVE_PUFF ...
 * Build (store only):      cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 ...
 */

#ifndef JUNZIP_NOMALLOC_H
#define JUNZIP_NOMALLOC_H

#include <limits.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#if CHAR_BIT != 8
#error "This decoder requires 8-bit bytes."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Compile-time resource caps (tunable via -D, deterministic) ---- */
#ifndef JZ_FILENAME_CAP
#define JZ_FILENAME_CAP 256u          /* max entry name incl. NUL */
#endif
#ifndef JZ_SCAN_CHUNK
#define JZ_SCAN_CHUNK 1024u           /* backward end-record scan granularity */
#endif
#ifndef JZ_IN_CHUNK
#define JZ_IN_CHUNK 16384u            /* streaming inflate input chunk */
#endif
#ifndef JZ_COMP_CAP
#define JZ_COMP_CAP (1024u * 1024u)   /* whole-entry compressed bound (puff path) */
#endif

#if JZ_FILENAME_CAP < 2 || JZ_SCAN_CHUNK < 64 || JZ_IN_CHUNK < 256
#error "Scratch caps are too small to be useful."
#endif

/* ---- Status codes (same convention as upstream: 0 ok, negative error) ---- */
#define JZ_OK 0
#define JZ_ERR (-1)              /* I/O failure or bad argument */
#define JZ_ERR_UNSUPPORTED (-2)  /* valid ZIP feature we deliberately reject */
#define JZ_ERR_CORRUPT (-3)      /* malformed or internally inconsistent */
#define JZ_ERR_SIZE (-4)         /* archive claim exceeds a fixed cap/bound */

typedef struct JZFile JZFile;
typedef uint64_t JZOffset;   /* unsigned 64-bit positions, always */

/* I/O vtable. Backends must fail (not wrap/clamp) on out-of-range seeks. */
struct JZFile {
    size_t (*read)(JZFile *file, void *buf, size_t size);
    int    (*tell)(JZFile *file, JZOffset *offset);
    int    (*seek)(JZFile *file, int64_t offset, int whence);
    int    (*error)(JZFile *file);
    void   (*close)(JZFile *file);
};

/* Logical, host-endian entry metadata (decoded from raw bytes by the core). */
typedef struct {
    uint16_t versionNeededToExtract;
    uint16_t generalPurposeBitFlag; /* bit 3 = data descriptor present */
    uint16_t compressionMethod;     /* 0 = store, 8 = deflate */
    uint16_t lastModFileTime;
    uint16_t lastModFileDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint32_t offset;                /* absolute local-header position */
} JZFileHeader;

typedef struct {
    uint32_t signature;
    uint16_t diskNumber;
    uint16_t centralDirectoryDiskNumber;
    uint16_t numEntriesThisDisk;
    uint16_t numEntries;
    uint32_t centralDirectorySize;
    uint32_t centralDirectoryOffset;
    uint16_t zipCommentLength;
} JZEndRecord;

/*
 * Callback for central-directory iteration. `filename` aliases scratch
 * memory: it is valid ONLY for the duration of the callback and is
 * overwritten on the next entry. Copy it if you need it longer.
 * Return 0 to stop iteration early (treated as success).
 */
typedef int (*JZRecordCallback)(JZFile *zip, int index,
        const JZFileHeader *header, const char *filename, void *user_data);

/*
 * Caller-owned working memory. Total footprint:
 *   JZ_FILENAME_CAP + JZ_SCAN_CHUNK + JZ_IN_CHUNK + JZ_COMP_CAP bytes
 * Default ~1.02 MiB, all fixed at compile time. Zero it once, reuse
 * forever. Not thread-shared: one scratch per concurrent operation.
 */
typedef struct {
    unsigned char filename[JZ_FILENAME_CAP]; /* per-entry name */
    unsigned char scan[JZ_SCAN_CHUNK];       /* header decode + scan window */
    unsigned char inbuf[JZ_IN_CHUNK];        /* streaming inflate input */
    unsigned char compbuf[JZ_COMP_CAP];      /* whole-entry input (puff) */
} JZScratch;

/* Standard CRC-32 (ZIP polynomial 0xEDB88320). Pass crc=0 for one shot. */
uint32_t jz_crc32(uint32_t crc, const void *data, size_t size);

/* Lexical relative-path validation ('/' and '\\' separators). Rejects
 * empty/absolute, drive letters, '.', '..', empty components, control
 * chars, DEL, ':'. Filesystem anchoring is still the caller's job. */
int jzFilenameIsSafe(const char *filename);

/*
 * Locate and validate the end record. Scans backward from EOF in
 * JZ_SCAN_CHUNK pieces (deterministic bounded work: at most
 * ceil(65557 / (JZ_SCAN_CHUNK - 21)) chunk reads), handles records
 * straddling chunk boundaries, rejects multi-disk and ZIP64 layouts,
 * and bounds the central directory against the record position in 64 bits.
 */
int jzReadEndRecord(JZFile *zip, JZEndRecord *endRecord, JZScratch *scratch);

/*
 * Iterate the central directory. Every header field is cross-checked;
 * iteration is bounded by centralDirectorySize; entry names longer than
 * JZ_FILENAME_CAP are rejected with JZ_ERR_SIZE (deliberate format cap).
 * Stops early if the callback returns 0.
 */
int jzReadCentralDirectory(JZFile *zip, JZEndRecord *endRecord,
        JZRecordCallback callback, void *user_data, JZScratch *scratch);

/*
 * Read the local header at the current position and verify it byte-for-byte
 * against the (already validated) central-directory entry. Central values
 * are never modified. With bit 3 set, zero local CRC/sizes are accepted but
 * nonzero values must agree; otherwise local must equal central exactly.
 */
int jzReadLocalFileHeaderChecked(JZFile *zip,
        const JZFileHeader *centralHeader, const char *centralFilename,
        JZScratch *scratch);

/*
 * Decode one entry into caller memory.
 * Contract (same as upstream hardened API, plus fixed caps):
 *  - uncompressedSize > bufferSize          -> JZ_ERR_SIZE, nothing written
 *  - compressedSize > compressedSizeLimit   -> JZ_ERR_SIZE
 *  - puff backend: compressedSize > JZ_COMP_CAP -> JZ_ERR_SIZE
 *  - exact production required; CRC32 verified over produced bytes
 *  - data descriptor (bit 3), if present, must agree with header values
 *  - encrypted / ZIP64 / unknown method     -> JZ_ERR_UNSUPPORTED
 * Failure may leave partial data in `buffer`; `*bytesProduced` is only
 * meaningful on JZ_OK. Callers needing transactional output extract to a
 * staging region and publish on success.
 */
int jzReadDataLimited(JZFile *zip, const JZFileHeader *header, void *buffer,
        size_t bufferSize, uint32_t compressedSizeLimit,
        uint32_t *bytesProduced, JZScratch *scratch);

/* Convenience: no compressed-input cap beyond the backend's fixed caps. */
int jzReadData(JZFile *zip, const JZFileHeader *header, void *buffer,
        size_t bufferSize, uint32_t *bytesProduced, JZScratch *scratch);

/* ---- stdio adapter with caller-owned storage (was malloc'd upstream) ---- */
typedef struct {
    JZFile handle;
    FILE *fp;
} JZStdioFile;

/* Populate `storage` and return its embedded JZFile*. No allocation. */
JZFile *jzfile_stdio_init(JZStdioFile *storage, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* JUNZIP_NOMALLOC_H */
