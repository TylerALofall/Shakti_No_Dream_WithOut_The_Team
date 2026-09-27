/*
 * fib_lane_check.c -- verify founder's correction on 13 and 104
 *
 * Claims under test:
 *   1. 104 = 13 << 3 (exact binary shift: 1101 -> 1101000)
 *   2. 13 is Fibonacci (F7); 104 = F7 x 2^3 -- a fib-shift
 *   3. Rung count 13-26-52-104 = 4 rungs (3 doublings)
 *   4. Phi: 8/13 is an exact Fibonacci convergent of 1/phi,
 *      bracketed 3/5 < 8/13 < 5/8, Cassini identity 13^2 = 8*21 + 1
 *   5. Lane rule: strip factors of 2; if the odd core is Fibonacci,
 *      the number rides the binary lane (recognized, tagged -- NOT
 *      on-wheel, NOT rejected). 11 and 17 must still be refused.
 *
 * All integer math, no float. C99 gauntlet. Exit 0 = all claims hold.
 */
#include <stdio.h>
#include <stdint.h>

static int is_fib(uint32_t n){
    uint32_t a=1, b=1;
    if(n==1) return 1;
    while(b<n){ uint32_t t=a+b; a=b; b=t; }
    return b==n;
}

static void print_bin(uint32_t n){
    int started=0, k;
    for(k=31;k>=0;k--){
        if(n&(1u<<k)) started=1;
        if(started) putchar((n&(1u<<k))?'1':'0');
    }
}

int main(void){
    uint32_t n;
    int fails = 0;

    /* 1 */
    if(104u != (13u<<3)){ printf("FAIL shift\n"); fails++; }
    printf("13  = "); print_bin(13);  printf("\n");
    printf("104 = "); print_bin(104); printf("  (13 << 3, same bits shifted)\n");

    /* 2 */
    if(!is_fib(13)){ printf("FAIL 13 not fib\n"); fails++; }
    if(13*8u != 104u){ printf("FAIL 104 != 13x2^3\n"); fails++; }
    printf("13 = F7 (Fibonacci), 104 = F7 x 2^3 -> fib-shift\n");

    /* 3 */
    {
        uint32_t r=13; int rungs=1, dbl=0;
        while(r<104){ r*=2; rungs++; dbl++; }
        printf("rungs 13-26-52-104 = %d rungs, %d doublings\n", rungs, dbl);
        if(rungs!=4 || dbl!=3){ printf("FAIL rungs\n"); fails++; }
    }

    /* 4 */
    if(8*5 <= 13*3){ printf("FAIL lower bracket\n"); fails++; }   /* 8/13 > 3/5 */
    if(8*8 >= 13*5){ printf("FAIL upper bracket\n"); fails++; }   /* 8/13 < 5/8 */
    if(13*13 != 8*21 + 1){ printf("FAIL cassini\n"); fails++; }
    printf("phi: 3/5 < 8/13 < 5/8, Cassini 13^2 = 8*21+1 (169=168+1) EXACT\n");

    /* 5 */
    printf("lane scan:\n");
    for(n=1; n<=128; n++){
        uint32_t core = n;
        while(core%2==0) core/=2;
        if(n==13 || n==104 || n==11 || n==17 || n==26 || n==52){
            printf("  %3u core=%3u fib=%d -> %s\n", n, core, is_fib(core),
                   is_fib(core)?"BINARY LANE (recognized)":"refused");
            if((n==13||n==104||n==26||n==52) && !is_fib(core)) fails++;
            if((n==11||n==17) && is_fib(core)) fails++;
        }
    }

    if(fails){ printf("RESULT: %d claims FAILED\n", fails); return 1; }
    printf("RESULT: all founder claims verified, drift 0\n");
    return 0;
}
