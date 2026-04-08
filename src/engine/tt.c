#include "engine/tt.h"
#include <stdlib.h>
#include <string.h>

static inline uint32_t idx_of(const tt_t* tt, uint64_t key){
    return (uint32_t)(key & (tt->size - 1));
}

int tt_init(tt_t* tt, uint32_t entries){
    if(!tt || entries==0) return -1;
    /* round up to power of two (simple) */
    uint32_t n=1; while(n<entries) n<<=1;
    tt->size = n;
    tt->table = (tt_entry_t*)calloc(tt->size, sizeof(tt_entry_t));
    tt->gen = 1;
    return tt->table ? 0 : -1;
}

void tt_free(tt_t* tt){
    if(!tt) return;
    free(tt->table);
    tt->table=NULL; tt->size=0; tt->gen=0;
}

void tt_clear(tt_t* tt){
    if(!tt || !tt->table) return;
    memset(tt->table, 0, tt->size*sizeof(tt_entry_t));
    tt->gen++;
}

void tt_store(tt_t* tt, uint64_t key, int depth, int score, tt_flag_t flag, game_move_t move){
    if(!tt || !tt->table) return;
    uint32_t i = idx_of(tt, key);
    tt_entry_t* e = &tt->table[i];
    /* replacement: prefer higher depth or empty slot; else overwrite same index */
    if(e->key==0 || e->key==key || depth >= e->depth){
        e->key   = key;
        e->move  = move;
        e->score = (int16_t)score;
        e->depth = (int8_t)(depth > 127 ? 127 : depth);
        e->flag  = (uint8_t)flag;
        e->gen   = tt->gen;
    }
}

int tt_probe(const tt_t* tt, uint64_t key, tt_entry_t* out){
    if(!tt || !tt->table) return 0;
    uint32_t i = idx_of(tt, key);
    const tt_entry_t* e = &tt->table[i];
    if(e->key == key && e->flag != TT_NONE){
        if(out) *out = *e;
        return 1;
    }
    return 0;
}

/* Mate score helpers: store closer-to-mate as larger magnitude */
#define MATE_SCORE 32000
int tt_mate_to_store(int score, int ply){
    if(score >  MATE_SCORE-1000) score += ply;     /* mate for us */
    if(score < -MATE_SCORE+1000) score -= ply;     /* mate for them */
    return score;
}
int tt_mate_from_store(int score, int ply){
    if(score >  MATE_SCORE-1000) score -= ply;
    if(score < -MATE_SCORE+1000) score += ply;
    return score;
}
