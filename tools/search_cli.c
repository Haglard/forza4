// =============================================
// File: tools/search_cli.c — forza4
// Usage: search_cli [position] [time_ms]
// =============================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/api.h"
#include "game/connect4_adapter.h"
#include "connect4/board.h"
#include "connect4/movegen.h"
#include "engine/search.h"

int main(int argc, char *argv[]) {
    const char *pos_str = (argc >= 2) ? argv[1] : "startpos";
    int time_ms         = (argc >= 3) ? atoi(argv[2]) : 2000;

    const GameAPI *api = c4_api();
    game_state_t  *st  = calloc(1, api->state_size);
    if (!st) return 1;

    if (c4_init_state_str(st, pos_str) != 0) {
        fprintf(stderr, "Invalid position\n"); free(st); return 1;
    }

    const c4_board_t *b = c4_state_as_board(st);
    char buf[64];
    c4_board_to_str(b, buf, sizeof(buf));
    printf("Position : %s\n", buf);
    printf("Side     : %s\n", b->side_to_move == C4_YELLOW ? "Yellow" : "Red");

    c4_move_list_t ml;
    int n = c4_generate_legal(b, &ml);
    printf("Legal    : %d moves\n\n", n);

    if (n == 0) { printf("No legal moves.\n"); free(st); return 0; }

    volatile int stop = 0;
    search_params_t sp = {0};
    sp.use_time = 1; sp.time_ms = time_ms; sp.max_depth = 99;
    sp.use_qsearch = 0; sp.tt_size_mb = 64; sp.stop = &stop;

    search_result_t res = {0};
    search_root(api, st, &sp, &res);

    char mv[4];
    c4_move_to_str(res.best_move, mv, sizeof(mv));
    printf("Best move : %s\n", mv);
    printf("Score     : %+.2f\n", res.score / 100.0);
    printf("Depth     : %d\n",    res.depth_searched);
    printf("Nodes     : %llu\n",  (unsigned long long)res.nodes);
    printf("NPS       : %.0f\n",  res.nps);

    free(st);
    return 0;
}
