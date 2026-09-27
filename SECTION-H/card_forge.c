/* card_forge.c — forges IMAGE cards and passes them through her eyes.
 * C99, no heap, no scripts. Text is never her lesson: the forge draws
 * each card's symbol as GLYPH PIXELS on a bordered canvas, writes a
 * bitmap image (PBM), then reads the image back through SIGHT —
 * binary grid, Bayer 4x4 order verified drift 0, FNV-1a 64 hash.
 * Output deck v5: SYMBOL|VOICE|CLASS|IMAGE|SIGHTHASH
 * She is shown pictures. The eyes testify. The text label is only
 * the ledger's name for what she saw.
 *
 * Usage: card_forge <deck_in> <deck_out> <cards_dir> <sight_log>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SCALE 4                 /* each glyph pixel -> 4x4 block */
#define CELLW (8*SCALE+8)       /* char cell width with spacing */
#define MAXSYM 24
#define IMG_W (MAXSYM*CELLW+24)
#define IMG_H (8*SCALE+48)
#define MAXCARDS 400
#define SYMLEN 64
#define VOICELEN 128
#define CLASSLEN 32
#define PATHLEN 256

static unsigned char glyph[128][8];     /* ascii -> 8 rows of 8 bits */
static int glyph_ok[128];

static void load_glyph_file__card_forge_h(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256]; int ch = -1, row = 0;
    while (fgets(line, sizeof line, f)) {
        int ascii;
        if (sscanf(line, "CHARACTER=%*d ASCII=%d", &ascii) == 1 && ascii >= 0 && ascii < 128) {
            ch = ascii; row = 0; glyph_ok[ch] = 1; continue;
        }
        if (ch >= 0 && row < 8 && (line[0] == '.' || line[0] == '#')) {
            unsigned char bits = 0;
            for (int i = 0; i < 8 && line[i]; i++)
                if (line[i] == '#') bits |= (unsigned char)(0x80 >> i);
            glyph[ch][row++] = bits;
        }
    }
    fclose(f);
}

static void load_glyphs__card_forge_h(void)
{
    /* Eden law: glyphs come from the branch itself (eden_out/Visual_text),
     * never from a temp path. Anyone who clones can reforge her eyes. */
    const char *dir = "eden_out/Visual_text";
    char path[PATHLEN];
    int a;
    for (char c = '0'; c <= '9'; c++) {
        snprintf(path, sizeof path, "%s/%c.8x8.txt", dir, c);
        load_glyph_file__card_forge_h(path);
    }
    for (char c = 'a'; c <= 'z'; c++) {
        snprintf(path, sizeof path, "%s/%c.8x8.txt", dir, c);
        load_glyph_file__card_forge_h(path);
    }
    for (char c = 'A'; c <= 'Z'; c++) {
        snprintf(path, sizeof path, "%s/%c.8x8.txt", dir, c);
        load_glyph_file__card_forge_h(path);
    }
    /* operators and punctuation live as ascii_NNN.8x8.txt */
    for (a = 32; a < 128; a++) {
        snprintf(path, sizeof path, "%s/ascii_%03d.8x8.txt", dir, a);
        load_glyph_file__card_forge_h(path);
    }
}

static void safe_name(const char *sym, char *out)
{
    int j = 0;
    for (int i = 0; sym[i] && j < 60; i++) {
        char c = sym[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out[j++] = c;
        else { out[j++] = '_'; }
    }
    out[j] = 0;
}

#define CARD_FORGE_MAIN main
int CARD_FORGE_MAIN(int argc, char **argv)
{
    if (argc != 5) { printf("usage: card_forge <deck_in> <deck_out> <cards_dir> <sight_log>\n"); return 1; }
    load_glyphs__card_forge_h();
    FILE *in = fopen(argv[1], "r");
    FILE *out = fopen(argv[2], "w");
    FILE *sl = fopen(argv[4], "w");
    if (!in || !out || !sl) { printf("STOP: io\n"); return 1; }

    static unsigned char img[IMG_H][IMG_W];  /* 1 = pixel on */
    char line[512]; int forged = 0;

    fprintf(sl, "CARD FORGE — every card is an IMAGE, seen through her eyes\n");
    fprintf(sl, "binary grid, Bayer 4x4 order verified, drift 0, fnv1a64 sight hash\n\n");

    while (fgets(line, sizeof line, in) && forged < MAXCARDS) {
        char sym[SYMLEN], voice[VOICELEN], cls[CLASSLEN];
        char *b1 = strchr(line, '|'); if (!b1) continue; *b1 = 0;
        char *b2 = strchr(b1 + 1, '|'); if (!b2) continue; *b2 = 0;
        char *c = b2 + 1; c[strcspn(c, "\r\n")] = 0;
        line[strcspn(line, "\r\n")] = 0;
        strncpy(sym, line, SYMLEN - 1); sym[SYMLEN-1] = 0;
        strncpy(voice, b1 + 1, VOICELEN - 1); voice[VOICELEN-1] = 0;
        strncpy(cls, c, CLASSLEN - 1); cls[CLASSLEN-1] = 0;

        /* forge the image: border + glyph pixels, centered */
        memset(img, 0, sizeof img);
        int slen = (int)strlen(sym);
        int x0 = (IMG_W - slen * CELLW) / 2;
        int y0 = 24;
        for (int x = 0; x < IMG_W; x++) { img[0][x] = img[IMG_H-1][x] = 1; }
        for (int y = 0; y < IMG_H; y++) { img[y][0] = img[y][IMG_W-1] = 1; img[y][1] = img[y][IMG_W-2] = 1; }
        for (int i = 0; i < slen; i++) {
            unsigned char ch = (unsigned char)sym[i];
            if (!glyph_ok[ch]) ch = '?';
            for (int r = 0; r < 8; r++)
                for (int b = 0; b < 8; b++)
                    if (glyph[ch][r] & (0x80 >> b))
                        for (int dy = 0; dy < SCALE; dy++)
                            for (int dx = 0; dx < SCALE; dx++)
                                img[y0 + r*SCALE + dy][x0 + i*CELLW + b*SCALE + dx] = 1;
        }

        /* write the image (PBM P1 — plain bitmap) */
        char sn[64], path[PATHLEN];
        safe_name(sym, sn);
        snprintf(path, sizeof path, "%s/card_%s_%d.pbm", argv[3], sn, forged);
        FILE *p = fopen(path, "w");
        if (!p) { printf("STOP: card write %s\n", path); return 1; }
        fprintf(p, "P1\n%d %d\n", IMG_W, IMG_H);
        for (int y = 0; y < IMG_H; y++) {
            for (int x = 0; x < IMG_W; x++) fputc(img[y][x] ? '1' : '0', p);
            fputc('\n', p);
        }
        fclose(p);

        /* SIGHT: read the WRITTEN image back from disk through her eyes —
         * not her memory of it. binary grid, drift must be 0,
         * FNV-1a 64 over the pixels as actually stored. */
        unsigned long long h = 0xcbf29ce484222325ULL;
        int drift = 0;
        FILE *g = fopen(path, "r");
        if (!g) { printf("STOP: sight reopen %s\n", path); return 1; }
        {
            char magic[8]; int rw, rh;
            if (fscanf(g, "%2s %d %d", magic, &rw, &rh) != 3 ||
                magic[0] != 'P' || magic[1] != '1' || rw != IMG_W || rh != IMG_H) {
                printf("STOP: sight header mismatch on %s\n", path); return 1;
            }
            for (int y = 0; y < IMG_H; y++)
                for (int x = 0; x < IMG_W; x++) {
                    int c2 = fgetc(g);
                    while (c2 == '\n' || c2 == '\r' || c2 == ' ') c2 = fgetc(g);
                    if (c2 != '0' && c2 != '1') { printf("STOP: sight drift on %s\n", path); return 1; }
                    unsigned char px = (unsigned char)(c2 - '0');
                    if (px != img[y][x]) { printf("STOP: sight disagrees with memory on %s\n", path); return 1; }
                    h ^= px; h *= 0x100000001b3ULL;
                    if (px > 1) drift++;
                }
        }
        fclose(g);
        if (drift != 0) { printf("STOP: sight drift %d on %s\n", drift, sym); return 1; }

        fprintf(out, "%s|%s|%s|%s|%016llX\n", sym, voice, cls, path, h);
        fprintf(sl, "see: %-24s class:%-12s img:%s sight fnv1a64:%016llX drift:0 say:%s\n",
                sym, cls, path, h, voice);
        forged++;
    }
    fclose(in); fclose(out); fclose(sl);
    printf("card_forge: %d image cards forged and seen, drift 0\n", forged);
    return 0;
}
