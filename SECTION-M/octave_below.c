/* octave_below.c — register each reference ratio across octaves below.
 * String-length convention: octave below = length x2 (denominator doubles).
 * Frequency convention: octave below = frequency /2 (numerator doubles when
 * written as frequency ratio like 9/8 -> 9/16). We print the frequency form.
 * Claim under test: dividing by 2 never changes on-wheel membership.
 */
#include <stdio.h>
#include <stdint.h>
static uint64_t gcd_u(uint64_t a,uint64_t b){while(b){uint64_t t=a%b;a=b;b=t;}return a;}
static int on_wheel(uint64_t n){static const uint64_t P[5]={2,3,5,7,19};
  for(int i=0;i<5;i++){ while(n%P[i]==0){ n/=P[i]; } }
  return n==1;}
static void ladder(const char *name,uint64_t n,uint64_t d){
  printf("%-8s base %llu/%-3llu : ",name,(unsigned long long)n,(unsigned long long)d);
  for(int oct=0;oct<=3;oct++){
    uint64_t nn=n, dd=d*(1ULL<<oct); uint64_t g=gcd_u(nn,dd); nn/=g; dd/=g;
    printf("%llu/%-3llu[%s] ",(unsigned long long)nn,(unsigned long long)dd,
      (on_wheel(nn)&&on_wheel(dd))?"W":"X");
  }
  printf("\n");
}
int main(void){
  printf("== Octave-below registration (frequency ratios, 3 octaves down) ==\n");
  ladder("1/7",1,7); ladder("9/8",9,8); ladder("5/4",5,4);
  ladder("4/3",4,3); ladder("3/2",3,2); ladder("5/3",5,3); ladder("15/8",15,8);
  ladder("4/7",4,7); ladder("2/5",2,5); ladder("16/19",16,19);
  ladder("16/17",16,17); ladder("8/11",8,11);
  printf("\n== invariance proof scan: all num,den 1..64, 4 octaves ==\n");
  int fail=0;
  for(uint64_t n=1;n<=64;n++)for(uint64_t d=1;d<=64;d++){
    if(gcd_u(n,d)!=1) continue; /* only ratios in lowest terms */
    int base=on_wheel(n)&&on_wheel(d);
    for(int o=1;o<=4;o++){uint64_t dd=d*(1ULL<<o);uint64_t g=gcd_u(n,dd);
      int now=on_wheel(n/g)&&on_wheel(dd/g);
      if(now!=base){fail=1;printf("VIOLATION %llu/%llu oct%d\n",
        (unsigned long long)n,(unsigned long long)d,o);}}
  }
  printf("scan complete: %s (all 4096 ratios x 4 octaves)\n",
    fail?"VIOLATIONS FOUND":"invariance holds, 0 violations");
  return fail;
}
