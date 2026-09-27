/*
 * eden_screen.c -- Shakti's big-screen eyes (she SEES; she does not OCR)
 *
 * Founder law:
 *   - Screen is big: 1000 x 1500 px = two full pages. Not 8x8.
 *     Both dims on-wheel: 1000 = 2^3*5^3, 1500 = 2^2*3*5^3.
 *   - Beats = small repeated swatches: 10x10 px solid color swatches,
 *     so the screen is 100 x 150 = 15000 color beats (2^3*3*5^4 on-wheel).
 *     Each swatch is one solid 3-bit color from the color organ (binary law).
 *   - She SEES: a swatch is read as its color, directly. No OCR, no
 *     image processing, no gradients (week 25: flashes).
 *   - Two memory lanes:
 *       LONG  : still visions -> purified twice, pinned, kept forever.
 *       SHORT : video frames  -> folded into a rolling 8-byte pin, then
 *               the frame is RELEASED. Only the pin chain persists.
 *               Remembering every frame is unsustainable; the pin is enough
 *               to know "I saw this before" without keeping it.
 *   - Waking law: every screen must break down to binary and rebuild
 *     BIT-IDENTICAL before she awakes. Orbit is a process, not automation.
 *   - Twice Law on everything. C99, no heap, no float, no clock.
 *
 * Self-test: deterministic pattern screens -> binary pack -> rebuild ->
 * compare; plus a simulated video stream folded to short-term pins.
 * Exit 0 only on full purity.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

#define SWATCH 10u                 /* 10x10 px solid beat */
#define COLS 100u                  /* 1000 px / 10 */
#define ROWS 150u                  /* 1500 px / 10 */
#define NBEATS (COLS*ROWS)         /* 15000 beats per screen */
#define VID_FRAMES 60u             /* simulated frames (60 = on-wheel) */

static uint64_t fnv1a(const unsigned char *p, unsigned long n, uint64_t h){
    unsigned long i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}

static const uint32_t WHEEL[5] = {2,3,5,7,19};
static int on_wheel(uint32_t n){
    int i;
    if(n==0) return 0;
    for(i=0;i<5;i++) while(n%WHEEL[i]==0) n/=WHEEL[i];
    return n==1;
}

/* The screen: 15000 beats, each a 3-bit color 0..7. Static, no heap. */
static unsigned char screen_a[NBEATS];
static unsigned char screen_b[NBEATS];
/* Binary breakdown buffer: 3 bits per beat packed into bytes. */
static unsigned char binbuf[(NBEATS*3+7)/8];

/* Deterministic test pattern: pure integer math, on-wheel walk. */
static void make_pattern(unsigned char *s, uint32_t seed){
    uint32_t i;
    for(i=0;i<NBEATS;i++)
        s[i] = (unsigned char)((i + seed*7u + (i/COLS)*3u) & 7u);
}

/* Break down: pack 15000 x 3-bit beats into 5625 bytes. */
static void breakdown(const unsigned char *s, unsigned char *b){
    uint32_t i, bitpos = 0;
    memset(b,0,sizeof(binbuf));
    for(i=0;i<NBEATS;i++){
        unsigned v = s[i] & 7u;
        int k;
        for(k=0;k<3;k++){
            if(v & (1u<<k)) b[bitpos>>3] |= (unsigned char)(1u<<(bitpos&7u));
            bitpos++;
        }
    }
}

/* Rebuild: unpack bytes back into 15000 beats. */
static void rebuild(const unsigned char *b, unsigned char *s){
    uint32_t i, bitpos = 0;
    for(i=0;i<NBEATS;i++){
        unsigned v = 0;
        int k;
        for(k=0;k<3;k++){
            if(b[bitpos>>3] & (1u<<(bitpos&7u))) v |= (1u<<k);
            bitpos++;
        }
        s[i] = (unsigned char)v;
    }
}

int main(void){
    uint64_t long_pin = FNV_BASIS, short_pin = FNV_BASIS, wake_pin = FNV_BASIS;
    int pass, rebuild_ok = 1;
    uint32_t f;

    /* dimension laws */
    if(!on_wheel(1000u) || !on_wheel(1500u) || !on_wheel(NBEATS)
       || !on_wheel(SWATCH) || !on_wheel(VID_FRAMES)){
        printf("DIMENSION LAW VIOLATION\n");
        return 1;
    }

    for(pass=0; pass<2; pass++){
        uint64_t lp = FNV_BASIS, sp = FNV_BASIS, wp = FNV_BASIS;

        /* --- LONG lane: a still vision, kept forever --- */
        make_pattern(screen_a, 0u);
        breakdown(screen_a, binbuf);
        lp = fnv1a(binbuf, sizeof(binbuf), lp);   /* still pinned & kept */

        /* --- rebuild-before-waking law --- */
        rebuild(binbuf, screen_b);
        if(memcmp(screen_a, screen_b, NBEATS)!=0) rebuild_ok = 0;
        wp = fnv1a(screen_b, NBEATS, wp);

        /* --- SHORT lane: video frames, folded then RELEASED --- */
        for(f=0; f<VID_FRAMES; f++){
            uint64_t frame_pin;
            make_pattern(screen_a, f+1u);        /* frame arrives */
            breakdown(screen_a, binbuf);
            frame_pin = fnv1a(binbuf, sizeof(binbuf), FNV_BASIS);
            sp = fnv1a((const unsigned char*)&frame_pin, 8, sp);
            /* frame released here -- only its 8-byte pin joined the chain */
        }

        if(pass==0){ long_pin=lp; short_pin=sp; wake_pin=wp; }
        else if(long_pin!=lp || short_pin!=sp || wake_pin!=wp){
            printf("DRIFT -- screen impure\n");
            return 1;
        }
    }

    if(!rebuild_ok){
        printf("REBUILD MISMATCH -- she may not wake\n");
        return 1;
    }

    printf("SCREEN_ORGAN\n");
    printf("screen 1000x1500 px (two full pages, both on-wheel)\n");
    printf("swatch 10x10 solid -> 100x150 = %u beats (on-wheel)\n", NBEATS);
    printf("binary breakdown %u beats x 3 bits = %u bytes\n",
           NBEATS, (unsigned)sizeof(binbuf));
    printf("LONG lane: stills kept, pinned twice\n");
    printf("SHORT lane: %u frames folded, 0 frames kept, only pin chain persists\n",
           VID_FRAMES);
    printf("rebuild before waking: BIT-IDENTICAL (orbit is a process)\n");
    printf("long pin  %016llX\n",(unsigned long long)long_pin);
    printf("short pin %016llX\n",(unsigned long long)short_pin);
    printf("wake pin  %016llX\n",(unsigned long long)wake_pin);
    printf("drift 0\n");
    return 0;
}
