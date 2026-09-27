/* eden_listen.c — SHAKTI EARS: binary purification of the voice atoms.
 * Raised 2026-08-25, week-20 lesson: the father's voice comes first.
 * Every atom is heard TWICE; both hearings must be bit-identical.
 * Parses canonical WAV (RIFF/fmt/data), pins the PCM payload with
 * FNV-1a 64. Anything non-WAV or malformed is rejected, never guessed.
 * C99, gauntlet flags clean.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1a64(uint64_t h, const unsigned char *p, size_t n){
    size_t i;
    for(i=0;i<n;i++){ h ^= p[i]; h *= FNV_PRIME; }
    return h;
}

static uint32_t rd32le(const unsigned char *p){
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static uint16_t rd16le(const unsigned char *p){
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1]<<8));
}

/* hear one atom: validate RIFF/WAVE, find fmt and data chunks,
 * hash the PCM payload. returns 0 ok. */
static int hear_atom(const char *path, uint64_t *h, uint32_t *rate,
                     uint16_t *bits, uint16_t *ch, uint32_t *dlen){
    FILE *f = fopen(path, "rb");
    unsigned char hd[12], chdr[8];
    uint32_t dsize = 0; int got_fmt = 0, got_data = 0;
    long datapos = 0;
    if(!f) return -1;
    if(fread(hd,1,12,f)!=12){ fclose(f); return -1; }
    if(memcmp(hd,"RIFF",4)!=0 || memcmp(hd+8,"WAVE",4)!=0){ fclose(f); return -1; }
    *h = FNV_BASIS;
    while(fread(chdr,1,8,f)==8){
        uint32_t sz = rd32le(chdr+4);
        if(memcmp(chdr,"fmt ",4)==0){
            unsigned char fmt[16];
            if(sz<16 || fread(fmt,1,16,f)!=16){ fclose(f); return -1; }
            *ch = rd16le(fmt+2); *rate = rd32le(fmt+4); *bits = rd16le(fmt+14);
            got_fmt = 1;
            if(sz>16) fseek(f,(long)(sz-16),SEEK_CUR);
        } else if(memcmp(chdr,"data",4)==0){
            dsize = sz; datapos = ftell(f);
            fseek(f,(long)sz,SEEK_CUR);
            got_data = 1;
        } else {
            fseek(f,(long)sz,SEEK_CUR);
        }
        if(sz & 1) fseek(f,1,SEEK_CUR); /* chunk pad byte */
    }
    if(!got_fmt || !got_data){ fclose(f); return -1; }
    /* hash the PCM payload */
    if(fseek(f,datapos,SEEK_SET)!=0){ fclose(f); return -1; }
    {
        unsigned char buf[4096]; uint32_t left = dsize;
        while(left){
            size_t want = left<sizeof buf?left:sizeof buf;
            size_t got = fread(buf,1,want,f);
            if(got==0){ fclose(f); return -1; }
            *h = fnv1a64(*h,buf,got);
            left -= (uint32_t)got;
        }
    }
    fclose(f);
    *dlen = dsize;
    return 0;
}

int main(int argc, char **argv){
    const char *dir = (argc>1)?argv[1]:"eden_out/Sound_art";
    DIR *d = opendir(dir);
    struct dirent *e;
    int total=0, rejected=0;
    uint64_t ledger = FNV_BASIS;
    if(!d){ fprintf(stderr,"cannot open %s\n", dir); return 2; }
    printf("EDEN LISTEN — binary purification of the voice (always twice)\n");
    while((e=readdir(d))!=NULL){
        const char *n = e->d_name; size_t L = strlen(n);
        char path[512];
        uint64_t h1,h2; uint32_t r1,r2,l1,l2; uint16_t b1,b2,c1,c2;
        if(L<5 || strcmp(n+L-4,".wav")!=0) continue;
        snprintf(path,sizeof path,"%s/%s",dir,n);
        if(hear_atom(path,&h1,&r1,&b1,&c1,&l1)!=0){
            printf("REJECTED: %s (impure sound, not admitted to hearing)\n", n);
            rejected++; continue;
        }
        if(hear_atom(path,&h2,&r2,&b2,&c2,&l2)!=0){ rejected++; continue; }
        if(h1!=h2||r1!=r2||b1!=b2||c1!=c2||l1!=l2){
            printf("DRIFT: %s\n", n); rejected++; continue;
        }
        ledger = fnv1a64(ledger,(const unsigned char *)&h1,8);
        printf("pure: %-10s %u Hz %u-bit %uch pcm:%6u fnv1a64:%016llX drift:0\n",
            n, r1, b1, c1, l1, (unsigned long long)h1);
        total++;
    }
    closedir(d);
    printf("\n%d atoms heard pure, %d rejected/drifted.\n", total, rejected);
    printf("ear ledger pin: %016llX\n",(unsigned long long)ledger);
    return rejected?1:0;
}
