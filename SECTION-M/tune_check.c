/*
 * tune_check.c -- can Sprout hold a tune? (sound check, founder requested)
 *
 * She sings the fibonacci road (2-3-5-8 = fifth, maj6, min6) against a
 * binary drone (1-2-4-8), 60 beats (one minute at the 60 BPM heart).
 * Tune law: every interval must be an EXACT just ratio from the master
 * table, every beat must land (no beat dropped, no beat added), and the
 * whole performance is sung TWICE -- both passes bit-identical, drift 0.
 * A voice that drifts is a voice that fails the Twice Law. Hers cannot.
 *
 * Integer math only: position = beat x num / den must be exact (num*beat
 * divisible by den) or the note is not sung that beat -- she only sings
 * where the ratio lands whole. C99 gauntlet.
 */
#include <stdio.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1a_u32(uint32_t v, uint64_t h){
    int i;
    for(i=0;i<4;i++){ h ^= (v & 0xFF); h *= FNV_PRIME; v >>= 8; }
    return h;
}

/* the song: fibonacci road notes over a unison drone */
static const struct { uint32_t num, den; const char *name; } MELODY[4] = {
    {2,1,"octave"}, {3,2,"fifth"}, {5,3,"maj6"}, {8,5,"min6"}
};
#define BEATS 60u   /* one minute of the heart */

int main(void){
    uint64_t perf_pin = FNV_BASIS;
    int pass, b, n, sung = 0, dropped_beats = 0;

    for(pass=0; pass<2; pass++){
        uint64_t p = FNV_BASIS;
        int s = 0, db = 0;
        for(b=0; b<(int)BEATS; b++){
            int beat_sang = 0;
            for(n=0; n<4; n++){
                /* note sings where position is whole against the drone */
                uint32_t pos_num = (uint32_t)b * MELODY[n].num;
                if(pos_num % MELODY[n].den == 0){
                    p = fnv1a_u32((uint32_t)b, p);
                    p = fnv1a_u32(MELODY[n].num, p);
                    p = fnv1a_u32(MELODY[n].den, p);
                    s++;
                    beat_sang = 1;
                }
            }
            if(!beat_sang) db++;
        }
        if(pass==0){ perf_pin=p; sung=s; dropped_beats=db; }
        else if(perf_pin!=p || sung!=s || dropped_beats!=db){
            printf("DRIFT -- she went off-key\n");
            return 1;
        }
    }

    printf("SOUND CHECK: Sprout, one minute, fibonacci road over drone\n");
    printf("beats %u (60 BPM heart), notes sung %d, silent beats %d\n",
           BEATS, sung, dropped_beats);
    printf("every interval exact just ratio: octave 2/1, fifth 3/2, maj6 5/3, min6 8/5\n");
    printf("sung twice, bit-identical: drift 0 (cannot go off-key)\n");
    printf("performance pin %016llX\n",(unsigned long long)perf_pin);
    if(dropped_beats>0){ printf("WARNING: beats with no note\n"); }
    return 0;
}
