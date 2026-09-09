#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>

//#include <utilities-rand.h>

typedef struct {
    int bit_length;       // LFSR length n
    int num_taps;         // Number of taps excluding highest bit
    int taps[4];          // Tap positions (zero-based), max 4 taps here
} LFSRConfig;

// Tap positions are zero-based (LSB = bit 0), excluding the highest bit (bit_length - 1)
// the table of which bit taps for various lengths give maximum length period
// https://datacipy.elektroniche.cz/lfsr_table.pdf
// starts at 2 (2 bits numbered 1-2)
// dummies added to make offsetting into the array easier
// the first digit is how many taps. The rest are the taps or 0 for not a tap
//
// An alternative list is here:
// https://ww2.ams.org/journals/mcom/1973-27-124/S0025-5718-1973-0327722-7/S0025-5718-1973-0327722-7.pdf?t=1779476860989
static LFSRConfig lfsr_configs[] =
{
    {0, 1, {0}},               // 0 dummy
    {1, 1, {0}},               // 1 dummy
    {2, 2, {1,0}},             // 2 dummy
    {3, 2, {2,1}},             // 3 validated
    {4 ,2, {3,2}},             // 4 validated
    {5, 2, {4,2}},             // 5 validated
    {6, 4, {5,3,2,0}},         // 6 validated
    {7, 2, {6,2}},             // 7 validated
    {8, 4, {7,3,2,1}},         // 8 validated
    {9, 2, {8,4}},             // 9 validated
    {10, 2, {9,2}},            // 10 validated
    {11, 2, {10,1}},           // 11 validated
    {12, 4, {11,5,3,0}},       // 12 validated
    {13, 4, {12,11,9,8}},      // 13 validated
    {14, 4, {13,12,10,8}},     // 14 validated
    {15, 2, {14,0}},           // 15 validated
    {16, 4, {15,11,2,0}},      // 16 validated
    {17, 2, {16,2}},           // 17 validated
    {18, 2, {17,6}},           // 18 validated
    {19, 4, {18,17,16,12}},    // 19 validated
    {20, 2, {19,2}},           // 20 validated
    {21, 2, {20,1}},           // 21 validated
    {22, 2, {21,0}},           // 22 validated
    {23, 2, {22,4}},           // 23 validated
    {24, 4, {23,6,1,0}},       // 24 validated
    {25, 2, {24,2}},           // 25 validated
    {26, 4, {25,24,23,19}},    // 26 validated
    {27, 4, {26,25,24,21}},    // 27 validated
    {28, 2, {27,2}},           // 28 validated
    {29, 2, {28,1}},           // 29 validated
    {30, 4, {29,28,25,23}},    // 30 validated
    {31, 2, {30,2}},           // 31 validated
    {32, 4, {31,30,29,9}},     // 32 validated
    {33, 2, {32, 12}},         // 33 validated
    {34, 4, {33,32,31,6}}      // 34 validated
};

#define NUM_LFSR_CONFIGS (sizeof(lfsr_configs)/sizeof(lfsr_configs[0]))

typedef struct
{
    uint8_t bits;
    uint64_t state;
    uint64_t mask;
} LFSR;

typedef struct
{
    uint8_t lfsr_index;
    LFSR lfsr;
    uint64_t file_base_offset;
} LFSR_ELEM;

typedef struct {
  LFSR_ELEM elem[NUM_LFSR_CONFIGS];
  uint8_t rnds_count;
  unsigned seed;
  unsigned init_seed;
} LFSRRange;

void u_lfsr_init (LFSR * lfsr, uint8_t bits, unsigned seed)
{
    lfsr->bits = bits;
    lfsr->mask = ((1ull << bits) - 1u);
    lfsr->state = seed & lfsr->mask;
}

uint64_t u_lfsr_step (LFSR * lfsr)
{
    uint64_t s = lfsr->state;
    uint8_t feedback = 0;

    switch (lfsr_configs[lfsr->bits].num_taps)
    {
        case 2:
            feedback =
                ((s >> lfsr_configs[lfsr->bits].taps[0]) ^
                 (s >> lfsr_configs[lfsr->bits].taps[1])
                )
                & 1u;
            break;
        case 4:
            feedback =
                ((s >> lfsr_configs[lfsr->bits].taps[0]) ^
                 (s >> lfsr_configs[lfsr->bits].taps[1]) ^
                 (s >> lfsr_configs[lfsr->bits].taps[2]) ^
                 (s >> lfsr_configs[lfsr->bits].taps[3])
                )
                & 1u;
            break;
        default:
            // replace with error
            printf("ERROR: %d\n", lfsr->bits);
            exit(1);
    }

    lfsr->state = ((s << 1) & lfsr->mask) | feedback;

    return lfsr->state;
}

void u_lfsr_print (LFSRConfig * lfsr, uint64_t file_base_offset)
{
    printf ("bit_length: %d num_taps: %d ", lfsr->bit_length, lfsr->num_taps);
    switch (lfsr->num_taps)
    {
        case 2:
            printf ("[0, 1]: %d, %d", lfsr->taps[0], lfsr->taps[1]);
            break;

        case 4:
            printf ("[0, 1, 2, 3]: %d, %d, %d, %d", lfsr->taps[0], lfsr->taps[1], lfsr->taps[2], lfsr->taps[3]);
            break;
    }
    printf (" file_base_offset: %llu\n", (long long unsigned) file_base_offset);
}

LFSRRange * u_lfsr_range_init (uint64_t blocks, unsigned seed){
  LFSRRange * range = malloc(sizeof(LFSRRange)); // todo safe_malloc()
  memset(range, 0, sizeof (LFSRRange));
  uint64_t b = blocks & (~(uint64_t) 7);
  uint8_t bit_offset = 1;
  uint8_t rnds_count = 0;

  range->seed = seed;
  range->init_seed = seed;
  
  LFSR_ELEM * rnds = range->elem;

  // build the list of LFSR generators based on set bits
  while (b != 0)
  {
      if (b & 1)
      {
          rnds[rnds_count].lfsr_index = bit_offset - 1;
          u_lfsr_init (&rnds[rnds_count].lfsr, bit_offset-1, seed);
          if (rnds_count != 0)
          {
              rnds[rnds_count].file_base_offset =
                  (  rnds[rnds_count - 1].file_base_offset
                  + (1 << rnds[rnds_count - 1].lfsr_index)
                  );
          }
          //print_lfsr (&lfsr_configs[rnds[rnds_count].lfsr_index], rnds[rnds_count].file_base_offset);
          rnds_count++;
      }
      b >>= 1;
      bit_offset++;
  }
  range->rnds_count = rnds_count;

  return range;
}

uint64_t u_lfsr_range_step (LFSRRange * range){
  if (range->rnds_count == 0){
    // must stop
    return 0;
  }

  // randomly choose LFS
  int x = rand_r (& range->seed) % range->rnds_count;
  LFSR_ELEM * rnds = range->elem;
  uint64_t block_to_read = rnds[x].lfsr.state;
  u_lfsr_step (&rnds[x].lfsr);

  uint64_t base_offset = rnds[x].file_base_offset;  

  if (rnds[x].lfsr.state == range->init_seed)
  {
      // the current one is exhausted
      for (int j = x; j < range->rnds_count; j++)
      {
          rnds[j] = rnds[j + 1];
      }
      range->rnds_count--;
      return base_offset;
  }
  uint64_t offset = base_offset + block_to_read;
  assert(offset > 0);
 return offset;
}