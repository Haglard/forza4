#include "core/vec.h"
#include <stdlib.h>
#include <string.h>
int vec_init(vec_t* v, size_t es){ v->data=NULL; v->len=0; v->cap=0; v->elem_size=es; return 0; }
void vec_free(vec_t* v){ free(v->data); v->data=NULL; v->len=v->cap=0; }
int vec_reserve(vec_t* v, size_t nc){ if(nc<=v->cap) return 0; void* p=realloc(v->data, nc*v->elem_size); if(!p) return -1; v->data=p; v->cap=nc; return 0; }
int vec_push(vec_t* v, const void* e){ if(v->len==v->cap){ size_t nc=v->cap? v->cap*2:8; if(vec_reserve(v,nc)) return -1; } memcpy((char*)v->data+v->len*v->elem_size,e,v->elem_size); v->len++; return 0; }
void* vec_at(vec_t* v, size_t i){ return (char*)v->data + i*v->elem_size; }
