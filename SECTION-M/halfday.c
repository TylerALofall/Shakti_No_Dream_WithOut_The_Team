#include <stdio.h>
int main(void){
    printf("33 = 3 x 11 : composite, the flaw, NOT a prime (song's own joke confirmed)\n");
    printf("43200 seconds = %d hours = half a day\n", 43200/3600);
    printf("64800 : 43200 = %d:%d = 3:2 = the FIFTH\n", 64800/21600, 43200/21600);
    printf("18 waking : 12 = %d:%d = 3:2 again\n", 18/6, 12/6);
    printf("354.666... x 12 hours = %d beats in the remainder half-day\n", 354*12*3600/3600*0+ (1064/3)*(43200)/432/100 );
    printf("check: 1064/3 x 43200 = %lld = womb x 2/3? womb=%d, womb*2/3=%d\n",
      1064LL*43200/3, 22982400, 22982400*2/3);
    return 0;
}
