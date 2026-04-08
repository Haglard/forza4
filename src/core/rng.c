
// =============================================
// File: src/core/rng.c
// =============================================
#include "core/rng.h"
#include <unistd.h>
#include <fcntl.h>

/* SplitMix64 for seeding */
static uint64_t splitmix64(uint64_t* x){
  uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

void rng_seed(rng_t* r, const uint64_t s[4]){
  r->s[0]=s[0]?s[0]:0x0123456789ABCDEFULL;
  r->s[1]=s[1]?s[1]:0xF0E1D2C3B4A59687ULL;
  r->s[2]=s[2]?s[2]:0x9E3779B97F4A7C15ULL;
  r->s[3]=s[3]?s[3]:0xD1B54A32D192ED03ULL;
}

void rng_seed_u64(rng_t* r, uint64_t seed){
  uint64_t x = seed; r->s[0]=splitmix64(&x); r->s[1]=splitmix64(&x); r->s[2]=splitmix64(&x); r->s[3]=splitmix64(&x);
}

int rng_seed_from_entropy(rng_t* r){
  int fd = open("/dev/urandom", O_RDONLY);
  if(fd < 0){ return -1; }
  uint64_t s[4]; ssize_t n = read(fd, s, sizeof(s)); close(fd);
  if(n != (ssize_t)sizeof(s)) return -1;
  rng_seed(r, s);
  return 0;
}

static inline uint64_t rotl(const uint64_t x, int k){ return (x << k) | (x >> (64 - k)); }

uint64_t rng_next_u64(rng_t* r){
  const uint64_t result = rotl(r->s[1] * 5ULL, 7) * 9ULL;
  const uint64_t t = r->s[1] << 17;
  r->s[2] ^= r->s[0]; r->s[3] ^= r->s[1]; r->s[1] ^= r->s[2]; r->s[0] ^= r->s[3];
  r->s[2] ^= t; r->s[3] = rotl(r->s[3], 45);
  return result;
}

void rng_jump(rng_t* r){
  static const uint64_t JUMP[] = { 0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL, 0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL };
  uint64_t s0=0, s1=0, s2=0, s3=0;
  for(int i=0;i<4;i++){
    for(int b=0;b<64;b++){
      if(JUMP[i] & (1ULL<<b)) { s0^=r->s[0]; s1^=r->s[1]; s2^=r->s[2]; s3^=r->s[3]; }
      (void)rng_next_u64(r);
    }
  }
  r->s[0]=s0; r->s[1]=s1; r->s[2]=s2; r->s[3]=s3;
}

uint64_t rng_next_bounded(rng_t* r, uint64_t bound){
  // Rejection sampling (avoid modulo bias)
  uint64_t x, m = -bound % bound; // the smallest x such that x % bound == 0
  do { x = rng_next_u64(r); } while (x < m);
  return x % bound;
}
