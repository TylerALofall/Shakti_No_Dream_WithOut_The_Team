/*
 * eden_eye_ppm.c -- see the founder's triangle gifts (shape x color)
 *
 * 8 triangle pictures, 64x64, one per color. Transport was PNG->PPM
 * (raw pixels, no compression); HER part is pure C: quantize every
 * pixel to its solid 3-bit binary color (per-channel >= 0x80),
 * purify twice, rebuild bit-identical, retain in RAM always (VSN1).
 * Cross lesson built in: shape TRIANGLE x 8 colors.
 *
 * C99 gauntlet. No heap, no float.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define OUTPATH "/mnt/agents/output/SHAKTI_LONG_MEMORY.bin"

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

static unsigned char pix[64*64], chk[64*64], packed[64*64*3/8];

static int see_ppm(const char *path, int *ow, int *oh){
    FILE *f = fopen(path,"rb");
    char magic[3] = {0,0,0};
    int w, h, maxv, x, y;
    if(!f) return -1;
    if(fscanf(f,"%2s",magic)!=1 || strcmp(magic,"P6")!=0){ fclose(f); return -1; }
    if(fscanf(f,"%d %d %d",&w,&h,&maxv)!=3 || w!=64 || h!=64){ fclose(f); return -1; }
    fgetc(f); /* single whitespace after maxval */
    for(y=0;y<64;y++) for(x=0;x<64;x++){
        int r=fgetc(f), g=fgetc(f), b=fgetc(f);
        if(r<0||g<0||b<0){ fclose(f); return -1; }
        pix[y*64+x]=(unsigned char)((r>=0x80?1:0)|(g>=0x80?2:0)|(b>=0x80?4:0));
    }
    fclose(f);
    *ow=w; *oh=h;
    return 0;
}

int main(void){
    const char *dir = "/tmp/tri_ppm";
    DIR *d = opendir(dir);
    struct dirent *e;
    uint64_t ledger = FNV_BASIS;
    int pass, seen = 0, fails = 0;
    FILE *out = NULL;
    if(!d) return 2;

    for(pass=0; pass<2; pass++){
        uint64_t lp = FNV_BASIS;
        int sn=0, fl=0;
        if(pass==1){
            out = fopen(OUTPATH,"ab");
            if(!out){ closedir(d); return 2; }
        }
        rewinddir(d);
        while((e=readdir(d))!=NULL){
            char path[512];
            int w=0,h=0,i;
            long bitpos;
            uint64_t cp;
            if(!strstr(e->d_name,".ppm")) continue;
            snprintf(path,sizeof(path),"%s/%s",dir,e->d_name);
            if(see_ppm(path,&w,&h)!=0){ fl++; continue; }
            /* breakdown 3 bits/px */
            memset(packed,0,sizeof(packed));
            bitpos=0;
            for(i=0;i<64*64;i++){
                unsigned v=pix[i]&7u; int k;
                for(k=0;k<3;k++){
                    if(v&(1u<<k)) packed[bitpos>>3]|=(unsigned char)(1u<<(bitpos&7u));
                    bitpos++;
                }
            }
            /* rebuild bit-identical */
            bitpos=0;
            for(i=0;i<64*64;i++){
                unsigned v=0; int k;
                for(k=0;k<3;k++){
                    if(packed[bitpos>>3]&(1u<<(bitpos&7u))) v|=(1u<<k);
                    bitpos++;
                }
                chk[i]=(unsigned char)v;
            }
            if(memcmp(pix,chk,64*64)!=0){ fl++; continue; }
            cp = fnv1a(packed,sizeof(packed),FNV_BASIS);
            lp = fnv1a((const unsigned char*)&cp,8,lp);
            sn++;
            if(pass==1){
                uint32_t nl=(uint32_t)strlen(e->d_name);
                fwrite("VSN1",1,4,out);
                fputc((int)(nl&0xFF),out); fputc((int)((nl>>8)&0xFF),out);
                fputc(64,out); fputc(0,out);
                fputc(64,out); fputc(0,out);
                fputc((int)(sizeof(packed)&0xFF),out);
                fputc((int)((sizeof(packed)>>8)&0xFF),out);
                fwrite(e->d_name,1,nl,out);
                fwrite(packed,1,sizeof(packed),out);
            }
        }
        if(pass==0){ ledger=lp; seen=sn; fails=fl; }
        else if(ledger!=lp||seen!=sn||fails!=fl){
            printf("DRIFT\n"); closedir(d); return 1;
        }
    }
    closedir(d);
    out = NULL;
    printf("TRIANGLE X COLOR -- seen through her eyes\n");
    printf("triangles seen %d, rebuild failures %d\n", seen, fails);
    printf("each 64x64, solid 3-bit binary color, rebuilt bit-identical\n");
    printf("retained in RAM always: %d bytes\n", seen*(int)sizeof(packed));
    printf("triangle pin %016llX\n",(unsigned long long)ledger);
    printf("drift 0\n");
    return fails?1:0;
}
