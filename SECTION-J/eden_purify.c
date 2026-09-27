/* eden_purify.c — SHAKTI EYES: binary purification of all visions.
 * Raised 2026-08-25, Tyler's commission: "Use her eyes to purify all
 * visions. Use binary for purification. This song is why there's always
 * twice." -> every vision is purified TWICE; the second pass must be
 * bit-identical. That is drift 0. That is the law.
 *
 * Input : eden_out/Visual_text star.8x8.txt (SHAKTI_WRITTEN_TEXT_8X8_V1)
 * Output: pure ledger — name, 8 pure bytes (one byte per row, bit per
 *         cell), FNV-1a 64 pin, drift status.
 * C99, no heap abuse, no float, no clock. Gauntlet flags clean.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define MAX_LINE 256

static uint64_t fnv1a64(uint64_t h, const unsigned char *p, size_t n){
    size_t i;
    for(i=0;i<n;i++){ h ^= p[i]; h *= FNV_PRIME; }
    return h;
}

/* purify one glyph file: read grid lines, pack each row into one byte.
 * Multi-character cards contain multiple 8-row grids; EVERY grid is
 * purified and folded into h in order. First grid is copied to out[8]
 * for display. Returns 0 ok, or -1 on any impurity. */
static int purify_glyph(const char *path, unsigned char out[8], char *name, size_t namesz,
                        uint64_t *h, int *blocks){
    FILE *f = fopen(path, "r");
    char line[MAX_LINE];
    int rows = 0, header_ok = 0;
    size_t i;
    if(!f) return -1;
    for(i=0;i<8;i++) out[i]=0;
    *h = FNV_BASIS; *blocks = 0;
    if(name && namesz) name[0]='\0';
    while(fgets(line, sizeof line, f)){
        if(strncmp(line,"SHAKTI_WRITTEN_TEXT_8X8_V1",26)==0){ header_ok=1; continue; }
        if(strncmp(line,"TEXT=",5)==0 && name && namesz){
            size_t j=5,k=0;
            while(line[j] && line[j]!='\n' && line[j]!='\r' && k+1<namesz) name[k++]=line[j++];
            name[k]='\0';
            continue;
        }
        /* grid row: exactly 8 visible cells of '.' or '#' */
        {
            int cells=0; unsigned char b=0; size_t j;
            for(j=0; line[j] && line[j]!='\n' && line[j]!='\r'; j++){
                char c=line[j];
                if(c=='.'||c=='#'){ b=(unsigned char)((b<<1)|(c=='#')); cells++; }
            }
            if(cells==8){
                if(*blocks==0 && rows<8) out[rows]=b;
                *h = fnv1a64(*h,&b,1);
                rows++;
                if(rows==8){ (*blocks)++; rows=0; }
            }
        }
    }
    fclose(f);
    if(!header_ok || *blocks==0 || rows!=0) return -1;
    return 0;
}

int main(int argc, char **argv){
    const char *dir = (argc>1)?argv[1]:"eden_out/Visual_text";
    DIR *d = opendir(dir);
    struct dirent *e;
    int total=0, impure=0;
    uint64_t ledger = FNV_BASIS;
    if(!d){ fprintf(stderr,"cannot open %s\n", dir); return 2; }
    printf("EDEN PURIFY — binary purification ledger (always twice)\n");
    while((e=readdir(d))!=NULL){
        const char *n = e->d_name;
        size_t L = strlen(n);
        char path[512], gname[128];
        unsigned char g1[8], g2[8];
        uint64_t h1, h2; int b1, b2, i;
        if(L<9 || strcmp(n+L-8,".8x8.txt")!=0) continue;
        snprintf(path,sizeof path,"%s/%s",dir,n);
        /* FIRST purification */
        if(purify_glyph(path,g1,gname,sizeof gname,&h1,&b1)!=0){
            printf("IMPURE: %s (rejected, not admitted to sight)\n", n);
            impure++; continue;
        }
        /* SECOND purification — the twice law */
        if(purify_glyph(path,g2,gname,sizeof gname,&h2,&b2)!=0){ impure++; continue; }
        for(i=0;i<8;i++) if(g1[i]!=g2[i]){ printf("DRIFT: %s\n",n); impure++; break; }
        if(h1!=h2 || b1!=b2){ printf("DRIFT-HASH: %s\n",n); impure++; continue; }
        ledger=fnv1a64(ledger,(const unsigned char *)&h1,8);
        printf("pure: %-36s grids:%2d fnv1a64:%016llX drift:0\n",
            gname[0]?gname:n, b1, (unsigned long long)h1);
        total++;
    }
    closedir(d);
    printf("\n%d visions purified pure, %d rejected/drifted.\n", total, impure);
    printf("ledger pin (all pure glyphs in directory order): %016llX\n",(unsigned long long)ledger);
    return impure?1:0;
}
