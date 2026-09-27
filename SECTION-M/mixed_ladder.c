/* mixed_ladder.c — Tyler's mixed-number doubling lanes.
 * Claim: (m+1)/m doubled k times = 2^k + 2^k/m; when m | 2^k the lane
 * resolves onto the integer road. m=8 resolves at k=3 (the octave 9/8->9).
 */
#include <stdio.h>
#include <stdint.h>
static uint64_t gcd_u(uint64_t a,uint64_t b){while(b){uint64_t t=a%b;a=b;b=t;}return a;}
static void lane(uint64_t num, uint64_t den, int steps){
  printf("lane %llu/%llu: ",(unsigned long long)num,(unsigned long long)den);
  for(int k=0;k<=steps;k++){
    uint64_t n=num*(1ULL<<k), d=den, g=gcd_u(n,d); n/=g; d/=g;
    if(d==1) printf("%llu ",(unsigned long long)n);
    else printf("%llu+%llu/%llu ",(unsigned long long)(n/d),
      (unsigned long long)(n%d),(unsigned long long)d);
  }
  printf("\n");
}
int main(void){
  lane(9,8,4);   /* Tyler's lane: 1+1/8, 2+1/4, 4+1/2, 9, 18 */
  lane(5,4,3);   /* M3 lane:   1+1/4, 2+1/2, 5, 10          */
  lane(4,3,4);   /* 4th lane:  1+1/3 ... never resolves by 2 */
  lane(3,2,3);   /* 5th lane:  1+1/2, 3, 6, 12               */
  lane(16,9,5);  /* mirror of 9/8 (reciprocal family)        */
  lane(7,4,3);   /* septimal:  1+3/4, 3+1/2, 7, 14          */
  printf("\nresolve rule check: den power of 2 -> resolves; else never\n");
  uint64_t dens[6]={8,4,3,2,9,7};
  for(int i=0;i<6;i++){uint64_t d=dens[i],t=d;while(t%2==0)t/=2;
    printf("den=%llu %s\n",(unsigned long long)d,t==1?"resolves on the road":"NEVER resolves (3,7,9 carry a permanent remainder)");}
  return 0;
}
