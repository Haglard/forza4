// =============================================
// File: src/core/hashmap.c
// Purpose: Robin Hood open-addressing hashmap (generic key/value blobs)
// License: MIT (c) 2025
// =============================================
#include "core/hashmap.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- Internals ---------- */

typedef struct {
  uint64_t h;      /* full 64-bit hash */
  uint32_t dist;   /* probe distance from home bucket */
  uint8_t  used;   /* 0 = empty, 1 = occupied */
  size_t   klen;
  size_t   vlen;
  void*    key;    /* malloc'd key bytes (klen) */
  void*    val;    /* malloc'd val bytes (vlen) or NULL if vlen==0 */
} bucket_t;

struct hashmap {
  size_t cap;          /* buckets count, power of two */
  size_t sz;           /* number of elements */
  size_t init_cap;     /* initial capacity (for hm_clear) */
  hash_func  hf;       /* user hash(key, len) */
  key_eq_func eq;      /* user eq(a,alen,b,blen) */
  bucket_t* b;         /* buckets array */
};

static int is_pow2(size_t x){ return x && ((x & (x-1))==0); }
static size_t idx_for(uint64_t h, size_t cap){ return (size_t)(h & (cap - 1)); }
static void bucket_swap(bucket_t* a, bucket_t* b){ bucket_t t=*a; *a=*b; *b=t; }

/* Rehash entire table to new_cap (power of two). Moves pointers, no key/val copies. */
static int hm_rehash(hashmap_t* m, size_t new_cap){
  if(!is_pow2(new_cap) || new_cap < 8) new_cap = 8;
  bucket_t* nb = (bucket_t*)calloc(new_cap, sizeof(bucket_t));
  if(!nb) return -1;

  bucket_t* old = m->b; size_t oldcap = m->cap;
  m->b = nb; m->cap = new_cap; m->sz = 0;

  for(size_t i=0;i<oldcap;i++){
    if(!old[i].used) continue;
    /* reinserisci l'entry */
    bucket_t e = old[i];
    e.dist = 0;
    e.used = 1; /* IMPORTANT: l'entry resta valida durante la sonda/insert */
    size_t idx = idx_for(e.h, m->cap);
    for(;;){
      bucket_t* cur = &m->b[idx];
      if(!cur->used){
        *cur = e;
        cur->used = 1;
        m->sz++;
        break;
      }
      if(cur->dist < e.dist){
        bucket_swap(cur, &e);
      }
      idx = (idx + 1) & (m->cap - 1);
      e.dist++;
    }
  }

  free(old);
  return 0;
}

static int hm_maybe_grow(hashmap_t* m){
  /* grow at ~0.75 load */
  if (m->sz + 1 <= (m->cap * 3) / 4) return 0;
  return hm_rehash(m, m->cap * 2);
}

/* Backshift deletion: free entry at idx, shift following cluster back by one */
static void backshift_delete(hashmap_t* m, size_t idx){
  /* free key/val of the removed entry */
  if(m->b[idx].key) free(m->b[idx].key);
  if(m->b[idx].val) free(m->b[idx].val);
  m->b[idx].key = NULL; m->b[idx].val = NULL;
  m->b[idx].klen = 0; m->b[idx].vlen = 0;
  m->b[idx].used = 0; m->b[idx].dist = 0; m->b[idx].h = 0;

  size_t cur = (idx + 1) & (m->cap - 1);
  while (m->b[cur].used && m->b[cur].dist > 0){
    size_t prev = (cur - 1) & (m->cap - 1);
    m->b[prev] = m->b[cur];
    m->b[prev].dist--;   /* shifted back by one */
    /* clear current slot */
    m->b[cur].used = 0; m->b[cur].dist = 0;
    m->b[cur].key = NULL; m->b[cur].val = NULL;
    m->b[cur].klen = 0; m->b[cur].vlen = 0; m->b[cur].h = 0;
    cur = (cur + 1) & (m->cap - 1);
  }
}

/* ---------- Public API ---------- */

hashmap_t* hm_create(size_t capacity_pow2, hash_func hf, key_eq_func eq){
  if(!hf || !eq) return NULL;
  if(!is_pow2(capacity_pow2) || capacity_pow2 < 8) capacity_pow2 = 64;
  hashmap_t* m = (hashmap_t*)calloc(1, sizeof(*m));
  if(!m) return NULL;
  m->cap = m->init_cap = capacity_pow2;
  m->hf = hf; m->eq = eq;
  m->b = (bucket_t*)calloc(m->cap, sizeof(bucket_t));
  if(!m->b){ free(m); return NULL; }
  return m;
}

void hm_destroy(hashmap_t* m){
  if(!m) return;
  for(size_t i=0;i<m->cap;i++){
    if(m->b[i].used){
      if(m->b[i].key) free(m->b[i].key);
      if(m->b[i].val) free(m->b[i].val);
    }
  }
  free(m->b);
  free(m);
}

int hm_reserve(hashmap_t* m, size_t capacity_pow2){
  if(!m) return -1;
  if(!is_pow2(capacity_pow2)) return -1;
  if(capacity_pow2 <= m->cap) return 0;
  return hm_rehash(m, capacity_pow2);
}

void hm_clear(hashmap_t* m){
  if(!m) return;
  for(size_t i=0;i<m->cap;i++){
    if(m->b[i].used){
      if(m->b[i].key) free(m->b[i].key);
      if(m->b[i].val) free(m->b[i].val);
    }
  }
  free(m->b);
  m->cap = m->init_cap;
  m->sz = 0;
  m->b = (bucket_t*)calloc(m->cap, sizeof(bucket_t));
}

/* Insert or replace (copies key/val). Returns 0 ok, -1 OOM/invalid. */
int hm_put(hashmap_t* m, const void* k, size_t kl, const void* v, size_t vl){
  if(!m || !k || kl==0) return -1;
  if(hm_maybe_grow(m)!=0) return -1;

  uint64_t h = m->hf(k, kl);
  bucket_t e; memset(&e, 0, sizeof(e));
  e.h = h; e.dist = 0; e.used = 1; e.klen = kl; e.vlen = vl;

  e.key = malloc(kl);
  if(!e.key) return -1;
  memcpy(e.key, k, kl);

  if(vl){
    e.val = malloc(vl);
    if(!e.val){ free(e.key); return -1; }
    memcpy(e.val, v, vl);
  } else {
    e.val = NULL;
  }

  size_t idx = idx_for(h, m->cap);
  for(;;){
    bucket_t* cur = &m->b[idx];
    if(!cur->used){
      *cur = e;
      m->sz++;
      return 0;
    }
    if(cur->h == h && cur->klen == kl && m->eq(cur->key, cur->klen, k, kl)){
      /* replace value; keep key */
      if(vl != cur->vlen){
        void* nv = vl ? realloc(cur->val, vl) : NULL;
        if(vl && !nv) return -1;
        cur->val = nv;
        cur->vlen = vl;
      }
      if(vl) memcpy(cur->val, v, vl);
      else { if(cur->val){ free(cur->val); cur->val=NULL; } cur->vlen = 0; }
      /* free temp e we allocated */
      free(e.key); if(e.val) free(e.val);
      return 0;
    }
    if(cur->dist < e.dist){
      bucket_swap(cur, &e);
    }
    idx = (idx + 1) & (m->cap - 1);
    e.dist++;
  }
}

/* Get value bytes. Returns: 0 ok; 1 buffer too small (out_len set); -1 not found */
int hm_get(hashmap_t* m, const void* k, size_t kl, void* out, size_t* out_len){
  if(!m || !k) return -1;
  uint64_t h = m->hf(k, kl);
  size_t idx = idx_for(h, m->cap);
  uint32_t dist = 0;

  for(;;){
    bucket_t* cur = &m->b[idx];
    if(!cur->used) return -1;           /* Robin Hood: empty stops search */
    if(cur->dist < dist) return -1;     /* cannot be further */
    if(cur->h == h && cur->klen == kl && m->eq(cur->key, cur->klen, k, kl)){
      if(!out_len) return 0;
      if(out && *out_len >= cur->vlen){
        if(cur->vlen) memcpy(out, cur->val, cur->vlen);
        *out_len = cur->vlen;
        return 0;
      }
      *out_len = cur->vlen;
      return 1; /* caller buffer too small */
    }
    idx = (idx + 1) & (m->cap - 1);
    dist++;
  }
}

/* Remove entry by key. Returns 0 if removed, -1 if not found. */
int hm_remove(hashmap_t* m, const void* k, size_t kl){
  if(!m || !k) return -1;
  uint64_t h = m->hf(k, kl);
  size_t idx = idx_for(h, m->cap);
  uint32_t dist = 0;

  for(;;){
    bucket_t* cur = &m->b[idx];
    if(!cur->used) return -1;
    if(cur->dist < dist) return -1;
    if(cur->h == h && cur->klen == kl && m->eq(cur->key, cur->klen, k, kl)){
      backshift_delete(m, idx);
      m->sz--;
      return 0;
    }
    idx = (idx + 1) & (m->cap - 1);
    dist++;
  }
}

size_t hm_size(const hashmap_t* m){ return m ? m->sz : 0; }
size_t hm_capacity(const hashmap_t* m){ return m ? m->cap : 0; }
double hm_load_factor(const hashmap_t* m){ return m ? (double)m->sz / (double)m->cap : 0.0; }

/* Iteration */
void hm_iter_begin(const hashmap_t* m, hm_iter_t* it){ if(!it){return;} it->m=m; it->idx=0; }

int hm_iter_next(hm_iter_t* it, const void** key, size_t* klen, const void** val, size_t* vlen){
  if(!it || !it->m) return 0;
  const hashmap_t* m = it->m;
  while(it->idx < m->cap){
    size_t i = it->idx++;
    if(m->b[i].used){
      if(key)  *key  = m->b[i].key;
      if(klen) *klen = m->b[i].klen;
      if(val)  *val  = m->b[i].val;
      if(vlen) *vlen = m->b[i].vlen;
      return 1;
    }
  }
  return 0;
}
