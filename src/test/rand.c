#include <stdio.h>

#include <utilities-rand.h>

int main(){

  LFSRRange * range = u_lfsr_range_init (16+32+8, 1); // with 1023

  uint64_t pos = 1;
  uint64_t count = -1;
  while(pos != -1){
    count++;
    pos = u_lfsr_range_step (range);
    printf("%lld\n", (long long unsigned) pos);
  }

  printf("Total scanned %lld\n", (long long unsigned) count);
  
  return 0;
}