
// =============================
// File: src/core/log.c
// Desc: Implementation for logging API (see header). POSIX only.
// =============================
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "core/log.h"
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#if !defined(__APPLE__)
#include <sys/syscall.h>
#endif

/* Internal global state */
static struct {
  log_level_t level;
  FILE*       sink;               /* NULL -> stderr */
  int         sink_is_file;       /* track if we opened it */
  int         enabled;
  int         want_pid_tid;
  int         want_ts;
  int         ts_clock_realtime;  /* 1 realtime, 0 monotonic */
  char        ts_fmt[64];         /* strftime format */
  pthread_mutex_t mtx;
} G = {
  .level = LOG_INFO,
  .sink  = NULL,
  .sink_is_file = 0,
  .enabled = 1,
  .want_pid_tid = 1,
  .want_ts = 1,
  .ts_clock_realtime = 1,
  .ts_fmt = "%Y-%m-%d %H:%M:%S",
  .mtx = PTHREAD_MUTEX_INITIALIZER
};

/* ---------- Runtime configuration ---------- */
void log_set_level(log_level_t lvl){ pthread_mutex_lock(&G.mtx); G.level = lvl; pthread_mutex_unlock(&G.mtx); }
void log_set_file(FILE* f){ pthread_mutex_lock(&G.mtx); if(G.sink_is_file && G.sink) fclose(G.sink); G.sink=f; G.sink_is_file=0; pthread_mutex_unlock(&G.mtx); }
void log_set_enabled(int en){ pthread_mutex_lock(&G.mtx); G.enabled = en; pthread_mutex_unlock(&G.mtx); }
void log_set_pid_tid(int en){ pthread_mutex_lock(&G.mtx); G.want_pid_tid = en; pthread_mutex_unlock(&G.mtx); }
void log_set_timestamp(int en){ pthread_mutex_lock(&G.mtx); G.want_ts = en; pthread_mutex_unlock(&G.mtx); }
void log_set_ts_clock_realtime(int use_rt){ pthread_mutex_lock(&G.mtx); G.ts_clock_realtime = use_rt; pthread_mutex_unlock(&G.mtx); }
void log_set_ts_format(const char* fmt){
  pthread_mutex_lock(&G.mtx);
  if (fmt && *fmt) { strncpy(G.ts_fmt, fmt, sizeof(G.ts_fmt)-1); G.ts_fmt[sizeof(G.ts_fmt)-1] = '\0'; }
  else { strcpy(G.ts_fmt, "%Y-%m-%d %H:%M:%S"); }
  pthread_mutex_unlock(&G.mtx);
}

int log_open_file(const char* path, const char* mode){
  if(!path) return -1; if(!mode) mode = "a";
  FILE* f = fopen(path, mode);
  if(!f) return -1;
  pthread_mutex_lock(&G.mtx);
  if(G.sink_is_file && G.sink) fclose(G.sink);
  G.sink = f; G.sink_is_file = 1;
  pthread_mutex_unlock(&G.mtx);
  return 0;
}

void log_close_file(void){
  pthread_mutex_lock(&G.mtx);
  if(G.sink_is_file && G.sink){ fclose(G.sink); }
  G.sink = NULL; G.sink_is_file = 0;
  pthread_mutex_unlock(&G.mtx);
}

/* ---------- Helpers ---------- */
static const char* lvl_name(log_level_t L){
  switch(L){ case LOG_TRACE: return "TRACE"; case LOG_DEBUG: return "DEBUG"; case LOG_INFO: return "INFO";
             case LOG_WARN: return "WARN"; case LOG_ERROR: return "ERROR"; default: return "?"; }
}

static void format_timestamp(char* buf, size_t n, int use_rt, const char* fmt){
  struct timespec ts;
  clock_gettime(use_rt ? CLOCK_REALTIME : CLOCK_MONOTONIC, &ts);
  struct tm tm; localtime_r(&ts.tv_sec, &tm);
  char datebuf[48]; strftime(datebuf, sizeof(datebuf), fmt, &tm);
  int ms = (int)(ts.tv_nsec / 1000000);
  snprintf(buf, n, "%s.%03d", datebuf, ms);
}

static unsigned long get_tid_fallback(void){
#if defined(SYS_gettid) && !defined(__APPLE__)
  return (unsigned long)syscall(SYS_gettid);
#else
  return (unsigned long)pthread_self();
#endif
}

/* ---------- Core logger ---------- */
void log_msg(log_level_t lvl, const char* file, int line, const char* fmt, ...){
  /* fast-path check outside the lock */
  pthread_mutex_lock(&G.mtx);
  int enabled = G.enabled && (lvl >= G.level);
  FILE* out = G.sink ? G.sink : stderr;
  int want_ts = G.want_ts, use_rt = G.ts_clock_realtime, want_pt = G.want_pid_tid;
  char tsf[64]; strcpy(tsf, G.ts_fmt);
  pthread_mutex_unlock(&G.mtx);
  if(!enabled) return;

  char tsbuf[80] = {0};
  if (want_ts) format_timestamp(tsbuf, sizeof(tsbuf), use_rt, tsf);

  int pid = (int)getpid();
  unsigned long tid = get_tid_fallback();

  /* header */
  if (want_ts && want_pt)
    fprintf(out, "[%s] [%s] [%d:%lu] %s:%d: ", tsbuf, lvl_name(lvl), pid, tid, file, line);
  else if (want_ts)
    fprintf(out, "[%s] [%s] %s:%d: ", tsbuf, lvl_name(lvl), file, line);
  else if (want_pt)
    fprintf(out, "[%s] [%d:%lu] %s:%d: ", lvl_name(lvl), pid, tid, file, line);
  else
    fprintf(out, "[%s] %s:%d: ", lvl_name(lvl), file, line);

  /* body */
  va_list ap; va_start(ap, fmt); vfprintf(out, fmt, ap); va_end(ap);
  fputc('\n', out);
  fflush(out);
}
