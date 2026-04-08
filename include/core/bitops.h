/*
================================================================================
 @file      include/core/bitops.h
 @brief     Primitivi di bit-manipulation 64-bit (header-only, inline) per bitboard e uso generico.
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Raccolta di funzioni `static inline` per popcount, ctz/clz, indici LSB/MSB, parita,
  isolamento/reset/estrazione del bit meno significativo (LS1B) e isolamento del MSB.
  Le funzioni sono sicure per `x == 0` secondo le convenzioni documentate e,
  quando disponibili, sfruttano le intrinsics del compilatore cadendo altrimenti
  su implementazioni portabili (SWAR e simili). Il file e pensato per ambienti 64-bit.
 @warning   La numerazione dei bit assume little-endian con bit 0 come LSB.
 @note      Header-only: nessuna dipendenza di linking; usare `#pragma once` o guardie include.
================================================================================
*/
// =============================================
// File: include/core/bitops.h
// Project: chess-engine (core utilities)
// Purpose: Fast 64-bit bit-operations helpers for bitboards and generic use.
// Author: (c) 2025 - MIT License (see LICENSE)
//
// Overview
// --------
// This header provides a small set of **header-only, inline** bit manipulation
// primitives tuned for 64-bit engines (e.g., chess bitboards). It wraps the
// compiler intrinsics when available and falls back to portable implementations
// otherwise. All functions are **well-defined for x == 0** (documented below).
//
// Design principles
// -----------------
// - Header-only: static inline to enable inlining and avoid link-time deps.
// - Ubiquitous operations first: popcount, ctz, clz, LSB/MSB index, isolate/reset LSB.
// - Zero-safe semantics: define the behavior when input == 0 explicitly.
// - Friendly names with `bo_` prefix (Bit Ops) to avoid namespace clashes.
// - Strict 64-bit types (uint64_t). If you need 32-bit, add a thin adapter.
//
// Conventions about *undefined* cases
// -----------------------------------
// - `bo_ctz64(0)`  returns 64
// - `bo_clz64(0)`  returns 64
// - `bo_lsb_index(0)` returns -1 (no set bits)
// - `bo_msb_index(0)` returns -1 (no set bits)
// - Functions marked "precondition: x != 0" will not check at runtime.
//
// Utilities provided
// ------------------
//  * Counting and scans: popcount, ctz, clz, parity
//  * Bit index helpers: lsb_index, msb_index, has_single_bit
//  * Common transforms: isolate/reset/extract LSB (and MSB isolate)
//  * Iteration macro over set bits
//
// Example usage
// -------------
//   uint64_t occ = ...;               // bitboard
//   while (occ) {
//     int sq = bo_extract_lsb_index(&occ); // remove & get lowest set bit
//     // use sq (0..63)
//   }
//
// WARNING: This header assumes a little-endian bit numbering where bit 0 is the
// least-significant bit. This matches the common chess bitboard conventions.
// =============================================
#pragma once

#include <stdint.h>

/* =============================
 * Feature detection for intrinsics
 * ============================= */
#ifndef BO_HAS_BUILTINS
#  if defined(__clang__) || defined(__GNUC__)
#    define BO_HAS_BUILTINS 1
#  else
#    define BO_HAS_BUILTINS 0
#  endif
#endif

/* =============================
 * Basic popcount/ctz/clz (safe on x==0 as documented)
 * ============================= */
/**
 * @brief Conta i bit impostati a 1 in un valore a 64 bit.
 * @param x Valore di input.
 * @return Numero di bit a 1 in `x` (0..64).
 * @details Usa `__builtin_popcountll` quando disponibile, altrimenti un'implementazione SWAR.
 * @complexity O(1) con intrinsics; O(log word size) nella fallback (comunque costante su 64 bit).
 */
static inline int bo_popcount64(uint64_t x) {
#if BO_HAS_BUILTINS
  return __builtin_popcountll((unsigned long long)x);
#else
  /* Hamming weight, SWAR */
  x = x - ((x >> 1) & 0x5555555555555555ULL);
  x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
  x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
  x = x + (x >> 8);
  x = x + (x >> 16);
  x = x + (x >> 32);
  return (int)(x & 0x7F);
#endif
}

/* Count trailing zeros; bo_ctz64(0) == 64 */
/**
 * @brief Conta gli zeri in coda (Trailing Zeros) su 64 bit.
 * @param x Valore di input.
 * @return Numero di zeri finali (0..64). Per `x == 0` restituisce **64**.
 * @details Usa `__builtin_ctzll` se disponibile, altrimenti un ciclo bit-shift portabile.
 * @complexity O(1) con intrinsics; O(posizione LSB) nella fallback.
 */
static inline int bo_ctz64(uint64_t x) {
#if BO_HAS_BUILTINS
  return x ? __builtin_ctzll((unsigned long long)x) : 64;
#else
  if (!x) return 64;
  int n = 0;
  while ((x & 1ULL) == 0ULL) { x >>= 1; ++n; }
  return n;
#endif
}

/* Count leading zeros; bo_clz64(0) == 64 */
/**
 * @brief Conta gli zeri iniziali (Leading Zeros) su 64 bit.
 * @param x Valore di input.
 * @return Numero di zeri iniziali (0..64). Per `x == 0` restituisce **64**.
 * @details Usa `__builtin_clzll` se disponibile, altrimenti un loop da MSB a LSB.
 * @complexity O(1) con intrinsics; O(64) nella fallback.
 */
static inline int bo_clz64(uint64_t x) {
#if BO_HAS_BUILTINS
  return x ? __builtin_clzll((unsigned long long)x) : 64;
#else
  if (!x) return 64; int n = 0;
  for (int i = 63; i >= 0; --i) { if (x & (1ULL << i)) break; ++n; }
  return n;
#endif
}

/* Parity of bits set: returns 0 if even popcount, 1 if odd. */
/**
 * @brief Parita dei bit impostati (0 se pari, 1 se dispari).
 * @param x Valore di input.
 * @return 0 se popcount(x) e pari, 1 se dispari.
 * @details Usa `__builtin_parityll` con GCC/Clang; fallback tramite riduzioni XOR e LUT a 16 voci.
 * @complexity O(1).
 */
static inline int bo_parity64(uint64_t x) {
#if BO_HAS_BUILTINS && defined(__GNUC__)
  return __builtin_parityll((unsigned long long)x);
#else
  x ^= x >> 32; x ^= x >> 16; x ^= x >> 8; x ^= x >> 4; x &= 0xFULL;
  static const int P[16] = {0,1,1,0, 1,0,0,1, 1,0,0,1, 0,1,1,0};
  return P[x];
#endif
}

/* =============================
 * Bit indices and predicates
 * ============================= */

/* Index of least significant set bit (0..63). Returns -1 if x==0. */
/**
 * @brief Indice (0..63) del bit meno significativo impostato (LSB). Restituisce -1 se x == 0.
 * @param x Valore di input.
 * @return Indice dell'LSB o -1 se nessun bit e impostato.
 * @details Implementato tramite `bo_ctz64(x)`; definito anche per `x == 0`.
 * @complexity O(1) (come `bo_ctz64`).
 */
static inline int bo_lsb_index(uint64_t x) {
  if (!x) return -1;
  return bo_ctz64(x);
}

/* Index of most significant set bit (0..63). Returns -1 if x==0. */
/**
 * @brief Indice (0..63) del bit piu significativo impostato (MSB). Restituisce -1 se x == 0.
 * @param x Valore di input.
 * @return Indice del MSB o -1 se nessun bit e impostato.
 * @details Calcolato come `63 - bo_clz64(x)`; definito anche per `x == 0`.
 * @complexity O(1) (come `bo_clz64`).
 */
static inline int bo_msb_index(uint64_t x) {
  if (!x) return -1;
  return 63 - bo_clz64(x);
}

/* True if x has exactly one bit set (power of two). */
/**
 * @brief Verifica se `x` contiene esattamente un singolo bit impostato (potenza di due).
 * @param x Valore di input.
 * @return 1 (true) se un solo bit e a 1; 0 altrimenti.
 * @complexity O(1).
 */
static inline int bo_has_single_bit(uint64_t x) { return x && !(x & (x - 1)); }

/* =============================
 * Common transforms
 * ============================= */

/* Isolate least significant 1-bit (LS1B). For x==0 returns 0. */
/**
 * @brief Isola il bit meno significativo (LS1B): restituisce `x & -x`.
 * @param x Valore di input.
 * @return Maschera con solo l'LS1B impostato; 0 se `x == 0`.
 * @complexity O(1).
 */
static inline uint64_t bo_isolate_lsb(uint64_t x) { return x & (0ULL - x); }

/* Reset least significant 1-bit. For x==0 returns 0. */
/**
 * @brief Azzera il bit meno significativo impostato (LS1B).
 * @param x Valore di input.
 * @return `x` con l'LS1B resettato; 0 se `x == 0`.
 * @complexity O(1).
 */
static inline uint64_t bo_reset_lsb(uint64_t x) { return x & (x - 1ULL); }

/* Extract (return and clear) LS1B from *x. Returns 0 if *x==0. */
/**
 * @brief Estrae (ritorna e azzera) l'LS1B da `*x`.
 * @param x Puntatore al valore; deve essere non NULL.
 * @return Maschera con solo l'LS1B estratto; 0 se `*x == 0`.
 * @details Equivalente a `ls1b = x & -x; *x ^= ls1b; return ls1b;`.
 * @warning Modifica in-place `*x`.
 * @complexity O(1).
 */
static inline uint64_t bo_extract_lsb(uint64_t* x) {
  uint64_t ls1b = bo_isolate_lsb(*x);
  *x ^= ls1b;
  return ls1b;
}

/* Extract LS1B index (0..63) from *x and clear it; returns -1 if *x==0. */
/**
 * @brief Estrae l'indice (0..63) dell'LS1B da `*x` e lo azzera.
 * @param x Puntatore al valore; deve essere non NULL.
 * @return Indice dell'LSB estratto, oppure -1 se `*x == 0`.
 * @details Implementato con `bo_ctz64(*x)` e reset LS1B via `*x &= (*x - 1)`.
 * @warning Modifica in-place `*x`.
 * @complexity O(1) (come `bo_ctz64`).
 */
static inline int bo_extract_lsb_index(uint64_t* x) {
  if (*x == 0ULL) return -1;
  int idx = bo_ctz64(*x);
  *x &= (*x - 1ULL); // reset LS1B
  return idx;
}

/* Isolate the most significant 1-bit. For x==0 returns 0. */
/**
 * @brief Isola il bit piu significativo impostato (MSB).
 * @param x Valore di input.
 * @return Maschera con il solo MSB impostato; 0 se `x == 0`.
 * @details Usa `bo_msb_index(x)` e poi `1ULL << s`.
 * @complexity O(1) (come `bo_msb_index`).
 */
static inline uint64_t bo_isolate_msb(uint64_t x) {
  if (!x) return 0ULL;
  int s = bo_msb_index(x);
  return 1ULL << s;
}

/* =============================
 * Iteration helper
 * ============================= */
/* Iterate all set bits in `bb`, yielding `sq` as [0..63]. Example:
 *   uint64_t bb = ...;
 *   int sq;
 *   BO_FOR_EACH_BIT(bb, sq) {
 *     // use sq
 *   }
 * After the loop, `bb` is zeroed. */
/**
 * @brief Itera su tutti i bit impostati in `bb`, producendo in `sq` gli indici [0..63].
 * @param bb Bitboard variabile (viene AZZERATO a fine iterazione).
 * @param sq Variabile indice locale creata dal macro (tipo `int`).
 * @details Pattern di uso:
 * @code
 *   uint64_t bb = ...;
 *   BO_FOR_EACH_BIT(bb, sq) {
 *     // usa sq
 *   }
 *   // qui bb == 0
 * @endcode
 * @warning `bb` e valutato piu volte; passare una variabile lvalue (non espressioni con side-effect).
 */
#define BO_FOR_EACH_BIT(bb, sq)   for (int sq = bo_extract_lsb_index(&(bb)); sq != -1; sq = bo_extract_lsb_index(&(bb)))

/* =============================
 * Tiny compile-time checks (optional)
 * ============================= */
/**
 * @brief Piccolo static assert portabile (fallback).
 * @param COND Condizione compile-time (boolean).
 * @param MSG  Nome simbolico per la typedef (diventa parte del nome).
 */
#ifndef BO_STATIC_ASSERT
#  define BO_STATIC_ASSERT(COND,MSG) typedef char static_assert_##MSG[(COND)?1:-1]
#endif
BO_STATIC_ASSERT(sizeof(unsigned long long) >= 8, need_64bit_ull);

/* End of include/core/bitops.h */