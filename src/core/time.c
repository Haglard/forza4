#define _POSIX_C_SOURCE 199309L
#include "core/time.h"
#include <time.h>
#include <stdint.h>

uint64_t time_now_ns(void){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t time_now_ms(void){
  return time_now_ns()/1000000ull;
}
