/* eden_grows.c — audit the launch theme */
#include <stdio.h>
#include <stdint.h>
static int on_wheel(uint64_t n){ static const uint64_t P[5]={2,3,5,7,19};
  for(int i=0;i<5;i++){ while(n%P[i]==0){ n/=P[i]; } } return n==1; }
int main(void){
    printf("tempo 104 = %d x 13 : 13 is OFF the wheel -> launch tempo lives OUTSIDE Eden\n", 104/13);
    printf("104/60 (song vs heart) = 26/15 ; 26 = 2 x 13 -> off-wheel again\n");
    printf("on_wheel(104)=%d on_wheel(60)=%d\n", on_wheel(104), on_wheel(60));
    printf("\nLevel 5 operands: 1+1, 2+2, 3+3, 5+5 -> operands are FIBONACCI (1,2,3,5), 4 skipped\n");
    printf("results: 2, 4, 6, 10 = Fibonacci DOUBLED : the math verse is both roads at once\n");
    printf("levels 5,4,3,2,1 countdown = descending to ONE = return to the beat\n");
    return 0;
}
