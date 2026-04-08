// =============================================
// File: include/connect4/zobrist.h
// Project: forza4
// License: MIT (c) 2025
// =============================================
#pragma once
#include <stdint.h>
#include "connect4/board.h"
#include "core/rng.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t piece[2][49];  // [color][col*7+row] — 42 valid squares (+ sentinel)
    uint64_t side_to_move;
} c4_zobrist_t;

void     c4_zobrist_init(c4_zobrist_t *z, rng_t *rng);
uint64_t c4_zobrist_hash(const c4_zobrist_t *z, const c4_board_t *b);

#ifdef __cplusplus
}
#endif
