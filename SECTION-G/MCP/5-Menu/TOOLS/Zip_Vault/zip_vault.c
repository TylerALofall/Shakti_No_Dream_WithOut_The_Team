/*
 * zip_vault.c -- Shakti loading-zone vault, SPEC v0.1. See zip_vault.h.
 */

#include "zip_vault.h"
#include "junzip_nomalloc.h"

#include <stdint.h>
#include <string.h>

/* ---- Fixed layout of B ---- */
#define ZV_HDR_SIZE   64u
#define ZV_IDX_ENTRY  20u
#define ZV_DATA_BASE  (ZV_HDR_SIZE + ZV_MAX_ENTRIES * ZV_IDX_ENTRY)
#define ZV_DATA_CAP   (ZV_CHAMBER - ZV_DATA_BASE)
#define ZV_OFF_COUNT  8u
#define ZV_OFF_USED   12u
#define ZV_OFF_PRINT  16u

#if ZV_CHAMBER < (ZV_HDR_SIZE + ZV_MAX_ENTRIES * ZV_IDX_ENTRY + 4096u)
#error "ZV_CHAMBER is too small for the header, index and a data area."
#endif
#if ZV_CHAMBER > 0x7FFFFFFFu
#error "B offsets are stored as 32-bit values."
#endif

static const unsigned char zv_magic[8] = { 'S','H','K','V','B','0','0','1' };

/* ---- The loading zone: one block, fixed order ---- */
typedef struct {
    unsigned char a[ZV_CHAMBER];
    unsigned char b[ZV_CHAMBER];
    unsigned char c[ZV_CHAMBER];
} ZvZone;

/* Compile-time proof that A, B and C sit back to back with no gaps. */
typedef char zv_layout_check[
    (offsetof(ZvZone, b) == (size_t)ZV_CHAMBER &&
     offsetof(ZvZone, c) == 2u * (size_t)ZV_CHAMBER &&
     sizeof(ZvZone) == 3u * (size_t)ZV_CHAMBER) ? 1 : -1];

static ZvZone zv_zone;
static JZScratch zv_scratch;
static int zv_now = ZV_EMPTY;

/* ---- Little-endian fields inside B ---- */
static void zv_put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
    p[2] = (unsigned char)((v >> 16) & 0xFFu);
    p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

static uint32_t zv_get32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---- In-memory reader for the archive the caller hands in ---- */
typedef struct {
    JZFile handle;              /* must stay first */
    const unsigned char *data;
    size_t size;
    size_t pos;
} ZvMem;

static size_t zv_mem_read(JZFile *f, void *buf, size_t n)
{
    ZvMem *m = (ZvMem *)(void *)f;
    size_t avail = m->size - m->pos;
    if (n > avail)
        n = avail;
    if (n > 0u)
        memcpy(buf, m->data + m->pos, n);
    m->pos += n;
    return n;
}

static int zv_mem_tell(JZFile *f, JZOffset *offset)
{
    ZvMem *m = (ZvMem *)(void *)f;
    if (offset == NULL)
        return -1;
    *offset = (JZOffset)m->pos;
    return 0;
}

static int zv_mem_seek(JZFile *f, int64_t offset, int whence)
{
    ZvMem *m = (ZvMem *)(void *)f;
    uint64_t base;

    if (whence == SEEK_SET)
        base = 0u;
    else if (whence == SEEK_CUR)
        base = (uint64_t)m->pos;
    else if (whence == SEEK_END)
        base = (uint64_t)m->size;
    else
        return -1;

    if (offset < 0) {
        uint64_t back = (uint64_t)(-(offset + 1)) + 1u;
        if (back > base)
            return -1;
        m->pos = (size_t)(base - back);
    } else {
        if ((uint64_t)offset > (uint64_t)m->size - base)
            return -1;                       /* fail, never clamp */
        m->pos = (size_t)(base + (uint64_t)offset);
    }
    return 0;
}

static int zv_mem_error(JZFile *f) { (void)f; return 0; }
static void zv_mem_close(JZFile *f) { (void)f; }

static JZFile *zv_mem_open(ZvMem *m, const unsigned char *d, size_t n)
{
    m->handle.read = zv_mem_read;
    m->handle.tell = zv_mem_tell;
    m->handle.seek = zv_mem_seek;
    m->handle.error = zv_mem_error;
    m->handle.close = zv_mem_close;
    m->data = d;
    m->size = n;
    m->pos = 0u;
    return &m->handle;
}

/* ---- Integrity ---- */
static uint32_t zv_fingerprint(void)
{
    uint32_t crc = jz_crc32(0u, zv_zone.b, ZV_OFF_PRINT);
    return jz_crc32(crc, zv_zone.b + ZV_OFF_PRINT + 4u,
            (size_t)ZV_CHAMBER - (ZV_OFF_PRINT + 4u));
}

/* Fixed work: every byte of A and C is read every time, no early exit. */
static int zv_walls_clear(void)
{
    unsigned char acc = 0u;
    size_t i;
    for (i = 0; i < (size_t)ZV_CHAMBER; i++)
        acc |= (unsigned char)(zv_zone.a[i] | zv_zone.c[i]);
    return acc == 0u;
}

static void zv_scrub_scratch(void)
{
    memset(&zv_scratch, 0, sizeof zv_scratch);
}

/* A breach wipes everything: A, B, C and the scratch. */
static int zv_breach(void)
{
    memset(&zv_zone, 0, sizeof zv_zone);
    zv_scrub_scratch();
    zv_now = ZV_EMPTY;
    return ZV_ERR_BREACH;
}

/* Index entry i, with every offset proven to lie inside B's data area.
 * Returns 0 when the entry is sound, -1 when B has been damaged. */
static int zv_index(unsigned long i, uint32_t *nameOff, uint32_t *nameLen,
        uint32_t *dataOff, uint32_t *dataLen, uint32_t *crc)
{
    const unsigned char *e = zv_zone.b + ZV_HDR_SIZE + (size_t)i * ZV_IDX_ENTRY;
    uint64_t nameEnd, dataEnd;

    *nameOff = zv_get32(e);
    *nameLen = zv_get32(e + 4);
    *dataOff = zv_get32(e + 8);
    *dataLen = zv_get32(e + 12);
    *crc = zv_get32(e + 16);

    nameEnd = (uint64_t)*nameOff + (uint64_t)*nameLen + 1u;
    dataEnd = (uint64_t)*dataOff + (uint64_t)*dataLen;
    if (*nameOff < ZV_DATA_BASE || nameEnd > (uint64_t)*dataOff ||
            dataEnd > (uint64_t)ZV_CHAMBER ||
            zv_zone.b[*nameOff + *nameLen] != 0u)
        return -1;
    return 0;
}

/* ---- Fill ---- */
typedef struct {
    int pass;               /* 1 = measure (B untouched), 2 = write */
    unsigned long count;
    uint64_t used;          /* data-area bytes needed / written */
    int rc;                 /* first failure, 0 if none */
} ZvFill;

static int zv_on_entry(JZFile *zip, int index, const JZFileHeader *h,
        const char *name, void *user)
{
    ZvFill *f = (ZvFill *)user;
    size_t nameLen = strlen(name);
    uint64_t need = (uint64_t)nameLen + 1u + (uint64_t)h->uncompressedSize;
    unsigned char *slot;
    unsigned long i;
    JZOffset save;
    uint32_t got = 0u;
    int rc;

    (void)index;

    if (h->compressionMethod != 0u) {
        f->rc = JZ_ERR_UNSUPPORTED;      /* stored only: no decompressor */
        return 0;
    }
    if (!jzFilenameIsSafe(name)) {
        f->rc = ZV_ERR_NAME;
        return 0;
    }
    if (f->count >= (unsigned long)ZV_MAX_ENTRIES ||
            f->used + need > (uint64_t)ZV_DATA_CAP) {
        f->rc = ZV_ERR_FULL;
        return 0;
    }

    if (f->pass == 1) {
        f->used += need;
        f->count++;
        return 1;
    }

    /* Pass 2: refuse a name already written into B. */
    for (i = 0; i < f->count; i++) {
        const unsigned char *e = zv_zone.b + ZV_HDR_SIZE + (size_t)i * ZV_IDX_ENTRY;
        if (zv_get32(e + 4) == (uint32_t)nameLen &&
                memcmp(zv_zone.b + zv_get32(e), name, nameLen) == 0) {
            f->rc = ZV_ERR_DUPLICATE;
            return 0;
        }
    }

    if (zip->tell(zip, &save)) {
        f->rc = JZ_ERR;
        return 0;
    }
    if (zip->seek(zip, (int64_t)h->offset, SEEK_SET)) {
        f->rc = JZ_ERR;
        return 0;
    }
    rc = jzReadLocalFileHeaderChecked(zip, h, name, &zv_scratch);
    if (rc != JZ_OK) {
        f->rc = rc;
        return 0;
    }

    slot = zv_zone.b + ZV_DATA_BASE + (size_t)f->used;
    memcpy(slot, name, nameLen + 1u);
    rc = jzReadDataLimited(zip, h, slot + nameLen + 1u,
            (size_t)h->uncompressedSize, 0xFFFFFFFFu, &got, &zv_scratch);
    if (rc != JZ_OK || got != h->uncompressedSize) {
        f->rc = rc != JZ_OK ? rc : JZ_ERR_CORRUPT;
        return 0;
    }

    {
        unsigned char *e = zv_zone.b + ZV_HDR_SIZE + (size_t)f->count * ZV_IDX_ENTRY;
        uint32_t nameOff = (uint32_t)(ZV_DATA_BASE + f->used);
        zv_put32(e, nameOff);
        zv_put32(e + 4, (uint32_t)nameLen);
        zv_put32(e + 8, nameOff + (uint32_t)nameLen + 1u);
        zv_put32(e + 12, got);
        zv_put32(e + 16, h->crc32);
    }
    f->used += need;
    f->count++;

    if (zip->seek(zip, (int64_t)save, SEEK_SET)) {
        f->rc = JZ_ERR;
        return 0;
    }
    return 1;
}

static int zv_walk(const unsigned char *archive, size_t size, ZvFill *f)
{
    ZvMem m;
    JZEndRecord er;
    JZFile *zip = zv_mem_open(&m, archive, size);
    int rc = jzReadEndRecord(zip, &er, &zv_scratch);
    if (rc != JZ_OK)
        return rc;
    rc = jzReadCentralDirectory(zip, &er, zv_on_entry, f, &zv_scratch);
    if (rc != JZ_OK)
        return rc;
    return f->rc;
}

int zv_fill(const unsigned char *archive, size_t size)
{
    ZvFill f;
    unsigned long measuredCount;
    uint64_t measuredUsed;
    int rc;

    if (zv_now != ZV_EMPTY)
        return ZV_ERR_STATE;
    if (archive == NULL || size == 0u)
        return JZ_ERR;

    /* Pass 1: measure everything. B is not touched. */
    memset(&f, 0, sizeof f);
    f.pass = 1;
    rc = zv_walk(archive, size, &f);
    if (rc != JZ_OK) {
        zv_scrub_scratch();
        return rc;
    }
    measuredCount = f.count;
    measuredUsed = f.used;

    /* Pass 2: write. Any failure wipes B. */
    zv_now = ZV_OPEN;
    memcpy(zv_zone.b, zv_magic, sizeof zv_magic);
    memset(&f, 0, sizeof f);
    f.pass = 2;
    rc = zv_walk(archive, size, &f);
    if (rc == JZ_OK && (f.count != measuredCount || f.used != measuredUsed))
        rc = ZV_ERR_CHANGED;
    if (rc != JZ_OK) {
        zv_wipe();
        return rc;
    }

    zv_put32(zv_zone.b + ZV_OFF_COUNT, (uint32_t)f.count);
    zv_put32(zv_zone.b + ZV_OFF_USED, (uint32_t)f.used);
    zv_scrub_scratch();

    /* Seal: walls must be untouched, then the fingerprint is recorded. */
    if (!zv_walls_clear())
        return zv_breach();
    zv_put32(zv_zone.b + ZV_OFF_PRINT, zv_fingerprint());
    zv_now = ZV_SEALED;
    return ZV_OK;
}

/* ---- Sealed access ---- */
unsigned long zv_count(void)
{
    if (zv_now != ZV_SEALED)
        return 0ul;
    return (unsigned long)zv_get32(zv_zone.b + ZV_OFF_COUNT);
}

int zv_entry(unsigned long index, const char **name, size_t *size)
{
    uint32_t nameOff, nameLen, dataOff, dataLen, crc;

    if (zv_now != ZV_SEALED)
        return ZV_ERR_STATE;
    if (name == NULL || size == NULL)
        return JZ_ERR;
    if (zv_count() > (unsigned long)ZV_MAX_ENTRIES)
        return zv_breach();               /* stored count is impossible */
    if (index >= zv_count())
        return ZV_ERR_NOTFOUND;
    if (zv_index(index, &nameOff, &nameLen, &dataOff, &dataLen, &crc) != 0)
        return zv_breach();
    *name = (const char *)(zv_zone.b + nameOff);
    *size = (size_t)dataLen;
    return ZV_OK;
}

int zv_get(const char *name, const unsigned char **data, size_t *size)
{
    unsigned long i, count;
    size_t len;

    if (zv_now != ZV_SEALED)
        return ZV_ERR_STATE;
    if (name == NULL || data == NULL || size == NULL)
        return JZ_ERR;

    len = strlen(name);
    count = zv_count();
    if (count > (unsigned long)ZV_MAX_ENTRIES)
        return zv_breach();

    for (i = 0; i < count; i++) {
        uint32_t nameOff, nameLen, dataOff, dataLen, crc;
        if (zv_index(i, &nameOff, &nameLen, &dataOff, &dataLen, &crc) != 0)
            return zv_breach();
        if ((size_t)nameLen != len ||
                memcmp(zv_zone.b + nameOff, name, len) != 0)
            continue;
        if (jz_crc32(0u, zv_zone.b + dataOff, (size_t)dataLen) != crc)
            return zv_breach();
        *data = zv_zone.b + dataOff;
        *size = (size_t)dataLen;
        return ZV_OK;
    }
    return ZV_ERR_NOTFOUND;
}

int zv_verify(void)
{
    if (zv_now != ZV_SEALED)
        return ZV_ERR_STATE;
    if (!zv_walls_clear() ||
            zv_get32(zv_zone.b + ZV_OFF_PRINT) != zv_fingerprint() ||
            memcmp(zv_zone.b, zv_magic, sizeof zv_magic) != 0)
        return zv_breach();
    return ZV_OK;
}

/* ---- Lifecycle ---- */
void zv_init(void)
{
    memset(&zv_zone, 0, sizeof zv_zone);   /* commit every page now */
    zv_scrub_scratch();
    zv_now = ZV_EMPTY;
}

int zv_state(void)
{
    return zv_now;
}

void zv_wipe(void)
{
    memset(zv_zone.b, 0, sizeof zv_zone.b);
    zv_scrub_scratch();
    zv_now = ZV_EMPTY;
}

#ifdef ZV_TEST_HOOKS
static unsigned char *zv_chamber(int chamber)
{
    return chamber == 0 ? zv_zone.a : chamber == 1 ? zv_zone.b : zv_zone.c;
}

void zv_test_flip(int chamber, size_t offset, unsigned char mask)
{
    if (offset < (size_t)ZV_CHAMBER)
        zv_chamber(chamber)[offset] ^= mask;
}

int zv_test_is_zero(int chamber)
{
    const unsigned char *p = zv_chamber(chamber);
    unsigned char acc = 0u;
    size_t i;
    for (i = 0; i < (size_t)ZV_CHAMBER; i++)
        acc |= p[i];
    return acc == 0u;
}
#endif
