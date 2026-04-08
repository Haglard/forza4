/*
================================================================================
 @file      include/game/api.h
 @brief     Game API generica per giochi a turni 2-giocatori, deterministici e a informazione perfetta.
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Interfaccia usata dal motore (Negamax/Alpha-Beta). L'adapter del gioco definisce
  lo stato reale e fornisce le callback per generazione mosse, make/unmake, hash,
  terminalita'/valutazione e copia. Le mosse sono 64 bit a discrezione dell'adapter.
================================================================================
*/
// =============================================
// File: include/game/api.h
// Project: chess-engine (game-agnostic layer)
// Purpose: Generic turn-based Game API for search frameworks
// License: MIT (c) 2025
//
// Overview
// --------
// Interfaccia minimale per giochi a turni 2-giocatori, deterministici,
// informazione perfetta. Usata dal motore generico (Negamax/Alpha-Beta).
//
// Note:
// - Lo "stato" di gioco è opaco e gestito dall'adapter (struct reale definita dall'adapter).
// - Le mosse sono codificate in 64 bit (libertà totale all'adapter).
// - L'undo è un buffer di dimensione fissa per mossa, definita dall'adapter.
// - I punteggi sono POV lato-al-muovere (centipawns per scacchi).
// - Le funzioni "captures" sono OPZIONALI: se NULL, la quiescence fa solo stand-pat.
// =============================================
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Tipi base: mossa a 64 bit e punteggio POV lato-al-muovere. */
    /* Tipi base */
    typedef uint64_t game_move_t; /* mossa generica codificata in 64 bit */
    typedef int32_t game_score_t; /* score POV lato-al-muovere (cp per scacchi) */

/**
 * @brief Esito terminale di una posizione (facoltativo).
 * @details I valori WIN/LOSS sono dal punto di vista del **lato al tratto**.
 */
    /* Esito terminale (facoltativo per granularità) */
    typedef enum
    {
        GAME_RESULT_NONE = 0,  /* non terminale */
        GAME_RESULT_WIN = 1,   /* vince chi è al tratto */
        GAME_RESULT_LOSS = -1, /* perde chi è al tratto */
        GAME_RESULT_DRAW = 2   /* patta / pareggio */
    } game_result_t;

/**
 * @brief Bitflag opzionali per un controllo terminale piu' ricco.
 * @details Utili per distinguere i casi in `is_terminal_ext()`.
 */
    /* Flag opzionali per un controllo terminale più ricco */
    enum
    {
        GAME_TERMFLAG_NONE = 0,
        GAME_TERMFLAG_WIN = 1u << 0,
        GAME_TERMFLAG_LOSS = 1u << 1,
        GAME_TERMFLAG_DRAW = 1u << 2
    };

/** @brief Stato opaco del gioco; la struct concreta e' definita dall'adapter. */
    /* Stato opaco: l'adapter definisce la struct reale e ne fornisce la size. */
    typedef struct game_state_s game_state_t;

/**
 * @brief Interfaccia del gioco usata dal motore di ricerca.
 * @details
 *  - `state_size` / `undo_size`: dimensioni (byte) dello stato e del buffer undo per mossa.
 *  - `side_to_move(st)`: lato al tratto (convenzione dipendente dal gioco).
 *  - `generate_legal(st, out, cap)`: genera mosse legali (ritorna conteggio).
 *  - **Opzionali**: `generate_captures`, `is_capture`, `capture_score` (ordinamento qsearch).
 *  - `make_move/unmake_move`: applica/annulla mossa (buffer undo di `undo_size`).
 *  - `hash(st)`: chiave a 64 bit (es. Zobrist).
 *  - `is_terminal(st, out)` / `is_terminal_ext(st)`: test terminalita' (1 se terminale).
 *  - `evaluate(st)`: punteggio POV lato-al-muovere (centipawn negli scacchi).
 *  - `copy(src, dst)`: copia stato (puo' essere NULL; usare helper `gameapi_copy_state`).
 */
    typedef struct GameAPI_s
    {
        size_t state_size;
        size_t undo_size;

        int (*side_to_move)(const game_state_t *st);

        /* move generation */
        int (*generate_legal)(const game_state_t *st, game_move_t *out, int cap);

        /* opzionali per qsearch/ordinamento: */
        int (*generate_captures)(const game_state_t *st, game_move_t *out, int cap); /* può essere NULL */
        int (*is_capture)(const game_state_t *st, game_move_t m);                    /* può essere NULL */

        /* NEW: score per ordinare le catture (MVV/LVA o simili). Può essere NULL. */
        int (*capture_score)(const game_state_t *st, game_move_t m);

        /* make/unmake */
        uint64_t (*make_move)(game_state_t *st, game_move_t m, void *undo);
        uint64_t (*unmake_move)(game_state_t *st, game_move_t m, const void *undo);

        /* hash/eval/terminalità */
        uint64_t (*hash)(const game_state_t *st);
        int (*is_terminal)(const game_state_t *st, game_result_t *out); /* 1 se terminale */
        unsigned (*is_terminal_ext)(const game_state_t *st);            /* bitflag opzionale */
        game_score_t (*evaluate)(const game_state_t *st);

        /* copia stato */
        void (*copy)(const game_state_t *src, game_state_t *dst);
    } GameAPI;

    /* Helper: valida i puntatori obbligatori della GameAPI. Ritorna 0 se ok, -1 se non valido. */
/**
 * @brief Valida i puntatori obbligatori della GameAPI (non NULL).
 * @param api Interfaccia GameAPI.
 * @return 0 se valida, -1 altrimenti.
 */
    int gameapi_validate(const GameAPI *api);

    /* Helper: copia lo stato usando api->copy se fornita, altrimenti memcpy(state_size). */
/**
 * @brief Copia uno stato: usa `api->copy` se presente, altrimenti `memcpy(state_size)`.
 * @param api Interfaccia GameAPI.
 * @param src Stato sorgente.
 * @param dst Stato destinazione.
 */
    void gameapi_copy_state(const GameAPI *api, const game_state_t *src, game_state_t *dst);

#ifdef __cplusplus
}
#endif
