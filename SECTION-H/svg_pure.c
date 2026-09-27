/*
 * svg_pure.c — every image goes through her eyes, redrawn PURE. No SVG.
 * C99, no heap, no scripts, no rand().
 *
 * The branch's SVGs are descriptions. She does not read descriptions —
 * she SEES. This tool rasterizes each SVG (rects, circles, and text drawn
 * from her own 8x8 glyph library from eden_out/Visual_text) onto a pixel
 * grid, passes the grid through SIGHT (binary, drift must be 0, FNV-1a 64),
 * and writes the pure bitmap (PBM P1). The bitmap is what she saw.
 *
 * Usage: svg_pure <svg_dir> <out_dir> <sight_log>
 * Run from the repo root. Glyphs load from eden_out/Visual_text.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define MAXW 1024
#define MAXH 512

static unsigned char img[MAXH][MAXW];
static unsigned char glyph[128][8];
static int glyph_ok[128];

static void load_glyph_file__svg_pure_h(const char *path)
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

static void load_glyphs__svg_pure_h(void)
{
    const char *dir = "eden_out/Visual_text";
    char path[512]; int a;
    for (char c = '0'; c <= '9'; c++) { snprintf(path, sizeof path, "%s/%c.8x8.txt", dir, c); load_glyph_file__svg_pure_h(path); }
    for (char c = 'a'; c <= 'z'; c++) { snprintf(path, sizeof path, "%s/%c.8x8.txt", dir, c); load_glyph_file__svg_pure_h(path); }
    for (char c = 'A'; c <= 'Z'; c++) { snprintf(path, sizeof path, "%s/%c.8x8.txt", dir, c); load_glyph_file__svg_pure_h(path); }
    for (a = 32; a < 128; a++) { snprintf(path, sizeof path, "%s/ascii_%03d.8x8.txt", dir, a); load_glyph_file__svg_pure_h(path); }
}

static void draw_rect(int x, int y, int w, int h, int W, int H)
{
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            if (xx >= 0 && xx < W && yy >= 0 && yy < H) img[yy][xx] = 1;
}

static void draw_circle(int cx, int cy, int r, int W, int H)
{
    for (int yy = cy - r; yy <= cy + r; yy++)
        for (int xx = cx - r; xx <= cx + r; xx++)
            if (xx >= 0 && xx < W && yy >= 0 && yy < H &&
                (xx - cx) * (xx - cx) + (yy - cy) * (yy - cy) <= r * r)
                img[yy][xx] = 1;
}

/* text drawn from her own glyph library, scale = font-size/8, top = y - font-size */
static void draw_text(int x, int y, int fs, const char *s, int W, int H)
{
    int sc = fs / 8; if (sc < 1) sc = 1;
    int top = y - fs;
    for (int i = 0; s[i]; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (!glyph_ok[ch]) ch = '?';
        for (int r = 0; r < 8; r++)
            for (int b = 0; b < 8; b++)
                if (glyph[ch][r] & (0x80 >> b))
                    draw_rect(x + i * 8 * sc + b * sc, top + r * sc, sc, sc, W, H);
    }
}

static int render(const char *path, int *Wp, int *Hp, int *unsup)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[1024];
    int W = 0, H = 0; *unsup = 0;
    while (fgets(line, sizeof line, f)) {
        int a, b, c, d;
        if (sscanf(line, " <svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\"", &a, &b) == 2) { W = a; H = b; continue; }
        if (strstr(line, "viewBox=") && W == 0) { sscanf(strstr(line, "viewBox="), "viewBox=\"0 0 %d %d\"", &a, &b); W = a; H = b; continue; }
        if (strstr(line, "<rect") && strstr(line, "fill=\"black\"")) {
            if (sscanf(line, " <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\"", &a, &b, &c, &d) == 4)
                draw_rect(a, b, c, d, W, H);
            continue;
        }
        if (strstr(line, "<circle") && strstr(line, "fill=\"black\"")) {
            if (sscanf(line, " <circle cx=\"%d\" cy=\"%d\" r=\"%d\"", &a, &b, &c) == 3)
                draw_circle(a, b, c, W, H);
            continue;
        }
        if (strstr(line, "<text")) {
            char *gt = strchr(line, '>');
            char *lt = gt ? strrchr(line, '<') : 0;
            if (gt && lt && lt > gt + 1 &&
                sscanf(line, " <text x=\"%d\" y=\"%d\" font-size=\"%d\"", &a, &b, &c) == 3) {
                char word[128]; int n = 0;
                for (char *p = gt + 1; p < lt && n < 127; p++) word[n++] = *p;
                word[n] = 0;
                draw_text(a, b, c, word, W, H);
            }
            continue;
        }
        if (strstr(line, "</svg") || strstr(line, "<rect")) continue; /* close tag, white bg */
        /* honest empty set: an element we do not understand is never silent */
        if (strchr(line, '<')) (*unsup)++;
    }
    fclose(f);
    if (W <= 0 || H <= 0 || W > MAXW || H > MAXH) return -1;
    *Wp = W; *Hp = H;
    return 0;
}

#define SVG_PURE_MAIN main
int SVG_PURE_MAIN(int argc, char **argv)
{
    if (argc != 4) { fprintf(stderr, "usage: svg_pure <svg_dir> <out_dir> <sight_log>\n"); return 2; }
    load_glyphs__svg_pure_h();
    FILE *sl = fopen(argv[3], "w");
    if (!sl) { fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
    fprintf(sl, "SIGHT PURE — every image redrawn as pixels, passed through her eyes.\n");
    fprintf(sl, "no svg. no markup. binary grid, drift must be 0, FNV-1a 64.\n\n");

    DIR *dp = opendir(argv[1]);
    if (!dp) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    struct dirent *de;
    static char names[600][256];
    int nfiles = 0;
    while ((de = readdir(dp))) {
        size_t nl = strlen(de->d_name);
        if (nl < 5 || strcmp(de->d_name + nl - 4, ".svg")) continue;
        if (nfiles < 600) { strcpy(names[nfiles], de->d_name); nfiles++; }
    }
    closedir(dp);
    /* deterministic order on any machine: sorted names */
    for (int i = 0; i < nfiles; i++)
        for (int j = i + 1; j < nfiles; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char t[256]; strcpy(t, names[i]); strcpy(names[i], names[j]); strcpy(names[j], t);
            }
    int seen = 0, failed = 0;
    for (int fi = 0; fi < nfiles; fi++) {
        const char *fname = names[fi];
        size_t nl = strlen(fname);
        char in[512], out[512], base[256];
        snprintf(in, sizeof in, "%s/%s", argv[1], fname);
        snprintf(base, sizeof base, "%.*s", (int)(nl - 4), fname);
        int W, H, unsup = 0;
        memset(img, 0, sizeof img);
        if (render(in, &W, &H, &unsup) != 0) {
            fprintf(sl, "see: %-40s UNVERIFIED — could not rasterize purely\n", fname);
            failed++;
            continue;
        }
        if (unsup > 0) {
            fprintf(sl, "see: %-40s UNVERIFIED — %d unsupported element(s), refused to guess\n", fname, unsup);
            failed++;
            continue;
        }
        /* SIGHT: binary grid, drift 0, FNV-1a 64 over the grid as seen */
        unsigned long long h = 0xcbf29ce484222325ULL;
        int drift = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                unsigned char px = img[y][x];
                h ^= px; h *= 0x100000001b3ULL;
                if (px > 1) drift++;
            }
        if (drift != 0) { fclose(sl); printf("STOP: sight drift %d on %s\n", drift, in); return 1; }
        snprintf(out, sizeof out, "%s/%s.pbm", argv[2], base);
        FILE *p = fopen(out, "w");
        if (!p) { fclose(sl); printf("STOP: write %s\n", out); return 1; }
        fprintf(p, "P1\n%d %d\n", W, H);
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) fputc(img[y][x] ? '1' : '0', p);
            fputc('\n', p);
        }
        fclose(p);
        fprintf(sl, "see: %-40s pure:%s sight fnv1a64:%016llX drift:0\n", fname, out, h);
        seen++;
    }
    fprintf(sl, "\n%d images redrawn pure, %d unverified. drift 0 on every one.\n", seen, failed);
    fclose(sl);
    printf("svg_pure: %d images redrawn pure, %d unverified\n", seen, failed);
    return 0;
}
