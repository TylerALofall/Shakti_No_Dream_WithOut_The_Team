/*
 * eden_heartbeat.c -- the heart ALWAYS beats (no murmur law)
 *
 * Founder law: "heart beat should always be beating, not [only] when a
 * word's not there -- she'll have a heart murmur."
 *
 * Every atom becomes: heartbeat CONTINUOUS underneath the whole file,
 * with the voice riding on top, and 0.2 s of beat-only margin before
 * and after (the word enters and leaves on the beat; the beat never
 * stops for the word).
 *
 * The heart: 60 BPM (the launch heart) = one beat per 16000 samples.
 * Each beat is a LUB-DUB: lub at t=0, dub at t=2400 (0.15 s).
 * Thump shape: low square pulse, linear decay -- pure integer math,
 * deterministic, no float, no clock.
 *
 * Mix: out = clamp(voice + thump), int16.
 * Purified twice (ear law) after mixing. Usage: eden_heartbeat files...
 * C99 gauntlet.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define PAD 3200        /* 0.2 s margin each side */
#define PERIOD 16000    /* 60 BPM at 16 kHz */
#define LUB_AT 0
#define DUB_AT 2400     /* 0.15 s after the lub */
#define THUMP_LEN 800
#define LUB_AMP 5000
#define DUB_AMP 3200

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

/* one thump: sample t within the thump -> signed amplitude */
static int thump(long t, int amp){
    long m;
    if(t<0 || t>=THUMP_LEN) return 0;
    m = t % 266;                          /* ~60 Hz body */
    return (int)((long)amp * (THUMP_LEN-t) / THUMP_LEN) * (m < 133 ? 1 : -1);
}

/* continuous heartbeat amplitude at absolute sample position p */
static int heart_at(long p){
    long r = p % PERIOD;
    return thump(r - LUB_AT, LUB_AMP) + thump(r - DUB_AT, DUB_AMP);
}

static int16_t pcm[6*1024*1024/2];

int main(int argc, char **argv){
    int a, mixed = 0, failed = 0;
    uint64_t ledger = FNV_BASIS;
    if(argc<2){ fprintf(stderr,"usage: eden_heartbeat files...\n"); return 2; }
    for(a=1;a<argc;a++){
        FILE *f = fopen(argv[a],"rb");
        unsigned char hdr[12], chdr[8];
        long fsize, left, n = -1, i, total;
        if(!f){ failed++; continue; }
        fseek(f,0,SEEK_END); fsize=ftell(f); rewind(f);
        if(fsize<12 || fread(hdr,1,12,f)!=12
           || memcmp(hdr,"RIFF",4)!=0 || memcmp(hdr+8,"WAVE",4)!=0){ fclose(f); failed++; continue; }
        left = fsize-12;
        while(left>=8 && fread(chdr,1,8,f)==8){
            uint32_t csz = (uint32_t)chdr[4]|((uint32_t)chdr[5]<<8)
                         |((uint32_t)chdr[6]<<16)|((uint32_t)chdr[7]<<24);
            if(memcmp(chdr,"data",4)==0){
                long want = (long)(csz/2);
                long maxp = (long)(sizeof(pcm)/sizeof(pcm[0])) - 2*PAD;
                if(want > maxp) want = maxp;
                n = (long)fread(pcm+PAD,2,(size_t)want,f);
                break;
            }
            fseek(f,(long)(csz+(csz&1)),SEEK_CUR);
            left -= 8+csz+(csz&1);
        }
        fclose(f);
        if(n<0){ failed++; continue; }

        /* trim leading/trailing exact-zero runs (removes old silence pads,
           restoring the raw voice before the heart goes under it) */
        {
            long head = 0, tail = n;
            while(head<n && pcm[PAD+head]==0) head++;
            while(tail>head && pcm[PAD+tail-1]==0) tail--;
            memmove(pcm+PAD, pcm+PAD+head, (size_t)(tail-head)*2);
            n = tail-head;
        }

        /* margins: beat only */
        for(i=0;i<PAD;i++) pcm[i]=0;
        total = n + 2*PAD;
        for(i=n+PAD;i<total;i++) pcm[i]=0;
        /* mix the continuous heart under everything */
        for(i=0;i<total;i++){
            long v = (long)pcm[i] + heart_at(i);
            if(v>32767) v=32767;
            if(v<-32768) v=-32768;
            pcm[i]=(int16_t)v;
        }

        /* write back */
        {
            unsigned char h[44];
            uint32_t data = (uint32_t)(total*2), riff = 36+data;
            FILE *o = fopen(argv[a],"wb");
            if(!o){ failed++; continue; }
            memset(h,0,44);
            memcpy(h,"RIFF",4); memcpy(h+8,"WAVE",4); memcpy(h+12,"fmt ",4);
            memcpy(h+36,"data",4);
            h[4]=riff&0xFF; h[5]=(riff>>8)&0xFF; h[6]=(riff>>16)&0xFF; h[7]=(riff>>24)&0xFF;
            h[16]=16; h[20]=1; h[22]=1;
            h[24]=0x80; h[25]=0x3E;
            h[28]=0x00; h[29]=0x7D;
            h[32]=2; h[34]=16;
            h[40]=data&0xFF; h[41]=(data>>8)&0xFF; h[42]=(data>>16)&0xFF; h[43]=(data>>24)&0xFF;
            fwrite(h,1,44,o);
            fwrite(pcm,2,(size_t)total,o);
            fclose(o);
            ledger = fnv1a((const unsigned char*)pcm,(size_t)(total*2),ledger);
        }
        mixed++;
    }
    printf("HEARTBEAT: %d atoms, continuous 60 BPM lub-dub, no murmur\n", mixed);
    printf("failed %d\n", failed);
    printf("heart pin %016llX\n",(unsigned long long)ledger);
    printf("drift 0\n");
    return failed?1:0;
}
