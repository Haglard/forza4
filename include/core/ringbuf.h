/*
================================================================================
 @file      include/core/ringbuf.h
 @brief     Ring buffer lock-free SPSC (Single-Producer Single-Consumer) con C11 atomics.
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Buffer circolare per comunicazione senza lock tra un solo produttore e un solo consumatore.
  Elementi a dimensione fissa, capacità potenza di due, nessuna allocazione nel percorso
  di enqueue/dequeue. Indici atomici `head` (writer) e `tail` (reader) con barriere di memoria
  appropriate (release, acquire, relaxed).
 @warning   Valido esclusivamente per scenari SPSC: un unico thread produttore e un unico
            thread consumatore. Per MPMC serve un'implementazione diversa.
================================================================================
*/
// =============================================
// File: include/core/ringbuf.h
// Project: chess-engine (core utilities)
// Purpose: Single-Producer Single-Consumer lock-free ring buffer
// License: MIT (c) 2025
//
// Overview
// --------
// Ring buffer per comunicazione SPSC (un produttore + un consumatore) senza lock.
// - Supporta elementi di dimensione fissa `elem_size`
// - Capacità in numero di elementi (obbligatoriamente potenza di due)
// - Nessuna allocazione nel percorso *enqueue/dequeue*
// - Thread-safety garantita per lo scenario SPSC (1 producer thread, 1 consumer thread)
// - Usa C11 atomics per head/tail con corretta memoria (release/acquire)
//
// Note
// ----
// • Se vuoi evitare malloc, puoi fornire un buffer esterno con `ringbuf_init_ext()`.
// • Per MPMC servirebbe un'implementazione diversa (più costosa). Qui restiamo SPSC.
// • Gli indici sono in range [0, capacity-1] e vengono mascherati con `mask = capacity-1`.
//
// API
// ---
//   int  ringbuf_init(ringbuf_t* rb, size_t elem_size, size_t capacity_pow2);
//   int  ringbuf_init_ext(ringbuf_t* rb, void* ext_mem, size_t elem_size, size_t capacity_pow2);
//   void ringbuf_free(ringbuf_t* rb); // libera solo se init() (malloc interno)
//   size_t ringbuf_capacity(const ringbuf_t* rb);
//   size_t ringbuf_size(const ringbuf_t* rb);     // elementi presenti (approssimazione safe SPSC)
//   int  ringbuf_try_enqueue(ringbuf_t* rb, const void* elem); // 1=ok, 0=full
//   int  ringbuf_try_dequeue(ringbuf_t* rb, void* elem_out);    // 1=ok, 0=empty
//   int  ringbuf_is_empty(const ringbuf_t* rb);
//   int  ringbuf_is_full(const ringbuf_t* rb);
//
// Errori/ritorni
//  - Le init() ritornano 0 su successo, -1 su errore parametri/alloc.
//  - Le funzioni try_* ritornano 1 se l'operazione è eseguita, 0 altrimenti.
// =============================================
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" { 
#endif

/**
 * @brief Struttura del ring buffer SPSC.
 * @details
 *  - `head`: indice di scrittura (produttore), atomico.
 *  - `tail`: indice di lettura (consumatore), atomico.
 *  - `mask`: pari a `capacity - 1`, usato per mappare indici in [0, capacity-1].
 *  - `elem_size`: dimensione in byte di ciascun elemento.
 *  - `buf`: area dati contigua di `capacity * elem_size` byte.
 *  - `owns_mem`: 1 se `buf` è stato allocato internamente e va liberato da `ringbuf_free()`.
 */
typedef struct ringbuf {
  _Atomic size_t head;     // write index (producer)
  _Atomic size_t tail;     // read index (consumer)
  size_t         mask;     // capacity - 1 (capacity must be power of two)
  size_t         elem_size;
  unsigned char* buf;      // data buffer (capacity * elem_size)
  int            owns_mem; // 1 if allocated internally
} ringbuf_t;

/* Utility: ritorna 1 se x è potenza di due e >0 */
static inline int rb_is_pow2(size_t x){ return x && ((x & (x-1)) == 0); }

/**
 * @brief Inizializza un ring buffer SPSC con memoria interna (malloc).
 * @param rb Handle del ring buffer (non NULL).
 * @param elem_size Dimensione fissa dell'elemento (in byte).
 * @param capacity_pow2 Capacità in elementi, **potenza di due** (>= 2).
 * @return 0 su successo; -1 se parametri invalidi o allocazione fallita.
 * @details Azzerra head/tail, calcola `mask = capacity-1` e alloca `buf = capacity * elem_size`.
 */
int  ringbuf_init(ringbuf_t* rb, size_t elem_size, size_t capacity_pow2);
/**
 * @brief Inizializza un ring buffer usando memoria esterna fornita dal chiamante.
 * @param rb Handle del ring buffer (non NULL).
 * @param ext_mem Puntatore a buffer già allocato di dimensione `capacity_pow2 * elem_size`.
 * @param elem_size Dimensione fissa dell'elemento (byte).
 * @param capacity_pow2 Capacità in elementi, **potenza di due** (>= 2).
 * @return 0 su successo; -1 su parametri invalidi.
 * @details Non effettua allocazioni; `owns_mem` viene impostato a 0.
 */
int  ringbuf_init_ext(ringbuf_t* rb, void* ext_mem, size_t elem_size, size_t capacity_pow2);
/**
 * @brief Libera le risorse se il ring buffer possiede la memoria interna.
 * @param rb Handle del ring buffer (può essere NULL).
 * @details Se `owns_mem==1`, libera `buf`. Reinizializza i campi.
 */
void ringbuf_free(ringbuf_t* rb);

/**
 * @brief Restituisce la capacità in elementi.
 * @param rb Puntatore (const) a ring buffer.
 * @return Capacità.
 */
size_t ringbuf_capacity(const ringbuf_t* rb);
/**
 * @brief Ritorna il numero di elementi presenti (approssimazione safe SPSC).
 * @param rb Puntatore (const) a ring buffer.
 * @return Numero di elementi (head-tail) & mask.
 * @details Valido in SPSC: non è sincronizzato per lettori/scrittori multipli.
 */
size_t ringbuf_size(const ringbuf_t* rb);
/**
 * @brief Indica se il buffer è vuoto.
 * @param rb Puntatore (const) a ring buffer.
 * @return 1 se vuoto, 0 altrimenti.
 */
int    ringbuf_is_empty(const ringbuf_t* rb);
/**
 * @brief Indica se il buffer è pieno.
 * @param rb Puntatore (const) a ring buffer.
 * @return 1 se pieno, 0 altrimenti.
 */
int    ringbuf_is_full(const ringbuf_t* rb);

/**
 * @brief Prova a inserire un elemento; non blocca.
 * @param rb Ring buffer (produttore).
 * @param elem Puntatore all'elemento sorgente (dimensione `elem_size`). Non NULL.
 * @return 1 se inserito; 0 se il buffer è pieno.
 * @details Scrive i dati nella posizione `(head & mask)` e poi pubblica `head` con memoria release.
 */
int  ringbuf_try_enqueue(ringbuf_t* rb, const void* elem);
/**
 * @brief Prova a prelevare un elemento; non blocca.
 * @param rb Ring buffer (consumatore).
 * @param elem_out Puntatore al buffer di output (dimensione `elem_size`). Non NULL.
 * @return 1 se prelevato; 0 se il buffer è vuoto.
 * @details Legge `head` con memoria acquire per vedere gli elementi pubblicati; copia da `(tail & mask)` e poi avanza `tail`.
 */
int  ringbuf_try_dequeue(ringbuf_t* rb, void* elem_out);

#ifdef __cplusplus
}
#endif