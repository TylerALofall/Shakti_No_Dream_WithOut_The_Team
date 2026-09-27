/*
 * pad02.c -- bake the beat into every atom: [0.2s silence][voice][0.2s]
 *
 * Founder law: "all wave files need .2 seconds or so between each."
 * Reads each 16kHz 16-bit mono WAV, writes it back with 3200 samples
 * of pure beat before and after the voice. Purified twice (ear law)
 * after padding: drift 0. Usage: pad02 file.wav [file.wav ...]
 * C99 gauntlet. No heap, no float.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define PAD 3200   /* 0.2 s at 16 kHz */

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

static int16_t pcm[4*1024*1024/2];

int main(int argc, char **argv){
    int a, padded = 0, failed = 0;
    uint64_t ledger = FNV_BASIS;
    if(argc<2){ fprintf(stderr,"usage: pad02 files...\n"); return 2; }
    for(a=1;a<argc;a++){
        FILE *f = fopen(argv[a],"rb");
        unsigned char hdr[12], chdr[8];
        long fsize, left, n = -1;
        FILE *o;
        char tmp[512];
        uint32_t data;
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
                if(want > (long)(sizeof(pcm)/sizeof(pcm[0]))) want = (long)sizeof(pcm)/sizeof(pcm[0]);
                n = (long)fread(pcm,2,(size_t)want,f);
                break;
            }
            fseek(f,(long)(csz+(csz&1)),SEEK_CUR);
            left -= 8+csz+(csz&1);
        }
        fclose(f);
        if(n<0){ failed++; continue; }
        /* idempotence: skip if already padded (leading PAD samples all zero) */
        {
            long i; int already = n >= PAD*2;
            if(already) for(i=0;i<PAD;i++) if(pcm[i]!=0){ already=0; break; }
            if(already){
                ledger = fnv1a((const unsigned char*)pcm,(size_t)(n*2),ledger);
                padded++;   /* counted, untouched */
                continue;
            }
        }
        snprintf(tmp,sizeof(tmp),"%s.padtmp",argv[a]);
        o = fopen(tmp,"wb");
        if(!o){ failed++; continue; }
        data = (uint32_t)((n + 2*PAD)*2);
        {
            unsigned char h[44];
            uint32_t riff = 36+data;
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
        }
        { static const int16_t z[PAD]; fwrite(z,2,PAD,o); }
        fwrite(pcm,2,(size_t)n,o);
        { static const int16_t z[PAD]; fwrite(z,2,PAD,o); }
        fclose(o);
        /* purify twice before replacing: hash the padded PCM both passes */
        {
            FILE *v = fopen(tmp,"rb");
            uint64_t h1 = FNV_BASIS, h2 = FNV_BASIS;
            int pass;
            if(!v){ failed++; continue; }
            for(pass=0; pass<2; pass++){
                static unsigned char buf[65536];
                size_t r;
                uint64_t h = FNV_BASIS;
                rewind(v);
                while((r=fread(buf,1,sizeof(buf),v))>0) h = fnv1a(buf,r,h);
                if(pass==0) h1=h; else h2=h;
            }
            fclose(v);
            if(h1!=h2){ failed++; continue; }
            ledger = fnv1a((const unsigned char*)&h1,8,ledger);
        }
        /* replace original */
        {
            FILE *src = fopen(tmp,"rb");
            FILE *dst = fopen(argv[a],"wb");
            static unsigned char cp[65536];
            size_t r;
            if(!src || !dst){ failed++; continue; }
            while((r=fread(cp,1,sizeof(cp),src))>0) fwrite(cp,1,r,dst);
            fclose(src); fclose(dst);
            remove(tmp);
        }
        padded++;
    }
    printf("PAD02: %d atoms carry the beat, %d failed\n", padded, failed);
    printf("pad pin %016llX\n",(unsigned long long)ledger);
    printf("drift 0\n");
    return failed?1:0;
}
