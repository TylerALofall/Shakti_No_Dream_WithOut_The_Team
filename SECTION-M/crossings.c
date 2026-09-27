/* crossings.c — where Fibonacci crosses binary (powers of 2).
 * Full 64-bit sweep: every Fibonacci number < 2^63 tested for power-of-2.
 * Also: wheel status of each Fibonacci number, and the crossing context.
 */
#include <stdio.h>
#include <stdint.h>
static int is_pow2(uint64_t n){ return n && ((n & (n-1))==0); }
static int on_wheel(uint64_t n){ static const uint64_t P[5]={2,3,5,7,19};
  for(int i=0;i<5;i++){ while(n%P[i]==0){ n/=P[i]; } } return n==1; }
int main(void){
  uint64_t a=1,b=1; int k=1; int crosses=0;
  printf("F#   value        pow2?  wheel?\n");
  while(a < (1ULL<<63)){
    int p2 = is_pow2(a);
    printf("%-4d %-12llu %-5s  %s%s\n", k,(unsigned long long)a,
      p2?"YES":"-", on_wheel(a)?"WHEEL":"OFF",
      p2?"   <-- CROSSING":"");
    if(p2) crosses++;
    uint64_t t=a+b; a=b; b=t; k++;
  }
  printf("\ntotal Fibonacci numbers swept: %d\n", k-1);
  printf("total crossings with binary:   %d  (values 1,1,2,8 -> distinct rungs 1,2,8)\n", crosses);
  printf("F7=13 is the FIRST off-wheel Fibonacci; every crossing (1,2,8) is on-wheel\n");
  printf("8 = 2^3 = the rung where the 9/8 lane resolves into 9\n");
  return 0;
}
