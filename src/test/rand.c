#include <stdio.h>

#include <utilities-rand.h>

int main(){

  LFSRRange * range = u_lfsr_range_init (1024+512, 1); // with 1024

  for (int i=0; i < 100; i++){
    uint64_t pos = u_lfsr_range_step (range);
    printf("%lld\n", (long long unsigned) pos);
  }
  
  return 0;
}