// =============================================
// File: include/game/connect4_adapter.h
// Project: forza4
// License: MIT (c) 2025
// =============================================
#pragma once
#include "game/api.h"
#include "connect4/board.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns the singleton GameAPI for Connect Four.
const GameAPI *c4_api(void);

// Initialize a game state from a string ("startpos" or compact string).
int c4_init_state_str(game_state_t *st, const char *pos);

// Format a move as "a"-"g" (column letter).
void c4_move_to_str(game_move_t m, char *out, size_t out_sz);

// Direct cast: state → board (board is first member).
const c4_board_t *c4_state_as_board(const game_state_t *st);

// Re-seed Zobrist with a time-based random seed (call at game start to vary play).
void c4_new_game_randomize(void);

#ifdef __cplusplus
}
#endif
