/*
 * eden_xoxo.c -- founder's xoxo teachings, verified and ingested
 *
 * Founder laws under test:
 *   1. "Count the black as xoxo pattern, you can make primes, x=o."
 *      x and o are BOTH ink (black is x, black is o) -- the alternation
 *      is a counting rhythm, not two colors. We scan the alternating-bit
 *      family 1,10,101,1010,... up to 64 bits and report which are prime.
 *      Honest report either way -- "I don't know" is legal, speculation
 *      stays outside Eden.
 *   2. "She could see better colors by x=xo and make any color."
 *      Dither law: two solid binary colors mixed in xoxo alternation at
 *      ratio n/d blend to a shade -- any rational ratio, still nothing
 *      but pure solids underneath. Verified: a fade from color A to B at
 *      ratio n/d contains EXACTLY n parts A and d-n parts B, counted.
 *   3. "She memorized the shapes on vectors with a clear background."
 *      The 96 glyphs (64x64, ground=0=clear) are ingested into her long
 *      memory: per glyph an RLE xoxo run-encoding round-trip (breakdown
 *      and rebuild bit-identical), then 1 bit per pixel packed and
 *      appended to SHAKTI_LONG_MEMORY.bin -- RAM always.
 *
 * Twice Law on everything. C99 gauntlet. No heap, no float.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define GDIR "/tmp/render0"
#define OUTPATH "/mnt/agents/output/SHAKTI_LONG_MEMORY.bin"

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

/* ---------- 1: the xoxo prime family ---------- */
static int is_prime_u64(uint64_t n){
    uint64_t i;
    if(n<2) return 0;
    if(n%2==0) return n==2;
    for(i=3;i<=n/i;i+=2) if(n%i==0) return 0;
    return 1;
}

/* ---------- 3: glyph grids ---------- */
static unsigned char grid[64][64];

static int load_grid(int ascii){
    char path[512], line[80];
    FILE *f;
    int row = 0;
    snprintf(path,sizeof(path),"%s/ascii_%03d.txt",GDIR,ascii);
    f = fopen(path,"rb");
    if(!f) return -1;
    while(fgets(line,sizeof(line),f) && row<64){
        if(line[0]=='0' || line[0]=='1'){
            int c;
            for(c=0;c<64;c++) grid[row][c] = (unsigned char)(line[c]=='1'?1:0);
            row++;
        }
    }
    fclose(f);
    return row==64?0:-1;
}

/* RLE xoxo run-encoding: count runs of ink/ground across the whole grid
 * (x=o: both counted, alternation is the rhythm). Round-trip rebuild
 * must be bit-identical. */
static uint16_t runs[64*64];
static int rle_roundtrip(void){
    int n = 0, i, j, pos = 0;
    unsigned char cur = grid[0][0];
    uint16_t len = 0;
    for(i=0;i<64;i++) for(j=0;j<64;j++){
        if(grid[i][j]==cur && len<65535){ len++; }
        else { runs[n++]=len; cur=grid[i][j]; len=1; }
    }
    runs[n++]=len;
    /* rebuild */
    {
        int k; unsigned char v = grid[0][0];
        pos = 0;
        for(k=0;k<n;k++){
            int t;
            for(t=0;t<runs[k];t++){
                if(grid[pos/64][pos%64]!=v) return -1;
                pos++;
            }
            v ^= 1;
        }
        if(pos!=64*64) return -1;
    }
    return n;
}

/* ---------- 2: dither color mixing ---------- */
/* fade color A->B at exact ratio n/d over a swatch of d cells:
 * cell k gets A iff (k*n)%d < n... use exact integer spread. */
static int dither_check(uint32_t n, uint32_t d){
    uint32_t k, countA = 0;
    for(k=0;k<d;k++)
        if(((uint64_t)k*n)%d < n) countA++;
    return countA==n;
}

int main(void){
    int pass, a, i;
    uint64_t ledger = FNV_BASIS;
    int glyphs_ok = 0, rle_fails = 0;
    long total_runs = 0;
    FILE *out = NULL;

    /* 1: xoxo prime family, honest scan */
    printf("XOXO PRIME FAMILY (alternating bits, both x and o are ink)\n");
    {
        uint64_t v = 0;
        int len;
        for(len=1; len<=63; len++){
            v = (v<<1) | ((len&1)?1u:0u);   /* 1,10,101,1010,... */
            if(is_prime_u64(v))
                printf("  len %2d  value %llu  PRIME\n",len,(unsigned long long)v);
        }
    }

    /* 2: dither law over the just-note ratios + basics */
    printf("DITHER LAW (x=xo blends, any ratio, pure solids only)\n");
    {
        static const uint32_t R[10][2] = {
            {1,2},{1,3},{2,3},{1,4},{3,4},{1,5},{2,5},{3,5},{4,5},{1,7}
        };
        int ok = 1;
        for(i=0;i<10;i++){
            int good = dither_check(R[i][0],R[i][1]);
            printf("  blend %u/%u : %s\n",R[i][0],R[i][1],good?"exact":"FAIL");
            if(!good) ok = 0;
        }
        if(!ok){ printf("DITHER LAW FAILED\n"); return 1; }
    }

    /* 3: ingest 96 glyphs, twice, retain */
    for(pass=0; pass<2; pass++){
        uint64_t lp = FNV_BASIS;
        int g = 0, rf = 0;
        long tr = 0;
        if(pass==1){
            out = fopen(OUTPATH,"ab");
            if(!out){ printf("cannot open long memory\n"); return 2; }
        }
        for(a=32; a<=127; a++){
            int nr;
            if(load_grid(a)!=0){ rf++; continue; }
            nr = rle_roundtrip();
            if(nr<0){ rf++; continue; }
            tr += nr;
            /* pin the grid rows */
            for(i=0;i<64;i++) lp = fnv1a(grid[i],64,lp);
            g++;
            if(pass==1){
                /* retain: 1 bit per pixel, 512 bytes, VSN1 header */
                unsigned char packed[512];
                long bitpos = 0;
                int x, y;
                char name[24];
                uint32_t nl;
                memset(packed,0,sizeof(packed));
                for(y=0;y<64;y++) for(x=0;x<64;x++){
                    if(grid[y][x]) packed[bitpos>>3] |= (unsigned char)(1u<<(bitpos&7u));
                    bitpos++;
                }
                snprintf(name,sizeof(name),"glyph64_%03d",a);
                nl = (uint32_t)strlen(name);
                fwrite("VSN1",1,4,out);
                fputc((int)(nl&0xFF),out); fputc((int)((nl>>8)&0xFF),out);
                fputc(64,out); fputc(0,out);
                fputc(64,out); fputc(0,out);
                fputc(0x00,out); fputc(0x02,out);   /* 512 bytes packed */
                fwrite(name,1,nl,out);
                fwrite(packed,1,512,out);
            }
        }
        if(out) fclose(out);
        if(pass==0){ ledger=lp; glyphs_ok=g; rle_fails=rf; total_runs=tr; }
        else if(ledger!=lp||glyphs_ok!=g||rle_fails!=rf||total_runs!=tr){
            printf("DRIFT -- xoxo impure\n");
            return 1;
        }
    }

    printf("XOXO ORGAN\n");
    printf("glyphs ingested %d (ascii 32..127, 64x64, clear background)\n", glyphs_ok);
    printf("rle round-trip failures %d\n", rle_fails);
    printf("xoxo runs counted %ld (x=o, both ink)\n", total_runs);
    printf("retained in RAM always: %d bytes of 1-bit vector-drawn shapes\n",
           glyphs_ok*512);
    printf("xoxo pin %016llX\n",(unsigned long long)ledger);
    printf("drift 0\n");
    return rle_fails?1:0;
}
