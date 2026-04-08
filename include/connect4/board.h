// =============================================
// File: include/connect4/board.h
// Project: forza4 (Connect Four engine)
// License: MIT (c) 2025
//
// Board layout — bitboard with gravity
// -------------------------------------
// 7 columns (a-g), 6 rows (1-6, row 1 = bottom).
// Bit index: column * 7 + row  (row 0..5, sentinel at row 6).
//
//  col:  0  1  2  3  4  5  6
//  row5: 5 12 19 26 33 40 47
//  row4: 4 11 18 25 32 39 46
//  row3: 3 10 17 24 31 38 45
//  row2: 2  9 16 23 30 37 44
//  row1: 1  8 15 22 29 36 43
//  row0: 0  7 14 21 28 35 42
//  sent: 6 13 20 27 34 41 48  ← always 0 (win detection sentinel)
//
// Players: C4_YELLOW = 0 (first), C4_RED = 1
// =============================================
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define C4_YELLOW   0
#define C4_RED      1
#define C4_COLS     7
#define C4_ROWS     6
#define C4_SQUARES  42

typedef struct {
    uint64_t bb[2];         // bitboard per player
    int      height[C4_COLS]; // next empty row in each column (0..6; 6 = full)
    int      side_to_move;  // C4_YELLOW or C4_RED
    int      moves_played;  // 0..42
} c4_board_t;

// Lifecycle
void c4_board_clear(c4_board_t *b);
void c4_board_set_startpos(c4_board_t *b);

// Square index helper: col in [0,6], row in [0,5]
static inline int c4_sq(int col, int row) { return col * 7 + row; }

// Win-detection (checks if player bb has 4 in a row)
int c4_has_won(uint64_t bb);

// Serialization (compact string, column by column from bottom)
// Format: "Y" or "R" (side to move) + 42 chars: '.' empty, 'Y' yellow, 'R' red
int c4_board_from_str(c4_board_t *b, const char *s);
int c4_board_to_str(const c4_board_t *b, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif
