// Check that u_lfsr_range_step() returns a permutation of [0, blocks):
// every block exactly once per "blocks" calls, for many block counts and
// seeds, including after the permutation restarts.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <utilities-rand.h>

// distinct prime factors of 2^k - 1, zero terminated
static const uint64_t mersenne_factors[64][12] = {
  [2] = { 3ULL, 0 },
  [3] = { 7ULL, 0 },
  [4] = { 3ULL, 5ULL, 0 },
  [5] = { 31ULL, 0 },
  [6] = { 3ULL, 7ULL, 0 },
  [7] = { 127ULL, 0 },
  [8] = { 3ULL, 5ULL, 17ULL, 0 },
  [9] = { 7ULL, 73ULL, 0 },
  [10] = { 3ULL, 11ULL, 31ULL, 0 },
  [11] = { 23ULL, 89ULL, 0 },
  [12] = { 3ULL, 5ULL, 7ULL, 13ULL, 0 },
  [13] = { 8191ULL, 0 },
  [14] = { 3ULL, 43ULL, 127ULL, 0 },
  [15] = { 7ULL, 31ULL, 151ULL, 0 },
  [16] = { 3ULL, 5ULL, 17ULL, 257ULL, 0 },
  [17] = { 131071ULL, 0 },
  [18] = { 3ULL, 7ULL, 19ULL, 73ULL, 0 },
  [19] = { 524287ULL, 0 },
  [20] = { 3ULL, 5ULL, 11ULL, 31ULL, 41ULL, 0 },
  [21] = { 7ULL, 127ULL, 337ULL, 0 },
  [22] = { 3ULL, 23ULL, 89ULL, 683ULL, 0 },
  [23] = { 47ULL, 178481ULL, 0 },
  [24] = { 3ULL, 5ULL, 7ULL, 13ULL, 17ULL, 241ULL, 0 },
  [25] = { 31ULL, 601ULL, 1801ULL, 0 },
  [26] = { 3ULL, 2731ULL, 8191ULL, 0 },
  [27] = { 7ULL, 73ULL, 262657ULL, 0 },
  [28] = { 3ULL, 5ULL, 29ULL, 43ULL, 113ULL, 127ULL, 0 },
  [29] = { 233ULL, 1103ULL, 2089ULL, 0 },
  [30] = { 3ULL, 7ULL, 11ULL, 31ULL, 151ULL, 331ULL, 0 },
  [31] = { 2147483647ULL, 0 },
  [32] = { 3ULL, 5ULL, 17ULL, 257ULL, 65537ULL, 0 },
  [33] = { 7ULL, 23ULL, 89ULL, 599479ULL, 0 },
  [34] = { 3ULL, 43691ULL, 131071ULL, 0 },
  [35] = { 31ULL, 71ULL, 127ULL, 122921ULL, 0 },
  [36] = { 3ULL, 5ULL, 7ULL, 13ULL, 19ULL, 37ULL, 73ULL, 109ULL, 0 },
  [37] = { 223ULL, 616318177ULL, 0 },
  [38] = { 3ULL, 174763ULL, 524287ULL, 0 },
  [39] = { 7ULL, 79ULL, 8191ULL, 121369ULL, 0 },
  [40] = { 3ULL, 5ULL, 11ULL, 17ULL, 31ULL, 41ULL, 61681ULL, 0 },
  [41] = { 13367ULL, 164511353ULL, 0 },
  [42] = { 3ULL, 7ULL, 43ULL, 127ULL, 337ULL, 5419ULL, 0 },
  [43] = { 431ULL, 9719ULL, 2099863ULL, 0 },
  [44] = { 3ULL, 5ULL, 23ULL, 89ULL, 397ULL, 683ULL, 2113ULL, 0 },
  [45] = { 7ULL, 31ULL, 73ULL, 151ULL, 631ULL, 23311ULL, 0 },
  [46] = { 3ULL, 47ULL, 178481ULL, 2796203ULL, 0 },
  [47] = { 2351ULL, 4513ULL, 13264529ULL, 0 },
  [48] = { 3ULL, 5ULL, 7ULL, 13ULL, 17ULL, 97ULL, 241ULL, 257ULL, 673ULL, 0 },
  [49] = { 127ULL, 4432676798593ULL, 0 },
  [50] = { 3ULL, 11ULL, 31ULL, 251ULL, 601ULL, 1801ULL, 4051ULL, 0 },
  [51] = { 7ULL, 103ULL, 2143ULL, 11119ULL, 131071ULL, 0 },
  [52] = { 3ULL, 5ULL, 53ULL, 157ULL, 1613ULL, 2731ULL, 8191ULL, 0 },
  [53] = { 6361ULL, 69431ULL, 20394401ULL, 0 },
  [54] = { 3ULL, 7ULL, 19ULL, 73ULL, 87211ULL, 262657ULL, 0 },
  [55] = { 23ULL, 31ULL, 89ULL, 881ULL, 3191ULL, 201961ULL, 0 },
  [56] = { 3ULL, 5ULL, 17ULL, 29ULL, 43ULL, 113ULL, 127ULL, 15790321ULL, 0 },
  [57] = { 7ULL, 32377ULL, 524287ULL, 1212847ULL, 0 },
  [58] = { 3ULL, 59ULL, 233ULL, 1103ULL, 2089ULL, 3033169ULL, 0 },
  [59] = { 179951ULL, 3203431780337ULL, 0 },
  [60] = { 3ULL, 5ULL, 7ULL, 11ULL, 13ULL, 31ULL, 41ULL, 61ULL, 151ULL, 331ULL, 1321ULL, 0 },
  [61] = { 2305843009213693951ULL, 0 },
  [62] = { 3ULL, 715827883ULL, 2147483647ULL, 0 },
  [63] = { 7ULL, 73ULL, 127ULL, 337ULL, 92737ULL, 649657ULL, 0 },
};

// apply the linear map with column images m[] to the vector v over GF(2)
static uint64_t gf2_apply(const uint64_t *m, uint64_t v)
{
  uint64_t r = 0;

  for (int j = 0; v; j++, v >>= 1)
    if (v & 1)
      r ^= m[j];
  return r;
}

// r = m^e for the k x k matrix m over GF(2)
static void gf2_pow(const uint64_t *m, uint64_t e, int k, uint64_t *r)
{
  uint64_t base[64], tmp[64];
  int j;

  for (j = 0; j < k; j++) {
    r[j] = 1ULL << j;
    base[j] = m[j];
  }
  while (e) {
    if (e & 1) {
      for (j = 0; j < k; j++)
        tmp[j] = gf2_apply(base, r[j]);
      memcpy(r, tmp, k * sizeof(*r));
    }
    for (j = 0; j < k; j++)
      tmp[j] = gf2_apply(base, base[j]);
    memcpy(base, tmp, k * sizeof(*base));
    e >>= 1;
  }
}

static int gf2_is_identity(const uint64_t *m, int k)
{
  for (int j = 0; j < k; j++)
    if (m[j] != 1ULL << j)
      return 0;
  return 1;
}

// A k-bit LFSR has the maximal period 2^k - 1 exactly when its state
// transition matrix M has order 2^k - 1: M^(2^k - 1) = I, and
// M^((2^k - 1) / q) != I for each prime q dividing 2^k - 1.
static int check_taps(int k)
{
  uint64_t mask = (1ULL << k) - 1, taps = u_lfsr_taps(k), n = mask;
  uint64_t m[64], r[64];
  int j;

  for (j = 0; j < k; j++) {
    uint64_t s = 1ULL << j;

    m[j] = ((s << 1) & mask) | (__builtin_parityll(s & taps));
  }
  gf2_pow(m, n, k, r);
  if (!gf2_is_identity(r, k))
    goto bad;
  for (j = 0; mersenne_factors[k][j]; j++) {
    gf2_pow(m, n / mersenne_factors[k][j], k, r);
    if (gf2_is_identity(r, k))
      goto bad;
  }
  return 0;
bad:
  fprintf(stderr, "LFSR taps %#llx for %d bits are not maximal length\n",
          (unsigned long long) taps, k);
  return 1;
}

static int check_perm(uint64_t blocks, uint64_t seed, int passes)
{
  unsigned char *seen = malloc(blocks ? blocks : 1);
  LFSRRange *range = u_lfsr_range_init(blocks, seed);
  int pass;

  if (seen == NULL || range == NULL) {
    fprintf(stderr, "out of memory for %llu blocks\n", (unsigned long long) blocks);
    exit(1);
  }
  for (pass = 0; pass < passes; pass++) {
    memset(seen, 0, blocks);
    for (uint64_t i = 0; i < blocks; i++) {
      uint64_t b = u_lfsr_range_step(range);

      if (b >= blocks || seen[b]) {
        fprintf(stderr, "blocks=%llu seed=%llu pass=%d step=%llu: %s block %llu\n",
                (unsigned long long) blocks, (unsigned long long) seed, pass,
                (unsigned long long) i, b >= blocks ? "out of range" : "repeated",
                (unsigned long long) b);
        return 1;
      }
      seen[b] = 1;
    }
  }
  u_lfsr_range_free(range);
  free(seen);
  return 0;
}

static int cmp_u64(const void *a, const void *b)
{
  uint64_t x = *(const uint64_t *) a, y = *(const uint64_t *) b;

  return x < y ? -1 : x > y;
}

// the first steps of a very large range are in range and do not repeat
static int check_large(uint64_t blocks, uint64_t seed, size_t steps)
{
  uint64_t *v = malloc(steps * sizeof(*v));
  LFSRRange *range = u_lfsr_range_init(blocks, seed);
  size_t i;

  for (i = 0; i < steps; i++) {
    v[i] = u_lfsr_range_step(range);
    if (v[i] >= blocks) {
      fprintf(stderr, "blocks=%llu seed=%llu: block %llu out of range\n",
              (unsigned long long) blocks, (unsigned long long) seed,
              (unsigned long long) v[i]);
      return 1;
    }
  }
  qsort(v, steps, sizeof(*v), cmp_u64);
  for (i = 1; i < steps; i++) {
    if (v[i] == v[i - 1]) {
      fprintf(stderr, "blocks=%llu seed=%llu: block %llu repeated\n",
              (unsigned long long) blocks, (unsigned long long) seed,
              (unsigned long long) v[i]);
      return 1;
    }
  }
  u_lfsr_range_free(range);
  free(v);
  return 0;
}

int main(void)
{
  int rc = 0;
  uint64_t blocks, seed;
  int k;

  // every LFSR width has maximal-length taps
  for (k = 2; k < 64; k++)
    rc |= check_taps(k);

  // every small block count, including those below 8
  for (blocks = 0; blocks <= 2048; blocks++)
    for (seed = 1; seed <= 16; seed++)
      rc |= check_perm(blocks, seed, 2);

  // every LFSR width up to 2^22, alone and with other sub-ranges
  for (k = 0; k <= 22; k++) {
    rc |= check_perm(1ULL << k, 1, 1);
    rc |= check_perm((1ULL << k) + 1000003, 7, 1);
  }

  // many ranks with IO500-like file sizes: multiples of 2MiB / 4KiB
  for (seed = 1; seed <= 100000; seed += 997)
    rc |= check_perm(512 * 1234 + 8 * (seed % 64), seed, 1);
  for (seed = 100000; seed <= 1000000; seed += 99991)
    rc |= check_perm(512 * 1234, seed, 1);

  // the widest sub-ranges, up to 2^63 blocks
  rc |= check_large((1ULL << 40) + (1ULL << 35) + 12345, 1, 1000000);
  rc |= check_large(UINT64_MAX, 123456789, 1000000);
  rc |= check_large(1ULL << 63, 99999, 1000000);

  printf("LFSR permutation test %s\n", rc ? "FAILED" : "passed");
  return rc;
}
