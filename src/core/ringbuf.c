
// =============================================
// File: src/core/ringbuf.c
// =============================================
#include "core/ringbuf.h"
#include <stdlib.h>
#include <string.h>

int ringbuf_init(ringbuf_t* rb, size_t elem_size, size_t capacity_pow2){
  if(!rb || elem_size==0 || !rb_is_pow2(capacity_pow2)) return -1;
  size_t bytes = elem_size * capacity_pow2;
  unsigned char* mem = (unsigned char*)malloc(bytes);
  if(!mem) return -1;
  rb->buf = mem; rb->elem_size = elem_size; rb->mask = capacity_pow2 - 1; rb->owns_mem = 1;
  atomic_store_explicit(&rb->head, 0, memory_order_relaxed);
  atomic_store_explicit(&rb->tail, 0, memory_order_relaxed);
  return 0;
}

int ringbuf_init_ext(ringbuf_t* rb, void* ext_mem, size_t elem_size, size_t capacity_pow2){
  if(!rb || !ext_mem || elem_size==0 || !rb_is_pow2(capacity_pow2)) return -1;
  rb->buf = (unsigned char*)ext_mem; rb->elem_size = elem_size; rb->mask = capacity_pow2 - 1; rb->owns_mem = 0;
  atomic_store_explicit(&rb->head, 0, memory_order_relaxed);
  atomic_store_explicit(&rb->tail, 0, memory_order_relaxed);
  return 0;
}

void ringbuf_free(ringbuf_t* rb){ if(rb && rb->owns_mem && rb->buf){ free(rb->buf); rb->buf=NULL; } }

size_t ringbuf_capacity(const ringbuf_t* rb){ return rb ? (rb->mask + 1) : 0; }

size_t ringbuf_size(const ringbuf_t* rb){
  // Safe per SPSC: head letto dal consumer può essere leggermente in avanti/indietro,
  // ma non viola safety; differenza modulo capacità.
  size_t h = atomic_load_explicit((atomic_size_t*)&rb->head, memory_order_acquire);
  size_t t = atomic_load_explicit((atomic_size_t*)&rb->tail, memory_order_acquire);
  return (h - t) & rb->mask;
}

int ringbuf_is_empty(const ringbuf_t* rb){
  size_t h = atomic_load_explicit((atomic_size_t*)&rb->head, memory_order_acquire);
  size_t t = atomic_load_explicit((atomic_size_t*)&rb->tail, memory_order_acquire);
  return h == t;
}

int ringbuf_is_full(const ringbuf_t* rb){
  size_t h = atomic_load_explicit((atomic_size_t*)&rb->head, memory_order_acquire);
  size_t t = atomic_load_explicit((atomic_size_t*)&rb->tail, memory_order_acquire);
  return ((h - t) & rb->mask) == rb->mask; // size == capacity-1 used (one slot left empty)
}

int ringbuf_try_enqueue(ringbuf_t* rb, const void* elem){
  size_t h = atomic_load_explicit(&rb->head, memory_order_relaxed);
  size_t t = atomic_load_explicit(&rb->tail, memory_order_acquire);
  size_t next_h = (h + 1) & rb->mask;
  if (next_h == (t & rb->mask)) {
    // buffer pieno: manteniamo un elemento sempre libero per distinguere empty/full
    return 0;
  }
  memcpy(rb->buf + (h & rb->mask) * rb->elem_size, elem, rb->elem_size);
  atomic_store_explicit(&rb->head, next_h, memory_order_release);
  return 1;
}

int ringbuf_try_dequeue(ringbuf_t* rb, void* elem_out){
  size_t t = atomic_load_explicit(&rb->tail, memory_order_relaxed);
  size_t h = atomic_load_explicit(&rb->head, memory_order_acquire);
  if (t == (h & rb->mask)) {
    // empty
    return 0;
  }
  memcpy(elem_out, rb->buf + (t & rb->mask) * rb->elem_size, rb->elem_size);
  size_t next_t = (t + 1) & rb->mask;
  atomic_store_explicit(&rb->tail, next_t, memory_order_release);
  return 1;
}
