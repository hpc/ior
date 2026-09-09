#include <stdio.h>

#include <utilities-rand.h>

int main(){

  LFSRRange * range = u_lfsr_range_init (8+16, 1); // with 1023

  uint64_t pos = 1;
  uint64_t count = 0;
  while(pos != 0){
    pos = u_lfsr_range_step (range);
    printf("%lld\n", (long long unsigned) pos);
    count++;
  }

  printf("Total scanned %lld\n", (long long unsigned) count);
  
  return 0;
}