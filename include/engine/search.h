/*
================================================================================
 @file      include/engine/search.h
 @brief     Interfaccia del motore di ricerca (negamax + alpha-beta, ID, qsearch, TT, aspiration, LMR, history).
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Espone i tipi generici per integrarsi con GameAPI, i parametri di ricerca e il
  risultato completo, inclusa la PV e le metriche runtime. L'entry-point `search_root`
  effettua una ricerca iterativa (ID) con negamax/alpha-beta, quiescence search,
  transposition table, aspiration windows, Late Move Reductions (LMR) e history heuristic.
================================================================================
*/
// =============================================
// File: include/engine/search.h
// Purpose: Interfaccia del motore di ricerca
//          (negamax + alpha-beta, ID, qsearch, TT, aspiration, LMR, history)
// License: MIT (c) 2025
// =============================================
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Tipi generici dipendenti dal gioco (mossa, punteggio, stato). */
// --- Tipi generici per giochi via GameAPI ---
typedef struct game_state_s game_state_t;
typedef uint64_t            game_move_t;
typedef int32_t             game_score_t;

/** @brief Forward declaration della GameAPI (definita in include/game/api.h). */
typedef struct GameAPI_s GameAPI; // fwd decl (definita in include/game/api.h)

// Score canonici
/**
 * @brief Score canonici della ricerca.
 * @details `SEARCH_SCORE_MATE` rappresenta un valore vicino a +inf per indicare mate.
 */
enum {
    SEARCH_SCORE_DRAW = 0,
    SEARCH_SCORE_MATE = 30000
};

/** @brief Massima lunghezza della principale variation (PV) conservata. */
#ifndef SEARCH_PV_MAX
#define SEARCH_PV_MAX 64
#endif

// Parametri di ricerca (tutti opzionali: 0 => default interni)
typedef struct {
    // ---- base ----
    int  max_depth;      // profondita' massima (ply). Se <=0 e use_time=1, usa solo tempo
    int  use_time;       // se true, usa budget di tempo
    int  time_ms;        // budget in millisecondi (se use_time)
    int  use_qsearch;    // abilita quiescence search quando depth==0
    int  verbose;        // log extra
    volatile int* stop;  // puntatore esterno a flag di stop (opzionale)

    // ---- aspiration windows ----
    // enable_aspiration: 0=off, 1=on (default 1)
    // asp_delta_cp: ampiezza iniziale finestra in centipawn (default 30)
    // asp_max_retries: massimo numero di re-search allargando la finestra (default 4)
    // asp_grow_x2: 1=raddoppia finestra, 0=+delta lineare (default 1)
    int  enable_aspiration;
    int  asp_delta_cp;
    int  asp_max_retries;
    int  asp_grow_x2;

    // ---- LMR (Late Move Reductions) ----
    // enable_lmr: 0=off, 1=on (default 1)
    // lmr_min_depth: applica LMR solo da questa profondita' in su (default 3)
    // lmr_move_threshold: riduci solo dalla mossa #N (0-based) in poi (default 4)
    // lmr_base_reduction: riduzione base in "frazioni di ply" (16 = 1 ply) (default 16)
    int  enable_lmr;
    int  lmr_min_depth;
    int  lmr_move_threshold;
    int  lmr_base_reduction;

    // ---- History heuristic ----
    // hist_decay_shift: aging (valori >> shift a ogni iterazione ID) (default 1, cioe' /2)
    // hist_bonus_scale: scala moltiplicativa sul bonus history (default 1)
    int  hist_decay_shift;
    int  hist_bonus_scale;

    // ---- Transposition Table ----
    // tt_size_mb: dimensione desiderata in MB (approssimata). 0=default interno.
    int  tt_size_mb;
/**
 * @brief Parametri di configurazione della ricerca (tutti opzionali).
 * @details Ogni campo puo' rimanere a 0 per usare il default interno. I campi includono:
 *  - Base: profondita' massima, gestione del tempo (use_time/time_ms), uso qsearch, verbose, flag di stop esterno.
 *  - Aspiration windows: abilitazione e controllo finestra (delta, grow, retries).
 *  - LMR: abilitazione e soglie (min depth, indice mossa, riduzione base in sedicesimi di ply).
 *  - History: aging (shift) e scaling del bonus.
 *  - Transposition Table: dimensione desiderata (MB).
 */
} search_params_t;

// Risultato della ricerca (con metriche)
typedef struct {
    // esito
    game_move_t  best_move;
    game_score_t score;
    uint64_t     nodes;
    int          depth_searched;
    int          stopped_by_time;

    // PV
    game_move_t  pv[SEARCH_PV_MAX];
    int          pv_len;

    // === METRICHE RUNTIME ===
    uint64_t qnodes;     // nodi esplorati in qsearch
    uint64_t tt_probes;  // numero di probe TT
    uint64_t tt_hits;    // probe con hit
    uint64_t tt_stores;  // store in TT
    uint64_t cutoffs;    // beta cutoffs
    double   nps;        // nodi/secondo
    uint64_t time_ns;    // durata ricerca in ns
/**
 * @brief Risultato della ricerca e metriche raccolte.
 * @details Contiene mossa migliore, punteggio, nodi esplorati, profondita' effettiva,
 *  motivo di stop (tempo), PV trovata, e contatori TT/cutoff oltre a NPS e durata.
 */
} search_result_t;

// Entry-point della ricerca (negamax + alpha-beta con ID, qsearch, TT, aspiration/LMR/history)
/**
 * @brief Entry-point della ricerca sullo stato corrente.
 * @param api Interfaccia GameAPI del gioco (funzioni di mosse, valutazione, ecc.).
 * @param state Stato di gioco mutabile da analizzare.
 * @param p Parametri di ricerca (tutti opzionali; NULL per usare default interni).
 * @param out Struttura di output popolata con best move, score, PV e metriche (non NULL).
 * @details Implementa negamax + alpha-beta con iterative deepening, TT, aspiration, LMR e qsearch.
 *  Se `p->use_time` e' non-zero, la ricerca considera `p->time_ms` come budget indicativo.
 *  Se `p->stop` e' non-NULL e viene posto a non-zero dall'esterno, la ricerca si interrompe al piu' presto.
 * @warning Non thread-safe a meno di protezioni esterne o istanze separate per thread.
 */
void search_root(const GameAPI* api, game_state_t* state,
                 const search_params_t* p, search_result_t* out);

#ifdef __cplusplus
}
#endif