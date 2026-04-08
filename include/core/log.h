/*
================================================================================
 @file      include/core/log.h
 @brief     API di logging thread-safe con controllo runtime e stripping a compile-time.
 @version   Progetto v1.0; Modulo v1.0
 @author    Sandro Borioni
 @author    ChatGPT
 @details
  Questa API fornisce logging thread-safe con livelli (TRACE, DEBUG, INFO, WARN, ERROR, OFF),
  prefissi opzionali (timestamp, pid:tid, file:line), e la possibilita' di eliminare
  a compile-time le macro sotto una certa soglia definendo LOG_COMPILED_LEVEL.
  Il livello runtime predefinito e' LOG_INFO; puo' essere cambiato con log_set_level().
  Il timestamp (formattato con strftime) appende automaticamente i millisecondi.
================================================================================
*/
// =============================
// File: include/core/log.h
// Desc: Thread-safe logging API with runtime controls and compile-time level stripping
// Notes:
//   - Set -DLOG_COMPILED_LEVEL=LOG_INFO (or higher) to strip TRACE/DEBUG at compile time
//   - Default runtime level is LOG_INFO; change with log_set_level()
//   - Timestamp format default: "%Y-%m-%d %H:%M:%S" (milliseconds appended)
//   - Thread-safety via internal mutex
// =============================
#pragma once
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

/* Compile-time level filter (strip macros below this at compile time)
   Example: add -DLOG_COMPILED_LEVEL=LOG_INFO to CMake to drop TRACE/DEBUG entirely */
#ifndef LOG_COMPILED_LEVEL
#define LOG_COMPILED_LEVEL 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Livelli di log supportati.
 * @details
 *  - LOG_TRACE (0): massimo dettaglio, molto verboso.
 *  - LOG_DEBUG (1): diagnostica di sviluppo.
 *  - LOG_INFO  (2): informazioni operative (predefinito).
 *  - LOG_WARN  (3): condizioni inattese ma non fatali.
 *  - LOG_ERROR (4): errori.
 *  - LOG_OFF   (5): disabilita completamente l'emissione.
 */
typedef enum { LOG_TRACE=0, LOG_DEBUG=1, LOG_INFO=2, LOG_WARN=3, LOG_ERROR=4, LOG_OFF=5 } log_level_t;

/* -------- Runtime configuration -------- */
/* Set the minimum level printed at runtime */
/** @brief Imposta il livello minimo stampato a runtime. @param lvl Livello minimo (vedi log_level_t). */
void log_set_level(log_level_t lvl);
/* Set output file (NULL -> stderr) */
/** @brief Seleziona il sink di output. @param f Puntatore FILE; se NULL, usa stderr. */
void log_set_file(FILE* f);
/* Globally enable/disable logging */
/** @brief Abilita o disabilita globalmente il logging. @param enabled 0=off, non-zero=on. */
void log_set_enabled(int enabled);
/* Include [pid:tid] prefix */
/** @brief Abilita il prefisso [pid:tid]. @param enabled 0=off, non-zero=on. */
void log_set_pid_tid(int enabled);
/* Include timestamp prefix (realtime/monotonic selectable) */
/** @brief Abilita il prefisso timestamp. @param enabled 0=off, non-zero=on. */
void log_set_timestamp(int enabled);
/* Select timestamp clock: 1 = CLOCK_REALTIME, 0 = CLOCK_MONOTONIC */
/** @brief Seleziona la sorgente oraria del timestamp. @param use_rt 1=CLOCK_REALTIME, 0=CLOCK_MONOTONIC. */
void log_set_ts_clock_realtime(int use_rt);
/* Set strftime() timestamp format (milliseconds automatically appended). NULL -> default */
/** @brief Imposta il formato timestamp per strftime. @param fmt Formato; se NULL si usa quello di default. @note I millisecondi sono aggiunti automaticamente. */
void log_set_ts_format(const char* fmt);

/* Convenience: open a file and set as sink. Returns 0 on success. */
/** @brief Apre un file e lo imposta come sink di logging. @param path Percorso del file. @param mode Modalita' fopen (es. "a"). @return 0 se successo, diverso da 0 in errore. */
int  log_open_file(const char* path, const char* mode);
/* Convenience: if sink is a file, close it (no-op for stderr). */
/** @brief Chiude il sink se e' un file (no-op su stderr). */
void log_close_file(void);

/* Core printf-like logger */
/**
 * @brief Logger core stile printf.
 * @param lvl Livello del messaggio.
 * @param file Nome file sorgente (__FILE__).
 * @param line Numero di linea (__LINE__).
 * @param fmt  Formato stile printf.
 * @param ...  Argomenti variadici.
 * @details Thread-safe; applica filtri runtime e formatta i prefissi configurati.
 * @note Attributo GCC per il controllo formato: format(printf,4,5).
 */
void log_msg(log_level_t lvl, const char* file, int line, const char* fmt, ...)
  __attribute__((format(printf,4,5)));

/* -------- Macros with compile-time stripping -------- */
#if LOG_COMPILED_LEVEL <= LOG_TRACE
/** @brief Log TRACE; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_TRACE. */
  #define LOGT(...) log_msg(LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#else
/** @brief Log TRACE; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_TRACE. */
  #define LOGT(...) (void)0
#endif

#if LOG_COMPILED_LEVEL <= LOG_DEBUG
/** @brief Log DEBUG; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_DEBUG. */
  #define LOGD(...) log_msg(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#else
/** @brief Log DEBUG; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_DEBUG. */
  #define LOGD(...) (void)0
#endif

#if LOG_COMPILED_LEVEL <= LOG_INFO
/** @brief Log INFO; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_INFO. */
  #define LOGI(...) log_msg(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#else
/** @brief Log INFO; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_INFO. */
  #define LOGI(...) (void)0
#endif

#if LOG_COMPILED_LEVEL <= LOG_WARN
/** @brief Log WARN; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_WARN. */
  #define LOGW(...) log_msg(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#else
/** @brief Log WARN; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_WARN. */
  #define LOGW(...) (void)0
#endif

#if LOG_COMPILED_LEVEL <= LOG_ERROR
/** @brief Log ERROR; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_ERROR. */
  #define LOGE(...) log_msg(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#else
/** @brief Log ERROR; eliminato a compile-time se LOG_COMPILED_LEVEL > LOG_ERROR. */
  #define LOGE(...) (void)0
#endif

#ifdef __cplusplus
}
#endif
