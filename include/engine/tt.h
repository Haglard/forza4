/*
================================================================================
 @file      include/engine/tt.h
 @brief     Tabella di trasposizione (TT) per il motore di ricerca.
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  API minimale per gestire una tabella di trasposizione: init/free/clear, store/probe,
  generazioni per aging e normalizzazione dei punteggi di matto in funzione della distanza
  (ply). Le entry contengono chiave a 64 bit, mossa hash, punteggio, profondita' e flag bound.
================================================================================
*/
#pragma once
#include <stdint.h>
#include "game/api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bound types */
/**
 * @brief Tipi di bound per le voci di TT.
 * @details
 *  - TT_NONE  : nessun bound specifico
 *  - TT_EXACT : valore esatto (alpha < v < beta)
 *  - TT_LOWER : lower bound (v >= beta)
 *  - TT_UPPER : upper bound (v <= alpha)
 */
typedef enum { TT_NONE=0, TT_EXACT=1, TT_LOWER=2, TT_UPPER=3 } tt_flag_t;

typedef struct {
    uint64_t    key;     /* full 64-bit key */
    game_move_t move;    /* best move (hash move) */
    int16_t     score;   /* stored score (see mate adjust helpers) */
    int8_t      depth;   /* search depth at store (ply) */
    uint8_t     flag;    /* tt_flag_t */
    uint8_t     gen;     /* generation for aging (optional) */
/**
 * @brief Entry della tabella di trasposizione.
 * @details
 *  - key : chiave a 64 bit (Zobrist o equivalente)
 *  - move: best move associata (hash move)
 *  - score: punteggio salvato (con eventuale codifica mate tramite helper)
 *  - depth: profondita' di ricerca alla quale e' stata salvata
 *  - flag : tipo di bound (tt_flag_t)
 *  - gen  : generazione per aging/eviction (opzionale)
 */
} tt_entry_t;

typedef struct {
    tt_entry_t* table;
    uint32_t    size;    /* number of entries (power of two preferred) */
    uint8_t     gen;
/**
 * @brief Handle della tabella di trasposizione.
 * @details
 *  - table: array di `tt_entry_t` di dimensione `size`
 *  - size : numero di entry (idealmente potenza di due)
 *  - gen  : contatore di generazione per aging
 */
} tt_t;

/**
 * @brief Inizializza la TT con un certo numero di entry.
 * @param tt Handle (non NULL).
 * @param entries Numero di entry desiderate (es. 1<<20 ~ 1M).
 * @return 0 su successo; <0 su errore (parametri/allocazione).
 * @details Alloca/inizializza l'array `table` e azzera le entry.
 */
int  tt_init(tt_t* tt, uint32_t entries);     /* entries: e.g., 1<<20 (~1M) */
/** @brief Libera la memoria della TT e azzera i campi. @param tt Handle (accetta NULL). */
void tt_free(tt_t* tt);
/** @brief Svuota/azzera tutte le entry della TT, preservando la capacita'. @param tt Handle. */
void tt_clear(tt_t* tt);
/** @brief Incrementa la generazione (`gen++`) per politiche di aging. @param tt Handle (opzionale). */
static inline void tt_newgen(tt_t* tt){ if(tt) tt->gen++; }

/**
 * @brief Salva/aggiorna una entry nella TT.
 * @param tt Handle TT.
 * @param key Chiave a 64 bit.
 * @param depth Profondita' (ply) alla quale e' valido il punteggio.
 * @param score Punteggio da salvare (normalizzare con tt_mate_to_store se necessario).
 * @param flag Tipo di bound (tt_flag_t).
 * @param move Mossa migliore associata (hash move).
 * @details Politiche di rimpiazzamento possono preferire profondita' maggiori o generazione corrente.
 */
void tt_store(tt_t* tt, uint64_t key, int depth, int score, tt_flag_t flag, game_move_t move);
/**
 * @brief Tenta di recuperare una entry dalla TT.
 * @param tt Handle (const).
 * @param key Chiave a 64 bit.
 * @param out Output opzionale della entry trovata (puo' essere NULL per controllo hit/miss).
 * @return 1 se trovato; 0 altrimenti.
 */
int  tt_probe(const tt_t* tt, uint64_t key, tt_entry_t* out);

/* Mate score normalization helpers (ply distance encode/decode) */
/**
 * @brief Converte un punteggio "mate in N" in una forma memorizzabile in TT.
 * @param score Punteggio (in centipawn o codifica mate).
 * @param ply Distanza in ply dal nodo radice (per normalizzare mate +-M).
 * @return Punteggio normalizzato per lo storage (evita ambiguita' di distanza).
 * @details Usare prima di `tt_store` in modo che i valori di mate rispettino la distanza.
 */
int  tt_mate_to_store(int score, int ply);
/**
 * @brief Converte un punteggio memorizzato in TT nella scala runtime.
 * @param score Punteggio salvato (potenzialmente codificato).
 * @param ply Distanza in ply dal nodo corrente.
 * @return Punteggio riportato alla scala runtime (ripristina distanza di mate).
 * @details Usare dopo `tt_probe` per confronti con alpha/beta e reporting all'utente.
 */
int  tt_mate_from_store(int score, int ply);

#ifdef __cplusplus
}
#endif
