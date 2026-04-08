// =============================================
// File: src/connect4/movegen.c
// Project: forza4
// License: MIT (c) 2025
// =============================================
#include "connect4/movegen.h"

static const int COL_ORDER[C4_COLS] = {3, 2, 4, 1, 5, 0, 6}; // centre-first

int c4_generate_legal(const c4_board_t *b, c4_move_list_t *ml) {
    ml->n = 0;
    for (int i = 0; i < C4_COLS; i++) {
        int col = COL_ORDER[i];
        if (b->height[col] < C4_ROWS)
            ml->data[ml->n++] = c4_move_make(col);
    }
    return ml->n;
}

void c4_make_move(c4_board_t *b, game_move_t mv, c4_undo_t *u) {
    int col = c4_move_col(mv);
    int row = b->height[col];
    int sq  = c4_sq(col, row);
    if (u) u->col = col;
    b->bb[b->side_to_move] |= 1ULL << sq;
    b->height[col]++;
    b->moves_played++;
    b->side_to_move ^= 1;
}

void c4_unmake_move(c4_board_t *b, game_move_t mv, const c4_undo_t *u) {
    (void)u;
    int col = c4_move_col(mv);
    b->side_to_move ^= 1;
    b->height[col]--;
    int row = b->height[col];
    int sq  = c4_sq(col, row);
    b->bb[b->side_to_move] &= ~(1ULL << sq);
    b->moves_played--;
}
