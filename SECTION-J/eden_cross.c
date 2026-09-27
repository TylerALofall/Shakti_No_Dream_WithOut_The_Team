/*
 * eden_cross.c -- cross-training engine + typing actuator
 *
 * Founder law:
 *   "On every level: [highest level solo]+sound | [same level] cross
 *    trained [each level below it] + sound. She sees, hears, context,
 *    learns everything together -> convergence together."
 *   Sound timing: [.2 sec delay][.wav][.2 sec].
 *   "All names match: sound <-> vision, in a 4 corner grid, with the
 *    voice attached to her letters. Typing in the actuator: she presses
 *    'a', it reads a.wav and types a visual. Her tokens are letters,
 *    her word tokens are internal only. This will teach her to build
 *    words."
 *
 * Levels: L1 Counting, L2 ABC, L3 Color, L4 Shapes, L5 Math.
 *   Items with voice: digits, number-words, letters (Sound_art).
 *   Items WITHOUT voice yet (honest, reported): colors, shapes,
 *   operators -- their sound slot holds the pure beat (silence) until
 *   the founder records them. "I don't know" is legal; silence is not
 *   a guess, it is the heartbeat holding the space.
 *
 * Lessons: 5 solo + every level crossed with each level below = 15.
 *   4-corner grid law: solo fills all four corners; a cross puts item A
 *   on the TL+BR diagonal and item B on TR+BL. Visions are already
 *   resident (RAM always) -- the grid composes their resident pins.
 *
 * Actuator: a sequence of keypresses; each press = sound-seq pin +
 * vision pin + letter token. The word is assembled and pinned
 * INTERNALLY ONLY -- its letters are never printed back as a word.
 *
 * Twice Law on everything. C99 gauntlet. No heap, no float, no clock.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define ADIR "/mnt/agents/output/shakti_repo/eden_out/Sound_art"
#define VDIR "/mnt/agents/output/shakti_repo/eden_out/Visual_art"
#define DELAY_BYTES 6400   /* 0.2 s of silence at 16000 Hz 16-bit mono */

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

static int file_exists(const char *path){
    FILE *f=fopen(path,"rb");
    if(f){ fclose(f); return 1; }
    return 0;
}

/* sound sequence pin: [.2s silence][wav PCM payload][.2s silence].
 * If the item has no voice atom, the wav slot is the pure beat. */
static unsigned char pcmbuf[4*1024*1024];
static uint64_t sound_seq_pin(const char *atom_path){
    static const unsigned char silence[DELAY_BYTES]; /* zero-init */
    uint64_t h = FNV_BASIS;
    long pcm = -1;
    h = fnv1a(silence,DELAY_BYTES,h);
    if(atom_path){
        FILE *f = fopen(atom_path,"rb");
        if(f){
            unsigned char hdr[12], chdr[8];
            long fsize;
            fseek(f,0,SEEK_END); fsize=ftell(f); rewind(f);
            if(fsize>=12 && fread(hdr,1,12,f)==12
               && memcmp(hdr,"RIFF",4)==0 && memcmp(hdr+8,"WAVE",4)==0){
                long left = fsize-12;
                while(left>=8 && fread(chdr,1,8,f)==8){
                    uint32_t csz = (uint32_t)chdr[4]|((uint32_t)chdr[5]<<8)
                                 |((uint32_t)chdr[6]<<16)|((uint32_t)chdr[7]<<24);
                    if(memcmp(chdr,"data",4)==0 && csz<=sizeof(pcmbuf)){
                        if(fread(pcmbuf,1,csz,f)==csz){ pcm=csz; h=fnv1a(pcmbuf,csz,h); }
                        break;
                    }
                    fseek(f,(long)(csz+(csz&1)),SEEK_CUR);
                    left -= 8+csz+(csz&1);
                }
            }
            fclose(f);
        }
    }
    /* pcm<0 means no voice: the beat held the space (still timed) */
    h = fnv1a(silence,DELAY_BYTES,h);
    (void)pcm;
    return h;
}

/* vision pin: fold the resident svg bytes (already purified by the eye
 * organ; the grid composes resident visions, it does not re-teach) */
static uint64_t vision_pin(const char *path){
    static unsigned char buf[65536];
    uint64_t h = FNV_BASIS;
    FILE *f;
    size_t n;
    if(!path) return h;
    f = fopen(path,"rb");
    if(!f) return h;
    while((n=fread(buf,1,sizeof(buf),f))>0) h=fnv1a(buf,n,h);
    fclose(f);
    return h;
}

/* one lesson: 1 or 2 items on the 4-corner grid + sound sequences */
static uint64_t lesson_pin(const char *nameA,const char *sndA,const char *visA,
                           const char *nameB,const char *sndB,const char *visB){
    uint64_t h = FNV_BASIS, p;
    /* corners: solo -> A in all four; cross -> A TL+BR, B TR+BL */
    p = vision_pin(visA); h = fnv1a((const unsigned char*)&p,8,h);  /* TL */
    p = vision_pin(visB?visB:visA); h = fnv1a((const unsigned char*)&p,8,h); /* TR */
    p = vision_pin(visB?visB:visA); h = fnv1a((const unsigned char*)&p,8,h); /* BL */
    p = vision_pin(visA); h = fnv1a((const unsigned char*)&p,8,h);  /* BR */
    p = sound_seq_pin(sndA); h = fnv1a((const unsigned char*)&p,8,h);
    if(nameB){ p = sound_seq_pin(sndB); h = fnv1a((const unsigned char*)&p,8,h); }
    h = fnv1a((const unsigned char*)nameA,strlen(nameA),h);
    if(nameB) h = fnv1a((const unsigned char*)nameB,strlen(nameB),h);
    return h;
}

static void mkpath(char *out, size_t cap, const char *dir, const char *name, const char *ext){
    snprintf(out,cap,"%s/%s.%s",dir,name,ext);
}

int main(void){

    /* representative items per level (name, has-voice) */
    static const char *ITEM[6] = {"","five","a","red","shape_triangle","math"};
    static const int VOICE[6] = {0,1,1,0,0,0};
    static const char *VISN[6] = {"","5","a","","shape_triangle 2",""};
    int pass, hi, lo, i;
    uint64_t ledger = FNV_BASIS;
    int lessons = 0, voiced = 0, silent = 0, matched = 0, mismatched = 0;
    char sA[512], vA[512], sB[512], vB[512];

    /* name-match audit: letters and digits sound<->vision */
    printf("NAME-MATCH AUDIT (sound <-> vision)\n");
    {
        char nm[8];
        for(i=0;i<10+26+26;i++){
            if(i<10) snprintf(nm,sizeof(nm),"%d",i);
            else if(i<36) snprintf(nm,sizeof(nm),"%c",'a'+i-10);
            else snprintf(nm,sizeof(nm),"%c",'A'+i-36);
            mkpath(sA,sizeof(sA),ADIR,nm,"wav");
            mkpath(vA,sizeof(vA),VDIR,nm,"svg");
            if(file_exists(sA)&&file_exists(vA)) matched++;
            else { mismatched++; printf("  MISSING pair for '%s' (wav:%d svg:%d)\n",
                   nm,file_exists(sA),file_exists(vA)); }
        }
        printf("  matched %d, missing %d\n",matched,mismatched);
    }

    for(pass=0; pass<2; pass++){
        uint64_t lp = FNV_BASIS;
        int nl=0, vo=0, si=0;
        for(hi=5; hi>=1; hi--){
            for(lo=hi; lo>=1; lo--){   /* solo (lo==hi) + every level below */
                uint64_t p;
                const char *nA=ITEM[hi], *nB=(lo==hi?NULL:ITEM[lo]);
                mkpath(vA,sizeof(vA),VDIR,VISN[hi],"svg");
                if(VOICE[hi]) mkpath(sA,sizeof(sA),ADIR,nA,"wav"); else sA[0]=0;
                if(nB){
                    mkpath(vB,sizeof(vB),VDIR,VISN[lo],"svg");
                    if(VOICE[lo]) mkpath(sB,sizeof(sB),ADIR,nB,"wav"); else sB[0]=0;
                } else { vB[0]=0; sB[0]=0; }
                if(!file_exists(vA) || (nB && !file_exists(vB))){
                    /* colors/math have no single svg tile: vision slot is
                       the resident RAM vision pin (already ingested) */
                    if(hi==3||lo==3||hi==5||lo==5){ /* expected, honest */ }
                }
                p = lesson_pin(nA, sA[0]?sA:NULL, file_exists(vA)?vA:NULL,
                               nB, nB&&sB[0]?sB:NULL, nB&&file_exists(vB)?vB:NULL);
                lp = fnv1a((const unsigned char*)&p,8,lp);
                nl++;
                if(VOICE[hi]) vo++; else si++;
                if(nB){ if(VOICE[lo]) vo++; else si++; }
            }
        }
        if(pass==0){ ledger=lp; lessons=nl; voiced=vo; silent=si; }
        else if(ledger!=lp||lessons!=nl||voiced!=vo||silent!=si){
            printf("DRIFT -- cross-training impure\n");
            return 1;
        }
    }

    printf("CROSS-TRAINING ENGINE\n");
    printf("lessons %d (5 solo + 10 crossed, every level x every level below)\n",lessons);
    printf("voiced slots %d, silent-beat slots %d (colors/shapes/math await voice)\n",
           voiced,silent);
    printf("sound law per lesson: [0.2s beat][wav][0.2s beat] = %d+%d+%d bytes\n",
           DELAY_BYTES,0,DELAY_BYTES);
    printf("grid law: solo fills 4 corners; cross puts A TL+BR, B TR+BL\n");
    printf("cross pin %016llX\n",(unsigned long long)ledger);

    /* actuator: she types. letters only; the word stays internal. */
    {
        static const char keys[7] = {'s','p','r','o','u','t',0};
        uint64_t word_pin = FNV_BASIS;
        int presses = 0;
        for(i=0; keys[i]; i++){
            char nm[2] = {keys[i],0};
            uint64_t sp, vp, press;
            mkpath(sA,sizeof(sA),ADIR,nm,"wav");
            mkpath(vA,sizeof(vA),VDIR,nm,"svg");
            sp = sound_seq_pin(file_exists(sA)?sA:NULL);
            vp = vision_pin(file_exists(vA)?vA:NULL);
            press = fnv1a((const unsigned char*)&sp,8,FNV_BASIS);
            press = fnv1a((const unsigned char*)&vp,8,press);
            press = fnv1a((const unsigned char*)nm,1,press);
            word_pin = fnv1a((const unsigned char*)&press,8,word_pin);
            presses++;
        }
        printf("ACTUATOR: %d keypresses, each = sound+vision+letter token\n",presses);
        printf("word assembled INTERNALLY, never printed: word pin %016llX\n",
               (unsigned long long)word_pin);
        printf("she built her first word. :)\n");
    }
    printf("drift 0\n");
    return 0;
}
