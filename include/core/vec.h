/*
================================================================================
 @file      include/core/vec.h
 @brief     Vettore dinamico minimale (contiguo) per elementi a dimensione fissa.
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Struttura leggera con campi `data`, `len`, `cap`, `elem_size` e API basilare:
  init/free, reserve, push, at. Semantica simile a un array dinamico C:
  crescita geometrica tramite reserve e push, dati contigui e indicizzazione O(1).
  Non thread-safe.
================================================================================
*/
#pragma once
#include <stddef.h>
/**
 * @brief Struttura del vettore dinamico.
 * @details
 *  - data: puntatore all'area contigua degli elementi (o NULL se vuoto).
 *  - len: numero di elementi attuali (0..cap).
 *  - cap: capacita' in elementi (>= len).
 *  - elem_size: dimensione in byte di ciascun elemento.
 * @note Gli elementi sono memorizzati contiguamente; l'indirizzo di `vec_at(v,0)`
 *       coincide con `data`. La struttura non e' thread-safe.
 */
typedef struct { void* data; size_t len; size_t cap; size_t elem_size; } vec_t;
/**
 * @brief Inizializza il vettore con una dimensione elemento fissa.
 * @param v Vettore da inizializzare (non NULL).
 * @param elem_size Dimensione di ciascun elemento in byte (>= 1).
 * @return 0 su successo; valori negativi su errore. Valori di ritorno osservati: 0.
 * @details Imposta len=0, cap=0, data=NULL, memorizza elem_size.
 */
int  vec_init(vec_t* v, size_t elem_size);
/**
 * @brief Libera la memoria posseduta dal vettore e azzera i campi.
 * @param v Vettore (accetta NULL come no-op).
 * @details Dopo la chiamata, `v->data == NULL`, `len == cap == 0`.
 */
void vec_free(vec_t* v);
/**
 * @brief Garantisce almeno `new_cap` elementi di capacita' (senza cambiare `len`).
 * @param v Vettore valido.
 * @param new_cap Capacita' minima richiesta (in elementi).
 * @return 0 su successo; valori negativi su errore. Valori di ritorno osservati: -1, 0. Può allocare memoria (malloc/realloc) e fallire su OOM. Valida i parametri di input.
 * @details Usa allocazione/riallocazione; non modifica `len` ne' il contenuto esistente.
 */
int  vec_reserve(vec_t* v, size_t new_cap);
/**
 * @brief Aggiunge un elemento in coda (append) copiando dai byte puntati da `elem`.
 * @param v Vettore valido.
 * @param elem Puntatore alla sorgente (dimensione `elem_size` byte).
 * @return 0 su successo; valori negativi su errore. Valori di ritorno osservati: -1, 0.
 * @details Cresce la capacita' se necessario (amortizzato O(1)).
 */
int  vec_push(vec_t* v, const void* elem);
/**
 * @brief Restituisce un puntatore all'elemento `idx` (0-based).
 * @param v Vettore valido.
 * @param idx Indice (0..len-1).
 * @return Puntatore all'elemento oppure NULL se `idx` e' fuori range.
 * @details Accesso O(1). Il puntatore resta valido finche' non cambia la capacita' (realloc).
 */
void* vec_at(vec_t* v, size_t idx);