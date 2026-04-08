// =============================================
// File: tools/selfplay_cli.c — forza4
// Usage: selfplay_cli [time_ms]
// =============================================
#include <stdio.h>
#include <stdlib.h>
#include "game/api.h"
#include "game/connect4_adapter.h"
#include "connect4/board.h"
#include "engine/search.h"

int main(int argc, char *argv[]) {
    int time_ms = (argc >= 2) ? atoi(argv[1]) : 500;
    const GameAPI *api = c4_api();
    game_state_t  *st  = calloc(1, api->state_size);
    if (!st) return 1;
    c4_init_state_str(st, "startpos");

    printf("=== Vibe Forza 4 — self-play (%d ms/mossa) ===\n\n", time_ms);

    volatile int stop = 0;
    for (int half = 0; half < 42; half++) {
        game_result_t result = GAME_RESULT_NONE;
        if (api->is_terminal(st, &result)) {
            const c4_board_t *b = c4_state_as_board(st);
            int prev = b->side_to_move ^ 1;
            printf("\n--- Partita finita ---\n");
            if (result == GAME_RESULT_LOSS)
                printf("Vince: %s\n", prev == C4_YELLOW ? "Giallo" : "Rosso");
            else
                printf("Pareggio!\n");
            break;
        }

        search_params_t sp = {0};
        sp.use_time = 1; sp.time_ms = time_ms; sp.max_depth = 99;
        sp.use_qsearch = 0; sp.tt_size_mb = 64; sp.stop = &stop;

        search_result_t res = {0};
        search_root(api, st, &sp, &res);
        if (!res.best_move) break;

        const c4_board_t *b = c4_state_as_board(st);
        char mv[4]; c4_move_to_str(res.best_move, mv, sizeof(mv));
        printf("%2d. [%s] col-%s  score=%+.2f  depth=%d\n",
               half/2+1,
               b->side_to_move == C4_YELLOW ? "G" : "R",
               mv, res.score/100.0, res.depth_searched);

        void *undo = calloc(1, api->undo_size);
        api->make_move(st, res.best_move, undo);
        free(undo);
    }
    free(st);
    return 0;
}
