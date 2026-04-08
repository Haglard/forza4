// =============================================
// File: src/connect4/board.c
// Project: forza4
// License: MIT (c) 2025
// =============================================
#include <string.h>
#include <stdio.h>
#include "connect4/board.h"
#include "core/log.h"

void c4_board_clear(c4_board_t *b) {
    memset(b, 0, sizeof(*b));
    b->side_to_move = C4_YELLOW;
}

void c4_board_set_startpos(c4_board_t *b) {
    c4_board_clear(b);
    LOGI("c4_board: starting position set (empty board)");
}

// ================================================================
// Win detection — standard bitboard shifts
//   horizontal : shift 7  (adjacent columns, same row)
//   vertical   : shift 1  (adjacent rows, same column)
//   diagonal ↗ : shift 8  (col+1, row+1)
//   diagonal ↘ : shift 6  (col+1, row-1)
// ================================================================
int c4_has_won(uint64_t bb) {
    uint64_t h = bb & (bb >> 7);
    if (h & (h >> 14)) return 1;
    uint64_t v = bb & (bb >> 1);
    if (v & (v >>  2)) return 1;
    uint64_t d1 = bb & (bb >> 8);
    if (d1 & (d1 >> 16)) return 1;
    uint64_t d2 = bb & (bb >> 6);
    if (d2 & (d2 >> 12)) return 1;
    return 0;
}

// ================================================================
// Serialization
// Format: "Y" or "R" (side) followed by 42 chars column-major,
//         row 0 first (bottom), using '.' 'Y' 'R'.
// Example: "Y" + 42 chars
// ================================================================
int c4_board_from_str(c4_board_t *b, const char *s) {
    if (!s) return -1;
    c4_board_clear(b);
    if (*s == 'Y' || *s == 'y')      b->side_to_move = C4_YELLOW;
    else if (*s == 'R' || *s == 'r') b->side_to_move = C4_RED;
    else return -1;
    s++;
    // Expect 42 chars
    for (int col = 0; col < C4_COLS && *s; col++) {
        for (int row = 0; row < C4_ROWS && *s; row++, s++) {
            int sq = c4_sq(col, row);
            if (*s == 'Y' || *s == 'y') {
                b->bb[C4_YELLOW] |= 1ULL << sq;
                b->height[col] = row + 1;
                b->moves_played++;
            } else if (*s == 'R' || *s == 'r') {
                b->bb[C4_RED] |= 1ULL << sq;
                b->height[col] = row + 1;
                b->moves_played++;
            }
        }
    }
    return 0;
}

int c4_board_to_str(const c4_board_t *b, char *out, size_t out_sz) {
    if (!out || out_sz < 44) return -1;
    int n = 0;
    out[n++] = (b->side_to_move == C4_YELLOW) ? 'Y' : 'R';
    for (int col = 0; col < C4_COLS; col++) {
        for (int row = 0; row < C4_ROWS; row++) {
            uint64_t bit = 1ULL << c4_sq(col, row);
            if      (b->bb[C4_YELLOW] & bit) out[n++] = 'Y';
            else if (b->bb[C4_RED]    & bit) out[n++] = 'R';
            else                              out[n++] = '.';
        }
    }
    out[n] = '\0';
    return n;
}
