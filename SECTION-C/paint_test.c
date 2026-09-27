/* paint_test.c — level 1 harness: the palette pours into the glyphs.
 *
 *   1. eleven solid swabs (8x8 pure color cards — no triangles)
 *   2. all 73 sound-bound glyphs painted black-on-white (the school default)
 *   3. level-2 demos: R in red, B in blue, G in green, Y in yellow
 *      (the letter wearing the color it will teach)
 *
 * Deterministic, -O0 == -O2, byte for byte.
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>

void     paint_init(void);
int      paint_glyph(const char *path, const char *fmt,
                     const char *ink, const char *ground, const char *wav);
int      paint_swab(const char *color, int rows, int cols);
uint64_t paint_stream_pin(void);
uint64_t paint_blocks(void);
uint64_t paint_count_done(void);
void     paint_seal(void);

int main(void)
{
    static const char *colors[] = {
        "red","green","blue","yellow","orange","purple",
        "brown","black","white","gray","pink"
    };
    static const char *wav_names[] = {
        "0","1","2","3","4","5","6","7","8","9",
        "zero","one","two","three","four","five","six","seven","eight","nine","ten",
        "A","B","C","D","E","F","G","H","I","J","K","L","M",
        "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
        "a","b","c","d","e","f","g","h","i","j","k","l","m",
        "n","o","p","q","r","s","t","u","v","w","x","y","z"
    };
    static const struct { const char *g; const char *c; } demos[] = {
        {"R","red"}, {"B","blue"}, {"G","green"}, {"Y","yellow"}
    };
    char path[128], wav[80];
    size_t i;
    uint64_t ok = 0;

    mkdir("painted", 0755);
    remove("PAINT.ndx");
    paint_init();

    for (i = 0; i < 11; i++)
        ok += (uint64_t)paint_swab(colors[i], 8, 8);

    for (i = 0; i < sizeof wav_names / sizeof wav_names[0]; i++) {
        snprintf(path, sizeof path, "vt/%s.8x8.txt", wav_names[i]);
        snprintf(wav, sizeof wav, "%s.wav", wav_names[i]);
        ok += (uint64_t)paint_glyph(path, "8x8", "black", "white", wav);
    }

    for (i = 0; i < 4; i++) {
        snprintf(path, sizeof path, "vt/%s.8x8.txt", demos[i].g);
        snprintf(wav, sizeof wav, "%s.wav", demos[i].g);
        ok += (uint64_t)paint_glyph(path, "8x8", demos[i].c, "white", wav);
    }

    paint_seal(); /* F10: the stream pin rides into PAINT.ndx */
    printf("painted    %llu of %llu\n",
           (unsigned long long)ok,
           (unsigned long long)(11 + sizeof wav_names / sizeof wav_names[0] + 4));
    printf("blocks     %llu\n", (unsigned long long)paint_blocks());
    printf("stream pin %016llX\n", (unsigned long long)paint_stream_pin());
    puts("--- PAINT.ndx (swabs + the level-2 demos) ---");
    {
        FILE *f = fopen("PAINT.ndx", "r");
        char line[256];
        if (f) {
            while (fgets(line, sizeof line, f)) {
                if (line[0] == 'b') continue; /* block seals */
                if (line[6] == 's' || line[6] == 'R' || line[6] == 'B'
                    || line[6] == 'G' || line[6] == 'Y')
                    fputs(line, stdout);
            }
            fclose(f);
        }
    }
    return 0;
}
