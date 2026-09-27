/* glyph_paint.c — level 1: any glyph, any two colors.
 *
 * Doctor's law, 2026-08-26: the binary grid is the vector. ink=1,
 * ground=0 — pour any two named colors into the shape and the glyph
 * wears them. Level 0 = raw binary. Level 1 = painted. Level 2 =
 * color-bound lessons: the letter R rendered IN red, and pure color
 * swabs — a solid grid of one color, no shapes pretending to be colors.
 *
 * Per bridge/HER_EYES_PURIFY.md the capture is the truth: a painted
 * glyph is sight-hashed WITH its colors folded in — the red A and the
 * blue A are different cards, as they should be.
 *
 * Input : glyph files (SHAKTI_WRITTEN_TEXT_8X8_V1 or
 *         SHAKTI_GLYPH_64X64_BINARY_V1) or a solid swab directive.
 * Output: PAINT.ndx ledger + painted grid files
 *         (SHAKTI_PAINTED_GRID_V1 — pixels, named colors, no W3C).
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

#define NAME_CAP    64
#define COLOR_CAP   16
#define LINE_CAP   256
#define GRID_MAX    64
#define ROWS_MAX   512
#define BLOCK_TICKETS 10
#define PAINT_DIR   "painted"
#define PAINT_NDX   "PAINT.ndx"

static const int BAYER[4][4] = {
    { 0, 8, 2,10},
    {12, 4,14, 6},
    { 3,11, 1, 9},
    {15, 7,13, 5}
};

static unsigned char PX[ROWS_MAX][GRID_MAX];
static int px_rows, px_cols;

static uint64_t stream_pin, block_pin, block_tickets, block_count, tseq, paint_count;

/* ---- the school palette (CURRICULUM_FORMAT §3, fixed hex) -------------- */
typedef struct { const char *name; const char *hex; } swatch_t;
static const swatch_t PALETTE[] = {
    {"red",    "#d23c3c"}, {"green",  "#3c8c46"}, {"blue",   "#3c64c8"},
    {"yellow", "#f0c93c"}, {"orange", "#e08232"}, {"purple", "#7d50a0"},
    {"brown",  "#7a5230"}, {"black",  "#1a1a1a"}, {"white",  "#ffffff"},
    {"gray",   "#9a9a9a"}, {"pink",   "#e08cb0"}
};
#define PALETTE_COUNT 11

static const char *palette_hex(const char *name)
{
    size_t i;
    for (i = 0; i < PALETTE_COUNT; i++)
        if (strcmp(PALETTE[i].name, name) == 0) return PALETTE[i].hex;
    return NULL;
}

/* ---- loaders (same pixel truth as eye_intake) -------------------------- */
static int load_8x8(FILE *f, char *name, size_t ncap)
{
    char line[LINE_CAP];
    int in_header = 1, loaded_chars = 0;
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
    return name[0] && loaded_chars > 0;
}

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

/* solid swab: a whole grid of one color — the pure color card */
static void make_swab(const char *name, int rows, int cols)
{
    int x, y;
    px_rows = rows; px_cols = cols;
    for (y = 0; y < rows; y++)
        for (x = 0; x < cols; x++)
            PX[y][x] = 1;
    (void)name;
}

/* ---- painted sight hash: colors are part of the capture ---------------- */
static uint64_t paint_hash(const char *name, const char *ink, const char *ground)
{
    uint64_t h = FNV_BASIS;
    int rank, x, y;
    h = fnv_str(h, "painted:");
    h = fnv_str(h, name);
    h = fnv_str(h, ink);
    h = fnv_str(h, ground);
    h = fnv1(h, (uint64_t)px_rows);
    h = fnv1(h, (uint64_t)px_cols);
    for (rank = 0; rank < 16; rank++)
        for (y = 0; y < px_rows; y++)
            for (x = 0; x < px_cols; x++)
                if (BAYER[y % 4][x % 4] == rank)
                    h = fnv1(h, (uint64_t)(PX[y][x] ? 1 : 0));
    return h;
}

/* ---- the ledger ---------------------------------------------------------- */
static void paint_record(const char *name, const char *ink, const char *ground,
                         uint64_t hash, const char *wav)
{
    FILE *f = fopen(PAINT_NDX, "a");
    uint64_t pin = FNV_BASIS;
    tseq++;
    pin = fnv1(pin, tseq);
    pin = fnv_str(pin, name);
    pin = fnv1(pin, hash);
    if (f) {
        fprintf(f, "paint %s ink %s ground %s hash %016llX wav %s pin %016llX\n",
                name, ink, ground, (unsigned long long)hash, wav,
                (unsigned long long)pin);
        fclose(f);
    }
    stream_pin = fnv_str(stream_pin, "paint:");
    stream_pin = fnv1(stream_pin, pin);
    block_pin = fnv1(block_pin, pin);
    block_tickets++;
    if (block_tickets == BLOCK_TICKETS) {
        block_count++;
        f = fopen(PAINT_NDX, "a");
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

/* ---- paint one glyph: write its painted grid, hash it, ledger it -------- */
static int paint_emit(const char *name, const char *ink, const char *ground,
                      const char *wav)
{
    char path[NAME_CAP + 64];
    FILE *o;
    uint64_t hash;
    int x, y;

    if (!palette_hex(ink) || !palette_hex(ground)) return 0;

    hash = paint_hash(name, ink, ground);
    snprintf(path, sizeof path, "%s/%s_%s_%s.pg.txt", PAINT_DIR, name, ink, ground);
    o = fopen(path, "w");
    if (!o) return 0;
    fprintf(o, "SHAKTI_PAINTED_GRID_V1\nNAME=%s\nINK=%s %s\nGROUND=%s %s\nGRID=%dx%d\n",
            name, ink, palette_hex(ink), ground, palette_hex(ground),
            px_cols, px_rows);
    for (y = 0; y < px_rows; y++) {
        for (x = 0; x < px_cols; x++) fputc(PX[y][x] ? '#' : '.', o);
        fputc('\n', o);
    }
    fclose(o);
    paint_count++;
    paint_record(name, ink, ground, hash, wav);
    return 1;
}

/* ---- public seam ---------------------------------------------------------- */
void paint_init(void)
{
    stream_pin = FNV_BASIS;
    block_pin = FNV_BASIS;
    block_tickets = block_count = tseq = paint_count = 0;
}

/* paint a glyph file; fmt "8x8" or "64x64". Returns 1 on success. */
int paint_glyph(const char *path, const char *fmt,
                const char *ink, const char *ground, const char *wav)
{
    FILE *f = fopen(path, "r");
    char name[NAME_CAP];
    char magic[LINE_CAP];
    int ok = 0;
    if (!f) return 0;
    memset(PX, 0, sizeof PX);
    if (!fgets(magic, sizeof magic, f)) { fclose(f); return 0; }
    if (strncmp(magic, "SHAKTI_WRITTEN_TEXT_8X8_V1", 26) == 0 && strcmp(fmt, "8x8") == 0)
        ok = load_8x8(f, name, sizeof name);
    else if (strncmp(magic, "SHAKTI_GLYPH_64X64_BINARY_V1", 28) == 0 && strcmp(fmt, "64x64") == 0)
        ok = load_64x64(f, name, sizeof name);
    fclose(f);
    if (!ok) return 0;
    return paint_emit(name, ink, ground, wav);
}

/* paint a solid swab: rows x cols of pure color */
int paint_swab(const char *color, int rows, int cols)
{
    char name[NAME_CAP];
    if (!palette_hex(color)) return 0;
    if (rows < 1 || rows > ROWS_MAX || cols < 1 || cols > GRID_MAX) return 0;
    snprintf(name, sizeof name, "swab_%s", color);
    make_swab(name, rows, cols);
    return paint_emit(name, color, color, "NONE");
}

/* F10: seal the ledger - the stream pin is written INTO PAINT.ndx as
 * the final line, so the file carries its own proof. Best-effort. */
void paint_seal(void)
{
    FILE *f = fopen(PAINT_NDX, "a");
    if (f) {
        fprintf(f, "stream %016llX\n", (unsigned long long)stream_pin);
        fclose(f);
    }
}

uint64_t paint_stream_pin(void) { return stream_pin; }
uint64_t paint_blocks(void)     { return block_count; }
uint64_t paint_count_done(void) { return paint_count; }
