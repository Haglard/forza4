// =============================================
// File: src/connect4/zobrist.c
// Project: forza4
// License: MIT (c) 2025
// =============================================
#include "connect4/zobrist.h"
#include "core/bitops.h"

void c4_zobrist_init(c4_zobrist_t *z, rng_t *rng) {
    for (int c = 0; c < 2; c++)
        for (int sq = 0; sq < 49; sq++)
            z->piece[c][sq] = rng_next_u64(rng);
    z->side_to_move = rng_next_u64(rng);
}

uint64_t c4_zobrist_hash(const c4_zobrist_t *z, const c4_board_t *b) {
    uint64_t h = 0;
    for (int c = 0; c < 2; c++) {
        uint64_t bb = b->bb[c];
        while (bb) {
            int sq = bo_extract_lsb_index(&bb);
            h ^= z->piece[c][sq];
        }
    }
    if (b->side_to_move == C4_RED) h ^= z->side_to_move;
    return h;
}
