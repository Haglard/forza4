/*
================================================================================
 @file      include/core/pool.h
 @brief     Memory pool a dimensione fissa (free-list) per alloc/free veloci di piccoli oggetti.
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Allocatore a blocchi con lista libera LIFO e crescita per chunk. Riduce overhead e
  frammentazione rispetto a malloc/free per oggetti piccoli. Non thread-safe
  (prevedere un pool per thread o protezione esterna).
  API principali: init, alloc, free, clear, destroy, get_stats.
================================================================================
*/
// =============================================
// File: include/core/pool.h
// Project: chess-engine (core utilities)
// Purpose: Fixed-size memory pool (free-list) for fast alloc/free of small objects
// License: MIT (c) 2025
//
// Overview
// --------
// Allocatore a blocchi di dimensione fissa, con lista libera LIFO e crescita per "chunk".
// Obiettivo: ridurre overhead e frammentazione rispetto a malloc/free per oggetti piccoli.
// Thread-safety: **non** thread-safe; se servono thread multipli, usare un pool per thread
// o proteggere le API con un lock esterno.
//
// Caratteristiche
//  - `pool_init(elem_size, align, elems_per_chunk)`
//  - Nessuna allocazione durante alloc/free salvo quando serve crescere (nuovo chunk)
//  - Allineamento personalizzabile (potenza di due)
//  - API semplice: alloc/free/clear/destroy + stats
//
// Note
//  - L'elemento deve stare in un blocco di dimensione >= sizeof(void*) per memorizzare i next
//  - `pool_clear()` libera tutti i chunk eccetto il primo (per riuso veloce)
//  - `pool_destroy()` libera tutto
// =============================================
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pool pool_t;

/**
 * @brief Statistiche correnti del pool.
 * @details
 *  - elem_size: dimensione richiesta dall'utente (byte).
 *  - stride: dimensione effettiva per blocco (arrotondata all'allineamento).
 *  - align: allineamento (potenza di due).
 *  - elems_per_chunk: blocchi allocati per ciascun chunk.
 *  - chunks: numero di chunk attualmente allocati.
 *  - free_count: blocchi liberi in free-list.
 *  - inuse_count: blocchi in uso (approssimato: totale - free).
 */
typedef struct pool_stats {
  size_t elem_size;        // dimensione richiesta dall'utente
  size_t stride;           // dimensione effettiva per blocco (allineata)
  size_t align;            // allineamento
  size_t elems_per_chunk;  // numero di blocchi per chunk
  size_t chunks;           // chunk attualmente allocati
  size_t free_count;       // blocchi liberi
  size_t inuse_count;      // blocchi in uso (approssimazione: totale - free)
} pool_stats_t;

/* Inizializza un pool. Ritorna 0 su successo, -1 su parametri invalidi, -2 su alloc fallita. */
/**
 * @brief Inizializza un pool a dimensione fissa.
 * @param out Puntatore di output al handle del pool (allocato dalla funzione).
 * @param elem_size Dimensione dell'oggetto (byte). Deve essere >= sizeof(void*).
 * @param align Allineamento (potenza di due). Se 0, viene usato un default sensato.
 * @param elems_per_chunk Numero di blocchi per chunk (crescita del pool).
 * @return 0 su successo; -1 su parametri non validi; -2 se allocazione fallisce.
 * @details Non alloca durante alloc/free salvo quando necessario per crescere (nuovo chunk).
 */
int  pool_init(pool_t** out, size_t elem_size, size_t align, size_t elems_per_chunk);
/* Alloca un elemento (dimensione fissa). Ritorna puntatore o NULL su OOM. */
/**
 * @brief Alloca un elemento dal pool.
 * @param p Pool valido.
 * @return Puntatore al blocco (dimensione fissa) oppure NULL se esaurito e allocazione chunk fallisce.
 * @details Amortizzato O(1). Può causare allocazione di un nuovo chunk se la free-list è vuota.
 */
void* pool_alloc(pool_t* p);
/* Libera un elemento precedentemente allocato dal pool. */
/**
 * @brief Rilascia un elemento precedentemente allocato dal pool.
 * @param p Pool valido.
 * @param ptr Puntatore a un blocco ottenuto da pool_alloc().
 * @details Inserisce il blocco in testa alla free-list (LIFO).
 * @warning `ptr` deve provenire da questo pool; comportamento indefinito altrimenti.
 */
void  pool_free(pool_t* p, void* ptr);
/* Libera tutti i chunk tranne il primo e resetta la free-list. */
/**
 * @brief Libera tutti i chunk tranne il primo e resetta la free-list.
 * @param p Pool valido.
 * @details Mantiene il primo chunk per un riuso rapido; non invalida il pool.
 */
void  pool_clear(pool_t* p);
/* Distrugge il pool e libera tutte le risorse. */
/**
 * @brief Distrugge il pool e libera tutte le risorse.
 * @param p Pool da distruggere (accetta NULL come no-op).
 */
void  pool_destroy(pool_t* p);
/* Statistiche correnti del pool. */
/**
 * @brief Restituisce le statistiche correnti del pool.
 * @param p Pool (const).
 * @return Struttura `pool_stats_t` con i contatori aggiornati.
 */
pool_stats_t pool_get_stats(const pool_t* p);

#ifdef __cplusplus
}
#endif

