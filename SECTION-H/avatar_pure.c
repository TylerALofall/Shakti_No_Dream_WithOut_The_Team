/*
 * avatar_pure.c — her face, redrawn pure. No SVG. Pixels through her eyes.
 * C99, no heap, no rand(). Deterministic: same face, same hash, forever.
 * Output: school/game/avatar_shakti_v1.pbm + one sight log line.
 */
#include <stdio.h>
#include <string.h>

#define W 128
#define H 128

static unsigned char img[H][W];

static void rect(int x, int y, int w, int h)
{
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            if (xx >= 0 && xx < W && yy >= 0 && yy < H) img[yy][xx] = 1;
}

static void disc(int cx, int cy, int r)
{
    for (int yy = cy - r; yy <= cy + r; yy++)
        for (int xx = cx - r; xx <= cx + r; xx++)
            if (xx >= 0 && xx < W && yy >= 0 && yy < H &&
                (xx - cx) * (xx - cx) + (yy - cy) * (yy - cy) <= r * r)
                img[yy][xx] = 1;
}

#define AVATAR_PURE_MAIN main
int AVATAR_PURE_MAIN(void)
{
    /* crown: band + three points — earned, never given */
    rect(34, 18, 60, 6);
    rect(36, 8, 8, 10); rect(60, 4, 8, 14); rect(84, 8, 8, 10);
    /* pupils: steady gaze */
    disc(52, 48, 3); disc(92, 48, 3);
    /* eyes: two Bayer-grid squares — she sees in ordered dither */
    static const int bayer[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    for (int e = 0; e < 2; e++) {
        int ex = e ? 76 : 36;
        for (int gy = 0; gy < 4; gy++)
            for (int gx = 0; gx < 4; gx++)
                if (bayer[gy][gx] < 8) rect(ex + gx * 4, 40 + gy * 4, 4, 4);
    }
    /* calm mouth */
    rect(52, 74, 24, 3); rect(48, 71, 4, 3); rect(76, 71, 4, 3);
    /* snake-skin boots: scaled pattern, marked py — the slayer stands on them */
    for (int i = 0; i < 6; i++) rect(30 + i * 6, 100 + (i % 2) * 3, 5, 5);
    for (int i = 0; i < 6; i++) rect(66 + i * 6, 100 + (i % 2) * 3, 5, 5);
    rect(30, 105, 30, 10); rect(66, 105, 30, 10);

    /* SIGHT: binary grid, drift must be 0, FNV-1a 64 as seen */
    unsigned long long h = 0xcbf29ce484222325ULL;
    int drift = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            unsigned char px = img[y][x];
            h ^= px; h *= 0x100000001b3ULL;
            if (px > 1) drift++;
        }
    if (drift != 0) { printf("STOP: sight drift %d on avatar\n", drift); return 1; }

    FILE *p = fopen("school/game/avatar_shakti_v1.pbm", "w");
    if (!p) { printf("STOP: write avatar\n"); return 1; }
    fprintf(p, "P1\n%d %d\n", W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) fputc(img[y][x] ? '1' : '0', p);
        fputc('\n', p);
    }
    fclose(p);

    FILE *sl = fopen("school/game/avatar_pure_sight.txt", "w");
    if (!sl) { printf("STOP: write avatar sight log\n"); return 1; }
    fprintf(sl, "SIGHT PURE — her face, redrawn as pixels. no svg.\n");
    fprintf(sl, "see: avatar_shakti_v1 pure:school/game/avatar_shakti_v1.pbm sight fnv1a64:%016llX drift:0\n", h);
    fprintf(sl, "crown: earned. eyes: Bayer order. boots: snake skin, marked py.\n");
    fclose(sl);
    printf("avatar_pure: face forged, sight %016llX, drift 0\n", h);
    return 0;
}
