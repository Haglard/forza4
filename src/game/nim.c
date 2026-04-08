// =============================================
// File: src/game/nim.c
// Purpose: Nim mock per Game API + logging dettagliato
// License: MIT (c) 2025
// =============================================
#include "game/nim.h"
#include "core/log.h"
#include <string.h>

/* move encoding: lower 16 bits = stones taken (1..max_take) */
static inline game_move_t mv_take(int n){ return (game_move_t)(uint16_t)n; }
static inline int mv_amount(game_move_t m){ return (int)(m & 0xFFFF); }

/* --- aggiunte per compatibilità con ordering --- */
static int nim_is_capture(const game_state_t* st, game_move_t m){
    (void)st;
    /* per il mock: consideriamo "cattura" qualsiasi mossa con bit0 = 1 */
    return ((uint64_t)m) & 1ull ? 1 : 0;
}
static int nim_capture_score(const game_state_t* st, game_move_t m){
    (void)st; (void)m;
    /* tutte le catture hanno lo stesso peso nel mock */
    return 1;
}


/* piccolo hash per i test */
static inline uint64_t hmix(uint64_t x){ x ^= x>>33; x*=0xff51afd7ed558ccdULL; x^=x>>33; x*=0xc4ceb9fe1a85ec53ULL; x^=x>>33; return x; }
static uint64_t nim_rehash(const nim_state_t* s){
    return hmix(((uint64_t)s->heap<<32) ^ ((uint64_t)s->max_take<<16) ^ (uint64_t)s->stm);
}

static int nim_side_to_move(const game_state_t* st){
    const nim_state_t* s=(const nim_state_t*)st;
    return s->stm;
}

static int nim_generate_legal(const game_state_t* st, game_move_t* out, int cap){
    const nim_state_t* s=(const nim_state_t*)st;
    if(s->heap <= 0){
        LOGD("nim_generate_legal: heap=0 => no moves");
        return 0;
    }
    int max = (s->heap < s->max_take ? s->heap : s->max_take);
    int n=0; 
    for(int k=1;k<=max && n<cap;k++) out[n++] = mv_take(k);

    LOGI("nim_generate_legal: stm=%d heap=%d max_take=%d -> moves=%d",
         s->stm, s->heap, s->max_take, n);
    LOGD("nim_generate_legal: list%s", ""); /* segnaposto per LOGD in loop */
    for(int i=0;i<n;i++){
        LOGD("  - take %d", mv_amount(out[i]));
    }
    return n;
}

static uint64_t nim_make_move(game_state_t* st, game_move_t m, void* undo){
    (void)undo;
    nim_state_t* s=(nim_state_t*)st;
    int take = mv_amount(m);
    int prev_heap = s->heap;
    int prev_stm  = s->stm;

    s->heap -= take;
    s->stm ^= 1;
    s->key = nim_rehash(s);

    LOGI("nim_make_move: stm=%d took=%d heap %d -> %d (next stm=%d) key=0x%016llx",
         prev_stm, take, prev_heap, s->heap, s->stm,
         (unsigned long long)s->key);
    return s->key;
}

static uint64_t nim_unmake_move(game_state_t* st, game_move_t m, const void* undo){
    (void)undo;
    nim_state_t* s=(nim_state_t*)st;
    int take = mv_amount(m);
    int prev_heap = s->heap;
    int prev_stm  = s->stm;

    s->stm ^= 1;
    s->heap += take;
    s->key = nim_rehash(s);

    LOGI("nim_unmake_move: restoring move (take=%d): heap %d -> %d (stm %d -> %d) key=0x%016llx",
         take, prev_heap, s->heap, prev_stm, s->stm,
         (unsigned long long)s->key);
    return s->key;
}

static uint64_t nim_hash(const game_state_t* st){
    const nim_state_t* s=(const nim_state_t*)st;
    return s->key;
}

static int nim_is_terminal(const game_state_t* st, game_result_t* out){
    const nim_state_t* s=(const nim_state_t*)st;
    if(s->heap == 0){
        if(out) *out = GAME_RESULT_LOSS; /* tocca a me con heap=0 => ho perso */
        LOGI("nim_is_terminal: heap=0 -> terminal (LOSS for stm=%d)", s->stm);
        return 1;
    }
    return 0;
}

static unsigned nim_is_terminal_ext(const game_state_t* st){
    const nim_state_t* s=(const nim_state_t*)st;
    return (s->heap==0) ? GAME_TERMFLAG_LOSS : GAME_TERMFLAG_NONE;
}

static game_score_t nim_evaluate(const game_state_t* st){
    const nim_state_t* s=(const nim_state_t*)st;
    /* losing se heap % (max_take+1) == 0 */
    int losing = (s->heap % (s->max_take+1) == 0);
    game_score_t sc = losing ? -100 : +100;
    LOGD("nim_evaluate: stm=%d heap=%d max_take=%d -> score=%d",
         s->stm, s->heap, s->max_take, (int)sc);
    return sc;
}

static void nim_copy(const game_state_t* src, game_state_t* dst){
    memcpy(dst, src, sizeof(nim_state_t));
}


/* TABELLA API (simboli interni) */
static GameAPI NIM_API = {
    .state_size        = sizeof(nim_state_t),
    .undo_size         = sizeof(int),

    .side_to_move      = nim_side_to_move,

    .generate_legal    = nim_generate_legal,

//    .generate_captures = nim_generate_captures, /* se non l’hai, metti NULL */
    .generate_captures = NULL, /* se non l’hai, metti NULL */
    .is_capture        = nim_is_capture,
    .capture_score     = nim_capture_score,     /* opzionale */

    .make_move         = nim_make_move,
    .unmake_move       = nim_unmake_move,

    .hash              = nim_hash,
    .is_terminal       = nim_is_terminal,
    .is_terminal_ext   = nim_is_terminal_ext,
    .evaluate          = nim_evaluate,

    .copy              = nim_copy
};



/* --- SIMBOLI PUBBLICI --- */
const GameAPI* nim_api(void){ return &NIM_API; }

void nim_init(nim_state_t* s, int heap, int max_take, int stm){
    s->heap = heap;
    s->max_take = max_take;
    s->stm = stm & 1;
    s->key = nim_rehash(s);
    LOGI("nim_init: heap=%d max_take=%d stm=%d key=0x%016llx",
         heap, max_take, s->stm, (unsigned long long)s->key);
}
