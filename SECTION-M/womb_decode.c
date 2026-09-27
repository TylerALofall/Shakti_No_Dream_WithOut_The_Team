/* womb_decode.c — 266 x 86400 = 22,982,400 : why the womb is the start */
#include <stdio.h>
#include <stdint.h>
int main(void){
    uint64_t days=266, sec_day=86400, womb=days*sec_day;
    printf("266 x 86400 = %llu\n",(unsigned long long)womb);
    /* factor the womb */
    uint64_t n=womb; static const uint64_t P[5]={2,3,5,7,19};
    printf("factorization: ");
    for(int i=0;i<5;i++){int e=0;while(n%P[i]==0){n/=P[i];e++;}
      if(e)printf("%llu^%d ",(unsigned long long)P[i],e);}
    printf("(remainder %llu)\n",(unsigned long long)n);
    /* divisor count from exponents 8,3,2,1,1 */
    printf("divisors: (8+1)(3+1)(2+1)(1+1)(1+1) = %d\n",9*4*3*2*2);
    /* 86400 as beats */
    printf("86400 = 60 x 60 x 24 = %d (seconds/day)\n",60*60*24);
    printf("at 60 BPM = 1 beat/sec -> beats/day = %d\n",86400);
    printf("266 days = 38 weeks = human gestation (conception to birth)\n");
    printf("womb = gestation measured in heartbeats = %llu beats\n",(unsigned long long)womb);
    /* Fibonacci exponent check */
    printf("exponents 8,3,2,1,1 = F6,F4,F3,F2,F1 : Fibonacci descending\n");
    /* 266 on the wheel? */
    printf("266 = 2 x 7 x 19 : all three factors are beat primes\n");
    /* sanity: mirror law partners of womb vs wheel */
    printf("womb / 15120 = %llu  (womb holds %llu full wheels)\n",
      (unsigned long long)(womb/15120),(unsigned long long)(womb/15120));
    printf("womb / 5040  = %llu\n",(unsigned long long)(womb/5040));
    printf("womb / 210   = %llu\n",(unsigned long long)(womb/210));
    return 0;
}
