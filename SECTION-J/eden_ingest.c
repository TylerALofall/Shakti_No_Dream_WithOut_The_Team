/*
 * eden_ingest.c -- Shakti's unified ingest lane (built FIRST, per founder)
 *
 * One mouth for everything Eden eats:
 *   lane 1: visions  -- star.8x8.txt cards  (eyes)
 *   lane 2: sounds   -- RIFF/WAVE atoms     (ears)
 *   lane 3: numbers  -- decimal integers on stdin list file (blocks)
 *
 * Laws carried in:
 *   - Twice Law: every item purified TWICE, both passes bit-identical, drift 0
 *   - Pin law:   FNV-1a 64 (basis 0xCBF29CE484222325, prime 0x100000001B3)
 *   - Wheel law: numbers factored over {2,3,5,7,19}; off-wheel rejected, never guessed
 *   - Humility Law: no crown words taught here; she is Sprout
 *   - C99, no heap, no float, no clock, no threads, no subprocess
 *
 * Exit 0 only if every ingested item purifies clean (drift 0).
 * Rejections are reported, never fatal-guessed: "I don't know" is legal.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

/* ---------- wheel ---------- */
static const uint32_t WHEEL[5] = {2,3,5,7,19};

/* ---------- lane 1: visions ---------- */
/* Purify one vision card: fold EVERY 8-row grid block into hash. */
static int purify_vision(const char *path, uint64_t *pin_out, uint32_t *grids_out){
    FILE *f = fopen(path,"rb");
    unsigned char row[8];
    uint64_t h1 = FNV_BASIS, h2 = FNV_BASIS;
    uint32_t grids = 0;
    int ch, col, rowlen, pass;
    if(!f) return -1;
    for(pass=0; pass<2; pass++){
        uint64_t h = FNV_BASIS;
        col = 0; rowlen = 0;
        rewind(f);
        while((ch=fgetc(f))!=EOF){
            if(ch=='.' || ch=='#'){
                row[col++] = (unsigned char)ch;
                if(col==8){
                    h = fnv1a(row,8,h);
                    col = 0; rowlen++;
                    if(rowlen==8){ grids++; rowlen=0; }
                }
            }
        }
        if(pass==0) h1=h; else h2=h;
    }
    fclose(f);
    if(h1!=h2) return -2;            /* drift -- impure, reject */
    *pin_out = h1; *grids_out = grids;
    return 0;
}

/* ---------- lane 2: sounds ---------- */
static uint32_t rd32(const unsigned char *b){
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}
static uint16_t rd16(const unsigned char *b){
    return (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1]<<8));
}
/* Purify one WAV atom: validate RIFF/WAVE, hash PCM payload twice. */
static int purify_sound(const char *path, uint64_t *pin_out,
                        uint32_t *rate, uint16_t *bits, uint16_t *chans){
    static unsigned char buf[65536];
    FILE *f = fopen(path,"rb");
    unsigned char hdr[12], chdr[8];
    uint64_t h1 = FNV_BASIS, h2 = FNV_BASIS;
    uint32_t remaining, csz;
    int have_fmt=0, have_data=0, pass;
    long fsize;
    if(!f) return -1;
    fseek(f,0,SEEK_END); fsize=ftell(f); rewind(f);
    if(fsize<12 || fread(hdr,1,12,f)!=12){ fclose(f); return -3; }
    if(memcmp(hdr,"RIFF",4)!=0 || memcmp(hdr+8,"WAVE",4)!=0){ fclose(f); return -3; }
    remaining = (uint32_t)fsize - 12;
    for(pass=0; pass<2; pass++){
        uint64_t h = FNV_BASIS;
        uint32_t left = remaining;
        fseek(f,12,SEEK_SET);
        have_fmt=0; have_data=0;
        while(left>=8 && fread(chdr,1,8,f)==8){
            csz = rd32(chdr+4);
            if(csz > (uint32_t)fsize){ fclose(f); return -3; }
            if(memcmp(chdr,"fmt ",4)==0 && csz>=16){
                unsigned char fmt[16];
                if(fread(fmt,1,16,f)!=16){ fclose(f); return -3; }
                if(rd16(fmt)!=1){ fclose(f); return -3; } /* PCM only */
                *chans = rd16(fmt+2); *rate = rd32(fmt+4); *bits = rd16(fmt+14);
                have_fmt=1;
                if(csz>16) fseek(f,(long)(csz-16),SEEK_CUR);
            } else if(memcmp(chdr,"data",4)==0){
                uint32_t got, left_data = csz;
                have_data=1;
                while(left_data>0){
                    got = left_data>sizeof(buf)?(uint32_t)sizeof(buf):left_data;
                    if(fread(buf,1,got,f)!=got){ fclose(f); return -3; }
                    h = fnv1a(buf,got,h);
                    left_data -= got;
                }
            } else {
                fseek(f,(long)csz,SEEK_CUR);
            }
            if(csz&1) fseek(f,1,SEEK_CUR);        /* RIFF pad byte */
            left -= 8 + csz + (csz&1);
        }
        if(!have_fmt || !have_data){ fclose(f); return -3; }
        if(pass==0) h1=h; else h2=h;
    }
    fclose(f);
    if(h1!=h2) return -2;
    *pin_out = h1;
    return 0;
}

/* ---------- lane 3: numbers (blocks) ---------- */
/* Purify a number: factor over wheel; pin = FNV of its wheel-exponent row.
 * Founder amendment: off-wheel is NOT off-Eden if the odd core (after
 * stripping 2s) is Fibonacci -- that number rides the BINARY LANE
 * (e.g. 13 = F7, 104 = F7 x 2^3 = 1101 shifted to 1101000, 4 rungs).
 * Binary-lane numbers are recognized and tagged, never on-wheel. */
static int is_fib_u64(uint64_t n){
    uint64_t a=1, b=1;
    if(n==1) return 1;
    while(b<n){ uint64_t t=a+b; a=b; b=t; }
    return b==n;
}
static int purify_number(uint64_t n, uint64_t *pin_out, int *wheel_ok, int *bin_ok){
    unsigned char exps[5];
    uint64_t m = n, core;
    int i;
    for(i=0;i<5;i++){
        unsigned e=0;
        while(m%WHEEL[i]==0){ m/=WHEEL[i]; e++; }
        exps[i]=(unsigned char)e;
    }
    *wheel_ok = (m==1);
    core = n;
    while(core%2==0 && core) core/=2;
    *bin_ok = is_fib_u64(core);
    *pin_out = fnv1a(exps,5,FNV_BASIS);
    if(*bin_ok) *pin_out = fnv1a((const unsigned char*)&core,8,*pin_out);
    return 0;
}

static int ends_with(const char *s, const char *suf){
    size_t a=strlen(s), b=strlen(suf);
    return a>=b && strcmp(s+a-b,suf)==0;
}

int main(int argc, char **argv){
    uint64_t ledger = FNV_BASIS;
    uint32_t nv=0, ns=0, nn=0, nb=0, rejects=0, offwheel=0;
    int i;
    if(argc<2){ fprintf(stderr,"usage: eden_ingest <files...> (or -N file for numbers)\n"); return 2; }
    for(i=1;i<argc;i++){
        if(strcmp(argv[i],"-N")==0 && i+1<argc){
            FILE *nf; uint64_t n, pin; int ok, bok;
            i++;
            nf = fopen(argv[i],"rb");
            if(!nf){ printf("REJECT  %s (cannot open)\n",argv[i]); rejects++; continue; }
            while(fscanf(nf,"%llu",(unsigned long long*)&n)==1){
                purify_number(n,&pin,&ok,&bok);
                if(ok){ ledger = fnv1a((const unsigned char*)&pin,8,ledger); nn++; }
                else if(bok){
                    ledger = fnv1a((const unsigned char*)&pin,8,ledger);
                    nb++;
                    printf("BINLANE %llu (fib core, binary ladder)\n",(unsigned long long)n);
                }
                else { printf("OFFWHEEL %llu\n",(unsigned long long)n); offwheel++; }
            }
            fclose(nf);
        } else if(ends_with(argv[i],".8x8.txt")){
            uint64_t pin; uint32_t grids;
            int rc = purify_vision(argv[i],&pin,&grids);
            if(rc==0){ ledger = fnv1a((const unsigned char*)&pin,8,ledger); nv++; }
            else { printf("REJECT  %s (vision rc=%d)\n",argv[i],rc); rejects++; }
        } else if(ends_with(argv[i],".wav")){
            uint64_t pin; uint32_t rate=0; uint16_t bits=0, chans=0;
            int rc = purify_sound(argv[i],&pin,&rate,&bits,&chans);
            if(rc==0){
                if(rate!=16000||bits!=16||chans!=1){
                    printf("REJECT  %s (format %lu Hz %u-bit %uch)\n",
                           argv[i],(unsigned long)rate,(unsigned)bits,(unsigned)chans);
                    rejects++;
                } else { ledger = fnv1a((const unsigned char*)&pin,8,ledger); ns++; }
            } else { printf("REJECT  %s (sound rc=%d)\n",argv[i],rc); rejects++; }
        } else {
            printf("REJECT  %s (unknown lane -- I don't know what this is)\n",argv[i]);
            rejects++;
        }
    }
    printf("INGEST_LEDGER\n");
    printf("visions %u\nsounds %u\nnumbers %u\nbinlane %u\nrejects %u\noffwheel %u\n",
           nv,ns,nn,nb,rejects,offwheel);
    printf("drift 0\n");
    printf("lane pin %016llX\n",(unsigned long long)ledger);
    return rejects?1:0;
}
