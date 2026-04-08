
// =============================================
// File: src/core/pool.c
// =============================================
#include "core/pool.h"
#include <stdlib.h>
#include <string.h>

/* Internals */
typedef struct pool_block { struct pool_block* next; } pool_block_t;

typedef struct pool_chunk {
  struct pool_chunk* next;
  // segue memoria dati (stride * elems_per_chunk)
} pool_chunk_t;

struct pool {
  size_t elem_size;
  size_t stride;
  size_t align;
  size_t elems_per_chunk;
  size_t chunks;
  pool_block_t* free_list;
  pool_chunk_t* first;   // primo chunk
  pool_chunk_t* last;    // ultimo chunk per append veloce
};

static size_t align_up(size_t x, size_t a){ return (x + (a-1)) & ~(a-1); }
static int is_pow2(size_t x){ return x && ((x & (x-1))==0); }

static int pool_add_chunk(pool_t* p){
  size_t payload_bytes = p->stride * p->elems_per_chunk;
  size_t total = sizeof(pool_chunk_t) + payload_bytes;
  pool_chunk_t* ck = (pool_chunk_t*)malloc(total);
  if(!ck) return -2;
  ck->next = NULL;
  if(!p->first) p->first = p->last = ck; else { p->last->next = ck; p->last = ck; }
  unsigned char* base = (unsigned char*)(ck + 1);
  // costruiamo la free list per questo chunk
  for(size_t i=0; i<p->elems_per_chunk; ++i){
    pool_block_t* b = (pool_block_t*)(base + i*p->stride);
    b->next = p->free_list;
    p->free_list = b;
  }
  p->chunks++;
  return 0;
}

int pool_init(pool_t** out, size_t elem_size, size_t align, size_t elems_per_chunk){
  if(!out || elem_size==0 || elems_per_chunk==0) return -1;
  if(align==0) align = sizeof(void*);
  if(!is_pow2(align)) return -1;

  pool_t* p = (pool_t*)calloc(1, sizeof(pool_t));
  if(!p) return -2;
  p->elem_size = elem_size;
  p->align = align;
  size_t min_stride = elem_size<sizeof(pool_block_t) ? sizeof(pool_block_t) : elem_size;
  p->stride = align_up(min_stride, align);
  p->elems_per_chunk = elems_per_chunk;

  int rc = pool_add_chunk(p);
  if(rc!=0){ free(p); return rc; }
  *out = p;
  return 0;
}

void* pool_alloc(pool_t* p){
  if(!p) return NULL;
  if(!p->free_list){
    if(pool_add_chunk(p)!=0) return NULL;
  }
  pool_block_t* b = p->free_list;
  p->free_list = b->next;
  return (void*)b;
}

void pool_free(pool_t* p, void* ptr){
  if(!p || !ptr) return;
  pool_block_t* b = (pool_block_t*)ptr;
  b->next = p->free_list;
  p->free_list = b;
}

void pool_clear(pool_t* p){
  if(!p) return;
  // tieni il primo chunk, libera dal secondo
  pool_chunk_t* keep = p->first;
  if(!keep) return;
  pool_chunk_t* it = keep->next;
  while(it){ pool_chunk_t* nxt = it->next; free(it); it = nxt; }
  keep->next = NULL; p->last = keep;
  // ricostruisci la free-list dal primo chunk
  p->free_list = NULL;
  unsigned char* base = (unsigned char*)(keep + 1);
  for(size_t i=0; i<p->elems_per_chunk; ++i){
    pool_block_t* b = (pool_block_t*)(base + i*p->stride);
    b->next = p->free_list; p->free_list = b;
  }
  p->chunks = 1;
}

void pool_destroy(pool_t* p){
  if(!p) return;
  pool_chunk_t* it = p->first;
  while(it){ pool_chunk_t* nxt = it->next; free(it); it = nxt; }
  free(p);
}

pool_stats_t pool_get_stats(const pool_t* p){
  pool_stats_t s = {0};
  if(!p) return s;
  s.elem_size = p->elem_size;
  s.stride = p->stride;
  s.align = p->align;
  s.elems_per_chunk = p->elems_per_chunk;
  s.chunks = p->chunks;
  // Conta free_list (O(n) nel numero di blocchi del primo chunk + extra)
  size_t freec = 0; const pool_block_t* it = p->free_list; while(it){ freec++; it = it->next; }
  s.free_count = freec;
  s.inuse_count = p->chunks * p->elems_per_chunk - freec;
  return s;
}
