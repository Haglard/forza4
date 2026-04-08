#pragma once
#include <stdint.h>
#include "game/api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct game_state_s {
    int      heap;      /* pietre rimanenti */
    int      max_take;  /* massimo rimovibile per mossa */
    int      stm;       /* side to move: 0/1 */
    uint64_t key;       /* hash semplice per test */
} nim_state_t;

/* Questi due simboli DEVONO essere visibili (non static) */
const GameAPI* nim_api(void);
void nim_init(nim_state_t* s, int heap, int max_take, int stm);

#ifdef __cplusplus
}
#endif
