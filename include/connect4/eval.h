// =============================================
// File: include/connect4/eval.h
// Project: forza4
// License: MIT (c) 2025
//
// Static evaluation — side-to-move POV (positive = good).
// Counts open windows of 2 and 3 pieces; centre bonus.
// =============================================
#pragma once
#include "connect4/board.h"

#ifdef __cplusplus
extern "C" {
#endif

int c4_eval(const c4_board_t *b);

#ifdef __cplusplus
}
#endif
