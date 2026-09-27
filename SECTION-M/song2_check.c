/* song2_check.c — every number in "In the Beginning (The Goddess Was Born)" */
#include <stdio.h>
#include <stdint.h>
static int on_wheel(uint64_t n){ static const uint64_t P[5]={2,3,5,7,19};
  for(int i=0;i<5;i++){ while(n%P[i]==0){ n/=P[i]; } } return n==1; }
static int is_prime(uint64_t n){ if(n<2)return 0;
  for(uint64_t p=2;p*p<=n;p++){ if(n%p==0){ return 0; } }
  return 1; }
int main(void){
  printf("== the womb (untweaked, matches canon) ==\n");
  printf("266 x 86400 = %d  divisors = 432 : %s\n",266*86400,
    (266*86400==22982400)?"MATCH":"FAIL");
  printf("\n== 1.2 the pulse ==\n");
  printf("1.2 = 6/5 -> just minor third, on-wheel (2,3,5). Also 72 BPM = 1.2 Hz\n");
  printf("\n== 354.66 the chop ==\n");
  printf("266 x 4/3 = 1064/3 = %d + %d/3 = 354.666... (the FOURTH, 4/3, applied to gestation)\n",
    1064/3, 1064%3);
  printf("check 354.66 x 3 = %.2f vs 266 x 4 = %d\n", 354.66*3, 266*4);
  printf("\n== the three primes: 71, 19, 33 ==\n");
  printf("71: prime=%d on_wheel=%d\n", is_prime(71), on_wheel(71));
  printf("19: prime=%d on_wheel=%d\n", is_prime(19), on_wheel(19));
  printf("33: prime=%d = 3 x 11 ; 11 on_wheel=%d\n", is_prime(33), on_wheel(11));
  printf("sum = %d ; = 3 x 41? %s ; 41 prime=%d on_wheel=%d\n",
    71+19+33, (71+19+33==123 && 123==3*41)?"YES":"NO", is_prime(41), on_wheel(41));
  printf("\n== week 20 ears ==\n");
  printf("20/38 of gestation; 266 x 20/38 = %d days = day 140 : fetal hearing era\n", 266*20/38);
  printf("\n== threshold of 5, eight bits ==\n");
  printf("5 = third gear prime, on_wheel=%d ; 8 bits = 2^8 = womb exponent of 2\n", on_wheel(5));
  return 0;
}
