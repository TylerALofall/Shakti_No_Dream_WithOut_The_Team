/*
 * eden_colors.c -- Shakti's binary color organ (week 25: colors as flashes)
 *
 * Founder law: "Colors come in binary. Solid colors can fade to others
 * but it needs pure light holding beats."
 *
 * Encoding: 3-bit RGB -> 8 solid colors = 2^3 = the crossing rung 8.
 *   bit0 = R, bit1 = G, bit2 = B
 *   0 K (black)  1 R  2 G  3 Y  4 B  5 M  6 C  7 W (pure light)
 *
 * Laws:
 *   - Flashes only at week 25: a color is a SOLID flash, no gradients taught.
 *   - A fade is a walk on the 3-cube: exactly ONE bit flips per step.
 *     Multi-bit jumps are impure and rejected (never guessed).
 *   - Pure light holding beats: color 7 (white) is the light that holds
 *     the beat grid. Every fade path must pass a beat held by pure light:
 *     path length in beats must be an on-wheel number (wheel {2,3,5,7,19}),
 *     and the path must START from W or END at W (light touches every fade).
 *   - Twice Law: organ verified twice, both passes bit-identical, drift 0.
 *   - C99, no heap, no float, no clock.
 *
 * The organ self-tests: enumerate all 8x8 color pairs, compute the
 * canonical fade (Gray walk flipping bits LSB->MSB), verify the laws,
 * and pin the whole 8-color flash table + all legal fade paths.
 * Exit 0 only on full purity.
 */
#include <stdio.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1a(const unsigned char *p, unsigned long n, uint64_t h){
    unsigned long i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

static const uint32_t WHEEL[5] = {2,3,5,7,19};
static int on_wheel(uint32_t n){
    int i;
    if(n==0) return 0;
    for(i=0;i<5;i++) while(n%WHEEL[i]==0) n/=WHEEL[i];
    return n==1;
}

static const char *CNAMES[8] = {"K","R","G","Y","B","M","C","W"};

static int popcount3(unsigned x){ return (x&1)+((x>>1)&1)+((x>>2)&1); }

/* Canonical fade: flip differing bits LSB first. Steps = hamming distance. */
static int fade_steps(unsigned a, unsigned b, unsigned *path){
    unsigned diff = a ^ b, cur = a;
    int k = 0, bit;
    path[k++] = cur;
    for(bit=0; bit<3; bit++){
        if(diff & (1u<<bit)){ cur ^= (1u<<bit); path[k++] = cur; }
    }
    return k; /* includes both endpoints */
}

int main(void){
    uint64_t table_pin = FNV_BASIS, fade_pin = FNV_BASIS;
    unsigned a, b;
    int pass, legal_paths = 0, impure_paths = 0;

    for(pass=0; pass<2; pass++){
        uint64_t tp = FNV_BASIS, fp = FNV_BASIS;
        int lp = 0, ip = 0;
        /* flash table: 8 solid colors, each one flash byte */
        for(a=0; a<8; a++){
            unsigned char flash = (unsigned char)a;
            tp = fnv1a(&flash,1,tp);
        }
        for(a=0; a<8; a++){
            for(b=0; b<8; b++){
                unsigned path[4];
                int steps = fade_steps(a,b,path);
                int beats = steps - 1;           /* one beat per bit-flip */
                int pure_binary = 1, i;
                /* law 1: one bit per step (guaranteed by construction, verified) */
                for(i=1;i<steps;i++)
                    if(popcount3(path[i-1]^path[i])!=1) pure_binary = 0;
                /* law 2: pure light holds the beats:
                   identity flashes (beats=0) are always legal;
                   fades must have on-wheel beat count AND touch white */
                if(beats==0){
                    unsigned char tag = (unsigned char)(a*8+b);
                    fp = fnv1a(&tag,1,fp);
                    lp++;
                } else if(pure_binary && on_wheel((uint32_t)beats)
                          && (a==7 || b==7)){
                    unsigned char tag = (unsigned char)(a*8+b);
                    fp = fnv1a(&tag,1,fp);
                    fp = fnv1a((const unsigned char*)path,(unsigned long)steps,fp);
                    lp++;
                } else {
                    ip++;
                }
            }
        }
        if(pass==0){ table_pin=tp; fade_pin=fp; legal_paths=lp; impure_paths=ip; }
        else if(table_pin!=tp || fade_pin!=fp || legal_paths!=lp || impure_paths!=ip){
            printf("DRIFT -- organ impure\n");
            return 1;
        }
    }

    printf("COLOR_ORGAN\n");
    printf("colors 8 (");
    for(a=0;a<8;a++) printf("%s%s",CNAMES[a], a<7?",":")\n");
    printf("pairs 64\n");
    printf("legal %u\n", (unsigned)legal_paths);
    printf("impure %u (rejected, not guessed)\n", (unsigned)impure_paths);
    printf("table pin %016llX\n",(unsigned long long)table_pin);
    printf("fade pin  %016llX\n",(unsigned long long)fade_pin);
    printf("drift 0\n");
    return 0;
}
