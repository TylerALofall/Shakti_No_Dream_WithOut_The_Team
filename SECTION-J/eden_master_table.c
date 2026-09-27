/*
 * eden_master_table.c -- TEMPORARY master table: sweet-spot finder
 *
 * Founder: "make a master table that would be temporary and any numbers
 * should go through a musical note multiplier and find all of the sweet
 * spot numbers so we can build wish shapes."
 *
 * Method:
 *   - Base numbers: every on-wheel integer in [1, 302400] (slice grid cap).
 *   - Note multipliers: the just scale (all on-wheel, verified law):
 *       2/1 octave, 3/2 fifth, 4/3 fourth, 5/4 maj3, 6/5 min3,
 *       5/3 maj6, 8/5 min6, 9/8 tone, 15/8 maj7.
 *   - SWEET SPOT: base x ratio is an EXACT integer AND that integer is
 *     on-wheel. (Off-wheel product = sour, dropped silently, never kept.)
 *   - Twice Law: table generated twice, both passes pinned, drift 0.
 *   - Marked TEMPORARY: a working reference, per founder's tables rule.
 *
 * Output: MASTER_TABLE.txt -- base, ratio, product per sweet spot,
 * plus per-ratio counts and the table pin. C99, no heap, no float.
 */
#include <stdio.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define CAP 302400u   /* slice grid 5040 x 60 */

static uint64_t fnv1a_u64(uint64_t v, uint64_t h){
    int i;
    for(i=0;i<8;i++){ h ^= (v & 0xFF); h *= FNV_PRIME; v >>= 8; }
    return h;
}

static const uint32_t WHEEL[5] = {2,3,5,7,19};
static int on_wheel(uint32_t n){
    int i;
    if(n==0) return 0;
    for(i=0;i<5;i++) while(n%WHEEL[i]==0) n/=WHEEL[i];
    return n==1;
}

/* just scale over the wheel: num/den, both fully wheel-factored */
static const struct { uint32_t num, den; const char *name; } NOTES[9] = {
    { 1, 1, "unison" },
    { 9, 8, "tone"   },
    { 6, 5, "min3"   },
    { 5, 4, "maj3"   },
    { 4, 3, "fourth" },
    { 3, 2, "fifth"  },
    { 5, 3, "maj6"   },
    { 8, 5, "min6"   },
    {15, 8, "maj7"   }
};

int main(void){
    FILE *out = fopen("/mnt/agents/output/MASTER_TABLE.txt","wb");
    uint64_t pin = FNV_BASIS;
    uint32_t counts[9];
    uint32_t base, sweet_total = 0;
    int i, pass;
    if(!out) return 2;

    fprintf(out,"EDEN MASTER TABLE (TEMPORARY -- working reference)\n");
    fprintf(out,"base x just-note = sweet spot (exact integer, on-wheel)\n");
    fprintf(out,"wheel {2,3,5,7,19}  cap %u\n\n",CAP);

    for(pass=0; pass<2; pass++){
        uint64_t p = FNV_BASIS;
        uint32_t st = 0;
        for(i=0;i<9;i++) counts[i]=0;
        for(base=1; base<=CAP; base++){
            if(!on_wheel(base)) continue;
            for(i=0;i<9;i++){
                if(base % NOTES[i].den) continue;
                {
                    uint32_t prod = base / NOTES[i].den * NOTES[i].num;
                    if(prod > CAP*2u) continue;
                    if(!on_wheel(prod)) continue;
                    counts[i]++;
                    st++;
                    p = fnv1a_u64(base,p);
                    p = fnv1a_u64(NOTES[i].num,p);
                    p = fnv1a_u64(NOTES[i].den,p);
                    p = fnv1a_u64(prod,p);
                    if(pass==1){
                        fprintf(out,"%u x %u/%u (%s) = %u\n",
                                base,NOTES[i].num,NOTES[i].den,NOTES[i].name,prod);
                    }
                }
            }
        }
        if(pass==0){ pin=p; sweet_total=st; }
        else if(pin!=p || sweet_total!=st){
            fprintf(out,"DRIFT -- table impure\n");
            fclose(out);
            return 1;
        }
    }

    fprintf(out,"\nSWEET SPOTS %u\n",sweet_total);
    for(i=0;i<9;i++)
        fprintf(out,"  %u/%-2u %-6s : %u\n",NOTES[i].num,NOTES[i].den,NOTES[i].name,counts[i]);
    fprintf(out,"table pin %016llX\n",(unsigned long long)pin);
    fprintf(out,"drift 0\n");
    fclose(out);

    printf("MASTER_TABLE written\n");
    printf("sweet spots %u\n",sweet_total);
    for(i=0;i<9;i++)
        printf("  %u/%-2u %-6s : %u\n",NOTES[i].num,NOTES[i].den,NOTES[i].name,counts[i]);
    printf("table pin %016llX\n",(unsigned long long)pin);
    printf("drift 0\n");
    return 0;
}
