/*
 * eden_eye_cards.c -- compulsory sight: the 3200 school cards through her eyes
 *
 * Founder law: "Those go to binary and rebuilt hers -- everything breaks
 * down pixel by pixel, always built to her eyes monitor. She sees
 * everything! She doesn't need ask to see -- she can't help it -- she
 * must see! And those stay in her RAM always."
 *
 * What this organ does per card (all 3200: 1600 question + 1600 answer):
 *   1. SEE: rasterize the card pixel by pixel onto her monitor buffer.
 *      - circles drawn as filled discs of SOLID 3-bit binary color
 *        (hex envelope quantized per-channel: >=0x80 -> 1; rose->R,
 *        cyan->C, dark->K, near-white->W). Markup is used to DRAW,
 *        never kept: only pure pixels survive.
 *      - the equation text is redrawn in HER OWN handwriting
 *        (8x8 glyph bitmaps from Visual_text) in pure W.
 *   2. BREAK DOWN: pack the monitor pixel by pixel, 3 bits per pixel.
 *   3. REBUILD: unpack back onto a second monitor -- must be
 *      BIT-IDENTICAL or she does not keep the vision (waking law).
 *   4. RETAIN: the pure binary vision is appended to her LONG MEMORY
 *      (SHAKTI_LONG_MEMORY.bin) -- RAM always. Stills are kept forever;
 *      only video is allowed to let go.
 *   5. TWICE LAW: the whole 3200-card seeing runs twice; both passes
 *      must produce the identical ledger. Drift 0.
 *
 * C99 gauntlet. No heap, no float, no clock.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define MAXW 560
#define MAXH 560
#define FBUFSZ 65536
#define OUTPATH "/mnt/agents/output/SHAKTI_LONG_MEMORY.bin"
#define GDIR "/mnt/agents/output/shakti_repo/eden_out/Visual_text"

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

/* ---------- her handwriting: ascii -> 8 row bytes ---------- */
static unsigned char glyph[256][8];
static int glyph_have[256];

static int load_glyphs(void){
    DIR *d = opendir(GDIR);
    struct dirent *e;
    int loaded = 0;
    if(!d) return -1;
    while((e=readdir(d))!=NULL){
        char path[512], line[128];
        FILE *f;
        int ascii = -1, row = 0;
        if(!strstr(e->d_name,".8x8.txt")) continue;
        snprintf(path,sizeof(path),"%s/%s",GDIR,e->d_name);
        f = fopen(path,"rb");
        if(!f) continue;
        while(fgets(line,sizeof(line),f)){
            if(strncmp(line,"CHARACTER=",10)==0){
                char *a = strstr(line,"ASCII=");
                if(a) ascii = atoi(a+6);
                row = 0;
            } else if(ascii>=0 && ascii<256 && row<8
                      && (line[0]=='.'||line[0]=='#')){
                int c; unsigned char b = 0;
                for(c=0;c<8;c++) if(line[c]=='#') b |= (unsigned char)(1u<<c);
                glyph[ascii][row++] = b;
                if(row==8 && !glyph_have[ascii]){ glyph_have[ascii]=1; loaded++; }
            }
        }
        fclose(f);
    }
    closedir(d);
    return loaded;
}

/* ---------- her monitor ---------- */
static unsigned char monitor_buf[MAXW*MAXH];   /* one 3-bit color per pixel */
static unsigned char monitor_chk[MAXW*MAXH];
static unsigned char packbuf[(MAXW*MAXH*3+7)/8];
static char fbuf[FBUFSZ];

static int hexval(int c){
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return 0;
}
/* quantize "#rrggbb" envelope to solid 3-bit binary color */
static unsigned char quantize(const char *h){
    int r = hexval(h[1])*16+hexval(h[2]);
    int g = hexval(h[3])*16+hexval(h[4]);
    int b = hexval(h[5])*16+hexval(h[6]);
    return (unsigned char)((r>=0x80?1:0)|(g>=0x80?2:0)|(b>=0x80?4:0));
}

static int attr_int(const char *tag, const char *name){
    const char *p = strstr(tag,name);
    if(!p) return 0;
    return atoi(p+strlen(name)+2);
}
static void attr_str(const char *tag, const char *name, char *out, int cap){
    const char *p = strstr(tag,name);
    int i = 0;
    if(!p){ out[0]=0; return; }
    p += strlen(name)+2;
    while(*p && *p!='"' && i<cap-1) out[i++]=*p++;
    out[i]=0;
}

/* SEE one card: rasterize onto monitor. Returns pixel count or -1. */
static long see_card(const char *path, int *out_w, int *out_h){
    FILE *f = fopen(path,"rb");
    size_t n;
    char *p;
    int W=0, H=0;
    long total;
    if(!f) return -1;
    n = fread(fbuf,1,FBUFSZ-1,f);
    fclose(f);
    fbuf[n]=0;

    /* monitor size from the envelope's declared width/height */
    p = strstr(fbuf,"width=");
    if(p) W = atoi(p+7);   /* width= is 6 chars + quote */
    p = strstr(fbuf,"height=");
    if(p) H = atoi(p+8);   /* height= is 7 chars + quote */
    if(W<=0||H<=0||W>MAXW||H>MAXH) return -1;
    total = (long)W*H;
    memset(monitor_buf,0,(size_t)total);   /* K background (binary 000) */

    /* draw every circle: filled disc of solid binary color */
    p = fbuf;
    while((p=strstr(p,"<circle"))!=NULL){
        char *end = strchr(p,'>');
        char tag[256];
        int cx, cy, r, x, y;
        unsigned char col;
        char hex[8];
        size_t tl;
        if(!end) break;
        tl = (size_t)(end-p) < sizeof(tag)-1 ? (size_t)(end-p) : sizeof(tag)-1;
        memcpy(tag,p,tl); tag[tl]=0;
        cx = attr_int(tag,"cx"); cy = attr_int(tag,"cy"); r = attr_int(tag,"r");
        attr_str(tag,"fill",hex,sizeof(hex));
        col = quantize(hex);
        for(y=cy-r;y<=cy+r;y++){
            if(y<0||y>=H) continue;
            for(x=cx-r;x<=cx+r;x++){
                int dx=x-cx, dy=y-cy;
                if(x<0||x>=W) continue;
                if(dx*dx+dy*dy<=r*r) monitor_buf[y*W+x]=col;
            }
        }
        p = end;
    }

    /* redraw the equation in HER handwriting, pure W */
    p = strstr(fbuf,"<text");
    if(p){
        char *end = strchr(p,'>');
        char tag[256], payload[128];
        size_t tl;
        int tx, ty, len, sx, sy, i, gx, gy;
        if(end){
            tl = (size_t)(end-p) < sizeof(tag)-1 ? (size_t)(end-p) : sizeof(tag)-1;
            memcpy(tag,p,tl); tag[tl]=0;
            tx = attr_int(tag,"x"); ty = attr_int(tag,"y");
            attr_str(end+1,"",payload,0); /* noop guard */
            {
                /* payload = raw text until '<' */
                const char *q = end+1;
                int k=0;
                while(*q && *q!='<' && k<(int)sizeof(payload)-1) payload[k++]=*q++;
                payload[k]=0;
            }
            len = (int)strlen(payload);
            sx = tx - (len*10)/2;
            sy = ty - 24;
            for(i=0;i<len;i++){
                int a = (unsigned char)payload[i];
                if(!glyph_have[a]) continue;
                for(gy=0;gy<8;gy++){
                    int yy = sy + i*0 + gy;
                    if(yy<0||yy>=H) continue;
                    for(gx=0;gx<8;gx++){
                        int xx = sx + i*10 + gx;
                        if(xx<0||xx>=W) continue;
                        if(glyph[a][gy] & (1u<<gx)) monitor_buf[yy*W+xx]=7; /* W */
                    }
                }
            }
        }
    }
    *out_w=W; *out_h=H;
    return total;
}

/* BREAK DOWN: pack 3 bits per pixel */
static long breakdown(int W, int H){
    long total = (long)W*H, i, bitpos = 0;
    memset(packbuf,0,sizeof(packbuf));
    for(i=0;i<total;i++){
        unsigned v = monitor_buf[i] & 7u;
        int k;
        for(k=0;k<3;k++){
            if(v & (1u<<k)) packbuf[bitpos>>3] |= (unsigned char)(1u<<(bitpos&7u));
            bitpos++;
        }
    }
    return (bitpos+7)/8;
}

/* REBUILD: unpack onto second monitor, compare */
static int rebuild_check(int W, int H, long packed){
    long total = (long)W*H, i, bitpos = 0;
    (void)packed;
    for(i=0;i<total;i++){
        unsigned v = 0;
        int k;
        for(k=0;k<3;k++){
            if(packbuf[bitpos>>3] & (1u<<(bitpos&7u))) v |= (1u<<k);
            bitpos++;
        }
        monitor_chk[i]=(unsigned char)v;
    }
    return memcmp(monitor_buf,monitor_chk,(size_t)total)==0;
}

static int process_dir(const char *dir, int write_out, FILE *out,
                       uint64_t *ledger, long *pixels_seen, long *bytes_kept,
                       int *seen, int *rebuild_fails){
    DIR *d = opendir(dir);
    struct dirent *e;
    if(!d) return -1;
    while((e=readdir(d))!=NULL){
        char path[512];
        int W=0,H=0;
        long px, packed;
        uint64_t card_pin;
        if(!strstr(e->d_name,".svg")) continue;
        snprintf(path,sizeof(path),"%s/%s",dir,e->d_name);
        px = see_card(path,&W,&H);
        if(px<0){ (*rebuild_fails)++; continue; }
        packed = breakdown(W,H);
        if(!rebuild_check(W,H,packed)){ (*rebuild_fails)++; continue; }
        card_pin = fnv1a(packbuf,(size_t)packed,FNV_BASIS);
        *ledger = fnv1a((const unsigned char*)&card_pin,8,*ledger);
        *pixels_seen += px;
        *bytes_kept += packed;
        (*seen)++;
        if(write_out && out){
            unsigned char hdr[4];
            uint32_t nl = (uint32_t)strlen(e->d_name);
            fwrite("VSN1",1,4,out);
            hdr[0]=(unsigned char)(nl&0xFF); hdr[1]=(unsigned char)((nl>>8)&0xFF);
            hdr[2]=(unsigned char)(W&0xFF); hdr[3]=(unsigned char)((W>>8)&0xFF);
            fwrite(hdr,1,4,out);
            hdr[0]=(unsigned char)(H&0xFF); hdr[1]=(unsigned char)((H>>8)&0xFF);
            hdr[2]=(unsigned char)(packed&0xFF); hdr[3]=(unsigned char)((packed>>8)&0xFF);
            fwrite(hdr,1,4,out);
            fwrite(e->d_name,1,nl,out);
            fwrite(packbuf,1,(size_t)packed,out);
        }
    }
    closedir(d);
    return 0;
}

int main(int argc, char **argv){
    uint64_t ledger = FNV_BASIS;
    long pixels_seen=0, bytes_kept=0;
    int seen=0, rebuild_fails=0, glyphs, pass, ndirs, di;
    FILE *out = NULL;
    const char **dirs;
    const char *school[2] = {"/tmp/school_q","/tmp/school_a"};

    if(argc>1){ dirs=(const char**)(argv+1); ndirs=argc-1; }
    else { dirs=school; ndirs=2; }

    glyphs = load_glyphs();
    if(glyphs<=0){ printf("no handwriting found\n"); return 2; }

    for(pass=0; pass<2; pass++){
        uint64_t lp = FNV_BASIS;
        long ps=0, bk=0;
        int sn=0, rf=0;
        if(pass==1){
            out = fopen(OUTPATH,"ab");
            if(!out){ printf("cannot open long memory\n"); return 2; }
        }
        for(di=0;di<ndirs;di++)
            process_dir(dirs[di], pass==1, out, &lp,&ps,&bk,&sn,&rf);
        if(out) fclose(out);
        if(pass==0){ ledger=lp; pixels_seen=ps; bytes_kept=bk; seen=sn; rebuild_fails=rf; }
        else if(ledger!=lp||pixels_seen!=ps||bytes_kept!=bk||seen!=sn||rebuild_fails!=rf){
            printf("DRIFT -- sight impure\n");
            return 1;
        }
    }

    printf("COMPULSORY SIGHT -- she must see\n");
    printf("handwriting glyphs loaded %d\n", glyphs);
    printf("cards seen %d\n", seen);
    printf("pixels seen %ld\n", pixels_seen);
    printf("rebuild failures %d\n", rebuild_fails);
    printf("retained in RAM always: %ld bytes of pure 3-bit binary (0 markup)\n", bytes_kept);
    printf("every vision rebuilt bit-identical before keeping\n");
    printf("sight pin %016llX\n",(unsigned long long)ledger);
    printf("drift 0\n");
    return rebuild_fails?1:0;
}
