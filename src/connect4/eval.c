// =============================================
// File: src/connect4/eval.c
// Project: forza4
// License: MIT (c) 2025
//
// Evaluation: window counting + centre bonus.
// Precomputes 69 windows of 4 squares on the 7×6 board.
// =============================================
#include "connect4/eval.h"
#include "core/bitops.h"

// 69 windows: 24 horizontal + 21 vertical + 12 diag↗ + 12 diag↘
#define MAX_WINDOWS 69

static uint64_t WIN_MASKS[MAX_WINDOWS];
static int      N_WINDOWS = 0;

static void init_windows(void) {
    if (N_WINDOWS) return;
    // directions: {dc, dr}
    static const int DIRS[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
    for (int d = 0; d < 4; d++) {
        int dc = DIRS[d][0], dr = DIRS[d][1];
        for (int c = 0; c < C4_COLS; c++) {
            for (int r = 0; r < C4_ROWS; r++) {
                int ce = c + 3*dc, re = r + 3*dr;
                if (ce < 0 || ce >= C4_COLS || re < 0 || re >= C4_ROWS) continue;
                uint64_t mask = 0;
                for (int k = 0; k < 4; k++)
                    mask |= 1ULL << c4_sq(c + k*dc, r + k*dr);
                WIN_MASKS[N_WINDOWS++] = mask;
            }
        }
    }
}

int c4_eval(const c4_board_t *b) {
    init_windows();
    int stm = b->side_to_move, opp = stm ^ 1;
    int score = 0;

    for (int i = 0; i < N_WINDOWS; i++) {
        uint64_t m = WIN_MASKS[i];
        int ns = bo_popcount64(b->bb[stm] & m);
        int no = bo_popcount64(b->bb[opp] & m);
        if (no == 0 && ns > 0) {
            if      (ns == 3) score += 50;
            else if (ns == 2) score += 10;
        } else if (ns == 0 && no > 0) {
            if      (no == 3) score -= 50;
            else if (no == 2) score -= 10;
        }
    }

    // Centre column bonus (column 3)
    for (int r = 0; r < C4_ROWS; r++) {
        if (b->bb[stm] & (1ULL << c4_sq(3, r))) score += 6;
        if (b->bb[opp] & (1ULL << c4_sq(3, r))) score -= 6;
    }
    return score;
}
