/* intake_test.c — the graft harness.
 *
 * Builds GLYPH_MANIFEST.txt in fixed deterministic order:
 *   1. the 107 Visual_text 8x8 glyphs (digits, number-words, A-Z, a-z,
 *      punctuation) — wav bound for every one the Doctor recorded,
 *      NONE for the punctuation sounds still missing
 *      (data/source/MISSING_FOUNDATION_SOUNDS.tsv, goddess-school branch)
 *   2. the 96 ascii64 64x64 glyph grids (ASCII 32..127) — sight only,
 *      these are Momma's font, not her alphabet (HER_EYES_PURIFY §4)
 * then runs eye_intake_run() and prints glyphs, blocks, stream pin.
 * -O0 == -O2, byte for byte.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void     eye_intake_init(void);
uint64_t eye_intake_run(void);
uint64_t eye_stream_pin(void);
uint64_t eye_blocks(void);
uint64_t eye_glyphs(void);
void     eye_seal(void);

static void write_manifest(void)
{
    static const char *wav_names[] = {
        "0","1","2","3","4","5","6","7","8","9",
        "zero","one","two","three","four","five","six","seven","eight","nine","ten",
        "A","B","C","D","E","F","G","H","I","J","K","L","M",
        "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
        "a","b","c","d","e","f","g","h","i","j","k","l","m",
        "n","o","p","q","r","s","t","u","v","w","x","y","z"
    };
    static const int punct_codes[] = {
        32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
        58,59,60,61,62,63,64,91,92,93,94,95,96,123,124,125,126
    };
    FILE *m = fopen("GLYPH_MANIFEST.txt", "w");
    size_t i;
    int c;
    if (!m) return;
    for (i = 0; i < sizeof wav_names / sizeof wav_names[0]; i++)
        fprintf(m, "vt/%s.8x8.txt 8x8 %s.wav\n", wav_names[i], wav_names[i]);
    for (i = 0; i < sizeof punct_codes / sizeof punct_codes[0]; i++)
        fprintf(m, "vt/ascii_%03d.8x8.txt 8x8 NONE\n", punct_codes[i]);
    for (c = 32; c <= 127; c++)
        fprintf(m, "a64/ascii_%03d.txt 64x64 NONE\n", c);
    fclose(m);
}

int main(void)
{
    remove("SIGHT.ndx");
    write_manifest();
    eye_intake_init();
    printf("glyphs     %llu\n", (unsigned long long)eye_intake_run());
    eye_seal(); /* F10: the stream pin rides into SIGHT.ndx */
    printf("blocks     %llu\n", (unsigned long long)eye_blocks());
    printf("stream pin %016llX\n", (unsigned long long)eye_stream_pin());
    puts("--- SIGHT.ndx (first 6 + last 3 sight lines) ---");
    {
        FILE *f = fopen("SIGHT.ndx", "r");
        char line[256];
        int shown = 0, skipped = 0;
        long pos[3];
        int npos = 0, total = 0;
        if (f) {
            while (fgets(line, sizeof line, f)) {
                if (strncmp(line, "sight ", 6) == 0) {
                    total++;
                    if (shown < 6) { fputs(line, stdout); shown++; }
                }
            }
            fclose(f);
        }
        (void)skipped; (void)pos; (void)npos;
        printf("... %d sight lines total\n", total);
    }
    return 0;
}
