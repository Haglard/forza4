// =============================================
// File: include/connect4/movegen.h
// Project: forza4
// License: MIT (c) 2025
//
// Moves in Connect Four are simply column drops (0..6).
// A move is legal iff height[col] < C4_ROWS.
// =============================================
#pragma once
#include "connect4/board.h"
#include "game/api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C4_MAX_MOVES  7   // at most one per column
#define C4_COL_ORDER  {3,2,4,1,5,0,6}  // centre-first move ordering

typedef struct {
    game_move_t data[C4_MAX_MOVES];
    int         n;
} c4_move_list_t;

// Move encoding: just the column number (0..6)
static inline game_move_t c4_move_make(int col)  { return (game_move_t)col; }
static inline int         c4_move_col(game_move_t m) { return (int)(m & 7); }

// Legal move generation (moves ordered centre-first)
int c4_generate_legal(const c4_board_t *b, c4_move_list_t *ml);

// Undo record (everything needed to reverse a move)
typedef struct {
    int col;  // column of the drop (redundant with move, but kept for safety)
} c4_undo_t;

// Make / unmake
void c4_make_move(c4_board_t *b, game_move_t mv, c4_undo_t *u);
void c4_unmake_move(c4_board_t *b, game_move_t mv, const c4_undo_t *u);

#ifdef __cplusplus
}
#endif
