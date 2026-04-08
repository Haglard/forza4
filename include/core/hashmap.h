/*
================================================================================
 @file      hashmap.h
 @brief     Interfaccia della libreria HashMap (tabella hash) a prestazioni costanti-amortizzate.
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Questo header dichiara una libreria di tabella hash generica, usata nel motore
  di scacchi e in altri moduli di sistema (caching, tabelle di trasposizione, mappe).
  Le operazioni principali (inserimento/ricerca/rimozione) sono O(1) *amortizzate*.
  La libreria non è thread-safe: sincronizzare dall'esterno in contesti concorrenti.
  Le funzioni rispettano un'API C minimale con gestione esplicita di errori/NULL.
================================================================================
*/
// =============================================
// File: include/core/hashmap.h
// Project: chess-engine (core utilities)
// Purpose: Robin Hood open-addressing hashmap (generic key/value bytes)
// License: MIT (c) 2025
//
// Overview
// --------
// Hashmap con indirizzamento aperto e strategia **Robin Hood**:
// - Probe sequence con distanza ("dist") mantenuta per ogni bucket
// - Inserimento con swapping se l'elemento corrente ha dist maggiore del bucket occupato
// - Cancellazione con **backshift** (nessun tombstone)
// - Resize su carico > ~0.75 (potenza di due)
//
// API generica (key/value = blob di byte):
//   hm_create(cap_pow2, hash, eq)          -> hashmap*
//   hm_destroy(h)
//   hm_clear(h)                            // svuota e compatta a capacità iniziale
//   hm_reserve(h, new_cap_pow2)            // rehash forzato
//   hm_put(h, key,klen, val,vlen)          // insert/replace
//   hm_get(h, key,klen, out,&out_len)      // 0=ok, 1=buf troppo piccolo (ritorna out_len), -1=not found
//   hm_remove(h, key,klen)
//   hm_size(h)
//   hm_load_factor(h)
//   Iterazione: hm_iter_begin / hm_iter_next
//
// Note
// ----
// • Le chiavi e i valori vengono copiati internamente (heap-allocated per entry)
// • Thread-safety: NO (usare lock esterni o separare per thread)
// • Prestazioni: distanze e backshift riducono la frammentazione logica
// =============================================
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Struttura dati `hm_iter_t`.
 * @details Vedere implementazione per invarianti e gestione collisioni.
 */
typedef struct hashmap hashmap_t;

typedef uint64_t (*hash_func)(const void* key, size_t len);
typedef int      (*key_eq_func)(const void* a, size_t alen, const void* b, size_t blen);

/* Costruzione/distruzione */
hashmap_t* hm_create(size_t capacity_pow2, hash_func hf, key_eq_func eq);
void       hm_destroy(hashmap_t* hm);

/* Capacità & housekeeping */
int        hm_reserve(hashmap_t* hm, size_t capacity_pow2);
void       hm_clear(hashmap_t* hm); /* mantiene la capacità iniziale con cui è stato creato */

/* CRUD */
int        hm_put(hashmap_t* hm, const void* key, size_t klen, const void* val, size_t vlen);
int        hm_get(hashmap_t* hm, const void* key, size_t klen, void* out, size_t* out_len); /* 0 ok; 1 out troppo piccolo; -1 assente */
int        hm_remove(hashmap_t* hm, const void* key, size_t klen); /* 0 ok; -1 assente */

/* Statistiche */
size_t     hm_size(const hashmap_t* hm);
double     hm_load_factor(const hashmap_t* hm);
size_t     hm_capacity(const hashmap_t* hm);

/* Iteratore */
typedef struct { const hashmap_t* m; size_t idx; } hm_iter_t;
/**
 * @brief Inizializza l'iteratore per scorrere gli elementi della mappa.
 * @param hm Handle della mappa; deve essere non NULL e valido.
 * @param it Iteratore fornito dal chiamante da inizializzare/avanzare.
 * @return Nessun valore di ritorno.
 * @warning Modifiche alla mappa durante l'iterazione possono invalidare l'iteratore.
 * @complexity O(1) amortizzata; O(n) nei casi peggiori o durante il resize.
 */
void       hm_iter_begin(const hashmap_t* hm, hm_iter_t* it);
/**
 * @brief Avanza l'iteratore e restituisce la voce corrente (chiave/valore).
 * @param it Iteratore fornito dal chiamante da inizializzare/avanzare.
 * @param key Puntatore alla chiave; buffer trattato come binario (lunghezza `klen`).
 * @param klen Lunghezza della chiave in byte.
 * @param val Puntatore al valore associato alla chiave.
 * @param vlen Lunghezza del valore in byte.
 * @return `1` in caso di successo, `0` in caso di fallimento (es. chiave assente, memoria esaurita).
 * @warning Modifiche alla mappa durante l'iterazione possono invalidare l'iteratore.
 * @complexity O(1) amortizzata; O(n) nei casi peggiori o durante il resize.
 */
int        hm_iter_next(hm_iter_t* it, const void** key, size_t* klen, const void** val, size_t* vlen);

#ifdef __cplusplus
}
#endif

