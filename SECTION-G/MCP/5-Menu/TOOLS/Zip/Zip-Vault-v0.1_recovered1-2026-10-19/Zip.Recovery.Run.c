/* Synthetic, in-process RUN for the supplied stored-ZIP core and vault.
 * No files, subprocesses, optional decoder, network or dynamic allocation.
 * Build every module with ZV_TEST_HOOKS to exercise the supplied guard hooks.
 */
#include "junzip_nomalloc.h"
#include "zip_vault.h"
#include <stdio.h>
#include <string.h>

#ifndef ZV_TEST_HOOKS
#error "This RUN requires the supplied vault damage/inspection hooks."
#endif

typedef struct {
    unsigned char data[70000];
    size_t size;
    size_t local[2], central[2], descriptor[2], end;
    unsigned count;
} Fixture;

typedef struct {
    JZFile io;
    const unsigned char *data;
    size_t size, position;
} Memory;

typedef struct {
    size_t output_cap;
    uint32_t input_cap;
    unsigned count;
    int result;
} Visit;

static Fixture fixture;
static JZScratch scratch;
static unsigned char payload[256], output[256];
static unsigned passed, failed;

static void check(int condition, const char *label)
{
    if (condition) {
        passed++;
        printf("PASS: %s\n", label);
    } else {
        failed++;
        printf("BENCH: %s\n", label);
    }
}

static void put16(size_t at, unsigned value)
{
    fixture.data[at] = (unsigned char)value;
    fixture.data[at + 1u] = (unsigned char)(value >> 8);
}

static void put32(size_t at, uint32_t value)
{
    put16(at, (unsigned)(value & 65535u));
    put16(at + 2u, (unsigned)(value >> 16));
}

/* Fixed two-entry builder: only synthetic names, 256-byte payloads and
 * a bounded ZIP comment. All field writes follow these capacity checks. */
static int make_zip(const char *first, const char *second,
        unsigned descriptor, unsigned comment)
{
    const char *names[2];
    uint32_t crc = jz_crc32(0u, payload, sizeof payload);
    size_t directory;
    unsigned i;
    names[0] = first;
    names[1] = second;
    if (descriptor > 2u || comment > 65535u ||
            (first == NULL && second != NULL))
        return 0;
    memset(&fixture, 0, sizeof fixture);
    fixture.count = first == NULL ? 0u : second == NULL ? 1u : 2u;
    for (i = 0; i < fixture.count; i++) {
        size_t length = strlen(names[i]);
        size_t at = fixture.size;
        if (length > 256u || at + 30u + length + sizeof payload + 16u >
                sizeof fixture.data)
            return 0;
        fixture.local[i] = at;
        put32(at, 0x04034b50u);
        put16(at + 4u, 20u);
        put16(at + 6u, descriptor != 0u ? 8u : 0u);
        if (descriptor == 0u) {
            put32(at + 14u, crc);
            put32(at + 18u, (uint32_t)sizeof payload);
            put32(at + 22u, (uint32_t)sizeof payload);
        }
        put16(at + 26u, (unsigned)length);
        memcpy(fixture.data + at + 30u, names[i], length);
        fixture.size += 30u + length;
        memcpy(fixture.data + fixture.size, payload, sizeof payload);
        fixture.size += sizeof payload;
        fixture.descriptor[i] = fixture.size;
        if (descriptor != 0u) {
            if (descriptor == 1u) {
                put32(fixture.size, 0x08074b50u);
                fixture.size += 4u;
            }
            put32(fixture.size, crc);
            put32(fixture.size + 4u, (uint32_t)sizeof payload);
            put32(fixture.size + 8u, (uint32_t)sizeof payload);
            fixture.size += 12u;
        }
    }
    directory = fixture.size;
    for (i = 0; i < fixture.count; i++) {
        size_t length = strlen(names[i]);
        size_t at = fixture.size;
        if (at + 46u + length > sizeof fixture.data)
            return 0;
        fixture.central[i] = at;
        put32(at, 0x02014b50u);
        put16(at + 4u, 20u);
        put16(at + 6u, 20u);
        put16(at + 8u, descriptor != 0u ? 8u : 0u);
        put32(at + 16u, crc);
        put32(at + 20u, (uint32_t)sizeof payload);
        put32(at + 24u, (uint32_t)sizeof payload);
        put16(at + 28u, (unsigned)length);
        put32(at + 42u, (uint32_t)fixture.local[i]);
        memcpy(fixture.data + at + 46u, names[i], length);
        fixture.size += 46u + length;
    }
    fixture.end = fixture.size;
    if (fixture.size + 22u + comment > sizeof fixture.data)
        return 0;
    put32(fixture.end, 0x06054b50u);
    put16(fixture.end + 8u, fixture.count);
    put16(fixture.end + 10u, fixture.count);
    put32(fixture.end + 12u, (uint32_t)(fixture.end - directory));
    put32(fixture.end + 16u, (uint32_t)directory);
    put16(fixture.end + 20u, comment);
    fixture.size += 22u;
    memset(fixture.data + fixture.size, 'Q', comment);
    fixture.size += comment;
    return 1;
}

static size_t memory_read(JZFile *io, void *buffer, size_t size)
{
    Memory *m = (Memory *)(void *)io;
    size_t remaining = m->size - m->position;
    if (size > remaining)
        size = remaining;
    if (size != 0u)
        memcpy(buffer, m->data + m->position, size);
    m->position += size;
    return size;
}

static int memory_tell(JZFile *io, JZOffset *position)
{
    Memory *m = (Memory *)(void *)io;
    if (position == NULL)
        return -1;
    *position = (JZOffset)m->position;
    return 0;
}

static int memory_seek(JZFile *io, int64_t offset, int whence)
{
    Memory *m = (Memory *)(void *)io;
    int64_t base;
    if (whence == SEEK_SET)
        base = 0;
    else if (whence == SEEK_CUR)
        base = (int64_t)m->position;
    else if (whence == SEEK_END)
        base = (int64_t)m->size;
    else
        return -1;
    if (offset < -base || offset > (int64_t)m->size - base)
        return -1;
    m->position = (size_t)(base + offset);
    return 0;
}

static int memory_error(JZFile *io) { (void)io; return 0; }
static void memory_close(JZFile *io) { (void)io; }

static int visit(JZFile *io, int index, const JZFileHeader *header,
        const char *name, void *user)
{
    Visit *v = (Visit *)user;
    JZOffset saved;
    uint32_t produced = 0u;
    (void)index;
    if (io->tell(io, &saved) != 0 ||
            io->seek(io, (int64_t)header->offset, SEEK_SET) != 0) {
        v->result = JZ_ERR;
        return 0;
    }
    v->result = jzReadLocalFileHeaderChecked(io, header, name, &scratch);
    if (v->result == JZ_OK)
        v->result = jzReadDataLimited(io, header, output, v->output_cap,
                v->input_cap, &produced, &scratch);
    if (v->result != JZ_OK)
        return 0;
    if (produced != sizeof payload ||
            memcmp(output, payload, sizeof payload) != 0) {
        v->result = JZ_ERR_CORRUPT;
        return 0;
    }
    v->count++;
    if (io->seek(io, (int64_t)saved, SEEK_SET) != 0) {
        v->result = JZ_ERR;
        return 0;
    }
    return 1;
}

static int core_read(size_t output_cap, uint32_t input_cap)
{
    Memory m;
    JZEndRecord end;
    Visit v;
    int rc;
    memset(&m, 0, sizeof m);
    memset(&v, 0, sizeof v);
    m.io.read = memory_read;
    m.io.tell = memory_tell;
    m.io.seek = memory_seek;
    m.io.error = memory_error;
    m.io.close = memory_close;
    m.data = fixture.data;
    m.size = fixture.size;
    v.output_cap = output_cap;
    v.input_cap = input_cap;
    rc = jzReadEndRecord(&m.io, &end, &scratch);
    if (rc == JZ_OK)
        rc = jzReadCentralDirectory(&m.io, &end, visit, &v, &scratch);
    if (rc != JZ_OK)
        return rc;
    if (v.result != JZ_OK)
        return v.result;
    return v.count == fixture.count ? JZ_OK : JZ_ERR_CORRUPT;
}

static int zero_chambers(void)
{
    return zv_test_is_zero(0) && zv_test_is_zero(1) && zv_test_is_zero(2);
}

static int refused(int expected)
{
    zv_init();
    return zv_fill(fixture.data, fixture.size) == expected &&
        zv_state() == ZV_EMPTY && zv_count() == 0ul && zero_chambers();
}

static int output_untouched(void)
{
    size_t i;
    for (i = 0; i < sizeof output; i++)
        if (output[i] != 0xa5u)
            return 0;
    return 1;
}

int main(void)
{
    const unsigned char *data = NULL;
    const char *name = NULL;
    size_t size = 0u;
    unsigned i;
    char long_name[257];
    for (i = 0u; i < sizeof payload; i++)
        payload[i] = (unsigned char)i;
    check(jz_crc32(0u, "123456789", 9u) == 0xcbf43926u &&
            jz_crc32(jz_crc32(0u, "1234", 4u), "56789", 5u) == 0xcbf43926u,
            "known CRC and incremental CRC");
    check(jzFilenameIsSafe("training/voice.wav") &&
            jzFilenameIsSafe("training/") &&
            !jzFilenameIsSafe("../escape") && !jzFilenameIsSafe("/root") &&
            !jzFilenameIsSafe("a/./b") && !jzFilenameIsSafe("a//b") &&
            !jzFilenameIsSafe("C:\\escape"), "relative-name filter");
    if (!make_zip("voice/tone.wav", "pixels/frame.rgb", 0u, 0u)) {
        check(0, "fixture capacity");
        return 1;
    }
    check(core_read(sizeof output, 256u) == JZ_OK,
            "core reads both entries and all 256 byte values");
    memset(output, 0xa5, sizeof output);
    check(core_read(255u, 256u) == JZ_ERR_SIZE && output_untouched(),
            "core refuses short output without writing");
    check(core_read(sizeof output, 255u) == JZ_ERR_SIZE && output_untouched(),
            "core refuses compressed-input cap without writing");
    zv_init();
    check(zv_fill(fixture.data, fixture.size) == ZV_OK && zv_count() == 2ul &&
            zv_verify() == ZV_OK, "vault fills and seals two entries");
    check(zv_entry(1ul, &name, &size) == ZV_OK && size == sizeof payload &&
            strcmp(name, "pixels/frame.rgb") == 0 &&
            zv_get(name, &data, &size) == ZV_OK && size == sizeof payload &&
            memcmp(data, payload, size) == 0, "vault names and payloads agree");
    check(zv_get("absent", &data, &size) == ZV_ERR_NOTFOUND &&
            zv_fill(fixture.data, fixture.size) == ZV_ERR_STATE &&
            zv_verify() == ZV_OK, "missing entry and sealed refill refuse");
    zv_wipe();
    check(zv_state() == ZV_EMPTY && zero_chambers() &&
            zv_get("voice/tone.wav", &data, &size) == ZV_ERR_STATE,
            "wipe clears chambers and closes access");

    check(make_zip(NULL, NULL, 0u, 0u) && core_read(256u, 256u) == JZ_OK &&
            zv_fill(fixture.data, fixture.size) == ZV_OK && zv_count() == 0ul &&
            zv_verify() == ZV_OK, "empty archive");
    zv_wipe();
    check(make_zip("tone", NULL, 1u, 0u) && core_read(256u, 256u) == JZ_OK &&
            zv_fill(fixture.data, fixture.size) == ZV_OK,
            "signed data descriptor");
    zv_wipe();
    check(make_zip("tone", NULL, 2u, 0u) && core_read(256u, 256u) == JZ_OK &&
            zv_fill(fixture.data, fixture.size) == ZV_OK,
            "signatureless data descriptor");
    zv_wipe();
    check(make_zip("tone", NULL, 0u, 65535u) && core_read(256u, 256u) == JZ_OK &&
            zv_fill(fixture.data, fixture.size) == ZV_OK,
            "maximum ZIP comment and end-record chunk crossing");
    zv_wipe();
    memset(long_name, 'a', sizeof long_name);
    long_name[255] = '\0';
    check(make_zip(long_name, NULL, 0u, 0u) && core_read(256u, 256u) == JZ_OK,
            "255-byte entry name fits default cap");
    long_name[255] = 'a';
    long_name[256] = '\0';
    check(make_zip(long_name, NULL, 0u, 0u) && refused(JZ_ERR_SIZE),
            "256-byte entry name holds whole archive");
    check(make_zip("../escape", NULL, 0u, 0u) && refused(ZV_ERR_NAME),
            "unsafe name holds whole archive");
    check(make_zip("same", "same", 0u, 0u) && refused(ZV_ERR_DUPLICATE),
            "duplicate names wipe partial fill");

    (void)make_zip("tone", NULL, 0u, 0u);
    fixture.data[fixture.local[0] + 34u] ^= 1u;
    check(core_read(256u, 256u) == JZ_ERR_CORRUPT && refused(JZ_ERR_CORRUPT),
            "payload CRC damage refuses and wipes");
    (void)make_zip("tone", NULL, 0u, 0u);
    fixture.data[fixture.local[0] + 30u] ^= 1u;
    check(core_read(256u, 256u) == JZ_ERR_CORRUPT && refused(JZ_ERR_CORRUPT),
            "local and central name mismatch refuses");
    (void)make_zip("tone", NULL, 1u, 0u);
    fixture.data[fixture.descriptor[0] + 4u] ^= 1u;
    check(core_read(256u, 256u) == JZ_ERR_CORRUPT && refused(JZ_ERR_CORRUPT),
            "damaged descriptor refuses");
    (void)make_zip("tone", NULL, 1u, 0u);
    put32(fixture.local[0] + 14u, 1u);
    check(core_read(256u, 256u) == JZ_ERR_CORRUPT && refused(JZ_ERR_CORRUPT),
            "nonzero mismatched local descriptor fields refuse");
    (void)make_zip("tone", NULL, 0u, 0u);
    fixture.size--;
    check(refused(JZ_ERR_CORRUPT), "truncated end record refuses");
    (void)make_zip("tone", NULL, 0u, 0u);
    put16(fixture.central[0] + 8u, 1u);
    check(refused(JZ_ERR_UNSUPPORTED), "encrypted flag refuses");
    (void)make_zip("tone", NULL, 0u, 0u);
    put16(fixture.end + 4u, 1u);
    check(refused(JZ_ERR_UNSUPPORTED), "multi-disk archive refuses");
    (void)make_zip("tone", NULL, 0u, 0u);
    put16(fixture.end + 8u, 65535u);
    put16(fixture.end + 10u, 65535u);
    check(refused(JZ_ERR_UNSUPPORTED), "ZIP64 sentinel refuses");
    (void)make_zip("tone", NULL, 0u, 0u);
    put32(fixture.central[0] + 24u, (uint32_t)ZV_CHAMBER);
    check(refused(ZV_ERR_FULL), "oversized claimed payload holds whole archive");
    (void)make_zip("tone", NULL, 0u, 0u);
    put16(fixture.local[0] + 8u, 8u);
    put16(fixture.central[0] + 10u, 8u);
    check(core_read(256u, 256u) == JZ_ERR_UNSUPPORTED &&
            refused(JZ_ERR_UNSUPPORTED), "deflate method requires a decoder");

    for (i = 0u; i < 3u; i++) {
        (void)make_zip("tone", NULL, 0u, 0u);
        zv_init();
        if (zv_fill(fixture.data, fixture.size) != ZV_OK) {
            check(0, "damage fixture fill");
            continue;
        }
        zv_test_flip((int)i, 0u, 1u);
        check(zv_verify() == ZV_ERR_BREACH && zv_state() == ZV_EMPTY &&
                zero_chambers(), i == 0u ? "A guard damage wipes all chambers" :
                i == 1u ? "B fingerprint damage wipes all chambers" :
                "C guard damage wipes all chambers");
    }
    (void)make_zip("tone", NULL, 0u, 0u);
    zv_init();
    if (zv_fill(fixture.data, fixture.size) == ZV_OK) {
        size_t payload_offset = 64u + (size_t)ZV_MAX_ENTRIES * 20u + 5u;
        zv_test_flip(1, payload_offset, 1u);
        check(zv_get("tone", &data, &size) == ZV_ERR_BREACH && zero_chambers(),
                "per-file CRC damage closes and wipes vault");
    } else {
        check(0, "per-file damage fixture fill");
    }
    printf("RESULT: %u passed, %u BENCH\n", passed, failed);
    printf("BOUNDS: scratch=%lu chamber=%lu entries=%lu\n",
            (unsigned long)sizeof scratch, (unsigned long)ZV_CHAMBER,
            (unsigned long)ZV_MAX_ENTRIES);
    return failed != 0u ? 1 : 0;
}
