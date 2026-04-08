// =============================================
// File: src/game/connect4_adapter.c
// Project: forza4
// License: MIT (c) 2025
// =============================================
#include <string.h>
#include <stdio.h>
#include "game/connect4_adapter.h"
#include "connect4/board.h"
#include "connect4/movegen.h"
#include "connect4/eval.h"
#include "connect4/zobrist.h"
#include "core/rng.h"
#include "core/log.h"

#define C4_HIST_MAX 42

typedef struct {
    c4_board_t b;                          // MUST be first member
    uint64_t   hash_history[C4_HIST_MAX];
    int        hist_len;
} c4_state_t;

// ---- Zobrist (lazy init) ----
static c4_zobrist_t gZ;
static int gZ_inited = 0;

static void ensure_zobrist(void) {
    if (!gZ_inited) {
        rng_t r;
        const uint64_t S[4] = {
            0xF04F04F0C4C4C4C4ULL, 0x1234567890ABCDEFULL,
            0xFEDCBA9876543210ULL, 0xDEADBEEFCAFEBABEULL
        };
        rng_seed(&r, S);
        c4_zobrist_init(&gZ, &r);
        gZ_inited = 1;
    }
}

// ---- GameAPI callbacks ----

static int c4_side_cb(const game_state_t *st) {
    return ((const c4_state_t *)st)->b.side_to_move;
}

static int c4_gen_legal_cb(const game_state_t *st, game_move_t *out, int cap) {
    c4_move_list_t ml;
    int n = c4_generate_legal(&((const c4_state_t *)st)->b, &ml);
    int cnt = (n < cap) ? n : cap;
    memcpy(out, ml.data, cnt * sizeof(game_move_t));
    return cnt;
}

static int c4_is_capture_cb(const game_state_t *st, game_move_t mv) {
    (void)st; (void)mv;
    return 0; // no captures in Connect Four
}

static uint64_t c4_make_move_cb(game_state_t *st, game_move_t mv, void *undo_buf) {
    ensure_zobrist();
    c4_state_t *S = (c4_state_t *)st;
    c4_undo_t  *u = (c4_undo_t *)undo_buf;
    uint64_t key = c4_zobrist_hash(&gZ, &S->b);
    if (S->hist_len < C4_HIST_MAX)
        S->hash_history[S->hist_len++] = key;
    c4_make_move(&S->b, mv, u);
    return c4_zobrist_hash(&gZ, &S->b);
}

static uint64_t c4_unmake_move_cb(game_state_t *st, game_move_t mv, const void *undo_buf) {
    ensure_zobrist();
    c4_state_t      *S = (c4_state_t *)st;
    const c4_undo_t *u = (const c4_undo_t *)undo_buf;
    if (S->hist_len > 0) S->hist_len--;
    c4_unmake_move(&S->b, mv, u);
    return c4_zobrist_hash(&gZ, &S->b);
}

static uint64_t c4_hash_cb(const game_state_t *st) {
    ensure_zobrist();
    return c4_zobrist_hash(&gZ, &((const c4_state_t *)st)->b);
}

static int c4_is_terminal_cb(const game_state_t *st, game_result_t *out) {
    const c4_state_t *S = (const c4_state_t *)st;
    // Check if the player who JUST moved has won
    int prev = S->b.side_to_move ^ 1;
    if (c4_has_won(S->b.bb[prev])) {
        if (out) *out = GAME_RESULT_LOSS; // current player loses
        return 1;
    }
    // Board full → draw
    if (S->b.moves_played == C4_SQUARES) {
        if (out) *out = GAME_RESULT_DRAW;
        return 1;
    }
    if (out) *out = GAME_RESULT_NONE;
    return 0;
}

static unsigned c4_is_terminal_ext_cb(const game_state_t *st) {
    game_result_t r = GAME_RESULT_NONE;
    if (c4_is_terminal_cb(st, &r)) {
        if (r == GAME_RESULT_LOSS) return GAME_TERMFLAG_LOSS;
        if (r == GAME_RESULT_DRAW) return GAME_TERMFLAG_DRAW;
        if (r == GAME_RESULT_WIN)  return GAME_TERMFLAG_WIN;
    }
    return GAME_TERMFLAG_NONE;
}

static game_score_t c4_evaluate_cb(const game_state_t *st) {
    return (game_score_t)c4_eval(&((const c4_state_t *)st)->b);
}

static void c4_copy_cb(const game_state_t *src, game_state_t *dst) {
    memcpy(dst, src, sizeof(c4_state_t));
}

static GameAPI C4_API = {
    .state_size        = sizeof(c4_state_t),
    .undo_size         = sizeof(c4_undo_t),
    .side_to_move      = c4_side_cb,
    .generate_legal    = c4_gen_legal_cb,
    .generate_captures = NULL,
    .is_capture        = c4_is_capture_cb,
    .capture_score     = NULL,
    .make_move         = c4_make_move_cb,
    .unmake_move       = c4_unmake_move_cb,
    .hash              = c4_hash_cb,
    .is_terminal       = c4_is_terminal_cb,
    .is_terminal_ext   = c4_is_terminal_ext_cb,
    .evaluate          = c4_evaluate_cb,
    .copy              = c4_copy_cb,
};

const GameAPI *c4_api(void) { return &C4_API; }

int c4_init_state_str(game_state_t *st, const char *pos) {
    c4_state_t *S = (c4_state_t *)st;
    S->hist_len = 0;
    if (!pos || strncmp(pos, "startpos", 8) == 0) {
        c4_board_set_startpos(&S->b);
        return 0;
    }
    return c4_board_from_str(&S->b, pos);
}

void c4_move_to_str(game_move_t m, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    snprintf(out, out_sz, "%c", 'a' + c4_move_col(m));
}

const c4_board_t *c4_state_as_board(const game_state_t *st) {
    return &((const c4_state_t *)st)->b;
}
