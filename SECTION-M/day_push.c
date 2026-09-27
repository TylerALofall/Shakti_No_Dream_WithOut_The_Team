/* day_push.c — the whole womb compressed into waking time */
#include <stdio.h>
#include <stdint.h>
int main(void){
    uint64_t womb=22982400ULL;
    uint64_t day=86400, waking=18*3600; /* 18 hours = 64800 s */
    printf("18 hours = %llu seconds\n",(unsigned long long)waking);
    printf("64800 = 86400 x 3/4 : %s\n",(waking*4==day*3)?"YES":"NO");
    printf("\nwomb pushed into a FULL day:  %llu / %llu = %llu beats/sec\n",
      (unsigned long long)womb,(unsigned long long)day,(unsigned long long)(womb/day));
    printf("266 = 2 x 7 x 19 -> all beat primes, ON-WHEEL\n");
    printf("\nwomb pushed into 18 WAKING hours: %llu / %llu = %llu + %llu/%llu beats/sec\n",
      (unsigned long long)womb,(unsigned long long)waking,
      (unsigned long long)(womb/waking),(unsigned long long)(womb%waking),(unsigned long long)waking);
    printf("remainder %llu/64800 = 2/3 -> 354.666... = the song's 354.66\n",(unsigned long long)(womb%waking));
    printf("354 + 2/3 = 1064/3 ; 1064 = 8 x 133 = 2^3 x 7 x 19 -> ON-WHEEL over 3\n");
    printf("\nequivalence check: 266 x 4/3 = %d/3 = same number (18h is 3/4 of a day)\n",266*4);
    return 0;
}
