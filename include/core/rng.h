/*
================================================================================
 @file      include/core/rng.h
 @brief     PRNG veloce xoshiro256** con helper di seeding (entropy, SplitMix64).
 @version   Progetto v1.0; Modulo v1.0
 @autor     Sandro Borioni
 @autor     ChatGPT
 @details
  Generatore xoshiro256** con stato a 256 bit (4 x 64 bit). Fornisce funzioni di
  seeding (da vettore di 256 bit, da u64 via SplitMix64 (racomandato), da entropia di sistema (/dev/urandom)),
  next a 64/32 bit, jump 2^128 e generazione uniforme bounded senza bias.
  Non thread-safe: usare un rng_t per thread o proteggere esternamente.
================================================================================
*/
// =============================================
// File: include/core/rng.h
// Project: chess-engine (core utilities)
// Purpose: Fast PRNG (xoshiro256**) with entropy seeding helpers
// License: MIT (c) 2025
//
// Overview
// --------
// • State: 256-bit (4×uint64_t)
// • Functions: seed, jump, next_u64/u32, range, seeding from /dev/urandom
// • Thread-safety: NO (one rng_t per thread or protect externally)
//
// References: https://prng.di.unimi.it/
// =============================================
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Stato del PRNG xoshiro256** (256 bit).
 * @details L'array `s[4]` contiene lo stato interno (non usare direttamente se non per seed).
 */
typedef struct {
  uint64_t s[4];
} rng_t;

/* Initialize RNG with a 256-bit seed (s[0..3] all non-zero recommended). */
/**
 * @brief Inizializza il PRNG con un seed di 256 bit.
 * @param r Puntatore all'istanza RNG (non NULL).
 * @param s Vettore di 4 x uint64_t (idealmente tutti non zero).
 * @details Se tutti zero, la sequenza e' degenerata (uso sconsigliato).
 */
void rng_seed(rng_t* r, const uint64_t s[4]);
/* Convenience: seed from a single 64-bit value (expanded via SplitMix64). */
/**
 * @brief Convenience: inizializza da un solo seed 64-bit con espansione SplitMix64.
 * @param r RNG (non NULL).
 * @param seed Semenza 64-bit.
 * @details Espande `seed` in 256 bit invocando 4 volte SplitMix64 e li carica in `s[0..3]`.
 */
void rng_seed_u64(rng_t* r, uint64_t seed);
/* Seed from system entropy (/dev/urandom). Returns 0 on success, -1 otherwise. */
/**
 * @brief Inizializza usando entropia di sistema.
 * @param r RNG (non NULL).
 * @return 0 su successo; -1 in caso di errore (es. impossibile leggere /dev/urandom).
 * @details Legge 256 bit di entropia /dev/urandom. Se fallisce lascia il contenuto precedente.
 */
int  rng_seed_from_entropy(rng_t* r);

/* Next values */
/**
 * @brief Restituisce il prossimo valore a 64 bit e avanza lo stato.
 * @param r RNG (non NULL).
 * @return Valore pseudo-casuale 64-bit.
 * @details Implementa xoshiro256**; ciascuna chiamata muta `r->s` (non thread-safe).
 */
uint64_t rng_next_u64(rng_t* r);
/**
 * @brief Restituisce i 32 bit piu' significativi di `rng_next_u64()`.
 * @param r RNG (non NULL).
 * @return Valore pseudo-casuale 32-bit.
 */
static inline uint32_t rng_next_u32(rng_t* r){ return (uint32_t)(rng_next_u64(r) >> 32); }

/* Jump ahead as if 2^128 calls were made (useful for parallel streams). */
/**
 * @brief Salta avanti come se fossero state effettuate 2^128 chiamate.
 * @param r RNG (non NULL).
 * @details Utile per creare stream indipendenti in parallelo (es. per thread).
 */
void rng_jump(rng_t* r);

/* Uniform integer in [0, bound) without bias (64-bit). bound must be >0. */
/**
 * @brief Restituisce un intero uniforme in [0, bound) senza bias.
 * @param r RNG (non NULL).
 * @param bound Limite superiore esclusivo; deve essere > 0.
 * @return Un valore in [0, bound).
 * @details Implementato tipicamente con rejection sampling oppure con
 *  moltiplicazione a 128 bit + confronto (metodo senza bias). In caso di
 *  `bound==0` il comportamento non e' definito (precondizione).
 */
uint64_t rng_next_bounded(rng_t* r, uint64_t bound);

#ifdef __cplusplus
}
#endif
