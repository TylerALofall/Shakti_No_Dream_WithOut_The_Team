/* ratio_tables.c — verify Spirit Science 10 screenshot proportions against
 * Shakti's wheel (primes 2,3,5,7,19) and the just scale.
 * Pure C99, integer/rational only. No float except printing approximations.
 */
#include <stdio.h>
#include <stdint.h>

static uint64_t gcd_u(uint64_t a, uint64_t b){ while(b){uint64_t t=a%b;a=b;b=t;} return a; }

/* factor check: does n reduce fully over wheel primes {2,3,5,7,19}? */
static int on_wheel(uint64_t n){
    static const uint64_t P[5] = {2,3,5,7,19};
    for(int i=0;i<5;i++) while(n%P[i]==0) n/=P[i];
    return n==1;
}
/* smallest off-wheel prime factor, 0 if fully on-wheel */
static uint64_t off_factor(uint64_t n){
    static const uint64_t P[5] = {2,3,5,7,19};
    for(int i=0;i<5;i++) while(n%P[i]==0) n/=P[i];
    if(n==1) return 0;
    for(uint64_t p=11;p*p<=n;p+=2) if(n%p==0) return p;
    return n;
}

static void row(const char *set, uint64_t num, uint64_t den, const char *claim){
    uint64_t g = gcd_u(num,den); uint64_t n=num/g, d=den/g;
    int okn = on_wheel(n), okd = on_wheel(d);
    printf("%-14s %3llu/%-3llu  red %llu/%llu  num:%s  den:%s  %s  %s\n",
        set,(unsigned long long)num,(unsigned long long)den,
        (unsigned long long)n,(unsigned long long)d,
        okn?"WHEEL":"off", okd?"WHEEL":(off_factor(d)?"off":"WHEEL"),
        (okn&&okd)?"ON-WHEEL":"OFF-WHEEL", claim?claim:"");
}

int main(void){
    printf("== SET A: harmonic division from 1/2 (video: octave,fifth,M3,tone,half) ==\n");
    /* 1/2, 2/3, 4/5, 8/9, 16/17 : term k = 2^k/(2^k+1) */
    for(int k=0;k<=4;k++){
        uint64_t n = 1ULL<<k;
        row("A: from 1/2", n, n+1, k==0?"octave":k==1?"fifth":k==2?"maj3rd":k==3?"tone":"'halftone'");
    }
    printf("\n== SET B: spectrum from 3/4 (video: 1/7 1/4 2/5 4/7 8/11 16/19) ==\n");
    row("B: from 3/4", 1, 7, "listed first");
    for(int k=0;k<=4;k++){
        uint64_t n = 1ULL<<k;
        row("B: from 3/4", n, 3+n, "");
    }
    printf("\n== formula check B: 2^k/(3+2^k) reproduces 1/4 2/5 4/7 8/11 16/19 ==\n");
    for(int k=0;k<=4;k++){ uint64_t n=1ULL<<k; printf("k=%d -> %llu/%llu\n",k,(unsigned long long)n,(unsigned long long)(3+n)); }

    printf("\n== SET C: just scale (reference, string lengths) ==\n");
    row("C: just",1,1,"T"); row("C: just",8,9,"M2"); row("C: just",4,5,"M3");
    row("C: just",3,4,"4th"); row("C: just",2,3,"5th"); row("C: just",3,5,"M6");
    row("C: just",8,15,"M7"); row("C: just",1,2,"8va");

    printf("\n== SET D: phi vs Fibonacci string ratios ==\n");
    /* phi ~ 0.6180339887; Fibonacci ratios f_k/f_{k+1}: 1/2,2/3,3/5,5/8,8/13,13/21 */
    uint64_t f[8]={1,1,2,3,5,8,13,21};
    for(int k=1;k<=6;k++) row("D: fib ratio", f[k], f[k+1], k==3?"5/8=0.625":(k==2?"3/5=0.600":""));
    printf("phi = 0.6180339887 sits between 3/5=0.600 and 5/8=0.625; 8/13=0.6154, 13/21=0.6190\n");

    printf("\n== SET E: polar-graph binary crossings (video table: angle->radial) ==\n");
    /* angles 0,120,190,240,280,360 -> radial 1,2,3,4,5,8 : trivia table */
    printf("0->1 120->2 190->3 240->4 280->5 360->8  (Fibonacci-ish: 1,2,3,5,8 appear; 4 is binary)\n");

    printf("\n== SET F: pulse gear teeth vs 15120 exponents vs Fibonacci ==\n");
    /* 15120 = 2^4*3^3*5^2*7^1 ; pulse teeth: 2->5, 3->4, 5->2, 7->2 ; womb: 8,3,2,1,1 */
    printf("15120 exponents (2,3,5,7): 4,3,2,1\n");
    printf("pulse teeth       (2,3,5,7): 5,4,2,2\n");
    printf("womb exponents (2,3,5,7,19): 8,3,2,1,1\n");
    printf("Fibonacci: 1,1,2,3,5,8\n");
    printf("teeth contain 5 (fib) at prime 2, 4 (binary) at prime 3 -- mixed, NOT pure Fibonacci\n");
    return 0;
}
