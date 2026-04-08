// =============================================
// File: src/engine/search.c
// Purpose: Ricerca negamax + alpha-beta, ID, qsearch, TT
// Note: metriche runtime + History (aging/scale) + Killer (quiet) + LMR + Aspiration
// License: MIT (c) 2025
// =============================================
#include "engine/search.h"
#include "engine/tt.h"
#include "game/api.h"
#include "core/log.h"
#include "core/time.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>

#define CLAMP_ALPHA  (-SEARCH_SCORE_MATE)
#define CLAMP_BETA   ( SEARCH_SCORE_MATE)
#define MAX_PLY      128
#define HIST_SIZE    (1u<<16)
#define HIST_MAX     10000000

typedef struct {
    const GameAPI* api;
    uint64_t  start_ns;
    uint64_t  budget_ns;
    volatile int* stop_ext;
    int       stopped;
    int       verbose;

    int       use_tt;
    int       use_qsearch;

    tt_t      tt;
    game_move_t killers[2][MAX_PLY];
    int         history[2][HIST_SIZE];

    // ===== METRICHE =====
    uint64_t qnodes;
    uint64_t tt_probes;
    uint64_t tt_hits;
    uint64_t tt_stores;
    uint64_t cutoffs;

    // ===== STATO HEURISTICHE =====
    int history_iter; /* contatore iterazioni (ID) per aging periodico */

    // ===== CONFIG DERIVATA DA search_params_t =====
    struct {
        // History
        int hist_decay_shift;   // default 1
        int hist_bonus_scale;   // default 1

        // LMR
        int enable_lmr;         // default 1
        int lmr_min_depth;      // default 3
        int lmr_move_threshold; // default 4 (0-based index)
        int lmr_base_reduction; // default 1

        // Aspiration
        int enable_asp;         // default 1
        int asp_delta_start;    // default 30 cp
        int asp_max_retries;    // default 4
        int asp_grow_x2;        // default 1 (raddoppio)

        // TT
        unsigned tt_entries;    // default 1<<18 (~ fissa pregressa)
    } cfg;
} search_ctx_t;

/* ---- helpers ---- */
static inline int should_stop(const search_ctx_t* cx){
    if (cx->stop_ext && *cx->stop_ext) return 1;
    if (cx->budget_ns){
        uint64_t now = time_now_ns();
        if (now - cx->start_ns >= cx->budget_ns) return 1;
    }
    return 0;
}

static inline unsigned hidx(game_move_t m){
    uint64_t x = (uint64_t)m;
    x ^= x>>33; x*=0xff51afd7ed558ccdULL; x^=x>>33; x*=0xc4ceb9fe1a85ec53ULL; x^=x>>33;
    return (unsigned)(x & (HIST_SIZE-1));
}

static inline void history_clamp(int* x){
    if (*x > HIST_MAX) *x = HIST_MAX;
    else if (*x < -HIST_MAX) *x = -HIST_MAX;
}

static inline int is_quiet_move(const GameAPI* api, game_state_t* st, game_move_t m){
    /* Killer e history hanno più senso per quiet; se non abbiamo is_capture, assumiamo quiet. */
    if (api->is_capture && api->is_capture(st, m)) return 0;
    return 1;
}

static inline int hist_bonus(const search_ctx_t* cx, int depth){
    /* bonus ∝ depth^2, scalato */
    int b = depth * depth;
    if (cx->cfg.hist_bonus_scale > 1) b *= cx->cfg.hist_bonus_scale;
    return b;
}

static void history_age(search_ctx_t* cx){
    int shift = (cx->cfg.hist_decay_shift <= 0) ? 1 : cx->cfg.hist_decay_shift;
    for (int s=0; s<2; ++s){
        for (int i=0; i<HIST_SIZE; ++i){
            cx->history[s][i] >>= shift;
        }
    }
}

static void killers_reset_ply(search_ctx_t* cx, int max_ply){
    for (int p=0; p<max_ply; ++p){
        cx->killers[0][p] = 0;
        cx->killers[1][p] = 0;
    }
}

/* Ordina: TT > killer > catture (via capture_score) > history */
static void score_moves(search_ctx_t* cx, game_state_t* st, int ply,
                        game_move_t* mv, int n, game_move_t tt_move, int* out_sc){
    int side = cx->api->side_to_move(st) & 1;
    for(int i=0;i<n;i++){
        int s = 0;
        if (mv[i]==tt_move) s = 1000000000;
        else if (mv[i]==cx->killers[0][ply]) s = 900000000;
        else if (mv[i]==cx->killers[1][ply]) s = 800000000;
        else if (cx->api->capture_score && cx->api->is_capture && cx->api->is_capture(st, mv[i])) {
            s = 700000000 + cx->api->capture_score(st, mv[i]);
        } else {
            s = cx->history[side][hidx(mv[i])];
        }
        out_sc[i] = s;
    }
}
static void order_moves_by_scores(game_move_t* mv, int* sc, int n){
    for(int i=1;i<n;i++){
        game_move_t m = mv[i]; int s = sc[i]; int j=i-1;
        while(j>=0 && sc[j] < s){ sc[j+1]=sc[j]; mv[j+1]=mv[j]; j--; }
        sc[j+1]=s; mv[j+1]=m;
    }
}

/* ===== LMR ===== */
static inline int lmr_should_reduce(const search_ctx_t* cx,
                                    game_state_t* st,
                                    int depth, int move_index,
                                    game_move_t m, game_move_t tt_move,
                                    game_move_t k0, game_move_t k1)
{
    if (!cx->cfg.enable_lmr) return 0;
    if (depth < cx->cfg.lmr_min_depth) return 0;
    if (move_index < cx->cfg.lmr_move_threshold) return 0;
    if (m == tt_move || m == k0 || m == k1) return 0;              /* non ridurre TT/killer */
    if (cx->api->is_capture && cx->api->is_capture(st, m)) return 0;/* riduci solo quiet    */
    return 1;
}

static inline int lmr_compute_R(const search_ctx_t* cx, int depth, int move_index){
    int R = cx->cfg.lmr_base_reduction;
    if (depth >= 6) R += 1;
    if (move_index >= 10) R += 1;
    if (R > depth-1) R = depth-1;
    if (R < 1) R = 1;
    return R;
}

/* ========== Quiescence Search ========== */
static game_score_t qsearch(search_ctx_t* cx, game_state_t* st, int ply,
                            game_score_t alpha, game_score_t beta,
                            uint64_t* nodes)
{
    if (should_stop(cx)) { cx->stopped = 1; return alpha; }
    (*nodes)++;
    cx->qnodes++; // metrica qnodes

    /* Terminal handling */
    game_result_t term = GAME_RESULT_NONE;
    if (cx->api->is_terminal(st, &term)) {
        switch (term) {
            case GAME_RESULT_WIN:  return  SEARCH_SCORE_MATE - ply;
            case GAME_RESULT_LOSS: return -SEARCH_SCORE_MATE + ply;
            case GAME_RESULT_DRAW: return  SEARCH_SCORE_DRAW;
            default: return SEARCH_SCORE_DRAW;
        }
    }

    /* Stand-pat */
    game_score_t stand_pat = cx->api->evaluate(st);
    if (stand_pat >= beta) return stand_pat;
    if (stand_pat > alpha) alpha = stand_pat;

    /* Se non abbiamo generator tattico, stop qui */
    if (!cx->api->generate_captures) return alpha;

    /* TT probe (facoltativa a depth=0) */
    game_move_t tt_move = 0;
    if (cx->use_tt) {
        tt_entry_t e;
        uint64_t key = cx->api->hash(st);
        cx->tt_probes++;
        if (tt_probe(&cx->tt, key, &e)) {
            cx->tt_hits++;
            int score = tt_mate_from_store(e.score, ply);
            if (e.flag == TT_EXACT) return score; /* posizione quieta già risolta */
            tt_move = e.move;
        }
    }

    /* Genera solo catture */
    game_move_t moves[256];
    int n = cx->api->generate_captures(st, moves, 256);
    if (n == 0) return alpha;

    /* Ordering minimale: tt-move prima; eventuale score cattura */
    int scores[256]; 
    for (int i=0;i<n;i++) {
        int s = (moves[i]==tt_move) ? 1000000000 : 0;
        if (cx->api->capture_score && cx->api->is_capture && cx->api->is_capture(st, moves[i])){
            s += cx->api->capture_score(st, moves[i]);
        }
        scores[i] = s;
    }
    order_moves_by_scores(moves, scores, n);

    for (int i=0;i<n;i++) {
        if (should_stop(cx)) { cx->stopped = 1; break; }
        void* undo = malloc(cx->api->undo_size);
        uint64_t k2 = cx->api->make_move(st, moves[i], undo); (void)k2;

        game_score_t score = -qsearch(cx, st, ply+1, -beta, -alpha, nodes);

        (void)cx->api->unmake_move(st, moves[i], undo);
        free(undo);

        if (cx->stopped) break;

        if (score >= beta) {
            /* TT store (lower bound) */
            if (cx->use_tt) {
                uint64_t key = cx->api->hash(st);
                tt_store(&cx->tt, key, 0, tt_mate_to_store(score, ply), TT_LOWER, moves[i]);
                cx->tt_stores++;
            }
            return score;
        }
        if (score > alpha) alpha = score;
    }

    /* TT store exact a fine qsearch */
    if (cx->use_tt) {
        uint64_t key = cx->api->hash(st);
        tt_store(&cx->tt, key, 0, tt_mate_to_store(alpha, ply), TT_EXACT, 0);
        cx->tt_stores++;
    }

    return alpha;
}

/* ========== Negamax + Alpha-Beta con TT/ordering + qsearch (+ LMR) ========== */
static game_score_t negamax_ab(search_ctx_t* cx, game_state_t* st, int depth, int ply,
                               game_score_t alpha, game_score_t beta,
                               uint64_t* nodes,
                               game_move_t* pv, int* pv_len, int pv_cap)
{
    if (should_stop(cx)) { cx->stopped = 1; if (pv_len) *pv_len = 0; return alpha; }
    (*nodes)++;

    /* Terminal? */
    game_result_t term = GAME_RESULT_NONE;
    if (cx->api->is_terminal(st, &term)) {
        switch (term) {
            case GAME_RESULT_WIN:  return  SEARCH_SCORE_MATE - ply;
            case GAME_RESULT_LOSS: return -SEARCH_SCORE_MATE + ply;
            case GAME_RESULT_DRAW: return  SEARCH_SCORE_DRAW;
            default: return SEARCH_SCORE_DRAW;
        }
    }

    /* Depth 0 -> quiescence (se abilitata), altrimenti eval */
    if (depth <= 0) {
        if (cx->use_qsearch) return qsearch(cx, st, ply, alpha, beta, nodes);
        return cx->api->evaluate(st);
    }

    /* TT probe */
    game_move_t tt_move = 0;
    if (cx->use_tt) {
        tt_entry_t e;
        uint64_t key = cx->api->hash(st);
        cx->tt_probes++;
        if (tt_probe(&cx->tt, key, &e)) {
            cx->tt_hits++;
            int score = tt_mate_from_store(e.score, ply);
            if (e.depth >= depth) {
                if (e.flag == TT_EXACT) return score;
                else if (e.flag == TT_LOWER && score > alpha) alpha = score;
                else if (e.flag == TT_UPPER && score < beta)  beta  = score;
                if (alpha >= beta) return score;
            }
            tt_move = e.move;
        }
    }

    /* >>> FIX: memorizza alpha originale (post TT-probe) per la scelta del flag TT */
    game_score_t alpha_orig = alpha;

    /* Genera mosse */
    game_move_t moves[256];
    int n = cx->api->generate_legal(st, moves, 256);
    if (n == 0) {
        // No legal moves: come in terminal (mate/stalemate) ma con distanza dal mate
        return -SEARCH_SCORE_MATE + ply;
    }

    /* Ordering */
    int scores[256];
    score_moves(cx, st, ply, moves, n, tt_move, scores);
    order_moves_by_scores(moves, scores, n);

    game_score_t best = SHRT_MIN;
    game_move_t best_move = 0;
    int best_child_pv_len = 0;
    game_move_t best_child_pv[SEARCH_PV_MAX];

    for (int i=0;i<n;i++) {
        if (should_stop(cx)) { cx->stopped = 1; break; }

        /* lato al tratto prima di fare la mossa: usalo per history */
        int side_before = cx->api->side_to_move(st) & 1;

        void* undo = malloc(cx->api->undo_size);
        uint64_t k2 = cx->api->make_move(st, moves[i], undo); (void)k2;

        game_move_t child_pv[SEARCH_PV_MAX];
        int child_pv_len = 0;
        game_score_t score;

        /* === LMR: prova ricerca ridotta su mosse quiet tardive === */
        int do_lmr = lmr_should_reduce(cx, st, depth, i, moves[i], tt_move,
                                       cx->killers[0][ply], cx->killers[1][ply]);
        if (do_lmr){
            int R = lmr_compute_R(cx, depth, i);
            /* null-window a profondità ridotta */
            score = -negamax_ab(cx, st, depth-1-R, ply+1, -(alpha+1), -alpha,
                                nodes, NULL, NULL, 0);
            /* se passa la finestra, ricerca piena a depth-1 */
            if (score > alpha){
                score = -negamax_ab(cx, st, depth-1, ply+1, -beta, -alpha,
                                    nodes, child_pv, &child_pv_len, pv_cap-1);
            }
        } else {
            score = -negamax_ab(cx, st, depth-1, ply+1, -beta, -alpha,
                                nodes, child_pv, &child_pv_len, pv_cap-1);
        }

        (void)cx->api->unmake_move(st, moves[i], undo);
        free(undo);

        if (cx->stopped) break;

        if (score > best) {
            best = score; best_move = moves[i];
            best_child_pv_len = child_pv_len;
            memcpy(best_child_pv, child_pv, sizeof(game_move_t)*child_pv_len);
        }
        if (best > alpha) alpha = best;

        if (alpha >= beta) {
            // cutoff
            cx->cutoffs++;
            if (is_quiet_move(cx->api, st, moves[i])) {
                if (cx->killers[0][ply] != moves[i]) {
                    cx->killers[1][ply] = cx->killers[0][ply];
                    cx->killers[0][ply] = moves[i];
                }
                int idx = hidx(moves[i]);
                cx->history[side_before][idx] += hist_bonus(cx, depth);
                history_clamp(&cx->history[side_before][idx]);
            }
            break;
        }

        /* piccolo PV-bonus per miglior mossa quiet (ordering iterazioni successive) */
        if (moves[i] == best_move && is_quiet_move(cx->api, st, moves[i])) {
            int idx = hidx(moves[i]);
            cx->history[side_before][idx] += (hist_bonus(cx, depth) >> 2);
            history_clamp(&cx->history[side_before][idx]);
        }
    }

    if (pv && pv_cap > 0) {
        int out_len = 0;
        pv[out_len++] = best_move;
        for (int j=0; j<best_child_pv_len && out_len<pv_cap; ++j) pv[out_len++] = best_child_pv[j];
        if (pv_len) *pv_len = out_len;
    }

    if (cx->use_tt) {
        uint64_t key = cx->api->hash(st);

        /* >>> FIX: scelta corretta del flag con alpha_orig (post TT-probe) */
        tt_flag_t flag;
        if (best <= alpha_orig)      flag = TT_UPPER;  /* fail-low */
        else if (best >= beta)       flag = TT_LOWER;  /* fail-high */
        else                         flag = TT_EXACT;  /* all'interno della finestra */

        tt_store(&cx->tt, key, depth, tt_mate_to_store(best, ply), flag, best_move);
        cx->tt_stores++;
    }

    return best;
}

/* arrotonda verso il basso alla potenza di due */
static unsigned round_down_pow2_u32(uint64_t x){
    if (x < 1u) return 1u;
    unsigned p = 1u;
    while ((uint64_t)(p<<1) <= x) p <<= 1;
    return p;
}

/* stima entries TT da MB (assume ~32 byte per entry; valore prudente) */
static unsigned estimate_tt_entries_from_mb(int mb){
    if (mb <= 0) return (1u<<18);
    uint64_t bytes = (uint64_t)mb * 1024u * 1024u;
    uint64_t entries = bytes / 32u;
    if (entries < (1u<<16)) entries = (1u<<16);
    return round_down_pow2_u32(entries);
}

/* ====== Root con ID + Time + Aspiration ====== */
void search_root(const GameAPI* api, game_state_t* state, const search_params_t* p, search_result_t* out)
{
    memset(out, 0, sizeof(*out));
    if (!api || !state || !p) return;

    search_ctx_t cx;
    memset(&cx, 0, sizeof(cx));
    cx.api       = api;
    cx.start_ns  = time_now_ns();
    cx.budget_ns = (p->use_time && p->time_ms > 0) ? (uint64_t)p->time_ms * 1000000ULL : 0ULL;
    cx.stop_ext  = p->stop;
    cx.verbose   = p->verbose;
    cx.use_tt    = 1;
    cx.use_qsearch = p->use_qsearch ? 1 : 0;

    /* === Config da parametri con default === */
    cx.cfg.hist_decay_shift   = (p->hist_decay_shift   > 0) ? p->hist_decay_shift   : 1;
    cx.cfg.hist_bonus_scale   = (p->hist_bonus_scale   > 0) ? p->hist_bonus_scale   : 1;

    /* >>> FIX: rispetta i parametri (senza forzare sempre a 1) */
    cx.cfg.enable_lmr         = (p->enable_lmr         == 0) ? 0 : 1; // default on se non 0
    cx.cfg.lmr_min_depth      = (p->lmr_min_depth      > 0) ? p->lmr_min_depth      : 3;
    cx.cfg.lmr_move_threshold = (p->lmr_move_threshold > 0) ? p->lmr_move_threshold : 4;
    cx.cfg.lmr_base_reduction = (p->lmr_base_reduction > 0) ? p->lmr_base_reduction : 1;

    cx.cfg.enable_asp         = (p->enable_aspiration  == 0) ? 0 : 1; // default on se non 0
    cx.cfg.asp_delta_start    = (p->asp_delta_cp       > 0) ? p->asp_delta_cp       : 30;
    cx.cfg.asp_max_retries    = (p->asp_max_retries    > 0) ? p->asp_max_retries    : 4;
    cx.cfg.asp_grow_x2        = (p->asp_grow_x2        == 0) ? 0 : 1; // default raddoppio se non 0

    cx.cfg.tt_entries         = (p->tt_size_mb > 0) ? estimate_tt_entries_from_mb(p->tt_size_mb)
                                                    : (1u<<18);

    if (cx.use_tt) tt_init(&cx.tt, cx.cfg.tt_entries);

    /* init heuristics */
    killers_reset_ply(&cx, MAX_PLY);
    cx.history_iter = 0;

    uint64_t total_nodes = 0;
    game_move_t best_pv[SEARCH_PV_MAX];
    int best_pv_len = 0;
    game_score_t best_score = 0;
    int reached_depth = 0;
    int have_prev_score = 0;

    /*
     * Fallback robusto: se il budget scade prima di completare anche solo d=1,
     * restituiamo comunque una mossa legale al root (se esiste).
     */
    {
        game_move_t root_moves[256];
        int root_n = api->generate_legal(state, root_moves, 256);
        if (root_n > 0) {
            best_pv[0] = root_moves[0];
            best_pv_len = 1;
        }
    }

    int maxd = (p->max_depth <= 0 ? 1 : p->max_depth);
    for (int d = 1; d <= maxd; ++d) {
        if (should_stop(&cx)) { cx.stopped = 1; break; }
        if (cx.use_tt) tt_newgen(&cx.tt);

        /* aging history periodico + reset killers */
        history_age(&cx);
        killers_reset_ply(&cx, MAX_PLY);

        game_move_t pv[SEARCH_PV_MAX];
        int pv_len = 0;

        uint64_t nodes_before = total_nodes;

        /* ---- Aspiration window ---- */
        game_score_t alpha, beta;
        int asp_retries = 0;
        int asp_delta = cx.cfg.asp_delta_start;

        if (cx.cfg.enable_asp && have_prev_score && d > 1) {
            alpha = best_score - asp_delta;
            beta  = best_score + asp_delta;
        } else {
            alpha = CLAMP_ALPHA;
            beta  = CLAMP_BETA;
        }

        game_score_t score = 0;
        for (;;) {
            score = negamax_ab(&cx, state, d, 0, alpha, beta, &total_nodes,
                               pv, &pv_len, SEARCH_PV_MAX);

            if (cx.stopped) break;

            if (score > alpha && score < beta) {
                /* successo */
                break;
            }

            if (!cx.cfg.enable_asp || !have_prev_score || d == 1 || asp_retries >= cx.cfg.asp_max_retries) {
                /* fallback: finestra piena */
                alpha = CLAMP_ALPHA;
                beta  = CLAMP_BETA;
                score = negamax_ab(&cx, state, d, 0, alpha, beta, &total_nodes,
                                   pv, &pv_len, SEARCH_PV_MAX);
                break;
            }

            /* espansione progressiva */
            if (cx.cfg.asp_grow_x2) asp_delta *= 2;
            else                    asp_delta += cx.cfg.asp_delta_start;

            if (score <= alpha) {
                alpha = best_score - asp_delta;
            } else { /* score >= beta */
                beta  = best_score + asp_delta;
            }
            ++asp_retries;

            if (cx.verbose) {
                LOGI("aspiration retry d=%d retry=%d new window=[%d,%d] last=%d",
                     d, asp_retries, (int)alpha, (int)beta, (int)score);
            }
        }

        if (cx.stopped) break;

        best_score = score;
        reached_depth = d;
        best_pv_len = pv_len;
        memcpy(best_pv, pv, sizeof(game_move_t)*pv_len);
        have_prev_score = 1;

        if (cx.verbose){
            LOGI("ID depth=%d score=%d nodes(iter)=%" PRIu64 " nodes(total)=%" PRIu64,
                 d, (int)score,
                 (uint64_t)(total_nodes - nodes_before),
                 (uint64_t)total_nodes);
        }
        cx.history_iter++;
    }

    uint64_t elapsed = time_now_ns() - cx.start_ns;
    double nps = (elapsed > 0) ? ((double)total_nodes * 1e9) / (double)elapsed : 0.0;

    /* riempi out */
    out->score = best_score;
    out->nodes = total_nodes;
    out->depth_searched = reached_depth;
    out->pv_len = best_pv_len;
    out->best_move = (best_pv_len > 0) ? best_pv[0] : 0;
    if (best_pv_len > 0) memcpy(out->pv, best_pv, sizeof(game_move_t)*best_pv_len);
    out->stopped_by_time = cx.stopped;

    /* metriche */
    out->qnodes    = cx.qnodes;
    out->tt_probes = cx.tt_probes;
    out->tt_hits   = cx.tt_hits;
    out->tt_stores = cx.tt_stores;
    out->cutoffs   = cx.cutoffs;
    out->time_ns   = elapsed;
    out->nps       = nps;

    if (p->verbose){
        LOGI("search_root: reached_depth=%d nodes=%" PRIu64 " score=%d stopped=%d",
             reached_depth, (uint64_t)total_nodes, (int)best_score, cx.stopped);
        LOGI("stats: time=%.2f ms  nodes=%" PRIu64 "  nps=%.0f  qnodes=%" PRIu64
             "  tt: probes=%" PRIu64 " hits=%" PRIu64 " stores=%" PRIu64
             "  cutoffs=%" PRIu64,
             (double)elapsed/1e6, (uint64_t)total_nodes, nps,
             (uint64_t)cx.qnodes, (uint64_t)cx.tt_probes, (uint64_t)cx.tt_hits,
             (uint64_t)cx.tt_stores, (uint64_t)cx.cutoffs);
    }

    if (cx.use_tt) tt_free(&cx.tt);
}
