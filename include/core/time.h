/*
================================================================================
 @file      include/core/time_now.h
 @brief     Helpers per ottenere timestamp ad alta risoluzione (ns/ms).
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Fornisce due funzioni per ottenere il tempo corrente in nanosecondi e millisecondi.
  L'origine dell'orologio (realtime o monotonic) dipende dall'implementazione nel .c.
  Usare un orologio monotonic quando serve evitare regressioni temporali dovute
  a modifiche dell'orario di sistema.
================================================================================
*/
#pragma once
#include <stdint.h>
/**
 * @brief Restituisce il timestamp corrente in nanosecondi.
 * @return Valore a 64 bit con i nanosecondi correnti.
 * @details L'origine del tempo (CLOCK_REALTIME o CLOCK_MONOTONIC) dipende
 *  dall'implementazione. Il valore e' non decrescente solo se si usa un clock monotonic.
 *  Eventuali errori della chiamata di basso livello sono dipendenti dalla piattaforma;
 *  in molte implementazioni la funzione non fallisce in condizioni normali.
 */
uint64_t time_now_ns(void);
/**
 * @brief Restituisce il timestamp corrente in millisecondi.
 * @return Valore a 64 bit con i millisecondi correnti.
 * @details Tipicamente derivato da time_now_ns() tramite divisione per 1e6 (troncamento).
 *  L'origine del tempo e le garanzie di monotonicita' sono le stesse di time_now_ns().
 */
uint64_t time_now_ms(void);
