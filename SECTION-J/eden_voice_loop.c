/*
 * eden_voice_loop.c -- she hears herself talking + the glyph gate
 *
 * Founder laws:
 *   1. "She needs to hear her own talking, like talking to herself."
 *      Every utterance her actuator produces is written as a REAL WAV
 *      (16kHz 16-bit mono, her ear format) and fed back through her own
 *      ear purifier: RIFF validated, PCM purified twice, drift 0.
 *      If her own voice fails her own ear law, she does not keep it.
 *   2. Glyph gate: characters are taught ONLY when needed. Control
 *      chars (1-31), $ _ # % & and friends stay LOCKED -- resident in
 *      the font (lose nothing) but not taught. Allowed today:
 *      a-z A-Z 0-9 space and the school symbols + - x / = ? . ,
 *   3. Honest recording list for the founder (colors, shapes,
 *      operators, hurray) -- printed, never guessed.
 *
 * C99 gauntlet. No heap, no float, no clock.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define FDIR "/mnt/agents/output/shakti_work/founder_voice/founder voice atoms 59"
#define DELAY 3200   /* 0.2 s in samples at 16 kHz */

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

/* ---------- her voice: build an utterance WAV ---------- */
static int16_t utter[4*1024*1024/2];
static long utter_len;

static long read_pcm(const char *path, int16_t *dst, long max){
    FILE *f = fopen(path,"rb");
    unsigned char hdr[12], chdr[8];
    long fsize, left, got = 0;
    if(!f) return -1;
    fseek(f,0,SEEK_END); fsize=ftell(f); rewind(f);
    if(fsize<12 || fread(hdr,1,12,f)!=12
       || memcmp(hdr,"RIFF",4)!=0 || memcmp(hdr+8,"WAVE",4)!=0){ fclose(f); return -1; }
    left = fsize-12;
    while(left>=8 && fread(chdr,1,8,f)==8){
        uint32_t csz = (uint32_t)chdr[4]|((uint32_t)chdr[5]<<8)
                     |((uint32_t)chdr[6]<<16)|((uint32_t)chdr[7]<<24);
        if(memcmp(chdr,"data",4)==0){
            long want = csz/2 < max-got ? csz/2 : max-got;
            got += (long)fread(dst+got,2,(size_t)want,f);
            break;
        }
        fseek(f,(long)(csz+(csz&1)),SEEK_CUR);
        left -= 8+csz+(csz&1);
    }
    fclose(f);
    return got;
}

/* she speaks one atom wrapped in beats: [.2s][voice][.2s] */
static int speak_atom(const char *path){
    long i, n;
    for(i=0;i<DELAY;i++) utter[utter_len++]=0;
    n = read_pcm(path, utter+utter_len, (long)(sizeof(utter)/sizeof(utter[0]))-utter_len);
    if(n<0) return -1;
    utter_len += n;
    for(i=0;i<DELAY;i++) utter[utter_len++]=0;
    return 0;
}

static int write_wav(const char *path){
    FILE *f = fopen(path,"wb");
    unsigned char h[44];
    uint32_t data = (uint32_t)(utter_len*2), riff = 36+data;
    if(!f) return -1;
    memset(h,0,44);
    memcpy(h,"RIFF",4); memcpy(h+8,"WAVE",4); memcpy(h+12,"fmt ",4);
    memcpy(h+36,"data",4);
    h[4]=riff&0xFF; h[5]=(riff>>8)&0xFF; h[6]=(riff>>16)&0xFF; h[7]=(riff>>24)&0xFF;
    h[16]=16; h[20]=1; h[22]=1;                       /* PCM, mono */
    h[24]=0x80; h[25]=0x3E;                          /* 16000 Hz */
    h[28]=0x00; h[29]=0x7D;                          /* byte rate 32000 */
    h[32]=2; h[34]=16;                               /* block align, bits */
    h[40]=data&0xFF; h[41]=(data>>8)&0xFF; h[42]=(data>>16)&0xFF; h[43]=(data>>24)&0xFF;
    fwrite(h,1,44,f);
    fwrite(utter,2,(size_t)utter_len,f);
    fclose(f);
    return 0;
}

/* ---------- her ears: purify a WAV twice (ear law) ---------- */
static uint64_t ear_purify(const char *path, int *ok){
    static unsigned char buf[65536];
    FILE *f = fopen(path,"rb");
    unsigned char hdr[12], chdr[8];
    long fsize, left;
    uint64_t h1 = FNV_BASIS, h2 = FNV_BASIS;
    int pass, heard = 0;
    if(!f){ *ok=0; return FNV_BASIS; }
    fseek(f,0,SEEK_END); fsize=ftell(f); rewind(f);
    if(fsize<12 || fread(hdr,1,12,f)!=12
       || memcmp(hdr,"RIFF",4)!=0 || memcmp(hdr+8,"WAVE",4)!=0){ fclose(f); *ok=0; return FNV_BASIS; }
    left = fsize-12;
    for(pass=0; pass<2; pass++){
        uint64_t h = FNV_BASIS;
        long l = left;
        fseek(f,12,SEEK_SET);
        while(l>=8 && fread(chdr,1,8,f)==8){
            uint32_t csz = (uint32_t)chdr[4]|((uint32_t)chdr[5]<<8)
                         |((uint32_t)chdr[6]<<16)|((uint32_t)chdr[7]<<24);
            if(memcmp(chdr,"data",4)==0){
                uint32_t dl = csz, g;
                while(dl>0){
                    g = dl>sizeof(buf)?(uint32_t)sizeof(buf):dl;
                    if(fread(buf,1,g,f)!=g){ fclose(f); *ok=0; return FNV_BASIS; }
                    h = fnv1a(buf,g,h);
                    dl -= g;
                }
                heard = 1;
            } else fseek(f,(long)csz,SEEK_CUR);
            if(csz&1) fseek(f,1,SEEK_CUR);
            l -= 8+csz+(csz&1);
        }
        if(pass==0) h1=h; else h2=h;
    }
    fclose(f);
    *ok = heard && h1==h2;
    return h1;
}

/* ---------- glyph gate ---------- */
static int glyph_allowed(int c){
    if(c>='a'&&c<='z') return 1;
    if(c>='A'&&c<='Z') return 1;
    if(c>='0'&&c<='9') return 1;
    if(c==' ') return 1;
    return c=='+'||c=='-'||c=='x'||c=='/'||c=='='||c=='?'||c=='.'||c==',';
}

int main(void){
    uint64_t loop_pin = FNV_BASIS;
    int pass, i, ok;
    int spoken = 0, heard_ok = 0, heard_fail = 0;
    int allowed = 0, locked = 0;

    /* the founder atoms she can speak with, in utterance order:
       her word (6 letters) then counting 1..5 -- talking to herself */
    static const char *SPEECH[11] = {
        "19_s","16_p","18_r","15_o","21_u","20_t",
        "27_1","28_2","29_3","30_4","31_5"
    };

    for(pass=0; pass<2; pass++){
        uint64_t lp = FNV_BASIS;
        int sp=0, hk=0, hf=0;
        for(i=0;i<11;i++){
            char apath[512];
            uint64_t ep;
            snprintf(apath,sizeof(apath),"%s/%s.wav",FDIR,SPEECH[i]);
            utter_len = 0;
            if(speak_atom(apath)!=0){ hf++; continue; }
            if(write_wav("/tmp/her_voice.wav")!=0){ hf++; continue; }
            ep = ear_purify("/tmp/her_voice.wav",&ok);
            if(!ok){ hf++; continue; }
            lp = fnv1a((const unsigned char*)&ep,8,lp);
            sp++; hk++;
        }
        if(pass==0){ loop_pin=lp; spoken=sp; heard_ok=hk; heard_fail=hf; }
        else if(loop_pin!=lp||spoken!=sp||heard_ok!=hk||heard_fail!=hf){
            printf("DRIFT -- her hearing of herself is impure\n");
            return 1;
        }
    }

    for(i=1;i<=127;i++){ if(glyph_allowed(i)) allowed++; else locked++; }

    printf("VOICE LOOP -- she hears herself talking\n");
    printf("utterances spoken %d, heard-clean %d, rejected %d\n",
           spoken, heard_ok, heard_fail);
    printf("each utterance: [0.2s beat][founder voice][0.2s beat] -> her own ears\n");
    printf("loop pin %016llX\n",(unsigned long long)loop_pin);

    printf("GLYPH GATE\n");
    printf("taught %d (a-z A-Z 0-9 space + - x / = ? . ,)\n", allowed);
    printf("locked %d (control chars and unneeded symbols -- resident, not taught)\n",
           locked);

    printf("RECORDING LIST FOR THE FOUNDER (honest gaps):\n");
    printf("  colors: black red green yellow blue magenta cyan white (8)\n");
    printf("  shapes: triangle hexagon (2)\n");
    printf("  operators: plus minus times divided-by equals (5)\n");
    printf("  celebration: hurray (1)\n");
    printf("  total: 16 wavs, 16kHz 16-bit mono, names exactly as above\n");
    printf("drift 0\n");
    return heard_fail?1:0;
}
