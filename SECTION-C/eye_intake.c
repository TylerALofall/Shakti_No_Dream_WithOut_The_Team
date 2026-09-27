/* eye_intake.c — the graft: purified pixels -> sight hashes -> bindings.
 *
 * The law (bridge/HER_EYES_PURIFY.md): everything visual enters through
 * her eyes, W3C stripped, pixels only, captured on the other side. The
 * capture IS the card: sight hash (fnv1a64) in Bayer 4x4 order, drift 0
 * or it is not the card.
 *
 * This organ reads glyph files in BOTH pixel formats from the room:
 *   SHAKTI_WRITTEN_TEXT_8X8_V1   (eden_out/Visual_text — 8x8, per char)
 *   SHAKTI_GLYPH_64X64_BINARY_V1 (ascii64_glyphs — 64x64, one glyph)
 * and writes SIGHT.ndx: one line per glyph — name, format, ink count,
 * sight hash, sound binding (wav or NONE). Sealed in blocks of 10,
 * stream pin over everything. Deterministic: the harness supplies the
 * manifest order; the organ reads no clock, no dir, no rand.
 *
 * Bayer 4x4 order (the forge's own order, canonical matrix):
 *   { 0, 8, 2,10}   pixels are visited rank by rank, 0..15;
 *   {12, 4,14, 6}   within a rank, row-major over the grid,
 *   { 3,11, 1, 9}   cell visited iff bayer[y%4][x%4] == rank.
 *   {15, 7,13, 5}
 *
 * Pure C99. No heap. No float. No child process. No clock.
 * Gauntlet: -std=c99 -pedantic -Wall -Wextra -Werror, -O0 == -O2.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1(uint64_t h, uint64_t v)
{
    int b;
    for (b = 0; b < 8; b++) { h ^= (unsigned char)((v >> (8 * b)) & 0xFF); h *= FNV_PRIME; }
    return h;
}
static uint64_t fnv_str(uint64_t h, const char *s)
{
    while (*s) h = fnv1(h, (unsigned char)*s++);
    return h;
}

#define PATH_CAP   256
#define NAME_CAP    64
#define LINE_CAP    256
#define GRID_MAX    64
#define ROWS_MAX   512   /* 8x8 files: up to 64 chars x 8 rows + slack */
#define BLOCK_TICKETS 10

#define SIGHT_PATH  "SIGHT.ndx"
#define MANIFEST    "GLYPH_MANIFEST.txt"

static const int BAYER[4][4] = {
    { 0, 8, 2,10},
    {12, 4,14, 6},
    { 3,11, 1, 9},
    {15, 7,13, 5}
};

static uint64_t stream_pin, block_pin, block_tickets, block_count, tseq, glyph_count;

/* pixel store: up to ROWS_MAX rows of up to GRID_MAX bits */
static unsigned char PX[ROWS_MAX][GRID_MAX];
static int px_rows, px_cols;

/* ---- sight hash: fold name, dims, then pixels in Bayer 4x4 order ------ */
static uint64_t sight_hash(const char *name, const char *fmt)
{
    uint64_t h = FNV_BASIS;
    int rank, x, y;
    h = fnv_str(h, "sight:");
    h = fnv_str(h, fmt);
    h = fnv_str(h, name);
    h = fnv1(h, (uint64_t)px_rows);
    h = fnv1(h, (uint64_t)px_cols);
    for (rank = 0; rank < 16; rank++)
        for (y = 0; y < px_rows; y++)
            for (x = 0; x < px_cols; x++)
                if (BAYER[y % 4][x % 4] == rank)
                    h = fnv1(h, (uint64_t)(PX[y][x] ? 1 : 0));
    return h;
}

/* ---- format 1: SHAKTI_WRITTEN_TEXT_8X8_V1 ------------------------------ */
static int load_8x8(FILE *f, char *name, size_t ncap)
{
    char line[LINE_CAP];
    int chars = -1, in_header = 1, loaded_chars = 0;
    px_rows = 0; px_cols = 8;
    name[0] = 0;
    while (fgets(line, sizeof line, f)) {
        if (in_header && !name[0] && strncmp(line, "TEXT=", 5) == 0
            && line[5] != '\n' && line[5] != 0) {
            size_t i = 5, n = 0;
            while (line[i] && line[i] != '\n' && n + 1 < ncap) name[n++] = line[i++];
            name[n] = 0;
            continue;
        }
        if (strncmp(line, "CHARACTER=", 10) == 0) { in_header = 0; continue; }
        if (line[0] == '.' || line[0] == '#') {
            int x = 0;
            while (x < 8 && (line[x] == '.' || line[x] == '#')) {
                if (px_rows < ROWS_MAX) PX[px_rows][x] = (line[x] == '#') ? 1 : 0;
                x++;
            }
            px_rows++;
            if (x == 8 && px_rows % 8 == 0) loaded_chars++;
            continue;
        }
    }
    chars = loaded_chars;
    return name[0] && chars > 0;
}

/* ---- format 2: SHAKTI_GLYPH_64X64_BINARY_V1 ---------------------------- */
static int load_64x64(FILE *f, char *name, size_t ncap)
{
    char line[LINE_CAP];
    int ascii = -1;
    px_rows = 0; px_cols = GRID_MAX;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "ASCII=", 6) == 0) {
            ascii = 0;
            {   const char *p = line + 6;
                while (*p >= '0' && *p <= '9') { ascii = ascii * 10 + (*p - '0'); p++; }
            }
            continue;
        }
        if (line[0] == '0' || line[0] == '1') {
            int x = 0;
            while (x < GRID_MAX && (line[x] == '0' || line[x] == '1')) {
                if (px_rows < ROWS_MAX) PX[px_rows][x] = (line[x] == '1') ? 1 : 0;
                x++;
            }
            if (x == GRID_MAX) px_rows++;
            continue;
        }
    }
    if (ascii >= 0 && px_rows == GRID_MAX) {
        snprintf(name, ncap, "ascii_%03d", ascii);
        return 1;
    }
    return 0;
}

/* ---- the ledger --------------------------------------------------------- */
static void sight_record(const char *name, const char *fmt, uint64_t hash,
                         uint64_t ink, const char *wav)
{
    FILE *f = fopen(SIGHT_PATH, "a");
    /* F11 (GAP 3): the stage says whether a sound is bound to the sight */
    const char *stage = (strcmp(wav, "NONE") == 0) ? "NONE" : "BOUND";
    uint64_t pin = FNV_BASIS;
    tseq++;
    pin = fnv1(pin, tseq);
    pin = fnv_str(pin, name);
    pin = fnv1(pin, hash);
    pin = fnv_str(pin, stage);
    if (f) {
        fprintf(f, "sight %s fmt %s ink %llu hash %016llX wav %s stage %s pin %016llX\n",
                name, fmt, (unsigned long long)ink,
                (unsigned long long)hash, wav, stage, (unsigned long long)pin);
        fclose(f);
    }
    stream_pin = fnv_str(stream_pin, "sight:");
    stream_pin = fnv1(stream_pin, pin);
    block_pin = fnv1(block_pin, pin);
    block_tickets++;
    if (block_tickets == BLOCK_TICKETS) {
        block_count++;
        f = fopen(SIGHT_PATH, "a");
        if (f) {
            fprintf(f, "block %llu pin %016llX\n",
                    (unsigned long long)block_count,
                    (unsigned long long)block_pin);
            fclose(f);
        }
        /* F9: the chain — the next block is seeded from this block's pin. */
        block_pin = fnv1(FNV_BASIS, block_pin);
        block_tickets = 0;
    }
}

/* ---- intake one manifest line: <path> <fmt:8x8|64x64> <wav|NONE> -------- */
static void intake(const char *path, const char *fmt, const char *wav)
{
    FILE *f = fopen(path, "r");
    char name[NAME_CAP];
    char magic[LINE_CAP];
    uint64_t hash, ink = 0;
    int x, y, ok = 0;

    if (!f) { stream_pin = fnv_str(stream_pin, "missing:"); stream_pin = fnv_str(stream_pin, path); return; }
    memset(PX, 0, sizeof PX);
    if (!fgets(magic, sizeof magic, f)) { fclose(f); return; }

    if (strncmp(magic, "SHAKTI_WRITTEN_TEXT_8X8_V1", 26) == 0)
        ok = load_8x8(f, name, sizeof name);
    else if (strncmp(magic, "SHAKTI_GLYPH_64X64_BINARY_V1", 28) == 0)
        ok = load_64x64(f, name, sizeof name);
    fclose(f);
    if (!ok) { stream_pin = fnv_str(stream_pin, "bad:"); stream_pin = fnv_str(stream_pin, path); return; }

    for (y = 0; y < px_rows; y++)
        for (x = 0; x < px_cols; x++)
            ink += PX[y][x] ? 1 : 0;

    hash = sight_hash(name, fmt);
    glyph_count++;
    sight_record(name, fmt, hash, ink, wav);
}

void eye_intake_init(void)
{
    stream_pin = FNV_BASIS;
    block_pin = FNV_BASIS;
    block_tickets = block_count = tseq = glyph_count = 0;
}

/* run the whole manifest; returns glyphs captured */
uint64_t eye_intake_run(void)
{
    FILE *m = fopen(MANIFEST, "r");
    char line[LINE_CAP], path[PATH_CAP], fmt[16], wav[NAME_CAP];
    if (!m) return 0;
    while (fgets(line, sizeof line, m)) {
        memset(path, 0, sizeof path);
        memset(fmt, 0, sizeof fmt);
        memset(wav, 0, sizeof wav);
        if (sscanf(line, "%255s %15s %63s", path, fmt, wav) == 3)
            intake(path, fmt, wav);
    }
    fclose(m);
    return glyph_count;
}

/* F10: seal the ledger - the stream pin is written INTO SIGHT.ndx as
 * the final line, so the file carries its own proof. Best-effort. */
void eye_seal(void)
{
    FILE *f = fopen(SIGHT_PATH, "a");
    if (f) {
        fprintf(f, "stream %016llX\n", (unsigned long long)stream_pin);
        fclose(f);
    }
}

uint64_t eye_stream_pin(void)  { return stream_pin; }
uint64_t eye_blocks(void)      { return block_count; }
uint64_t eye_glyphs(void)      { return glyph_count; }
